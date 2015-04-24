#include <boost/test/unit_test.hpp>
#include <silicium/to_unique.hpp>
#include <silicium/to_shared.hpp>
#include "server/graph.hpp"

namespace
{
	graph::value identity(graph::value v)
	{
		return v;
	}

	graph::value uint32_to_le(graph::value v)
	{
		auto const * const i32 = Si::try_get_ptr<std::uint32_t>(v);
		BOOST_REQUIRE(i32);
		std::vector<char> le(4);
		le[0] = static_cast<char>(*i32);
		le[1] = static_cast<char>(*i32 >>  8);
		le[2] = static_cast<char>(*i32 >> 16);
		le[3] = static_cast<char>(*i32 >> 24);
		return graph::blob{std::move(le)};
	}

	graph::value uint32_add(graph::value v)
	{
		auto const * const arguments = Si::try_get_ptr<std::shared_ptr<graph::listing>>(v);
		BOOST_REQUIRE(arguments);
		auto const * first = graph::find_entry_of_type<std::uint32_t>(**arguments, "first");
		BOOST_REQUIRE(first);
		auto const * second = graph::find_entry_of_type<std::uint32_t>(**arguments, "second");
		BOOST_REQUIRE(second);
		return static_cast<std::uint32_t>(*first + *second);
	}

	graph::listing_type make_pair_type(graph::type first, graph::type second)
	{
		graph::listing_type result;
		result.entries.insert(std::make_pair("first", std::move(first)));
		result.entries.insert(std::make_pair("second", std::move(second)));
		return result;
	}

	graph::listing make_pair(graph::value first, graph::value second)
	{
		graph::listing result;
		result.entries.insert(std::make_pair("first", std::move(first)));
		result.entries.insert(std::make_pair("second", std::move(second)));
		return result;
	}
}

BOOST_AUTO_TEST_CASE(graph_typed_transformation)
{
	graph::typed_transformation const tf_id_uint32{graph::atomic_type::uint32, graph::atomic_type::uint32, &identity};
	graph::typed_transformation const tf_id_absolute_path{graph::atomic_type::absolute_path, graph::atomic_type::absolute_path, &identity};
	graph::typed_transformation const tf_id_blob{graph::atomic_type::blob, graph::atomic_type::blob, &identity};
	graph::typed_transformation const tf_uint32_to_le{graph::atomic_type::uint32, graph::atomic_type::blob, &uint32_to_le};
	graph::typed_transformation const tf_uint32_add{Si::to_unique(make_pair_type(graph::atomic_type::uint32, graph::atomic_type::uint32)), graph::atomic_type::uint32, &uint32_add};
	graph::value const result = tf_id_blob.transform(
		tf_uint32_to_le.transform(
			tf_uint32_add.transform(
				Si::to_shared(make_pair(std::uint32_t(40), std::uint32_t(500)))
			)
		)
	);
	std::vector<char> const expected
	{
		28,
		2,
		0,
		0
	};
	auto const * const blob_result = Si::try_get_ptr<graph::blob>(result);
	BOOST_REQUIRE(blob_result);
	BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), blob_result->content.begin(), blob_result->content.end());
}
