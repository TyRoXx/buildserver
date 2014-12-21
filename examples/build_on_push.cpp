#include "server/find_cmake.hpp"
#include "server/find_gcc.hpp"
#include "server/find_executable.hpp"
#include "server/cmake.hpp"
#include <silicium/http/receive_request.hpp>
#include <silicium/http/uri.hpp>
#include <silicium/asio/tcp_acceptor.hpp>
#include <silicium/asio/writing_observable.hpp>
#include <silicium/asio/posting_observable.hpp>
#include <silicium/observable/spawn_coroutine.hpp>
#include <silicium/observable/spawn_observable.hpp>
#include <silicium/observable/erased_observer.hpp>
#include <silicium/observable/total_consumer.hpp>
#include <silicium/observable/thread.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/http/generate_response.hpp>
#include <silicium/boost_threading.hpp>
#include <silicium/run_process.hpp>
#include <silicium/range_value.hpp>
#include <boost/program_options.hpp>
#include <boost/optional.hpp>
#include <boost/range/algorithm/equal.hpp>
#include <boost/thread.hpp>
#include <unordered_map>
#include <functional>
#include <iostream>

namespace web
{
	enum class request_handler_result
	{
		handled,
		not_found
	};

	typedef std::function<request_handler_result (boost::asio::ip::tcp::socket &, Si::iterator_range<Si::memory_range const *>, Si::spawn_context)> request_handler;

	request_handler make_directory(std::unordered_map<Si::range_value<Si::memory_range>, request_handler> entries)
	{
		return [entries = std::move(entries)](
			boost::asio::ip::tcp::socket &client,
			Si::iterator_range<Si::memory_range const *> remaining_path,
			Si::spawn_context yield
		) -> request_handler_result
		{
			Si::memory_range current_path_element = (remaining_path.empty() ? Si::make_c_str_range("") : remaining_path.front());
			auto entry = entries.find(Si::make_range_value(current_path_element));
			if (entry == entries.end())
			{
				return request_handler_result::not_found;
			}
			if (!remaining_path.empty())
			{
				remaining_path.pop_front();
			}
			return entry->second(client, remaining_path, yield);
		};
	}
}

namespace
{
	template <class AsyncWriteStream, class YieldContext, class Status, class StatusText>
	void quick_final_response(AsyncWriteStream &client, YieldContext &&yield, Status &&status, StatusText &&status_text, std::string const &content)
	{
		std::vector<char> response;
		{
			auto response_writer = Si::make_container_sink(response);
			Si::http::generate_status_line(response_writer, "HTTP/1.0", std::forward<Status>(status), std::forward<StatusText>(status_text));
			Si::http::generate_header(response_writer, "Content-Length", boost::lexical_cast<Si::noexcept_string>(content.size()));
			Si::append(response_writer, "\r\n");
			Si::append(response_writer, content);
		}

		//you can handle the error if you want
		boost::system::error_code error = Si::asio::write(client, Si::make_memory_range(response), yield);

		//ignore shutdown failures, they do not matter here
		client.shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
	}

	struct notification
	{
	};

	template <class Observer>
	struct saturating_notifier
	{
		typedef notification element_type;

		template <class ActualObserver>
		void async_get_one(ActualObserver &&observer)
		{
			return Si::visit<void>(
				m_observer_or_notification,
				[this, &observer](Observer &my_observer) mutable
				{
					assert(!my_observer.get());
					my_observer = Observer(observer);
				},
				[this, &observer](notification)
				{
					m_observer_or_notification = Observer();
					std::move(observer).got_element(notification());
				}
			);
		}

		void notify()
		{
			Si::visit<void>(
				m_observer_or_notification,
				[this](Observer &observer)
				{
					if (observer.get())
					{
						Si::exchange(observer, Observer()).got_element(notification());
					}
					else
					{
						m_observer_or_notification = notification();
					}
				},
				[](notification const &)
				{
				}
			);
		}

	private:

		Si::fast_variant<Observer, notification> m_observer_or_notification;
	};

	template <class YieldContext, class NotifierObserver>
	web::request_handler_result notify(
		boost::asio::ip::tcp::socket &client,
		YieldContext &&yield,
		Si::noexcept_string const &path,
		Si::noexcept_string const &secret,
		saturating_notifier<NotifierObserver> &notifier)
	{
		if (std::string::npos == path.find(secret))
		{
			quick_final_response(client, yield, "403", "Forbidden", "the path does not contain the correct secret");
			return web::request_handler_result::handled;
		}

		notifier.notify();

		quick_final_response(client, yield, "200", "OK", "the server has been successfully notified");
		return web::request_handler_result::handled;
	}

	template <class YieldContext, class NotifierObserver>
	void serve_client(boost::asio::ip::tcp::socket &client, YieldContext &&yield, Si::noexcept_string const &secret, saturating_notifier<NotifierObserver> &notifier)
	{
		Si::error_or<boost::optional<Si::http::request>> maybe_request = Si::http::receive_request(client, yield);
		if (maybe_request.is_error())
		{
			std::cerr << client.remote_endpoint().address() << ": " << maybe_request.error() << '\n';
			return;
		}

		if (!maybe_request.get())
		{
			return;
		}

		Si::http::request const &request = *maybe_request.get();

		boost::optional<Si::http::uri> relative_uri = Si::http::parse_uri(Si::make_memory_range(request.path));
		if (!relative_uri)
		{
			return;
		}

		auto handle_request = web::make_directory({
			{
				Si::make_c_str_range(""),
				web::request_handler([](boost::asio::ip::tcp::socket &client, Si::iterator_range<Si::memory_range const *>, Si::spawn_context yield)
				{
					quick_final_response(client, yield, "200", "OK", "<h1>Overview</h1>");
					return web::request_handler_result::handled;
				})
			},
			{
				Si::make_c_str_range("notify"),
				web::request_handler([request, &secret, &notifier](boost::asio::ip::tcp::socket &client, Si::iterator_range<Si::memory_range const *>, Si::spawn_context yield)
				{
					return notify(client, yield, request.path, secret, notifier);
				})
			}
		});

		switch (handle_request(client, Si::make_contiguous_range(relative_uri->path), yield))
		{
		case web::request_handler_result::handled:
			break;

		case web::request_handler_result::not_found:
			quick_final_response(client, yield, "404", "Not Found", "404 - Not Found");
			break;
		}
	}

