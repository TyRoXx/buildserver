#ifndef BUILDSERVER_LUA_ENVIRONMENT_HPP
#define BUILDSERVER_LUA_ENVIRONMENT_HPP

#include <lua.h>
#include <lauxlib.h>
#include <boost/config.hpp>
#include <boost/system/system_error.hpp>
#include <memory>
#include <silicium/memory_range.hpp>
#include <silicium/config.hpp>
#include <silicium/override.hpp>
#include <silicium/noexcept_string.hpp>
#include <silicium/fast_variant.hpp>
#include <silicium/source/empty.hpp>

namespace lua
{
	struct lua_error_category : boost::system::error_category
	{
		virtual const char *name() const BOOST_SYSTEM_NOEXCEPT SILICIUM_OVERRIDE;
		virtual std::string message(int ev) const SILICIUM_OVERRIDE;
	};

	boost::system::error_category const &get_lua_error_category();

	struct lua_deleter
	{
		void operator()(lua_State *L) const BOOST_NOEXCEPT;
	};

	typedef std::unique_ptr<lua_State, lua_deleter> state_ptr;

	state_ptr create_lua();

	boost::system::error_code load_buffer(lua_State &L, Si::memory_range code, char const *name);

	namespace safe
	{
		enum class type
		{
			nil = LUA_TNIL,
			boolean = LUA_TBOOLEAN,
			light_user_data = LUA_TLIGHTUSERDATA,
			number = LUA_TNUMBER,
			string = LUA_TSTRING,
			table = LUA_TTABLE,
			function = LUA_TFUNCTION,
			user_data = LUA_TUSERDATA,
			thread = LUA_TTHREAD
		};

		struct pushable
		{
			virtual ~pushable()
			{
			}
			virtual void push(lua_State &L) const = 0;
		};

		inline void push(lua_State &L, pushable const &p)
		{
			p.push(L);
		}

		inline void push(lua_State &L, pushable const *p)
		{
			assert(p);
			push(L, *p);
		}

		inline void push(lua_State &L, lua_Number value)
		{
			lua_pushnumber(&L, value);
		}

		inline void push(lua_State &L, Si::noexcept_string const &value)
		{
			lua_pushlstring(&L, value.data(), value.size());
		}

		inline void push(lua_State &L, bool value)
		{
			lua_pushboolean(&L, value);
		}

		namespace detail
		{
			struct pusher
			{
				typedef void result_type;

				lua_State *L;

				template <class T>
				void operator()(T const &value) const
				{
					push(*L, value);
				}
			};
		}

		template <class ...T>
		inline void push(lua_State &L, Si::fast_variant<T...> const &value)
		{
			return Si::apply_visitor(detail::pusher{&L}, value);
		}

		struct any_local : pushable
		{
			explicit any_local(int from_bottom)
				: m_from_bottom(from_bottom)
			{
			}

			int from_bottom() const
			{
				return m_from_bottom;
			}

			virtual void push(lua_State &L) const SILICIUM_OVERRIDE
			{
				lua_pushvalue(&L, m_from_bottom);
			}

		private:

			int m_from_bottom;
		};

		struct array
		{
			array(int begin, int length)
				: m_begin(begin)
				, m_length(length)
			{
			}

			int begin() const
			{
				return m_begin;
			}

			int length() const
			{
				return m_length;
			}

			any_local operator[](int index) const
			{
				assert(index < m_length);
				return any_local(m_begin + index);
			}

		private:

			int m_begin;
			int m_length;
		};

		template <type Type>
		struct typed_local : any_local
		{
			explicit typed_local(int from_bottom)
				: any_local(from_bottom)
			{
			}
		};

		struct lua_exception : std::runtime_error
		{
			explicit lua_exception(std::string message)
				: std::runtime_error(std::move(message))
			{
			}
		};

		namespace detail
		{
			struct owner_of_the_top : boost::noncopyable
			{
				explicit owner_of_the_top(lua_State &lua, int count) BOOST_NOEXCEPT
					: m_lua(lua)
					, m_count(count)
				{
					assert(m_count >= 0);
				}

				~owner_of_the_top() BOOST_NOEXCEPT
				{
					lua_pop(&m_lua, m_count);
				}

			private:

				lua_State &m_lua;
				int m_count;
			};
		}

		struct stack
		{
			stack() BOOST_NOEXCEPT
			{
			}

			explicit stack(state_ptr state) BOOST_NOEXCEPT
				: m_state(std::move(state))
			{
			}

			template <class SuccessHandler>
			void load_buffer(Si::memory_range code, char const *name, SuccessHandler const &on_success)
			{
				auto error = lua::load_buffer(*m_state, code, name);
				if (error)
				{
					boost::throw_exception(boost::system::system_error(error));
				}
				detail::owner_of_the_top const owner(*m_state, 1);
				int top = lua_gettop(m_state.get());
				typed_local<type::function> compiled(top);
				on_success(compiled);
			}

			template <class Pushable, class ArgumentSource, class ResultHandler>
			void call(Pushable const &function, ArgumentSource &&arguments, boost::optional<int> expected_result_count, ResultHandler const &on_results)
			{
				int const stack_before = lua_gettop(m_state.get());
				push(*m_state, function);
				int argument_count = 0;
				for (;;)
				{
					auto argument = Si::get(arguments);
					if (!argument)
					{
						break;
					}
					push(*m_state, *argument);
					++argument_count;
				}
				int const nresults = expected_result_count ? *expected_result_count : LUA_MULTRET;
				//TODO: stack trace in case of an error
				if (lua_pcall(m_state.get(), argument_count, nresults, 0) != 0)
				{
					std::string message = lua_tostring(m_state.get(), -1);
					lua_pop(m_state.get(), 1);
					boost::throw_exception(lua_exception(std::move(message)));
				}
				array const results(stack_before + 1, lua_gettop(m_state.get()) - stack_before);
				detail::owner_of_the_top const owner(*m_state, results.length());
				on_results(results);
			}

			type get_type(any_local const &local)
			{
				return static_cast<type>(lua_type(m_state.get(), local.from_bottom()));
			}

			lua_Number to_number(any_local const &local)
			{
				return lua_tonumber(m_state.get(), local.from_bottom());
			}

			Si::noexcept_string to_string(any_local const &local)
			{
				return lua_tostring(m_state.get(), local.from_bottom());
			}

			bool to_boolean(any_local const &local)
			{
				return lua_toboolean(m_state.get(), local.from_bottom());
			}

			boost::optional<lua_Number> get_number(any_local const &local)
			{
				type const t = get_type(local);
				if (t != type::number)
				{
					return boost::none;
				}
				return to_number(local);
			}

			boost::optional<Si::noexcept_string> get_string(any_local const &local)
			{
				type const t = get_type(local);
				if (t != type::string)
				{
					return boost::none;
				}
				return to_string(local);
			}

			boost::optional<bool> get_boolean(any_local const &local)
			{
				type const t = get_type(local);
				if (t != type::boolean)
				{
					return boost::none;
				}
				return to_boolean(local);
			}

		private:

			state_ptr m_state;
		};

		inline Si::empty_source<pushable *> no_arguments()
		{
			return {};
		}
	}
}

#endif
