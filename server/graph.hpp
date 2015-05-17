#ifndef BUILDSERVER_GRAPH_HPP
#define BUILDSERVER_GRAPH_HPP

#include <silicium/fast_variant.hpp>
#include <silicium/absolute_path.hpp>
#include <silicium/path_segment.hpp>
#include <silicium/function.hpp>
#include <vector>
#include <map>
#include <cstdint>

namespace graph
{
	struct listing;

	struct blob
	{
		std::vector<char> content;
	};

	struct uri
	{
		Si::noexcept_string value;
	};

	struct filesystem_directory_ownership
	{
		Si::absolute_path owned;
	};

	typedef Si::fast_variant<
		blob,
		std::shared_ptr<listing>,
		uri,
		filesystem_directory_ownership,
		Si::absolute_path,
		Si::path_segment,
		std::uint32_t
	> value;

	enum class atomic_type
	{
		blob,
		uri,
		filesystem_directory_ownership,
		absolute_path,
		path_segment,
		uint32
	};

	struct listing_type;

	typedef Si::fast_variant<
		atomic_type,
		std::shared_ptr<listing_type>
	> type;

	struct listing_type
	{
		std::map<Si::noexcept_string, type> entries;
	};

	struct listing
	{
		std::map<Si::noexcept_string, value> entries;
	};

	template <class T, class Listing>
	T *find_entry_of_type(Listing &list, Si::noexcept_string const &key)
	{
		auto i = list.entries.find(key);
		if (i == list.entries.end())
		{
			return nullptr;
		}
		return Si::try_get_ptr<T>(i->second);
	}

	struct input_type_mismatch
	{
	};

	value expect_value(Si::fast_variant<input_type_mismatch, value> maybe)
	{
		return Si::visit<value>(
			maybe,
			[](input_type_mismatch) -> value { throw std::invalid_argument("input type mismatch"); },
			[](value &result) { return std::move(result); }
		);
	}

	typedef Si::function<value (value)> untyped_transformation;

	struct typed_transformation
	{
		type input;
		type output;
		untyped_transformation transform;
	};
}

#endif
