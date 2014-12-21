#include <boost/program_options.hpp>
#include <boost/optional.hpp>
#include <iostream>
#include <boost/thread.hpp>
#include <silicium/http/receive_request.hpp>
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
#include <functional>
#include "server/find_cmake.hpp"
#include "server/find_gcc.hpp"
#include "server/find_executable.hpp"
#include "server/cmake.hpp"

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
	struct notification_server
	{
		typedef notification element_type;

		notification_server(boost::asio::io_service &io, boost::asio::ip::tcp::endpoint endpoint, Si::noexcept_string secret)
			: m_server(
				Si::erase_unique(
					Si::transform(
						Si::asio::make_tcp_acceptor(boost::asio::ip::tcp::acceptor(io, endpoint)),
						[this](Si::asio::tcp_acceptor_result maybe_client) -> Si::nothing
						{
							auto client = maybe_client.get();
							Si::spawn_coroutine([this, client](Si::spawn_context yield)
							{
								serve_client(*client, yield);
							});
							return{};
						}
					)
				)
			)
			, m_is_running(false)
			, m_secret(std::move(secret))
		{
		}

		template <class ActualObserver>
		void async_get_one(ActualObserver &&observer)
		{
			if (!m_is_running)
			{
				m_server.start();
				m_is_running = true;
			}
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

	private:

		Si::total_consumer<Si::unique_observable<Si::nothing>> m_server;
		bool m_is_running;
		Si::noexcept_string m_secret;
		Si::fast_variant<Observer, notification> m_observer_or_notification;

		template <class YieldContext>
		void serve_client(boost::asio::ip::tcp::socket &client, YieldContext &&yield)
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
			if (std::string::npos == request.path.find(m_secret))
			{
				quick_final_response(client, yield, "403", "Forbidden", "the path does not contain the correct secret");
				return;
			}

			Si::visit<void>(
				m_observer_or_notification,
				[](Observer &observer)
				{
					Si::exchange(observer, Observer()).got_element(notification());
				},
				[](notification const &)
				{
				}
			);

			quick_final_response(client, yield, "200", "OK", "the server has been successfully notified");
		}
	};

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
		notification_server<Si::erased_observer<notification>> notifications(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(), parsed_options->port), parsed_options->secret);
		for (;;)
		{
			boost::optional<notification> notification_ = yield.get_one(Si::ref(notifications));
			assert(notification_);
			std::cerr << "Received a notification\n";
			try
			{
				Si::spawn_observable(
					Si::transform(
						Si::asio::make_posting_observable(
							io,
							Si::make_thread_observable<Si::boost_threading>([&]()
							{
								boost::filesystem::remove_all(workspace);
								boost::filesystem::create_directories(workspace);
								return build(parsed_options->repository, workspace);
							})
						),
						[](build_result result)
						{
							switch (result)
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
							return Si::nothing();
						}
					)
				);
			}
			catch (std::exception const &ex)
			{
				std::cerr << "Exception: " <<  ex.what() << '\n';
			}
		}
	});

	io.run();
}
