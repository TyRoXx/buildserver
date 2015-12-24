#include <boost/test/unit_test.hpp>
#include <silicium/variant.hpp>
#include <silicium/function.hpp>
#include <silicium/array_view.hpp>
#include <silicium/sink/ostream_sink.hpp>
#include <silicium/sink/append.hpp>
#include <boost/scope_exit.hpp>
#include <iostream>

namespace
{
	struct value;
	struct type;

	struct integer
	{
		typedef std::uint64_t digit;

		std::vector<digit> digits;

		integer()
		{
			// TODO: remove default constructor
		}

		explicit integer(digit single_digit)
		{
			digits.emplace_back(single_digit);
		}
	};

	struct integer_type
	{
	};

	struct tuple
	{
		std::vector<value> elements;
	};

	struct identifier
	{
		std::string value;

		explicit identifier(std::string value)
		    : value(std::move(value))
		{
		}
	};

	bool operator==(identifier const &left, identifier const &right)
	{
		return (left.value == right.value);
	}

	bool operator<(identifier const &left, identifier const &right)
	{
		return (left.value < right.value);
	}

	struct struct_type
	{
		struct element
		{
			std::shared_ptr<type const> type_of;
			identifier name;
		};

		std::vector<element> elements;
	};

	struct tuple_type
	{
		std::vector<type> elements;
	};

	struct extern_function
	{
		Si::function<value(value const &)> call;
	};

	struct identifier_type
	{
	};

	struct expression;

	struct declaration
	{
		identifier name;
		std::unique_ptr<expression> initial_value;
	};

	struct closure
	{
		std::shared_ptr<value const> bound;
		std::shared_ptr<expression const> body;

		closure(std::shared_ptr<value const> bound, std::shared_ptr<expression const> body)
		    : bound(std::move(bound))
		    , body(std::move(body))
		{
		}
	};

	struct function_type
	{
		std::shared_ptr<type const> parameter;
		std::shared_ptr<type const> result;

		explicit function_type(std::shared_ptr<type const> parameter, std::shared_ptr<type const> result)
		    : parameter(std::move(parameter))
		    , result(std::move(result))
		{
		}
	};

	struct generic_function_type
	{
		std::shared_ptr<type const> parameter;

		explicit generic_function_type(std::shared_ptr<type const> parameter)
		    : parameter(std::move(parameter))
		{
		}
	};

	struct constrained_type
	{
		std::shared_ptr<type const> original;
		std::shared_ptr<closure const> is_valid;
	};

	struct variant_type
	{
		std::vector<type> variants;
	};

	struct type_type
	{
	};

	struct type
	{
		Si::variant<integer_type, tuple_type, struct_type, function_type, generic_function_type, constrained_type,
		            type_type, variant_type, identifier_type> content;

		explicit type(decltype(content) content)
		    : content(std::move(content))
		{
		}
	};

	struct value
	{
		Si::variant<integer, tuple, closure, extern_function, type, identifier> content;

		explicit value(decltype(content) content)
		    : content(std::move(content))
		{
		}
	};

	value make_unit()
	{
		return value(tuple());
	}

	type make_unit_type()
	{
		return type(tuple_type());
	}

	struct pattern
	{
		std::unique_ptr<expression> matches;
	};

	struct match
	{
		struct branch
		{
			pattern key;
			std::unique_ptr<expression> value;
		};

		std::vector<branch> branches;
	};

	struct literal
	{
		value literal_value;

		explicit literal(value literal_value)
		    : literal_value(std::move(literal_value))
		{
		}
	};

	struct call
	{
		std::unique_ptr<expression> callee;
		std::unique_ptr<expression> argument;

		call(std::unique_ptr<expression> callee, std::unique_ptr<expression> argument)
		    : callee(std::move(callee))
		    , argument(std::move(argument))
		{
		}
	};

	struct local_value
	{
		identifier name;
	};

	struct less
	{
		std::unique_ptr<expression> left, right;
	};

	struct bound_value
	{
	};

	struct lambda
	{
		std::unique_ptr<expression> bound;
		std::unique_ptr<expression> parameter;
		std::shared_ptr<expression const> body;

