#include "nanoweb/nanoweb.hpp"
#include "server/find_cmake.hpp"
#include "server/find_executable.hpp"
#include "server/find_git.hpp"
#include "server/cmake.hpp"
#include <silicium/asio/tcp_acceptor.hpp>
#include <silicium/asio/writing_observable.hpp>
#include <silicium/asio/posting_observable.hpp>
#include <silicium/observable/spawn_observable.hpp>
#include <silicium/observable/erased_observer.hpp>
#include <silicium/observable/total_consumer.hpp>
#include <silicium/observable/while.hpp>
#include <silicium/observable/thread.hpp>
#include <silicium/sink/ostream_sink.hpp>
#include <silicium/source/range_source.hpp>
#include <ventura/open.hpp>
#include <silicium/variant.hpp>
#include <silicium/std_threading.hpp>
#include <ventura/run_process.hpp>
#include <silicium/html/generator.hpp>
#include <ventura/async_process.hpp>
#include <ventura/absolute_path.hpp>
#include <ventura/path_segment.hpp>
#include <boost/program_options.hpp>
#include <boost/optional.hpp>
#include <boost/range/algorithm/equal.hpp>
#include <boost/thread.hpp>
#include <unordered_map>
#include <functional>
#include <iostream>

namespace
{
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

		Si::variant<Observer, notification> m_observer_or_notification;
	};

	template <class YieldContext, class NotifierObserver>
	nanoweb::request_handler_result notify(
		boost::asio::ip::tcp::socket &client,
		YieldContext &&yield,
		Si::noexcept_string const &path,
		Si::noexcept_string const &secret,
		saturating_notifier<NotifierObserver> &notifier)
	{
		if (std::string::npos == path.find(secret))
		{
			nanoweb::quick_final_response(client, yield, "403", "Forbidden", Si::make_c_str_range("the path does not contain the correct secret"));
			return nanoweb::request_handler_result::handled;
		}

		notifier.notify();

		nanoweb::quick_final_response(client, yield, "200", "OK", Si::make_c_str_range("the server has been successfully notified"));
		return nanoweb::request_handler_result::handled;
	}

	enum class build_result
	{
		success,
		failure
	};

	struct step_history
	{
		bool is_building = false;
		Si::optional<build_result> last_result;
	};
	
	template <class CharSink, class StepRange>
	void render_overview_page(CharSink &&rendered, StepRange &&steps)
	{
		auto doc = Si::html::make_generator(std::forward<CharSink>(rendered));
		doc("html", [&]()
		{
			doc("head", [&]()
			{
				doc("title", [&]()
				{
					doc.write("buildserver overview");
				});
			});
			doc("body", [&]()
			{
				doc("h1", [&]()
				{
					doc.write("Overview");
				});
				doc("table",
					[&]()
				{
					doc.attribute("border", "1");
				},
					[&]()
				{
					for (auto &&step : steps)
					{
						doc("tr", [&]()
						{
							doc("td", [&]()
							{
								doc.write(step.first);
							});
							step_history const &history = step.second;
							doc("td", [&]()
							{
								doc.write(history.is_building ? "building.." : "idle");
							});
							doc("td", [&]()
							{
								if (!history.last_result)
								{
									doc.write("not built");
									return;
								}
								doc.write("last build ");
								switch (*history.last_result)
								{
								case build_result::success:
									doc.write("succeeded");
									break;
								case build_result::failure:
									doc.write("failed");
									break;
								}
							});
						});
					}
				});
			});
		});
	}

	struct step_history_registry
	{
		std::map<Si::noexcept_string, step_history> name_to_step;
	};

	template <class NotifierObserver>
	nanoweb::request_handler make_root_request_handler(Si::noexcept_string const &secret, saturating_notifier<NotifierObserver> &notifier, step_history_registry const &registry)
	{
		auto handle_request = nanoweb::make_directory({
			{
				Si::make_c_str_range(""),
				nanoweb::request_handler([&registry](boost::asio::ip::tcp::socket &client, Si::http::request const &, Si::iterator_range<Si::memory_range const *>, Si::spawn_context yield)
				{
					std::vector<char> content;
					render_overview_page(Si::make_container_sink(content), registry.name_to_step);
					nanoweb::quick_final_response(client, yield, "200", "OK", Si::make_memory_range(content));
					return nanoweb::request_handler_result::handled;
				})
			},
			{
				Si::make_c_str_range("notify"),
				nanoweb::request_handler([&secret, &notifier](boost::asio::ip::tcp::socket &client, Si::http::request const &request, Si::iterator_range<Si::memory_range const *>, Si::spawn_context yield)
				{
					return notify(client, yield, request.path, secret, notifier);
				})
			}
		});
		return handle_request;
	}

	typedef Si::os_string git_repository_address;

	struct options
	{
		git_repository_address repository;
		boost::uint16_t port;
		Si::noexcept_string secret;
		ventura::absolute_path workspace;
	};

	boost::optional<options> parse_options(int argc, char **argv)
	{
		options result;
		result.port = 8080;
		
		boost::program_options::options_description desc("Allowed options");
		desc.add_options()
			("help", "produce help message")
			("repository,r", boost::program_options::
#ifdef _WIN32
				wvalue
#else
				value
#endif
				(&result.repository), "the URI for git cloning the code")
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
	
	int run_process(ventura::async_process_parameters const &parameters, Si::sink<char, Si::success> &output)
	{
		Si::pipe standard_output_and_error = SILICIUM_MOVE_IF_COMPILER_LACKS_RVALUE_QUALIFIERS(Si::make_pipe().get());
		Si::file_handle standard_input = SILICIUM_MOVE_IF_COMPILER_LACKS_RVALUE_QUALIFIERS(ventura::open_reading(Si::native_path_string(SILICIUM_SYSTEM_LITERAL("/dev/null"))).get());
		ventura::async_process process = SILICIUM_MOVE_IF_COMPILER_LACKS_RVALUE_QUALIFIERS(ventura::launch_process(
			parameters,
			standard_input.handle,
			standard_output_and_error.write.handle,
			standard_output_and_error.write.handle,
			std::vector<std::pair<Si::os_char const *, Si::os_char const *>>(),
			ventura::environment_inheritance::inherit
		).get());
		boost::asio::io_service io;

		boost::promise<void> stop_polling;
		boost::shared_future<void> stopped_polling = stop_polling.get_future().share();

		ventura::experimental::read_from_anonymous_pipe(io, Si::ref_sink(output), std::move(standard_output_and_error.read), stopped_polling);
		standard_output_and_error.write.close();
		io.run();
		int exit_code = process.wait_for_exit().get();
		return exit_code;
	}

	void git_clone(git_repository_address const &repository, ventura::absolute_path const &destination, ventura::path_segment const &clone_name, ventura::absolute_path const &git_exe, Si::sink<char, Si::success> &output)
	{
		ventura::async_process_parameters parameters;
		parameters.executable = git_exe;
		parameters.current_path = destination;
		parameters.arguments.emplace_back(Si::to_os_string("clone"));
		parameters.arguments.emplace_back(repository);
		parameters.arguments.emplace_back((destination / clone_name).c_str());
		int exit_code = run_process(parameters, output);
		if (exit_code != 0)
		{
			throw std::runtime_error("git-clone failed");
		}
	}

	build_result run_test(ventura::absolute_path const &build_dir, Si::sink<char, Si::success> &output)
	{
		ventura::absolute_path const test_dir = build_dir / "test";
		ventura::absolute_path const test_exe = test_dir / "unit_test";
		ventura::async_process_parameters parameters;
		parameters.executable = test_exe;
		parameters.current_path = test_dir;
		int exit_code = run_process(parameters, output);
		if (exit_code == 0)
		{
			return build_result::success;
		}
		else
		{
			return build_result::failure;
		}
	}

	build_result build(
		git_repository_address const &repository,
		ventura::absolute_path const &workspace,
		ventura::absolute_path const &git,
		ventura::absolute_path const &cmake,
		Si::sink<char, Si::success> &output)
	{
		ventura::path_segment const clone_name = *ventura::path_segment::create("source.git");
		git_clone(repository, workspace, clone_name, git, output);
		ventura::absolute_path const source = workspace / clone_name;

		ventura::absolute_path const build = workspace / "build";
		boost::filesystem::create_directories(build.to_boost_path());

		buildserver::cmake_exe cmake_builder(cmake);
		boost::system::error_code error = cmake_builder.generate(source, build, boost::unordered_map<std::string, std::string>{}, output);
		if (error)
		{
			boost::throw_exception(boost::system::system_error(error));
		}

		error = cmake_builder.build(build, boost::thread::hardware_concurrency(), output);
		if (error)
		{
			boost::throw_exception(boost::system::system_error(error));
		}

		return run_test(build, output);
	}
}

