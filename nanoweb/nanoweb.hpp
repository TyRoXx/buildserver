#ifndef BUILDSERVER_NANOWEB_HPP
#define BUILDSERVER_NANOWEB_HPP

#include <silicium/asio/writing_observable.hpp>
#include <silicium/http/generate_response.hpp>
#include <silicium/http/receive_request.hpp>
#include <silicium/http/uri.hpp>
#include <silicium/observable/spawn_coroutine.hpp>
#include <silicium/range_value.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <unordered_map>
#include <functional>
#include <boost/asio/ip/tcp.hpp>

namespace nanoweb
{
	enum class request_handler_result
	{
		handled,
		not_found
	};

	typedef std::function<request_handler_result (
		boost::asio::ip::tcp::socket &,
		Si::http::request const &,
		Si::iterator_range<Si::memory_range const *>,
		Si::spawn_context
	)> request_handler;

	inline request_handler make_directory(std::unordered_map<Si::range_value<Si::memory_range>, request_handler> entries)
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

	template <class Socket, class YieldContext, class Status, class StatusText>
	void quick_final_response(Socket &client, YieldContext &&yield, Status &&status, StatusText &&status_text, Si::memory_range const &content)
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

	template <class Socket, class YieldContext>
	boost::system::error_code serve_client(Socket &client, YieldContext &&yield, request_handler const &root_request_handler)
	{
		Si::error_or<boost::optional<Si::http::request>> maybe_request = Si::http::receive_request(client, yield);
		if (maybe_request.is_error())
		{
			return maybe_request.error();
		}

		if (!maybe_request.get())
		{
			return {}; //TODO
		}

		Si::http::request const &request = *maybe_request.get();

		boost::optional<Si::http::uri> relative_uri = Si::http::parse_uri(Si::make_memory_range(request.path));
		if (!relative_uri)
		{
			return {}; //TODO
		}

		switch (root_request_handler(client, request, Si::make_contiguous_range(relative_uri->path), yield))
		{
		case request_handler_result::handled:
			break;

		case request_handler_result::not_found:
			quick_final_response(client, yield, "404", "Not Found", Si::make_c_str_range("404 - Not Found"));
			break;
		}

		return {};
	}
}

#endif