		lambda(std::unique_ptr<expression> bound, std::unique_ptr<expression> parameter,
		       std::shared_ptr<expression const> body)
		    : bound(std::move(bound))
		    , parameter(std::move(parameter))
		    , body(std::move(body))
		{
		}
	};

	struct constrain
	{
		std::unique_ptr<expression> original;
		lambda is_valid;
	};

	struct module_name
	{
		identifier name;
	};

	struct symbol_value
	{
		module_name import;
		identifier key;
	};

	struct local_symbol
	{
		identifier name;

		explicit local_symbol(identifier name)
		    : name(std::move(name))
		{
		}
	};

	struct make_tuple
	{
		std::vector<expression> elements;
	};

	struct tuple_at
	{
		std::unique_ptr<expression> tuple;
		std::unique_ptr<expression> index;
	};

	struct visit_variant
	{
		std::unique_ptr<expression> variant;
		std::vector<expression> visitors;
	};

	struct break_loop
	{
		std::unique_ptr<expression> result;
	};

	struct block
	{
		std::vector<expression> steps;
		std::unique_ptr<expression> result;
	};

	struct assignment
	{
		identifier target;
		std::unique_ptr<expression> new_value;
	};

	struct loop
	{
		std::unique_ptr<expression> repeated;
	};

	struct return_from_function
	{
		std::unique_ptr<expression> result;

		explicit return_from_function(std::unique_ptr<expression> result)
		    : result(std::move(result))
		{
		}
	};

	struct new_type
	{
		std::shared_ptr<expression const> original;
	};

	struct argument
	{
	};

	struct expression
	{
		Si::variant<match, literal, local_value, less, constrain, lambda, bound_value, symbol_value, local_symbol,
		            make_tuple, tuple_at, visit_variant, return_from_function, break_loop, block, assignment, loop,
		            new_type, argument, call> content;

		explicit expression(decltype(content) content)
		    : content(std::move(content))
		{
		}
	};

	struct module_declaration
	{
		struct exported
		{
			type type_;
			identifier key;
		};
		std::vector<exported> exports;
	};

	struct module_instance
	{
		struct exported
		{
			value value_;
			identifier key;
		};
		std::vector<exported> exports;
	};

	struct module_definition
	{
		struct exported
		{
			identifier key;
			expression value;

			exported(identifier key, expression value)
			    : key(std::move(key))
			    , value(std::move(value))
			{
			}
		};
		std::vector<module_name> imports;
		std::vector<exported> exports;
	};

	struct local_symbol_table;

	Si::optional<type> type_of(expression const &root, Si::optional<value const &> bound,
	                           Si::optional<type const &> argument_type, Si::optional<value const &> argument_,
	                           local_symbol_table &symbols);

	Si::optional<value> evaluate(expression const &root, Si::optional<value const &> bound,
	                             Si::optional<value const &> argument_, local_symbol_table &symbols);

	struct local_symbol_table
	{
		struct symbol_info
		{
			Si::optional<type> known_type;
			Si::optional<value> known_value;
			bool is_being_evaluated;

			symbol_info()
			    : is_being_evaluated(false)
			{
			}
		};

		std::vector<module_definition::exported> const &exports;
		std::map<identifier, symbol_info> symbols;

		explicit local_symbol_table(std::vector<module_definition::exported> const &exports)
		    : exports(exports)
		{
		}

		Si::optional<type> type_of(identifier const &symbol, Si::optional<value const &> bound,
		                           Si::optional<type const &> argument_type, Si::optional<value const &> argument_)
		{
			auto const found =
			    std::find_if(exports.begin(), exports.end(), [&symbol](module_definition::exported const &exported)
			                 {
				                 return exported.key == symbol;
				             });
			if (found == exports.end())
			{
				throw std::logic_error("not implemented");
			}
			symbol_info &info = symbols[symbol];
			if (info.is_being_evaluated)
			{
				throw std::invalid_argument("Type of symbol " + symbol.value + " depends on itself");
			}
			if (info.known_type)
			{
				return *info.known_type;
			}
			info.is_being_evaluated = true;
			BOOST_SCOPE_EXIT(&info)
			{
				info.is_being_evaluated = false;
			}
			BOOST_SCOPE_EXIT_END;
			Si::optional<type> calculated_type = ::type_of(found->value, bound, argument_type, argument_, *this);
			if (!calculated_type)
			{
				return Si::none;
			}
			info.known_type = std::move(*calculated_type);
			return info.known_type;
		}