	struct options
	{
		std::string repository;
		boost::uint16_t port;
		Si::noexcept_string secret;
	};

	boost::optional<options> parse_options(int argc, char **argv)
	{
		options result;
		result.port = 8080;

		boost::program_options::options_description desc("Allowed options");
		desc.add_options()
			("help", "produce help message")
			("repository,r", boost::program_options::value(&result.repository), "the URI for git cloning the code")
			("port,p", boost::program_options::value(&result.port), "port to listen on for POSTed push notifications")
			("secret,s", boost::program_options::value(&result.secret), "a string that needs to be in the query for the notification to be accepted")
		;

		boost::program_options::positional_options_description positional;
		positional.add("repository", 1);
		positional.add("port", 1);
		positional.add("secret", 1);
		boost::program_options::variables_map vm;
		try
		{
			boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(positional).run(), vm);
		}
		catch (boost::program_options::error const &ex)
		{
			std::cerr
				<< ex.what() << '\n'
				<< desc << "\n";
			return boost::none;
		}

		boost::program_options::notify(vm);

		if (vm.count("help"))
		{
		    std::cerr << desc << "\n";
			return boost::none;
		}

		if (result.repository.empty())
		{
			std::cerr << "Missing option value --repository\n";
			std::cerr << desc << "\n";
			return boost::none;
		}

		return std::move(result);
	}

	enum class build_result
	{
		success,
		failure,
		missing_dependency
	};

	Si::error_or<boost::optional<boost::filesystem::path>> find_git()
	{
		return buildserver::find_executable_unix("git", {});
	}

	void git_clone(std::string const &repository, boost::filesystem::path const &destination, boost::filesystem::path const &git_exe)
	{
		Si::process_parameters parameters;
		parameters.executable = git_exe;
		parameters.current_path = destination.parent_path();
		parameters.arguments = {"clone", repository, destination.string()};
		if (Si::run_process(parameters) != 0)
		{
			throw std::runtime_error("git-clone failed");
		}
	}

	build_result build(std::string const &repository, boost::filesystem::path const &workspace)
	{
		boost::optional<boost::filesystem::path> maybe_git = find_git().get();
		if (!maybe_git)
		{
			return build_result::missing_dependency;
		}

		boost::optional<boost::filesystem::path> maybe_cmake = buildserver::find_cmake().get();
		if (!maybe_cmake)
		{
			return build_result::missing_dependency;
		}

		boost::filesystem::path const source = workspace / "source.git";
		git_clone(repository, source, *maybe_git);

		boost::filesystem::path const build = workspace / "build";
		boost::filesystem::create_directories(build);

		buildserver::cmake_exe cmake(*maybe_cmake);
		boost::system::error_code error = cmake.generate(source, build, {});
		if (error)
		{
			boost::throw_exception(boost::system::system_error(error));
		}

		error = cmake.build(build, boost::thread::hardware_concurrency());
		if (error)
		{
			boost::throw_exception(boost::system::system_error(error));
		}

		return build_result::success;
	}
}

int main(int argc, char **argv)
{
	auto parsed_options = parse_options(argc, argv);
	if (!parsed_options)
	{
		return 1;
	}

	//TODO: make the workspace configurable
	boost::filesystem::path const &workspace = boost::filesystem::current_path();

	boost::asio::io_service io;

	Si::spawn_coroutine([&](Si::spawn_context yield)
	{
		saturating_notifier<Si::erased_observer<notification>> notifier;

		auto const &secret = parsed_options->secret;
		Si::spawn_observable(Si::transform(
			Si::asio::make_tcp_acceptor(boost::asio::ip::tcp::acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(), parsed_options->port))),
			[&secret, &notifier](Si::asio::tcp_acceptor_result maybe_client) -> Si::nothing
			{
				auto client = maybe_client.get();
				Si::spawn_coroutine([client, &secret, &notifier](Si::spawn_context yield)
				{
					serve_client(*client, yield, secret, notifier);
				});
				return{};
			}
		));

		for (;;)
		{
			boost::optional<notification> notification_ = yield.get_one(Si::ref(notifier));
			assert(notification_);
			std::cerr << "Received a notification\n";
			try
			{
				boost::optional<build_result> result = yield.get_one(
					Si::asio::make_posting_observable(
						io,
						Si::make_thread_observable<Si::boost_threading>([&]()
						{
							boost::filesystem::remove_all(workspace);
							boost::filesystem::create_directories(workspace);
							return build(parsed_options->repository, workspace);
						})
					)
				);
				assert(result);
				switch (*result)
				{
				case build_result::success:
					std::cerr << "Build success\n";
					break;

				case build_result::failure:
					std::cerr << "Build failure\n";
					break;

				case build_result::missing_dependency:
					std::cerr << "Build dependency missing\n";
					break;
				}
			}
			catch (std::exception const &ex)
			{
				std::cerr << "Exception: " <<  ex.what() << '\n';
			}
		}
	});

	io.run();
}
