#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include "server/cmake.hpp"
#include "server/find_cmake.hpp"
#include "server/find_executable.hpp"
#include <silicium/error_or.hpp>
#include <ventura/run_process.hpp>
#include <silicium/sink/virtualized_sink.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/unordered_map.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/thread.hpp>

BOOST_AUTO_TEST_CASE(cmake_exe_test)
{
	auto cmake = buildserver::find_cmake().get();
	BOOST_REQUIRE(cmake);
	buildserver::cmake_exe const cmake_driver(*cmake);
	ventura::absolute_path const build_path =
	    *ventura::absolute_path::create(boost::filesystem::temp_directory_path() / "buildtest123456");
	boost::filesystem::remove_all(build_path.to_boost_path());
	boost::filesystem::create_directories(build_path.to_boost_path());
	ventura::absolute_path const resources_path = *ventura::absolute_path::create(
	    boost::filesystem::path(__FILE__).parent_path().parent_path() / "test-resources");
	Si::virtualized_sink<Si::null_sink<char, Si::success>> ignored;
	cmake_driver.generate(resources_path / "test1", build_path, boost::unordered_map<Si::os_string, Si::os_string>{},
	                      ignored);
	cmake_driver.build(build_path, boost::thread::hardware_concurrency(), ignored);
	ventura::absolute_path const built_exe = build_path
#ifdef _WIN32
	                                         / "Debug"
#endif
	                                         / "test1"
#ifdef _WIN32
	                                           ".exe"
#endif
	    ;
	ventura::process_parameters parameters;
	parameters.executable = built_exe;
	parameters.current_path = build_path;
	std::string output;
	auto stdout_ = Si::virtualize_sink(Si::make_container_sink(output));
	parameters.out = &stdout_;
	parameters.err = &stdout_;
	BOOST_CHECK_EQUAL(0, ventura::run_process(parameters));
	auto expected_output = "It works!"
#ifdef _WIN32
	                       "\r"
#endif
	                       "\n";
	BOOST_CHECK_EQUAL(expected_output, output);
}
