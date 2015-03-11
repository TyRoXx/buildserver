#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include "server/cmake.hpp"
#include "server/find_cmake.hpp"
#include "server/find_executable.hpp"
#include "server/find_gcc.hpp"
#include <silicium/error_or.hpp>
#include <silicium/run_process.hpp>
#include <silicium/sink/virtualized_sink.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/unordered_map.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/thread.hpp>

BOOST_AUTO_TEST_CASE(find_executable_unix_test)
{
	BOOST_CHECK_EQUAL(Si::none, buildserver::find_executable_unix(*Si::path_segment::create("does-not-exist"), {}));

#ifndef _WIN32
	BOOST_CHECK_EQUAL(Si::absolute_path::create("/bin/sh"), buildserver::find_executable_unix(*Si::path_segment::create("sh"), {}));
	BOOST_CHECK_EQUAL(Si::none, buildserver::find_file_in_directories(*Si::path_segment::create("sh"), {}));

	auto gnuc = buildserver::find_gcc_unix();
	BOOST_REQUIRE(gnuc.get());
	BOOST_CHECK_EQUAL("/usr/bin/gcc", gnuc.get()->gcc);
	BOOST_CHECK_EQUAL("/usr/bin/g++", gnuc.get()->gxx);
#endif
}

BOOST_AUTO_TEST_CASE(cmake_exe_test)
{
	auto cmake = buildserver::find_cmake().get();
	BOOST_REQUIRE(cmake);
	buildserver::cmake_exe const cmake_driver(*cmake);
	Si::absolute_path const build_path = *Si::absolute_path::create(boost::filesystem::temp_directory_path() / "buildtest123456");
	boost::filesystem::remove_all(build_path.to_boost_path());
	boost::filesystem::create_directories(build_path.to_boost_path());
	Si::absolute_path const resources_path = *Si::absolute_path::create(boost::filesystem::path(__FILE__).parent_path().parent_path() / "test-resources");
	cmake_driver.generate(resources_path / "test1", build_path, boost::unordered_map<std::string, std::string>{});
	cmake_driver.build(build_path, boost::thread::hardware_concurrency());
	Si::absolute_path const built_exe = build_path
#ifdef _WIN32
		/ "Debug"
#endif
		/ "test1"
#ifdef _WIN32
		".exe"
#endif
		;
	Si::process_parameters parameters;
	parameters.executable = built_exe.to_boost_path();
	parameters.current_path = build_path.to_boost_path();
	std::string output;
	auto stdout_ = Si::virtualize_sink(Si::make_container_sink(output));
	parameters.out = &stdout_;
	parameters.err = &stdout_;
	BOOST_CHECK_EQUAL(0, Si::run_process(parameters));
	auto expected_output = "It works!"
#ifdef _WIN32
		"\r"
#endif
		"\n";
	BOOST_CHECK_EQUAL(expected_output, output);
}
