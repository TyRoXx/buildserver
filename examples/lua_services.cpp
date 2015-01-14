#include <boost/asio/io_service.hpp>
#include <luacpp/register_any_function.hpp>
#include <luacpp/load.hpp>
#include <boost/range/algorithm/equal.hpp>

namespace
{
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

int main()
{
	boost::asio::io_service io;

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