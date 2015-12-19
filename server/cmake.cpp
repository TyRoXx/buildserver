#include "cmake.hpp"
#include <ventura/run_process.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/sink/virtualized_sink.hpp>
#include <boost/lexical_cast.hpp>

namespace buildserver
{
	cmake::~cmake()
	{
	}

	cmake_exe::cmake_exe(ventura::absolute_path exe)
	    : m_exe(std::move(exe))
	{
	}

	boost::system::error_code cmake_exe::generate(ventura::absolute_path const &source,
	                                              ventura::absolute_path const &build,
	                                              boost::unordered_map<Si::os_string, Si::os_string> const &definitions,
	                                              Si::Sink<char, Si::success>::interface &output) const
	{
		std::vector<Si::os_string> arguments;
		arguments.emplace_back(to_os_string(source));
		for (auto const &definition : definitions)
		{
			// TODO: is this properly encoded in all cases? I guess not
			Si::os_string encoded = SILICIUM_OS_STR("-D") + definition.first + SILICIUM_OS_STR("=") + definition.second;
			arguments.emplace_back(std::move(encoded));
		}
		ventura::process_parameters parameters;
		parameters.executable = m_exe;
		parameters.current_path = build;
		parameters.arguments = std::move(arguments);
		parameters.out = &output;
		parameters.err = &output;
		int const rc = ventura::run_process(parameters).get();
		if (rc != 0)
		{
			throw std::runtime_error("Unexpected CMake return code");
		}
		return {};
	}

	boost::system::error_code cmake_exe::build(ventura::absolute_path const &build, unsigned cpu_parallelism,
	                                           Si::Sink<char, Si::success>::interface &output) const
	{
		std::vector<Si::os_string> arguments{SILICIUM_OS_STR("--build"), SILICIUM_OS_STR(".")
#ifndef _WIN32
		                                     // assuming make..
		                                     ,
		                                     "--", "-j", boost::lexical_cast<std::string>(cpu_parallelism)
#endif
		};
#ifdef _WIN32
		boost::ignore_unused_variable_warning(cpu_parallelism);
#endif
		ventura::process_parameters parameters;
		parameters.executable = m_exe;
		parameters.current_path = build;
		parameters.arguments = std::move(arguments);
		parameters.out = &output;
		parameters.err = &output;
		int const rc = ventura::run_process(parameters).get();
		if (rc != 0)
		{
			throw std::runtime_error("Unexpected CMake return code");
		}
		return {};
	}
}
