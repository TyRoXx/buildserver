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
#include <silicium/source/single_source.hpp>

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
			virtual ~pushable() BOOST_NOEXCEPT
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

		inline void push(lua_State &L, lua_Number value) BOOST_NOEXCEPT
		{
			lua_pushnumber(&L, value);
		}

		inline void push(lua_State &L, Si::noexcept_string const &value) BOOST_NOEXCEPT
		{
			lua_pushlstring(&L, value.data(), value.size());
		}

		inline void push(lua_State &L, char const *c_str) BOOST_NOEXCEPT
		{
			lua_pushstring(&L, c_str);
		}

		inline void push(lua_State &L, bool value) BOOST_NOEXCEPT
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
			any_local() BOOST_NOEXCEPT
				: m_from_bottom(-1)
			{
			}

			explicit any_local(int from_bottom) BOOST_NOEXCEPT
				: m_from_bottom(from_bottom)
			{
				assert(m_from_bottom >= 1);
			}

			int from_bottom() const BOOST_NOEXCEPT
			{
				return m_from_bottom;
			}

			virtual void push(lua_State &L) const SILICIUM_OVERRIDE
			{
				assert(m_from_bottom >= 1);
				lua_pushvalue(&L, m_from_bottom);
			}

		private:

			int m_from_bottom;
		};

		struct array
		{
			array(int begin, int length) BOOST_NOEXCEPT
				: m_begin(begin)
				, m_length(length)
			{
			}

			int begin() const BOOST_NOEXCEPT
			{
				return m_begin;
			}

			int length() const BOOST_NOEXCEPT
			{
				return m_length;
			}

			any_local operator[](int index) const BOOST_NOEXCEPT
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
			typed_local() BOOST_NOEXCEPT
			{
			}

			explicit typed_local(int from_bottom) BOOST_NOEXCEPT
				: any_local(from_bottom)
			{
			}
		};

		struct reference
		{
			reference() BOOST_NOEXCEPT
				: m_state(nullptr)
			{
			}

			explicit reference(lua_State &state, int key) BOOST_NOEXCEPT
				: m_state(&state)
				, m_key(key)
			{
			}

			reference(reference &&other) BOOST_NOEXCEPT
				: m_state(other.m_state)
				, m_key(other.m_key)
			{
				other.m_state = nullptr;
			}

			reference &operator = (reference &&other) BOOST_NOEXCEPT
			{
				std::swap(m_state, other.m_state);
				std::swap(m_key, other.m_key);
				return *this;
			}

			~reference() BOOST_NOEXCEPT
			{
				if (!m_state)
				{
					return;
				}
				luaL_unref(m_state, LUA_REGISTRYINDEX, m_key);
			}

			bool empty() const BOOST_NOEXCEPT
			{
				return !m_state;
			}

			void push() const BOOST_NOEXCEPT
			{
				assert(!empty());
				lua_rawgeti(m_state, LUA_REGISTRYINDEX, m_key);
			}

		private:

			lua_State *m_state;
			int m_key;

			SILICIUM_DELETED_FUNCTION(reference(reference const &))
			SILICIUM_DELETED_FUNCTION(reference &operator = (reference const &))
		};

		inline void push(lua_State &, reference const &ref) BOOST_NOEXCEPT
		{
			ref.push();
		}

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
#ifndef NDEBUG
					, m_initial_top(lua_gettop(&lua))
#endif
				{
					assert(m_count >= 0);
					assert(m_initial_top >= m_count);
				}

				~owner_of_the_top() BOOST_NOEXCEPT
				{
					assert(lua_gettop(&m_lua) == m_initial_top);
					lua_pop(&m_lua, m_count);
					assert(lua_gettop(&m_lua) == (m_initial_top - m_count));
				}

			private:

				lua_State &m_lua;
				int m_count;
#ifndef NDEBUG
				int m_initial_top;
