#include <boost/asio/io_service.hpp>
#include <luacpp/register_any_function.hpp>
#include <luacpp/load.hpp>
#include <boost/range/algorithm/equal.hpp>
#include <boost/asio/steady_timer.hpp>

namespace buildserver
{
	struct scoped_temporary_directory : private boost::noncopyable
	{
		boost::filesystem::path where;
		std::function<void (boost::filesystem::path const &)> on_obsoletion;

		scoped_temporary_directory(
			boost::filesystem::path where,
			std::function<void (boost::filesystem::path const &)> on_obsoletion)
			: where(std::move(where))
			, on_obsoletion(std::move(on_obsoletion))
		{
		}

		~scoped_temporary_directory() BOOST_NOEXCEPT
		{
			if (on_obsoletion)
			{
				return;
			}
			on_obsoletion(where);
		}
	};

	struct memory_blob
	{
		std::vector<char> content;
	};

	struct failure_description
	{
		Si::noexcept_string message;
	};

	typedef Si::fast_variant<
		failure_description,
		std::shared_ptr<scoped_temporary_directory>,
		memory_blob
	> process_result;

	struct process
	{
		virtual ~process()
		{
		}
		virtual void async_get_result(std::function<void (process_result)> result_handler) = 0;
	};

	typedef std::function<std::unique_ptr<process> (process_result const &)> step;

	struct dag_node
	{
		step value;
		std::vector<std::unique_ptr<dag_node>> edges;

		dag_node()
		{
		}

		explicit dag_node(step value)
			: value(std::move(value))
		{
		}
	};

	struct delay : process
	{
		explicit delay(boost::asio::io_service &io, boost::asio::steady_timer::duration amount, process_result output)
			: m_timer(io)
			, m_amount(amount)
			, m_output(std::move(output))
		{
		}

		virtual void async_get_result(std::function<void (process_result)> result_handler) SILICIUM_OVERRIDE
		{
			m_timer.expires_from_now(m_amount);
			auto &output = m_output;
			m_timer.async_wait([result_handler, output
#if SILICIUM_COMPILER_HAS_EXTENDED_CAPTURE
				= std::move(output)
#endif
			](boost::system::error_code ec) mutable
			{
				if (!ec)
				{
					result_handler(failure_description{"A timer failed on the system level: " + Si::to_noexcept_string(ec.message())});
					return;
				}
				std::cerr << "timer elapsed\n";
				result_handler(std::move(output));
			});
			std::cerr << "timer started\n";
		}

	private:

		boost::asio::steady_timer m_timer;
		boost::asio::steady_timer::duration m_amount;
		process_result m_output;
	};
}

namespace
{
	struct step_a : buildserver::process
	{
		explicit step_a(boost::asio::io_service &io, buildserver::process_result input)
			: m_io(&io)
			, m_input(std::move(input))
		{
		}

		virtual void async_get_result(std::function<void (buildserver::process_result)> result_handler) SILICIUM_OVERRIDE
		{
			std::cerr << "Step A\n";
			m_io->post([result_handler]()
			{
				result_handler(buildserver::memory_blob{});
			});
		}

	private:

		boost::asio::io_service *m_io;
		buildserver::process_result m_input;
	};

	void run_experiment()
	{
		auto lua_state = lua::create_lua();
		lua::stack_value script = lua::load_file(*lua_state, boost::filesystem::path(__FILE__).parent_path() / "lua_services.lua").value();
		script.release();
		lua::stack_array script_main = lua::pcall(*lua_state, 0, 1);
		if (script_main.size() != 1)
		{
			return;
		}
		lua::stack s(*lua_state);
		lua::stack_value require = lua::register_any_function(s, [](Si::memory_range name, Si::memory_range version, lua_State &stack) -> lua::stack_value
		{
			if (boost::range::equal(name, Si::make_c_str_range("steps")) &&
			    boost::range::equal(version, Si::make_c_str_range("1.0")))
			{
				lua::stack_value module = lua::create_table(stack);
				lua::stack s(stack);
				module.set("map", lua::register_any_function(s, [](lua::any_local start)
				{
				}));
				module.set("sequence", lua::register_any_function(s, [](lua::any_local elements)
				{
				}));
				return module;
			}
			return lua::push_nil(stack);
		});
		script_main.release();
		require.release();
		lua::stack_array main_results = lua::pcall(*lua_state, 1, 1);
	}
}

void handle_input(buildserver::dag_node const &start, buildserver::process_result const &input)
{
	std::unique_ptr<buildserver::process> started = start.value(input);
	if (!started)
	{
		std::cerr << "input ignored\n";
		return;
	}
	std::shared_ptr<buildserver::process> started_copyable(std::move(started));
	started_copyable->async_get_result([&start, started_copyable](buildserver::process_result result)
	{
		std::cerr << "got result\n";
		for (auto const &next : start.edges)
		{
			handle_input(*next, result);
		}
	});
}

int main()
{
	boost::asio::io_service io;
	buildserver::dag_node root;
	root.value = [&io](buildserver::process_result const &input) -> std::unique_ptr<buildserver::process>
	{
		return Si::make_unique<step_a>(io, input);
	};
	root.edges.emplace_back(Si::make_unique<buildserver::dag_node>(root.value));
	root.edges.emplace_back(Si::make_unique<buildserver::dag_node>([&io](buildserver::process_result const &input)
	{
		return Si::make_unique<buildserver::delay>(io, std::chrono::milliseconds(30), input);
	}));
	root.edges.emplace_back(Si::make_unique<buildserver::dag_node>(root.value));
	handle_input(root, {});

	io.run();

	try
	{
		run_experiment();
	}
	catch (std::exception const &ex)
	{
		std::cerr << ex.what() << '\n';
		return 1;
	}

	io.run();
}
