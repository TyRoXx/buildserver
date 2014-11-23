#include <boost/test/unit_test.hpp>
#include "lua_wrapper/lua_environment.hpp"
#include <lauxlib.h>
#include <boost/optional/optional_io.hpp>

BOOST_AUTO_TEST_CASE(lua_wrapper_create)
{
	auto s = lua::create_lua();
	BOOST_CHECK(s);
}

BOOST_AUTO_TEST_CASE(lua_wrapper_load)
{
	auto state = lua::create_lua();
	lua_State &L = *state;
	lua::safe::stack s(std::move(state));
	std::string const code = "return 3";
	boost::optional<lua_Number> result;
	s.load_buffer(Si::make_memory_range(code), "test", [&](lua::safe::typed_local<lua::safe::type::function> &compiled)
	{
		s.call(compiled, lua::safe::no_arguments(), 1, [&](lua::safe::array const &results)
		{
			result = s.to_number(results[0]);
		});
	});
	BOOST_CHECK_EQUAL(boost::make_optional(3.0), result);
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}
