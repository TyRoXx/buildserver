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
#include <silicium/open.hpp>
#include <silicium/fast_variant.hpp>
#include <silicium/std_threading.hpp>
#include <silicium/run_process.hpp>
#include <silicium/html.hpp>
#include <silicium/async_process.hpp>
#include <silicium/absolute_path.hpp>
#include <silicium/path_segment.hpp>
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

		Si::fast_variant<Observer, notification> m_observer_or_notification;
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
		Si::absolute_path workspace;
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
	
	int run_process(Si::async_process_parameters const &parameters, Si::sink<char, Si::success> &output)
	{
		Si::pipe standard_output_and_error = SILICIUM_MOVE_IF_COMPILER_LACKS_RVALUE_QUALIFIERS(Si::make_pipe().get());
		Si::file_handle standard_input = SILICIUM_MOVE_IF_COMPILER_LACKS_RVALUE_QUALIFIERS(Si::open_reading("/dev/null").get());
		Si::async_process process = SILICIUM_MOVE_IF_COMPILER_LACKS_RVALUE_QUALIFIERS(Si::launch_process(
			parameters,
			standard_input.handle,
			standard_output_and_error.write.handle,
			standard_output_and_error.write.handle
		).get());
		boost::asio::io_service io;
		Si::experimental::read_from_anonymous_pipe(io, Si::ref_sink(output), std::move(standard_output_and_error.read));
		standard_output_and_error.write.close();
		io.run();
		int exit_code = process.wait_for_exit().get();
		return exit_code;
	}

	void git_clone(git_repository_address const &repository, Si::absolute_path const &working_directory, Si::absolute_path const &destination, Si::absolute_path const &git_exe, Si::sink<char, Si::success> &output)
	{
		Si::async_process_parameters parameters;
		parameters.executable = git_exe;
		parameters.current_path = working_directory;
		parameters.arguments.emplace_back(Si::to_os_string("clone"));
		parameters.arguments.emplace_back(repository);
		parameters.arguments.emplace_back(destination.c_str());
		int exit_code = run_process(parameters, output);
		if (exit_code != 0)
		{
			throw std::runtime_error("git-clone failed");
		}
	}

	struct listing;

	struct blob
	{
		std::vector<char> content;
	};

	struct uri
	{
		Si::noexcept_string value;
	};

	struct filesystem_directory_ownership
	{
		Si::absolute_path owned;
	};

	typedef Si::fast_variant<
		blob,
		std::shared_ptr<listing>,
		uri,
		filesystem_directory_ownership,
		Si::absolute_path,
		Si::path_segment,
		std::uint32_t
	> value;

	struct listing
	{
		std::map<Si::noexcept_string, value> entries;
	};

	template <class T>
	T *find_entry_of_type(listing &list, Si::noexcept_string const &key)
	{
		auto i = list.entries.find(key);
		if (i == list.entries.end())
		{
			return nullptr;
		}
		return Si::try_get_ptr<T>(i->second);
	}

	struct input_type_mismatch
	{
	};

	value expect_value(Si::fast_variant<input_type_mismatch, value> maybe)
	{
		return Si::visit<value>(
			maybe,
			[](input_type_mismatch) -> value { throw std::invalid_argument("input type mismatch"); },
			[](value &result) { return std::move(result); }
		);
	}
}

namespace example_graph
{
	Si::fast_variant<input_type_mismatch, value>
	clone(value input)
	{
		std::shared_ptr<listing> * const input_listing = Si::try_get_ptr<std::shared_ptr<listing>>(input);
		if (!input_listing)
		{
			return input_type_mismatch{};
		}
		uri * const repository = find_entry_of_type<uri>(**input_listing, "repository");
		if (!repository)
		{
			return input_type_mismatch{};
		}
		Si::absolute_path const * const destination = find_entry_of_type<Si::absolute_path>(**input_listing, "destination");
		if (!destination)
		{
			return input_type_mismatch{};
		}
		Si::absolute_path const * const git_exe = find_entry_of_type<Si::absolute_path>(**input_listing, "git");
		if (!git_exe)
		{
			return input_type_mismatch{};
		}
		std::vector<char> output;
		auto output_sink = Si::virtualize_sink(Si::make_container_sink(output));
		git_clone(Si::to_os_string(repository->value), Si::get_current_working_directory(), *destination, *git_exe, output_sink);

		listing results;
		results.entries.insert(std::make_pair("output", blob{std::move(output)}));
		results.entries.insert(std::make_pair("destination", *destination));
		return value{Si::to_shared(std::move(results))};
	}