int main(int argc, char **argv)
{
	auto parsed_options = parse_options(argc, argv);
	if (!parsed_options)
	{
		return 1;
	}

	Si::optional<ventura::absolute_path> maybe_git = buildserver::find_git().get();
	if (!maybe_git)
	{
		std::cerr << "Could not find Git\n";
		return 1;
	}

	Si::optional<ventura::absolute_path> maybe_cmake = buildserver::find_cmake().get();
	if (!maybe_cmake)
	{
		std::cerr << "Could not find CMake\n";
		return 1;
	}

	boost::asio::io_service io;

	saturating_notifier<Si::erased_observer<notification>> notifier;
	step_history_registry registry;

	nanoweb::request_handler root_request_handler = make_root_request_handler(parsed_options->secret, notifier, registry);
	Si::spawn_observable(Si::transform(
		Si::asio::make_tcp_acceptor(boost::asio::ip::tcp::acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(), parsed_options->port))),
		[&root_request_handler](Si::asio::tcp_acceptor_result maybe_client) -> Si::nothing
		{
			auto client = maybe_client.get();
			Si::spawn_coroutine([client, &root_request_handler](Si::spawn_context yield)
			{
				auto error = nanoweb::serve_client(*client, yield, root_request_handler);
				if (!!error)
				{
					std::cerr << client->remote_endpoint().address() << ": " << error << '\n';
				}
			});
			return{};
		}
	));

	registry.name_to_step["silicium"] = step_history();

	for (auto &step : registry.name_to_step)
	{
		step_history &history = step.second;
		Si::spawn_coroutine([&history, &notifier, &io, &parsed_options, &maybe_git, &maybe_cmake](Si::spawn_context yield)
		{
			Si::optional<notification> notification_ = yield.get_one(Si::ref(notifier));
			assert(notification_);
			std::cerr << "Received a notification\n";
			try
			{
				history.is_building = true;
				Si::optional<std::future<build_result>> maybe_result = yield.get_one(
					Si::asio::make_posting_observable(
					io,
					Si::make_thread_observable<Si::std_threading>([&]()
					{
						boost::filesystem::remove_all(parsed_options->workspace.to_boost_path());
						boost::filesystem::create_directories(parsed_options->workspace.to_boost_path());
						auto output = Si::virtualize_sink(Si::ostream_ref_sink(std::cerr));
						return build(parsed_options->repository, parsed_options->workspace, *maybe_git, *maybe_cmake, output);
					}))
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
				history.last_result = result;
			}
			catch (std::exception const &ex)
			{
				std::cerr << "Exception: " << ex.what() << '\n';
				history.last_result = build_result::failure;
			}
			history.is_building = false;
		});
	}

	io.run();
}
