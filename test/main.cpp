#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <silicium/error_or.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/filesystem/operations.hpp>

namespace buildserver
{
	Si::error_or<bool> file_exists(boost::filesystem::path const &candidate)
	{
		boost::system::error_code ec;
		boost::filesystem::file_status status = boost::filesystem::status(candidate, ec);
		if (status.type() == boost::filesystem::file_not_found)
		{
			return false;
		}
		if (ec)
		{
			return ec;
		}
		return true;
	}

	Si::error_or<boost::optional<boost::filesystem::path>> find_file_in_directories(
		boost::filesystem::path const &filename,
		std::vector<boost::filesystem::path> const &directories)
	{
		for (auto const &directory : directories)
		{
			auto candidate = directory / filename;
			auto result = file_exists(candidate);
			if (result.is_error())
			{
				return result.error();
			}
			if (result.get())
			{
				return std::move(candidate);
			}
		}
		return boost::none;
	}

	Si::error_or<boost::optional<boost::filesystem::path>> find_executable_unix(
		boost::filesystem::path const &filename,
		std::vector<boost::filesystem::path> additional_directories)
	{
		additional_directories.emplace_back("/bin");
		additional_directories.emplace_back("/usr/bin");
		additional_directories.emplace_back("/usr/local/bin");
		return find_file_in_directories(filename, additional_directories);
	}
}
BOOST_AUTO_TEST_CASE(find_executable_unix_test)
{
	BOOST_CHECK_EQUAL(boost::none, buildserver::find_executable_unix("does-not-exist", {}));

#ifndef _WIN32
	BOOST_CHECK_EQUAL(boost::filesystem::path("/bin/sh"), buildserver::find_executable_unix("sh", {}));
	BOOST_CHECK_EQUAL(boost::none, buildserver::find_file_in_directories("sh", {}));
#endif
}
