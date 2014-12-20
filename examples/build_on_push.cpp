#include <boost/program_options.hpp>
#include <boost/optional.hpp>
#include <iostream>
#include <boost/thread.hpp>
#include <silicium/http/receive_request.hpp>
#include <silicium/asio/tcp_acceptor.hpp>
#include <silicium/asio/writing_observable.hpp>
#include <silicium/observable/spawn_coroutine.hpp>
#include <silicium/observable/spawn_observable.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/http/generate_response.hpp>
#include <silicium/observable/total_consumer.hpp>
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

		void async_get_one(Si::ptr_observer<Si::observer<element_type>> observer)
		{
			if (!m_is_running)
			{
				m_server.start();
				m_is_running = true;
			}
			return Si::visit<void>(
				m_observer_or_notification,
				[this, observer](Si::observer<element_type> * &my_observer)
				{
					assert(!my_observer);
					my_observer = observer.get();
				},
				[this, observer](notification)
				{
					m_observer_or_notification = static_cast<Si::observer<element_type> *>(nullptr);
					observer.got_element(notification());
				}
			);
		}

	private:

		Si::total_consumer<Si::unique_observable<Si::nothing>> m_server;
		bool m_is_running;
		Si::noexcept_string m_secret;
		Si::fast_variant<Si::observer<element_type> *, notification> m_observer_or_notification;

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
				[](Si::observer<element_type> * &observer)
				{
					Si::exchange(observer, nullptr)->got_element(notification());
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

namespace Si
{
	template <class Element, class ThreadingAPI>
	struct thread_observable2
	{
		typedef Element element_type;

		explicit thread_observable2(std::function<element_type ()> action)
			: m_action(std::move(action))
		{
		}

		template <class Observer>
		void async_get_one(Observer &&observer)
		{
			assert(m_action);
			auto action = std::move(m_action);
			m_worker = ThreadingAPI::launch_async([
				observer
#if SILICIUM_COMPILER_HAS_EXTENDED_CAPTURE
					= std::forward<Observer>(observer)
#endif
				,
				action
#if SILICIUM_COMPILER_HAS_EXTENDED_CAPTURE
					= std::move(action)
#endif
				]() mutable
			{
				std::forward<Observer>(observer).got_element(action());
			});
		}

	private:

		std::function<element_type ()> m_action;
		typename ThreadingAPI::template future<void>::type m_worker;
	};

	template <class ThreadingAPI, class Action>
	auto make_thread_observable2(Action &&action)
	{
		return thread_observable2<decltype(action()), ThreadingAPI>(std::forward<Action>(action));
	}

	template <class Next>
	struct posting_observable : private observer<typename Next::element_type>
	{
		typedef typename Next::element_type element_type;

		explicit posting_observable(boost::asio::io_service &io, Next next)
			: m_io(&io)
			, m_observer(nullptr)
			, m_next(std::move(next))
		{
		}

		template <class Observer>
		void async_get_one(Observer &&observer_)
		{
			m_observer = observer_.get();
			m_next.async_get_one(extend(std::forward<Observer>(observer_), observe_by_ref(static_cast<observer<element_type> &>(*this))));
		}

	private:

		boost::asio::io_service *m_io;
		observer<element_type> *m_observer;
		Next m_next;

		virtual void got_element(element_type value) SILICIUM_OVERRIDE
		{
			auto observer_ = Si::exchange(m_observer, nullptr);
			m_io->post([observer_, value = std::move(value)]() mutable
			{
				observer_->got_element(std::move(value));
			});
		}

		virtual void ended() SILICIUM_OVERRIDE
		{
			auto observer_ = Si::exchange(m_observer, nullptr);
			m_io->post([observer_]() mutable
			{
				observer_->ended();
			});
		}
	};

	template <class Next>
	auto make_posting_observable(boost::asio::io_service &io, Next &&next)
	{
		return posting_observable<typename std::decay<Next>::type>(io, std::forward<Next>(next));
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
	notification_server notifications(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(), parsed_options->port), parsed_options->secret);
	auto all_done = Si::make_total_consumer(
		Si::transform(
			Si::ref(notifications),
			[&](boost::optional<notification> element)
			{
				assert(element);
				std::cerr << "Received a notification\n";
				try
				{
					Si::spawn_observable(
						Si::transform(
							Si::make_posting_observable(
								io,
								Si::make_thread_observable2<Si::boost_threading>([&]()
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
				return Si::nothing();
			}
		)
	);
	all_done.start();
	io.run();
}