		Si::optional<value> evaluate(identifier const &symbol, Si::optional<value const &> bound,
		                             Si::optional<value const &> argument_)
		{
			auto const found =
			    std::find_if(exports.begin(), exports.end(), [&symbol](module_definition::exported const &exported)
			                 {
				                 return exported.key == symbol;
				             });
			if (found == exports.end())
			{
				throw std::logic_error("not implemented");
			}
			symbol_info &info = symbols[symbol];
			if (info.is_being_evaluated)
			{
				throw std::invalid_argument("Value of symbol " + symbol.value + " depends on itself");
			}
			info.is_being_evaluated = true;
			BOOST_SCOPE_EXIT(&info)
			{
				info.is_being_evaluated = false;
			}
			BOOST_SCOPE_EXIT_END;
			Si::optional<value> calculated_value = ::evaluate(found->value, bound, argument_, *this);
			if (!calculated_value)
			{
				return Si::none;
			}
			info.known_value = std::move(*calculated_value);
			return info.known_value;
		}
	};

	type type_of(value const &root)
	{
		return Si::visit<type>(root.content,
		                       [](integer const &content) -> type
		                       {
			                       throw std::logic_error("not implemented");
			                   },
		                       [](tuple const &content) -> type
		                       {
			                       throw std::logic_error("not implemented");
			                   },
		                       [](closure const &content) -> type
		                       {
			                       throw std::logic_error("not implemented");
			                   },
		                       [](extern_function const &content) -> type
		                       {
			                       throw std::logic_error("not implemented");
			                   },
		                       [](type const &content) -> type
		                       {
			                       return type(type_type());
			                   },
		                       [](identifier const &content) -> type
		                       {
			                       throw std::logic_error("not implemented");
			                   });
	}

	Si::optional<value> evaluate(expression const &root, Si::optional<value const &> bound,
	                             Si::optional<value const &> argument_, local_symbol_table &symbols)
	{
		return Si::visit<Si::optional<value>>(
		    root.content,
		    [](match const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			},
		    [](literal const &content) -> value
		    {
			    return content.literal_value;
			},
		    [](local_value const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			},
		    [](less const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			},
		    [](constrain const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			},
		    [&bound, &argument_, &symbols](lambda const &content) -> value
		    {
			    Si::optional<value> new_bound = evaluate(*content.bound, bound, argument_, symbols);
			    if (!new_bound)
			    {
				    throw std::logic_error("not implemented");
			    }
			    return value(closure(Si::to_shared(std::move(*new_bound)), content.body));
			},
		    [&bound](bound_value const &) -> Si::optional<value>
		    {
			    return bound ? Si::optional<value>(*bound) : Si::none;
			},
		    [](symbol_value const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			},
		    [&bound, &argument_, &symbols](local_symbol const &content) -> Si::optional<value>
		    {
			    return symbols.evaluate(content.name, bound, argument_);
			},
		    [](make_tuple const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			},
		    [](tuple_at const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			},
		    [](visit_variant const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			},
		    [](return_from_function const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			},
		    [](break_loop const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			},
		    [](block const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			},
		    [](assignment const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			},
		    [](loop const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			},
		    [](new_type const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			},
		    [&argument_](argument const &) -> Si::optional<value>
		    {
			    if (!argument_)
			    {
				    return Si::none;
			    }
			    return *argument_;
			},
		    [](call const &content) -> value
		    {
			    throw std::logic_error("not implemented");
			});
	}

