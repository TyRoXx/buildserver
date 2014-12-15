#include <boost/program_options.hpp>
#include <boost/optional.hpp>
#include <iostream>
#include <silicium/http/receive_request.hpp>
#include <silicium/asio/tcp_acceptor.hpp>
#include <silicium/asio/writing_observable.hpp>
#include <silicium/observable/flatten.hpp>
#include <silicium/observable/coroutine.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/http/generate_response.hpp>
#include <silicium/observable/total_consumer.hpp>

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
					Si::flatten(
						Si::transform(
							Si::asio::make_tcp_acceptor(Si::make_unique<boost::asio::ip::tcp::acceptor>(io, endpoint)),
							[this](Si::asio::tcp_acceptor_result maybe_client)
							{
								auto client = maybe_client.get();
								auto client_handler = Si::make_coroutine([this, client](Si::yield_context yield) -> Si::nothing
								{
									serve_client(*client, yield);
									return {};
								});
								return Si::erase_unique(std::move(client_handler));
							}
						)
					)
				)
			)
			, m_is_running(false)
			, m_secret(std::move(secret))
		{
		}

		void async_get_one(Si::observer<element_type> &observer)
		{
			if (!m_is_running)
			{
				m_server.start();
				m_is_running = true;
			}
			return Si::visit<void>(
				m_observer_or_notification,
				[this, &observer](Si::observer<element_type> * &my_observer)
				{
					assert(!my_observer);
					my_observer = &observer;
				},
				[this, &observer](notification)
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

		void serve_client(boost::asio::ip::tcp::socket &client, Si::yield_context yield)
		{
			Si::error_or<boost::optional<Si::http::request>> maybe_request = Si::http::receive_request(client, yield);
			if (maybe_request.is_error())
			{
				std::cerr << client.remote_endpoint().address() << ": " << maybe_request.error() << '\n';
				return;
			}

			if (!maybe_request.get())
			{
				std::cerr << client.remote_endpoint().address() << ": invalid request\n";
				return;
			}

			Si::http::request const &request = *maybe_request.get();
			if (std::string::npos == request.path.find(m_secret))
			{
				std::cerr << client.remote_endpoint().address() << ": wrong secret\n";
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

		return std::move(result);
	}
}

int main(int argc, char **argv)
{
	auto parsed_options = parse_options(argc, argv);
	if (!parsed_options)
	{
		return 1;
	}

	boost::asio::io_service io;
	notification_server notifications(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(), parsed_options->port), parsed_options->secret);
	auto all_done = Si::make_total_consumer(Si::ref(notifications));
	all_done.start();
	io.run();
}
