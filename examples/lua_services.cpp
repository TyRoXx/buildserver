#include <boost/asio/io_service.hpp>
#include <luacpp/register_any_function.hpp>
#include <luacpp/load.hpp>
#include <boost/range/algorithm/equal.hpp>

namespace
{

}

int main()
{
	boost::asio::io_service io;

	auto lua_state = lua::create_lua();
	lua::stack_value script = lua::load_file(*lua_state, boost::filesystem::path(__FILE__).parent_path() / "lua_services.lua").value();
	script.release();
	lua::stack_array script_main = lua::pcall(*lua_state, 0, 1);
	if (script_main.size() != 1)
	{
		return 1;
	}
	lua::stack s(*lua_state);
	lua::stack_value require = lua::register_any_function(s, [](Si::memory_range name, Si::memory_range version)
	{
		if (boost::range::equal(name, Si::make_c_str_range("steps")) &&
		    boost::range::equal(version, Si::make_c_str_range("1.0")))
		{

		}
	});
	script_main.release();
	require.release();
	lua::stack_array main_results = lua::pcall(*lua_state, 1, 1);

	io.run();
}