	Si::optional<type> type_of(expression const &root, Si::optional<value const &> bound,
	                           Si::optional<type const &> argument_type, Si::optional<value const &> argument_,
	                           local_symbol_table &symbols)
	{
		return Si::visit<Si::optional<type>>(
		    root.content,
		    [](match const &content) -> type
		    {
			    throw std::logic_error("not implemented");
			},
		    [](literal const &content) -> type
		    {
			    return type_of(content.literal_value);
			},
		    [](local_value const &content) -> type
		    {
			    throw std::logic_error("not implemented");
			},
		    [](less const &content) -> type
		    {
			    throw std::logic_error("not implemented");
			},
		    [](constrain const &content) -> type
		    {
			    throw std::logic_error("not implemented");
			},
		    [&argument_type, &bound, &argument_, &symbols](lambda const &content) -> Si::optional<type>
		    {
			    Si::optional<value> new_bound = evaluate(*content.bound, bound, argument_, symbols);
			    Si::optional<value const &> new_bound_ref =
			        new_bound ? Si::optional<value const &>(*new_bound) : Si::none;
			    Si::optional<value> parameter_evaluated =
			        evaluate(*content.parameter, new_bound_ref, argument_, symbols);
			    if (!parameter_evaluated)
			    {
				    return Si::none;
			    }
			    type *const parameter = Si::try_get_ptr<type>(parameter_evaluated->content);
			    if (!parameter)
			    {
				    throw std::logic_error("not implemented");
			    }
			    Si::optional<type> result =
			        type_of(*content.body, new_bound_ref, parameter ? Si::optional<type const &>(*parameter) : Si::none,
			                Si::none, symbols);
			    if (result)
			    {
				    return type(function_type(Si::to_unique(std::move(*parameter)), Si::to_unique(std::move(*result))));
			    }
			    return type(generic_function_type(Si::to_unique(std::move(*parameter))));
			},
		    [](bound_value const &content) -> type
		    {
			    throw std::logic_error("not implemented");
			},
		    [](symbol_value const &content) -> type
		    {
			    throw std::logic_error("not implemented");
			},
		    [&argument_type, &bound, &argument_, &symbols](local_symbol const &content) -> Si::optional<type>
		    {
			    return symbols.type_of(content.name, bound, argument_type, argument_);
			},
		    [](make_tuple const &content) -> type
		    {
			    throw std::logic_error("not implemented");
			},
		    [](tuple_at const &content) -> type
		    {
			    throw std::logic_error("not implemented");
			},
		    [](visit_variant const &content) -> type
		    {
			    throw std::logic_error("not implemented");
			},
		    [&argument_type, &bound, &argument_, &symbols](return_from_function const &content) -> Si::optional<type>
		    {
			    return type_of(*content.result, bound, argument_type, argument_, symbols);
			},
		    [](break_loop const &content) -> type
		    {
			    throw std::logic_error("not implemented");
			},
		    [](block const &content) -> type
		    {
			    throw std::logic_error("not implemented");
			},
		    [](assignment const &content) -> type
		    {
			    throw std::logic_error("not implemented");
			},
		    [](loop const &content) -> type
		    {
			    throw std::logic_error("not implemented");
			},
		    [](new_type const &content) -> type
		    {
			    throw std::logic_error("not implemented");
			},
		    [&argument_type](argument const &) -> Si::optional<type>
		    {
			    if (argument_type)
			    {
				    return type(*argument_type);
			    }
			    return Si::none;
			},
		    [&argument_type, &bound, &argument_, &symbols](call const &content) -> Si::optional<type>
		    {
			    Si::optional<type> callee_type = type_of(*content.callee, bound, argument_type, argument_, symbols);
			    if (!callee_type)
			    {
				    throw std::logic_error("not implemented");
			    }
			    return Si::visit<Si::optional<type>>(
			        callee_type->content,
			        [](integer_type const &) -> type
			        {
				        throw std::logic_error("not implemented");
				    },
			        [](tuple_type const &content) -> type
			        {
				        throw std::logic_error("not implemented");
				    },
			        [](struct_type const &content) -> type
			        {
				        throw std::logic_error("not implemented");
				    },
			        [](function_type const &content) -> type
			        {
				        return *content.result;
				    },
			        [&content, &bound, &argument_, &symbols](generic_function_type const &) -> Si::optional<type>
			        {
				        Si::optional<value> callee = evaluate(*content.callee, bound, argument_, symbols);
				        Si::optional<value> argument = evaluate(*content.argument, bound, argument_, symbols);
				        if (!callee)
				        {
					        throw std::logic_error("not implemented");
				        }
				        if (!argument)
				        {
					        throw std::logic_error("not implemented");
				        }
				        closure *const closure_ = Si::try_get_ptr<closure>(callee->content);
				        if (!closure_)
				        {
					        throw std::logic_error("not implemented");
				        }
				        return type_of(*closure_->body, *closure_->bound, Si::none, *argument, symbols);
				    },
			        [](constrained_type const &content) -> type
			        {
				        throw std::logic_error("not implemented");
				    },
			        [](type_type const &) -> type
			        {
				        throw std::logic_error("not implemented");
				    },
			        [](variant_type const &content) -> type
			        {
				        throw std::logic_error("not implemented");
				    },
			        [](identifier_type const &content) -> type
			        {
				        throw std::logic_error("not implemented");
				    });
			});
	}

