#include <boost/test/unit_test.hpp>
#include <silicium/variant.hpp>
#include <silicium/function.hpp>
#include <silicium/array_view.hpp>

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

	struct tuple_type
	{
		std::vector<type> elements;
	};

	struct extern_function
	{
		Si::function<value(value const &)> call;
	};

	struct identifier
	{
		std::string value;
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
		Si::variant<integer_type, tuple_type, function_type, constrained_type, type_type, variant_type, identifier_type>
		    content;
	};

	struct value
	{
		Si::variant<integer, tuple, closure, extern_function, type, variant, identifier> content;
	};

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
	};

	struct call
	{
		std::unique_ptr<expression> callee;
		std::unique_ptr<expression> argument;
	};

	struct named_value
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
		integer index;
	};

	struct lambda
	{
		std::vector<expression> bound;
		type result;
		type parameter;
		std::unique_ptr<expression> body;
	};

	struct constrain
	{
		std::unique_ptr<expression> original;
		lambda is_valid;
	};

	struct symbol
	{
		identifier name;
	};

	struct module_name
	{
		identifier name;
	};

	struct symbol_value
	{
		module_name import;
		symbol key;
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
		std::vector<expression> visitors;
	};

	struct return_from_function
	{
		value result;
	};

	struct break_loop
	{
	};

	struct block
	{
		std::vector<expression> steps;
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

	struct new_type
	{
		std::unique_ptr<expression> original;
	};

	struct expression
	{
		Si::variant<match, literal, named_value, equal, less, not_, constrain, lambda, bound_value, symbol_value,
		            make_tuple, tuple_at, visit_variant, return_from_function, break_loop, block,
		            assignment, loop, new_type> content;
	};

	struct module_declaration
	{
		struct exported
		{
			type type_;
			symbol key;
		};
		std::vector<exported> exports;
	};

	struct module_instance
	{
		struct exported
		{
			value value_;
			symbol key;
		};
		std::vector<exported> exports;
	};

	struct module_definition
	{
		struct exported
		{
			symbol key;
			expression value;
		};
		std::vector<module_name> imports;
		std::vector<exported> exports;
	};

	// TODO
	module_instance instantiate_module(module_definition const &definition, Si::array_view<module_instance> imports);
}

BOOST_AUTO_TEST_CASE(language_trivial)
{
	module_definition m;
}
