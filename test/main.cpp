#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include "server/cmake.hpp"
#include <silicium/error_or.hpp>
#include <silicium/override.hpp>
#include <silicium/process.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/unordered_map.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/thread.hpp>

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

	struct gcc_location
	{
		boost::filesystem::path gcc, gxx;
	};

	Si::error_or<boost::optional<gcc_location>> find_gcc_unix()
	{
		auto gcc = find_executable_unix("gcc", {});
		if (gcc.is_error())
		{
			return gcc.error();
		}
		if (!gcc.get())
		{
			return boost::none;
		}
		auto gxx = find_file_in_directories("g++", {gcc.get()->parent_path()});
		return Si::map(std::move(gxx), [&gcc](boost::optional<boost::filesystem::path> gxx_path) -> boost::optional<gcc_location>
		{
			if (gxx_path)
			{
				gcc_location result;
				result.gcc = std::move(*gcc.get());
				result.gxx = std::move(*gxx_path);
				return result;
			}
			else
			{
				return boost::none;
			}
		});
	}

	Si::error_or<boost::optional<boost::filesystem::path>> find_cmake_unix()
	{
		return find_executable_unix("cmake", {});
	}
}

BOOST_AUTO_TEST_CASE(find_executable_unix_test)
{
	BOOST_CHECK_EQUAL(boost::none, buildserver::find_executable_unix("does-not-exist", {}));

#ifndef _WIN32
	BOOST_CHECK_EQUAL(boost::filesystem::path("/bin/sh"), buildserver::find_executable_unix("sh", {}));
	BOOST_CHECK_EQUAL(boost::none, buildserver::find_file_in_directories("sh", {}));

	auto gnuc = buildserver::find_gcc_unix();
	BOOST_REQUIRE(gnuc.get());
	BOOST_CHECK_EQUAL("/usr/bin/gcc", gnuc.get()->gcc);
	BOOST_CHECK_EQUAL("/usr/bin/g++", gnuc.get()->gxx);
#endif
}

BOOST_AUTO_TEST_CASE(cmake_exe_test)
{
	auto cmake = buildserver::find_cmake_unix().get();
	BOOST_REQUIRE(cmake);
	buildserver::cmake_exe const cmake_driver(*cmake);
	boost::filesystem::path const build_path = "/tmp/buildtest123456";
	boost::filesystem::remove_all(build_path);
	boost::filesystem::create_directories(build_path);
	boost::filesystem::path const dev_path = boost::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
	cmake_driver.generate(dev_path / "buildserver", build_path, {{"SILICIUM_INCLUDE_DIR", (dev_path / "silicium").string()}});
	cmake_driver.build(build_path, boost::thread::hardware_concurrency());
}