	typedef Si::Sink<char, Si::success>::interface text_writer;

	void format(text_writer &writer, char const *utf8)
	{
		Si::append(writer, utf8);
	}

	void format(text_writer &writer, std::string const &utf8)
	{
		Si::append(writer, utf8);
	}

	template <class T>
	struct ostreamed
	{
		T const &value;

		explicit ostreamed(T const &value)
		    : value(value)
		{
		}
	};

	template <class T>
	void format(text_writer &writer, ostreamed<T> const &formatted)
	{
		std::ostringstream buffer;
		buffer << formatted.value;
		format(writer, buffer.str());
	}

	template <class T>
	ostreamed<T> ostream(T const &value)
	{
		return ostreamed<T>(value);
	}

	void print(text_writer &)
	{
	}

	template <class Arg0, class... Args>
	void print(text_writer &writer, Arg0 &&arg0, Args &&... args)
	{
		format(writer, std::forward<Arg0>(arg0));
		print(writer, std::forward<Args>(args)...);
	}

	void pretty_print(text_writer &writer, type const &root)
	{
		Si::visit<void>(root.content,
		                [&writer](integer_type const &)
		                {
			                print(writer, "%integer");
			            },
		                [&writer](tuple_type const &content)
		                {
			                print(writer, "%(");
			                bool first = true;
			                for (type const &element : content.elements)
			                {
				                if (!first)
				                {
					                first = false;
				                }
				                else
				                {
					                print(writer, " ,");
				                }
				                pretty_print(writer, element);
			                }
			                print(writer, ")");
			            },
		                [&writer](struct_type const &content)
		                {
			                print(writer, "%(");
			                bool first = true;
			                for (struct_type::element const &element : content.elements)
			                {
				                if (!first)
				                {
					                first = false;
				                }
				                else
				                {
					                print(writer, " ,");
				                }
				                print(writer, element.name.value, ": ");
				                pretty_print(writer, *element.type_of);
			                }
			                print(writer, ")");
			            },
		                [&writer](function_type const &content)
		                {
			                pretty_print(writer, *content.parameter);
			                print(writer, " -> ");
			                pretty_print(writer, *content.result);
			            },
		                [&writer](generic_function_type const &content)
		                {
			                pretty_print(writer, *content.parameter);
			                print(writer, " -> ");
			                print(writer, "%generic");
			            },
		                [&writer](constrained_type const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](type_type const &)
		                {
			                print(writer, "%type");
			            },
		                [&writer](variant_type const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](identifier_type const &content)
		                {
			                throw std::logic_error("not implemented");
			            });
	}

	void pretty_print(text_writer &writer, value const &root)
	{
		Si::visit<void>(root.content,
		                [&writer](integer const &content)
		                {
			                for (integer::digit digit : content.digits)
			                {
				                print(writer, ostream(digit));
			                }
			            },
		                [&writer](tuple const &content)
		                {
			                print(writer, "(");
			                bool first = true;
			                for (value const &element : content.elements)
			                {
				                if (!first)
				                {
					                first = false;
				                }
				                else
				                {
					                print(writer, " ,");
				                }
				                pretty_print(writer, element);
			                }
			                print(writer, ")");
			            },
		                [&writer](closure const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](extern_function const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](type const &content)
		                {
			                pretty_print(writer, content);
			            },
		                [&writer](identifier const &content)
		                {
			                print(writer, content.value);
			            });
	}

