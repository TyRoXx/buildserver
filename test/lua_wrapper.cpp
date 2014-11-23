#include <boost/test/unit_test.hpp>
#include "lua_wrapper/lua_environment.hpp"
#include <lauxlib.h>
#include <boost/optional/optional_io.hpp>
#include <silicium/source/memory_source.hpp>

BOOST_AUTO_TEST_CASE(lua_wrapper_create)
{
	auto s = lua::create_lua();
	BOOST_CHECK(s);
}

BOOST_AUTO_TEST_CASE(lua_wrapper_load_buffer)
{
	auto state = lua::create_lua();
	lua_State &L = *state;
	lua::safe::stack s(std::move(state));
	std::string const code = "return 3";
	boost::optional<lua_Number> result;
	s.load_buffer(Si::make_memory_range(code), "test", [&](lua::safe::typed_local<lua::safe::type::function> compiled)
	{
		s.call(compiled, lua::safe::no_arguments(), 1, [&](lua::safe::array results)
		{
			result = s.get_number(results[0]);
		});
	});
	BOOST_CHECK_EQUAL(boost::make_optional(3.0), result);
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}

BOOST_AUTO_TEST_CASE(lua_wrapper_call)
{
	auto state = lua::create_lua();
	lua_State &L = *state;
	lua::safe::stack s(std::move(state));
	std::string const code = "return function (a, b, str) return a * 3 + b, 7, str end";
	boost::optional<lua_Number> result_a, result_b;
	boost::optional<Si::noexcept_string> result_str;
	s.load_buffer(Si::make_memory_range(code), "test", [&](lua::safe::typed_local<lua::safe::type::function> compiled)
	{
		s.call(compiled, lua::safe::no_arguments(), 1, [&](lua::safe::array func)
		{
			std::array<Si::fast_variant<lua_Number, Si::noexcept_string>, 3> const arguments
			{{
				1.0,
				2.0,
				Si::noexcept_string("ff")
			}};
			s.call(func[0], Si::make_container_source(arguments), 3, [&](lua::safe::array results)
			{
				result_a = s.get_number(results[0]);
				result_b = s.get_number(results[1]);
				result_str = s.get_string(results[2]);
			});
		});
	});
	BOOST_CHECK_EQUAL(boost::make_optional(5.0), result_a);
	BOOST_CHECK_EQUAL(boost::make_optional(7.0), result_b);
	BOOST_CHECK_EQUAL(boost::optional<Si::noexcept_string>("ff"), result_str);
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}