#endif
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
			auto load_buffer(Si::memory_range code, char const *name, SuccessHandler const &on_success)
			{
				auto error = lua::load_buffer(*m_state, code, name);
				if (error)
				{
					boost::throw_exception(boost::system::system_error(error));
				}
				detail::owner_of_the_top const owner(*m_state, 1);
				int top = lua_gettop(m_state.get());
				typed_local<type::function> compiled(top);
				return on_success(compiled);
			}

			template <class Pushable, class ArgumentSource, class ResultHandler>
			auto call(Pushable const &function, ArgumentSource &&arguments, boost::optional<int> expected_result_count, ResultHandler const &on_results)
			{
				int const top_before = checked_top();
				push(*m_state, function);
				assert(checked_top() == (top_before + 1));
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
				assert(checked_top() == (top_before + 1 + argument_count));
				int const nresults = expected_result_count ? *expected_result_count : LUA_MULTRET;
				//TODO: stack trace in case of an error
				if (lua_pcall(m_state.get(), argument_count, nresults, 0) != 0)
				{
					std::string message = lua_tostring(m_state.get(), -1);
					lua_pop(m_state.get(), 1);
					boost::throw_exception(lua_exception(std::move(message)));
				}
				int const top_after_call = checked_top();
				assert(top_after_call >= top_before);
				array const results(top_before + 1, top_after_call - top_before);
				detail::owner_of_the_top const owner(*m_state, results.length());
				return on_results(results);
			}

			template <class Pushable>
			reference create_reference(Pushable const &value)
			{
				push(*m_state, value);
				int key = luaL_ref(m_state.get(), LUA_REGISTRYINDEX);
				return reference(*m_state, key);
			}

			template <class ResultHandler>
			auto register_function(int (*function)(lua_State *L), ResultHandler const &on_result)
			{
				lua_pushcfunction(m_state.get(), function);
				detail::owner_of_the_top const owner(*m_state, 1);
				return on_result(typed_local<type::function>(checked_top()));
			}

			template <class ResultHandler, class UpvalueSource>
			auto register_function(int (*function)(lua_State *L), UpvalueSource &&values, ResultHandler const &on_result)
			{
#ifndef NDEBUG
				int const initial_top = checked_top();
#endif
				int upvalue_count = 0;
				for (;;)
				{
					auto value = Si::get(values);
					if (!value)
					{
						break;
					}
					push(*m_state, *value);
					++upvalue_count;
					assert(checked_top() == (initial_top + upvalue_count));
				}
				assert(checked_top() >= upvalue_count);
				lua_pushcclosure(m_state.get(), function, upvalue_count);
				int const top = checked_top();
				assert(top >= 1);
				detail::owner_of_the_top const owner(*m_state, 1);
				return on_result(typed_local<type::function>(top));
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

			void *to_user_data(any_local const &local)
			{
				return lua_touserdata(m_state.get(), local.from_bottom());
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

			template <class ResultHandler>
			auto create_user_data(std::size_t size, ResultHandler const &on_result)
			{
				void *user_data = lua_newuserdata(m_state.get(), size);
				assert(size == 0 || user_data);
				detail::owner_of_the_top const owner(*m_state, 1);
				return on_result(typed_local<type::user_data>(lua_gettop(m_state.get())));
			}

			template <class ResultHandler>
			auto create_table(ResultHandler const &on_result)
			{
				lua_createtable(m_state.get(), 0, 0);
				detail::owner_of_the_top const owner(*m_state, 1);
				return on_result(typed_local<type::table>(lua_gettop(m_state.get())));
			}

			template <class Table, class Key, class Element>
			void set_element(Table const &table, Key const &key, Element const &element)
			{
				push(*m_state, table);
				detail::owner_of_the_top const table_owner(*m_state, 1);
				push(*m_state, key);
				push(*m_state, element);
				lua_settable(m_state.get(), -3);
			}

			template <class Object, class Metatable>
			void set_meta_table(Object const &object, Metatable const &meta)
			{
				push(*m_state, object);
				detail::owner_of_the_top const object_owner(*m_state, 1);
				push(*m_state, meta);
				lua_setmetatable(m_state.get(), -2);
			}

		private:

			state_ptr m_state;

			int checked_top() const BOOST_NOEXCEPT
			{
				int top = lua_gettop(m_state.get());
				assert(top >= 0);
				return top;
			}
		};

		namespace detail
		{
			template <class Function>
			int call_upvalue_function(lua_State *L) BOOST_NOEXCEPT
			{
				Function * const f_stored = static_cast<Function *>(lua_touserdata(L, lua_upvalueindex(1)));
				assert(f_stored);
				return (*f_stored)(L);
			}

			template <class Function>
			int delete_function(lua_State *L) BOOST_NOEXCEPT
			{
				Function * const function = static_cast<Function *>(lua_touserdata(L, -1));
				assert(function);
				function->~Function();
				return 0;
			}

			struct placement_destructor
			{
				template <class T>
				void operator()(T *object) const BOOST_NOEXCEPT
				{
					object->~T();
				}
			};
		}

		template <class Function, class ResultHandler>
		auto register_closure(stack &s, Function &&f, ResultHandler const &on_result)
		{
			typedef typename std::decay<Function>::type clean_function;
			return s.create_user_data(sizeof(f), [&](typed_local<type::user_data> data)
			{
				clean_function * const f_stored = static_cast<clean_function *>(s.to_user_data(data));
				assert(f_stored);
				new (f_stored) clean_function{std::forward<Function>(f)};
				std::unique_ptr<clean_function, detail::placement_destructor> f_stored_handle(f_stored);
				return s.create_table([&](typed_local<type::table> meta_table)
				{
					//TODO: cache metatable
					s.register_function(detail::delete_function<clean_function>, [&](typed_local<type::function> destructor)
					{
						s.set_element(meta_table, "__gc", destructor);
					});
					s.set_meta_table(data, meta_table);
					f_stored_handle.release();
					return s.register_function(
						detail::call_upvalue_function<clean_function>,
						Si::make_single_source(data),
						on_result
					);
				});
			});
		}

		inline Si::empty_source<pushable *> no_arguments()
		{
			return {};
		}
	}
}

#endif