	void pretty_print(text_writer &writer, expression const &root)
	{
		Si::visit<void>(root.content,
		                [&writer](match const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](literal const &content)
		                {
			                pretty_print(writer, content.literal_value);
			            },
		                [&writer](local_value const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](less const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](constrain const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](lambda const &content)
		                {
			                print(writer, "[");
			                pretty_print(writer, *content.bound);
			                print(writer, "](");
			                pretty_print(writer, *content.parameter);
			                print(writer, ") -> ");
			                pretty_print(writer, *content.body);
			            },
		                [&writer](bound_value const &)
		                {
			                print(writer, "#bound");
			            },
		                [&writer](symbol_value const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](local_symbol const &content)
		                {
			                print(writer, "#local[", content.name.value, "]");
			            },
		                [&writer](make_tuple const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](tuple_at const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](visit_variant const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](return_from_function const &content)
		                {
			                print(writer, "#return ");
			                pretty_print(writer, *content.result);
			            },
		                [&writer](break_loop const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](block const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](assignment const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](loop const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](new_type const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](argument const &)
		                {
			                print(writer, "#argument");
			            },
		                [&writer](call const &content)
		                {
			                pretty_print(writer, *content.callee);
			                print(writer, "(");
			                pretty_print(writer, *content.argument);
			                print(writer, ")");
			            });
	}

	void pretty_print(text_writer &writer, module_definition const &root, local_symbol_table &symbols)
	{
		for (module_name const &import : root.imports)
		{
			print(writer, "import ", import.name.value, "\n");
		}
		for (module_definition::exported const &exported : root.exports)
		{
			print(writer, exported.key.value, ": ");
			Si::optional<type> type_ = type_of(exported.value, Si::none, Si::none, Si::none, symbols);
			if (type_)
			{
				pretty_print(writer, *type_);
			}
			else
			{
				print(writer, "%generic");
			}
			print(writer, " = ");
			pretty_print(writer, exported.value);
			print(writer, "\n");
		}
	}
}

BOOST_AUTO_TEST_CASE(language_integer_identity)
{
	module_definition m;

	m.exports.emplace_back(module_definition::exported(
	    identifier("identity"), expression(lambda(Si::make_unique<expression>(literal(make_unit())),
	                                              Si::make_unique<expression>(literal(value(type(integer_type())))),
	                                              Si::make_unique<expression>(return_from_function(
	                                                  Si::make_unique<expression>(argument())))))));

	m.exports.emplace_back(module_definition::exported(
	    identifier("call identity"),
	    expression(
	        lambda(Si::make_unique<expression>(literal(make_unit())),
	               Si::make_unique<expression>(literal(value(make_unit_type()))),
	               Si::make_unique<expression>(call(Si::make_unique<expression>(local_symbol(identifier("identity"))),
	                                                Si::make_unique<expression>(literal(value(integer(456))))))))));

	local_symbol_table symbols(m.exports);

	auto writer = Si::Sink<char, Si::success>::erase(Si::ostream_ref_sink(std::cout));
	pretty_print(writer, m, symbols);
}

BOOST_AUTO_TEST_CASE(language_generic_identity)
{
	module_definition m;

	m.exports.emplace_back(module_definition::exported(
	    identifier("generic identity"),
	    expression(
	        lambda(Si::make_unique<expression>(literal(make_unit())),
	               Si::make_unique<expression>(literal(value(type(type_type())))),
	               Si::make_unique<expression>(lambda(
	                   Si::make_unique<expression>(argument()), Si::make_unique<expression>(bound_value()),
	                   Si::make_unique<expression>(return_from_function(Si::make_unique<expression>(argument())))))))));

	m.exports.emplace_back(module_definition::exported(
	    identifier("call generic identity"),
	    expression(lambda(Si::make_unique<expression>(literal(make_unit())),
	                      Si::make_unique<expression>(literal(value(make_unit_type()))),
	                      Si::make_unique<expression>(
	                          call(Si::make_unique<expression>(
	                                   call(Si::make_unique<expression>(local_symbol(identifier("generic identity"))),
	                                        Si::make_unique<expression>(literal(value(type(integer_type())))))),
	                               Si::make_unique<expression>(literal(value(integer(456))))))))));

	local_symbol_table symbols(m.exports);

	auto writer = Si::Sink<char, Si::success>::erase(Si::ostream_ref_sink(std::cout));
	pretty_print(writer, m, symbols);
}
