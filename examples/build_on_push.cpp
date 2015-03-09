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
#include <silicium/observable/while.hpp>
#include <silicium/observable/thread.hpp>
#include <silicium/open.hpp>
#include <silicium/fast_variant.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/http/generate_response.hpp>
#include <silicium/std_threading.hpp>
#include <silicium/run_process.hpp>
#include <silicium/range_value.hpp>
#include <silicium/html.hpp>
#include <silicium/async_process.hpp>
#include <boost/program_options.hpp>
#include <boost/optional.hpp>
#include <boost/range/algorithm/equal.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem/operations.hpp>
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

	typedef std::function<request_handler_result (boost::asio::ip::tcp::socket &, Si::http::request const &, Si::iterator_range<Si::memory_range const *>, Si::spawn_context)> request_handler;

	request_handler make_directory(std::unordered_map<Si::range_value<Si::memory_range>, request_handler> entries)
	{
		return [entries
#if SILICIUM_COMPILER_HAS_EXTENDED_CAPTURE
			= std::move(entries)
#endif
			](
			boost::asio::ip::tcp::socket &client,
			Si::http::request const &request,
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
			return entry->second(client, request, remaining_path, yield);
		};
	}
}

namespace
{
	template <class AsyncWriteStream, class YieldContext, class Status, class StatusText>
	void quick_final_response(AsyncWriteStream &client, YieldContext &&yield, Status &&status, StatusText &&status_text, Si::memory_range const &content)
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
			quick_final_response(client, yield, "403", "Forbidden", Si::make_c_str_range("the path does not contain the correct secret"));
			return web::request_handler_result::handled;
		}

		notifier.notify();

		quick_final_response(client, yield, "200", "OK", Si::make_c_str_range("the server has been successfully notified"));
		return web::request_handler_result::handled;
	}

	template <class YieldContext>
	void serve_client(boost::asio::ip::tcp::socket &client, YieldContext &&yield, web::request_handler const &root_request_handler)
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

		switch (root_request_handler(client, request, Si::make_contiguous_range(relative_uri->path), yield))
		{
		case web::request_handler_result::handled:
			break;

		case web::request_handler_result::not_found:
			quick_final_response(client, yield, "404", "Not Found", Si::make_c_str_range("404 - Not Found"));
			break;
		}
	}

	enum class build_result
	{
		success,
		failure
	};

	struct overview_state
	{
		bool is_building = false;
		boost::optional<build_result> last_result;
	};

	template <class NotifierObserver>
	web::request_handler make_root_request_handler(Si::noexcept_string const &secret, saturating_notifier<NotifierObserver> &notifier, overview_state const &overview)
	{
		auto handle_request = web::make_directory({
			{
				Si::make_c_str_range(""),
				web::request_handler([&overview](boost::asio::ip::tcp::socket &client, Si::http::request const &, Si::iterator_range<Si::memory_range const *>, Si::spawn_context yield)
				{
					std::vector<char> content;
					auto html = Si::html::make_generator(Si::make_container_sink(content));
					html.element("html", [&]()
					{
						html.element("head", [&]()
						{
							html.element("title", [&]()
							{
								html.write("buildserver overview");
							});
						});
						html.element("body", [&]()
						{
							html.element("h1", [&]()
							{
								html.write("Overview");
							});
							html.element("p", [&]()
							{
								html.write(overview.is_building ? "building.." : "idle");
							});
							if (overview.last_result)
							{
								html.element("p", [&]()
								{
									html.write("last build ");
									switch (*overview.last_result)
									{
									case build_result::success:
										html.write("succeeded");
										break;
									case build_result::failure:
										html.write("failed");
										break;
									}
								});
							}
						});
					});
					quick_final_response(client, yield, "200", "OK", Si::make_memory_range(content));
					return web::request_handler_result::handled;
				})
			},
			{
				Si::make_c_str_range("notify"),
				web::request_handler([&secret, &notifier](boost::asio::ip::tcp::socket &client, Si::http::request const &request, Si::iterator_range<Si::memory_range const *>, Si::spawn_context yield)
				{
					return notify(client, yield, request.path, secret, notifier);
				})
			}
		});
		return handle_request;
	}

	struct options
	{
		std::string repository;
		boost::uint16_t port;
		Si::noexcept_string secret;
		boost::filesystem::path workspace;
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
			("workspace,w", boost::program_options::value(&result.workspace), "")
		;

		boost::program_options::positional_options_description positional;
		positional.add("repository", 1);
		positional.add("port", 1);
		positional.add("secret", 1);
		positional.add("workspace", 1);
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

		if (result.workspace.empty())
		{
			std::cerr << "Missing option value --workspace\n";
			std::cerr << desc << "\n";
			return boost::none;
		}

		return std::move(result);
	}

	Si::error_or<Si::optional<boost::filesystem::path>> find_git()
	{
#ifdef _WIN32
		return buildserver::find_file_in_directories("git.exe", {"C:\\Program Files (x86)\\Git\\bin"});
#else
		return buildserver::find_executable_unix("git", {});
#endif
	}

	void git_clone(std::string const &repository, boost::filesystem::path const &destination, boost::filesystem::path const &git_exe)
	{
		Si::async_process_parameters parameters;
		parameters.executable = git_exe;
		parameters.current_path = destination.parent_path();
		parameters.arguments = {"clone", repository, destination.string()};
		Si::pipe standard_output_and_error = SILICIUM_MOVE_IF_COMPILER_LACKS_RVALUE_QUALIFIERS(Si::make_pipe().get());
		Si::file_handle standard_input = SILICIUM_MOVE_IF_COMPILER_LACKS_RVALUE_QUALIFIERS(Si::open_reading("/dev/null").get());
		Si::async_process process = SILICIUM_MOVE_IF_COMPILER_LACKS_RVALUE_QUALIFIERS(Si::launch_process(
			parameters,
			standard_input.handle,
			standard_output_and_error.write.handle,
			standard_output_and_error.write.handle
		).get());
		boost::asio::io_service io;
		Si::spawn_observable(
			Si::while_(
				Si::transform(
					Si::process_output(Si::to_unique(Si::make_asio_file_stream<Si::process_output::stream>(io, std::move(standard_output_and_error.read)))),
					[](Si::error_or<Si::memory_range> element)
					{
						if (element.is_error())
						{
							return false;
						}
						else
						{
							std::cerr.write(element->begin(), element->size());
							return true;
						}
					}
				),
				[](bool v) { return v; }
			)
		);
		standard_output_and_error.write.close();
		io.run();
		int exit_code = process.wait_for_exit().get();
		if (exit_code != 0)
		{
			throw std::runtime_error("git-clone failed");
		}
	}

	build_result build(
		std::string const &repository,
		boost::filesystem::path const &workspace,
		boost::filesystem::path const &git,
		boost::filesystem::path const &cmake)
	{
		boost::filesystem::path const source = workspace / "source.git";
		git_clone(repository, source, git);

		boost::filesystem::path const build = workspace / "build";
		boost::filesystem::create_directories(build);

		buildserver::cmake_exe cmake_builder(cmake);
		boost::system::error_code error = cmake_builder.generate(source, build, boost::unordered_map<std::string, std::string>{});
		if (error)
		{
			boost::throw_exception(boost::system::system_error(error));
		}

		error = cmake_builder.build(build, boost::thread::hardware_concurrency());
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

	Si::optional<boost::filesystem::path> maybe_git = find_git().get();
	if (!maybe_git)
	{
		std::cerr << "Could not find Git\n";
		return 1;
	}

	Si::optional<boost::filesystem::path> maybe_cmake = buildserver::find_cmake().get();
	if (!maybe_cmake)
	{
		std::cerr << "Could not find CMake\n";
		return 1;
	}

	boost::asio::io_service io;

	Si::spawn_coroutine([&](Si::spawn_context yield)
	{
		saturating_notifier<Si::erased_observer<notification>> notifier;
		overview_state overview;

		web::request_handler root_request_handler = make_root_request_handler(parsed_options->secret, notifier, overview);
		Si::spawn_observable(Si::transform(
			Si::asio::make_tcp_acceptor(boost::asio::ip::tcp::acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(), parsed_options->port))),
			[&root_request_handler](Si::asio::tcp_acceptor_result maybe_client) -> Si::nothing
			{
				auto client = maybe_client.get();
				Si::spawn_coroutine([client, &root_request_handler](Si::spawn_context yield)
				{
					serve_client(*client, yield, root_request_handler);
				});
				return{};
			}
		));

		for (;;)
		{
			Si::optional<notification> notification_ = yield.get_one(Si::ref(notifier));
			assert(notification_);
			std::cerr << "Received a notification\n";
			try
			{
				overview.is_building = true;
				Si::optional<std::future<build_result>> maybe_result = yield.get_one(
					Si::asio::make_posting_observable(
						io,
						Si::make_thread_observable<Si::std_threading>([&]()
						{
							boost::filesystem::remove_all(parsed_options->workspace);
							boost::filesystem::create_directories(parsed_options->workspace);
							return build(parsed_options->repository, parsed_options->workspace, *maybe_git, *maybe_cmake);
						})
					)
				);
				assert(maybe_result);
				auto const result = maybe_result->get();
				switch (result)
				{
				case build_result::success:
					std::cerr << "Build success\n";
					break;

				case build_result::failure:
					std::cerr << "Build failure\n";
					break;
				}
				overview.last_result = result;
			}
			catch (std::exception const &ex)
			{
				std::cerr << "Exception: " <<  ex.what() << '\n';
				overview.last_result = build_result::failure;
			}
			overview.is_building = false;
		}
	});

	io.run();
}