	Si::fast_variant<input_type_mismatch, value>
	cmake_generate(value input)
	{
		std::shared_ptr<listing> * const input_listing = Si::try_get_ptr<std::shared_ptr<listing>>(input);
		if (!input_listing)
		{
			return input_type_mismatch{};
		}
		Si::absolute_path const * const cmake_exe = find_entry_of_type<Si::absolute_path>(**input_listing, "cmake");
		if (!cmake_exe)
		{
			return input_type_mismatch{};
		}
		Si::absolute_path const * const source = find_entry_of_type<Si::absolute_path>(**input_listing, "source");
		if (!source)
		{
			return input_type_mismatch{};
		}
		Si::absolute_path const * const build = find_entry_of_type<Si::absolute_path>(**input_listing, "build");
		if (!build)
		{
			return input_type_mismatch{};
		}

		buildserver::cmake_exe cmake(*cmake_exe);
		std::vector<char> output;
		auto output_sink = Si::virtualize_sink(Si::make_container_sink(output));
		boost::unordered_map<std::string, std::string> definitions; //TODO
		boost::system::error_code error = cmake.generate(*source, *build, definitions, output_sink);
		assert(!error); //TODO

		listing results;
		results.entries.insert(std::make_pair("output", blob{std::move(output)}));
		results.entries.insert(std::make_pair("build", *build));
		return value{Si::to_shared(std::move(results))};
	}

	Si::fast_variant<input_type_mismatch, value>
	cmake_build(value input)
	{
		std::shared_ptr<listing> * const input_listing = Si::try_get_ptr<std::shared_ptr<listing>>(input);
		if (!input_listing)
		{
			return input_type_mismatch{};
		}
		Si::absolute_path const * const cmake_exe = find_entry_of_type<Si::absolute_path>(**input_listing, "cmake");
		if (!cmake_exe)
		{
			return input_type_mismatch{};
		}
		Si::absolute_path const * const build = find_entry_of_type<Si::absolute_path>(**input_listing, "build");
		if (!build)
		{
			return input_type_mismatch{};
		}
		std::uint32_t const * const parallelism = find_entry_of_type<std::uint32_t>(**input_listing, "parallelism");
		if (!parallelism)
		{
			return input_type_mismatch{};
		}

		buildserver::cmake_exe cmake(*cmake_exe);
		std::vector<char> output;
		auto output_sink = Si::virtualize_sink(Si::make_container_sink(output));
		boost::system::error_code error = cmake.build(*build, *parallelism, output_sink);
		assert(!error); //TODO

		listing results;
		results.entries.insert(std::make_pair("output", blob{std::move(output)}));
		results.entries.insert(std::make_pair("build", *build));
		return value{Si::to_shared(std::move(results))};
	}
}

int main(int argc, char **argv)
{
	auto parsed_options = parse_options(argc, argv);
	if (!parsed_options)
	{
		return 1;
	}

	Si::optional<Si::absolute_path> maybe_git = buildserver::find_git().get();
	if (!maybe_git)
	{
		std::cerr << "Could not find Git\n";
		return 1;
	}

	Si::optional<Si::absolute_path> maybe_cmake = buildserver::find_cmake().get();
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
			for (;;)
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
						Si::make_thread_observable<Si::std_threading>([&]() -> build_result
						{
							Si::absolute_path const &workspace = parsed_options->workspace;
							boost::filesystem::remove_all(workspace.to_boost_path());
							boost::filesystem::create_directories(workspace.to_boost_path());

							Si::absolute_path const source_dir = workspace / *Si::path_segment::create("source.git");
							Si::absolute_path const build_dir = workspace / *Si::path_segment::create("build");

							listing clone_input;
							clone_input.entries.insert(std::make_pair("repository", uri{parsed_options->repository}));
							clone_input.entries.insert(std::make_pair("git", *maybe_git));
							clone_input.entries.insert(std::make_pair("destination", source_dir));
							value clone_output = expect_value(example_graph::clone(Si::to_shared(std::move(clone_input))));

							boost::filesystem::create_directories(build_dir.to_boost_path());

							listing generate_input;
							generate_input.entries.insert(std::make_pair("cmake", *maybe_cmake));
							generate_input.entries.insert(std::make_pair("source", source_dir));
							generate_input.entries.insert(std::make_pair("build", build_dir));
							value generate_output = expect_value(example_graph::cmake_generate(Si::to_shared(std::move(generate_input))));

							listing build_input;
							build_input.entries.insert(std::make_pair("cmake", *maybe_cmake));
							build_input.entries.insert(std::make_pair("parallelism", static_cast<std::uint32_t>(4)));
							build_input.entries.insert(std::make_pair("build", build_dir));
							value build_output = expect_value(example_graph::cmake_build(Si::to_shared(std::move(build_input))));

							return build_result::success;
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
			}
		});
	}

	io.run();
}
