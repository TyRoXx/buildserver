#include <boost/test/unit_test.hpp>
#include <silicium/variant.hpp>
#include <silicium/function.hpp>
#include <silicium/array_view.hpp>
#include <silicium/sink/ostream_sink.hpp>
#include <silicium/sink/append.hpp>
#include <iostream>

namespace
{
	struct value;
	struct type;

	struct integer
	{
		typedef std::uint64_t digit;

		std::vector<digit> digits;
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

	struct struct_type
	{
		struct element
		{
			std::unique_ptr<type> type_of;
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

	struct variant
	{
		std::unique_ptr<value> content;
	};

	struct expression;

	struct declaration
	{
		identifier name;
		std::unique_ptr<expression> initial_value;
	};

	struct closure
	{
		std::vector<value> bound;
		std::unique_ptr<expression> body;
	};

	struct function_type
	{
		std::unique_ptr<type> parameter;
		std::unique_ptr<type> result;
	};

	struct constrained_type
	{
		std::unique_ptr<type> original;
		std::unique_ptr<closure> is_valid;
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
		Si::variant<integer_type, tuple_type, struct_type, function_type, constrained_type, type_type, variant_type,
		            identifier_type> content;

		explicit type(decltype(content) content)
		    : content(std::move(content))
		{
		}
	};

	struct value
	{
		Si::variant<integer, tuple, closure, extern_function, type, variant, identifier> content;

		explicit value(decltype(content) content)
		    : content(std::move(content))
		{
		}
	};

	value make_unit()
	{
		return value(tuple());
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
	};

	struct local_value
	{
		identifier name;
	};

	struct equal
	{
		std::unique_ptr<expression> left, right;
	};

	struct less
	{
		std::unique_ptr<expression> left, right;
	};

	struct not_
	{
		std::unique_ptr<expression> input;
	};

	struct bound_value
	{
	};

	struct lambda
	{
		std::unique_ptr<expression> bound;
		std::unique_ptr<expression> parameter;
		std::unique_ptr<expression> body;

		lambda(std::unique_ptr<expression> bound, std::unique_ptr<expression> parameter,
		       std::unique_ptr<expression> body)
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
		std::unique_ptr<expression> original;
	};

	struct argument
	{
	};

	struct expression
	{
		Si::variant<match, literal, local_value, equal, less, not_, constrain, lambda, bound_value, symbol_value,
		            make_tuple, tuple_at, visit_variant, return_from_function, break_loop, block, assignment, loop,
		            new_type, argument> content;

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
			                throw std::logic_error("not implemented");
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
		                [&writer](variant const &content)
		                {
			                throw std::logic_error("not implemented");
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
		                [&writer](equal const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](less const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](not_ const &content)
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
		                [&writer](bound_value const &content)
		                {
			                throw std::logic_error("not implemented");
			            },
		                [&writer](symbol_value const &content)
		                {
			                throw std::logic_error("not implemented");
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
			            });
	}

	void pretty_print(text_writer &writer, module_definition const &root)
	{
		for (module_name const &import : root.imports)
		{
			print(writer, "import ", import.name.value, "\n");
		}
		for (module_definition::exported const &exported : root.exports)
		{
			print(writer, exported.key.value, ": ");
			pretty_print(writer, exported.value);
			print(writer, "\n");
		}
	}

	// TODO
	module_instance instantiate_module(module_definition const &definition, Si::array_view<module_instance> imports);
}

BOOST_AUTO_TEST_CASE(language_trivial)
{
	module_definition m;
	m.exports.emplace_back(module_definition::exported(
	    identifier("identity"), expression(lambda(Si::make_unique<expression>(literal(make_unit())),
	                                              Si::make_unique<expression>(literal(value(type(integer_type())))),
	                                              Si::make_unique<expression>(return_from_function(
	                                                  Si::make_unique<expression>(argument())))))));
	m.exports.emplace_back(module_definition::exported(
	    identifier("generic identity"),
	    expression(
	        lambda(Si::make_unique<expression>(literal(make_unit())),
	               Si::make_unique<expression>(literal(value(type(type_type())))),
	               Si::make_unique<expression>(lambda(
	                   Si::make_unique<expression>(literal(make_unit())), Si::make_unique<expression>(argument()),
	                   Si::make_unique<expression>(return_from_function(Si::make_unique<expression>(argument())))))))));

	auto writer = Si::Sink<char, Si::success>::erase(Si::ostream_ref_sink(std::cout));
	pretty_print(writer, m);
}
