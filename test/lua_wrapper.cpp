#include <boost/test/unit_test.hpp>
#include "lua_wrapper/lua_environment.hpp"
#include <lauxlib.h>
#include <boost/optional/optional_io.hpp>
#include <silicium/source/memory_source.hpp>
#include <silicium/source/single_source.hpp>

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
	{
		lua::safe::stack_value const compiled = s.load_buffer(Si::make_memory_range(code), "test");
		lua::safe::stack_array const results = s.call(compiled, lua::safe::no_arguments(), 1);
		auto result = s.get_number(lua::safe::at(results, 0));
		BOOST_CHECK_EQUAL(boost::make_optional(3.0), result);
	}
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}

BOOST_AUTO_TEST_CASE(lua_wrapper_call_multret)
{
	auto state = lua::create_lua();
	lua_State &L = *state;
	lua::safe::stack s(std::move(state));
	std::string const code = "return 1, 2, 3";
	{
		lua::safe::stack_value compiled = s.load_buffer(
					Si::make_memory_range(code),
					"test");
		lua::safe::stack_array results = s.call(std::move(compiled), lua::safe::no_arguments(), boost::none);
		BOOST_REQUIRE_EQUAL(3, results.size());
		std::vector<lua_Number> result_numbers;
		for (int i = 0; i < results.size(); ++i)
		{
			result_numbers.emplace_back(s.to_number(at(results, i)));
		}
		std::vector<lua_Number> const expected{1, 2, 3};
		BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), result_numbers.begin(), result_numbers.end());
	}
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}

BOOST_AUTO_TEST_CASE(lua_wrapper_call)
{
	auto state = lua::create_lua();
	lua_State &L = *state;
	lua::safe::stack s(std::move(state));
	{
		std::string const code = "return function (a, b, str, bool) return a * 3 + b, 7, str, not bool end";
		boost::optional<lua_Number> result_a, result_b;
		boost::optional<Si::noexcept_string> result_str;
		boost::optional<bool> result_bool;
		lua::safe::stack_value compiled = s.load_buffer(Si::make_memory_range(code), "test");
		lua::safe::stack_array func = s.call(compiled, lua::safe::no_arguments(), 1);
		std::array<Si::fast_variant<lua_Number, Si::noexcept_string, bool>, 4> const arguments
		{{
			1.0,
			2.0,
			Si::noexcept_string("ff"),
			false
		}};
		lua::safe::stack_array results = s.call(at(func, 0), Si::make_container_source(arguments), 4);
		result_a = s.get_number(at(results, 0));
		result_b = s.get_number(at(results, 1));
		result_str = s.get_string(at(results, 2));
		result_bool = s.get_boolean(at(results, 3));
		BOOST_CHECK_EQUAL(boost::make_optional(5.0), result_a);
		BOOST_CHECK_EQUAL(boost::make_optional(7.0), result_b);
		BOOST_CHECK_EQUAL(boost::optional<Si::noexcept_string>("ff"), result_str);
		BOOST_CHECK_EQUAL(boost::make_optional(true), result_bool);
	}
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}

BOOST_AUTO_TEST_CASE(lua_wrapper_reference)
{
	auto state = lua::create_lua();
	lua_State &L = *state;
	lua::safe::stack s(std::move(state));
	std::string const code = "return 3";
	lua::safe::reference const ref = s.create_reference(s.load_buffer(Si::make_memory_range(code), "test"));
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
	BOOST_REQUIRE(!ref.empty());
	{
		lua::safe::stack_array results = s.call(ref, lua::safe::no_arguments(), 1);
		boost::optional<lua_Number> const result = s.get_number(at(results, 0));
		BOOST_CHECK_EQUAL(boost::make_optional(3.0), result);
	}
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}

namespace
{
	int return_3(lua_State *L) BOOST_NOEXCEPT
	{
		lua_pushinteger(L, 3);
		return 1;
	}
}

BOOST_AUTO_TEST_CASE(lua_wrapper_register_c_function)
{
	auto state = lua::create_lua();
	lua_State &L = *state;
	lua::safe::stack s(std::move(state));
	{
		lua::safe::stack_value func = s.register_function(return_3);
		lua::safe::stack_array results = s.call(func, lua::safe::no_arguments(), 1);
		boost::optional<lua_Number> const result = s.get_number(at(results, 0));
		BOOST_CHECK_EQUAL(boost::make_optional(3.0), result);
	}
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}

namespace
{
	int return_upvalues_subtracted(lua_State *L) BOOST_NOEXCEPT
	{
		lua_pushnumber(L, lua_tonumber(L, lua_upvalueindex(1)) - lua_tonumber(L, lua_upvalueindex(2)));
		return 1;
	}
}

BOOST_AUTO_TEST_CASE(lua_wrapper_register_c_closure)
{
	auto state = lua::create_lua();
	lua_State &L = *state;
	lua::safe::stack s(std::move(state));
	{
		std::array<lua_Number, 2> const upvalues{{1.0, 2.0}};
		lua::safe::stack_value func = s.register_function(return_upvalues_subtracted, Si::make_container_source(upvalues));
		lua::safe::stack_array results = s.call(func, lua::safe::no_arguments(), 1);
		boost::optional<lua_Number> const result = s.get_number(at(results, 0));
		BOOST_CHECK_EQUAL(boost::make_optional(-1.0), result);
	}
	BOOST_CHECK_EQUAL(0, lua_gettop(&L));
}

BOOST_AUTO_TEST_CASE(lua_wrapper_register_cpp_closure)
{
	auto bound = std::make_shared<lua_Number>(2);
	{
		auto state = lua::create_lua();
		lua_State &L = *state;
		lua::safe::stack s(std::move(state));
		{
			lua::safe::stack_value closure = lua::safe::register_closure(
				s,
				[bound](lua_State *L)
				{
					lua_pushnumber(L, *bound);
					return 1;
				}
			);
			BOOST_REQUIRE_EQUAL(lua::safe::type::function, closure.get_type());
			lua::safe::stack_array results = s.call(closure, lua::safe::no_arguments(), 1);
			boost::optional<lua_Number> const result = s.get_number(at(results, 0));
			BOOST_CHECK_EQUAL(boost::make_optional(2.0), result);
		}
		BOOST_CHECK_EQUAL(0, lua_gettop(&L));
	}
	BOOST_CHECK_EQUAL(1, bound.use_count());
}

BOOST_AUTO_TEST_CASE(lua_wrapper_register_cpp_closure_with_upvalues)
{
	auto bound = std::make_shared<lua_Number>(2);
	{
		auto state = lua::create_lua();
		lua_State &L = *state;
		lua::safe::stack s(std::move(state));
		{
			std::array<lua_Number, 1> const upvalues{{ 3.0 }};
			lua::safe::stack_value closure = lua::safe::register_closure(
				s,
				[bound](lua_State *L)
				{
					lua_pushvalue(L, lua_upvalueindex(2));
					return 1;
				},
				Si::make_container_source(upvalues)
			);
			lua::safe::stack_array results = s.call(closure, lua::safe::no_arguments(), 1);
			boost::optional<lua_Number> const result = s.get_number(at(results, 0));
			BOOST_CHECK_EQUAL(boost::make_optional(upvalues[0]), result);
		}
		BOOST_CHECK_EQUAL(0, lua_gettop(&L));
	}
	BOOST_CHECK_EQUAL(1, bound.use_count());
}
