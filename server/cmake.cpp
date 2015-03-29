#include "cmake.hpp"
#include <silicium/run_process.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/sink/virtualized_sink.hpp>
#include <boost/lexical_cast.hpp>

namespace buildserver
{
	cmake::~cmake()
	{
	}


	cmake_exe::cmake_exe(
		Si::absolute_path exe)
		: m_exe(std::move(exe))
	{
	}

	boost::system::error_code cmake_exe::generate(
		Si::absolute_path const &source,
		Si::absolute_path const &build,
		boost::unordered_map<std::string, std::string> const &definitions,
		Si::sink<char, Si::success> &output
	) const
	{
		std::vector<std::string> arguments;
		arguments.emplace_back(source.to_boost_path().string().c_str());
		for (auto const &definition : definitions)
		{
			//TODO: is this properly encoded in all cases? I guess not
			auto encoded = "-D" + definition.first + "=" + definition.second;
			arguments.emplace_back(std::move(encoded));
		}
		Si::process_parameters parameters;
		parameters.executable = m_exe.to_boost_path();
		parameters.current_path = build.to_boost_path();
		parameters.arguments = std::move(arguments);
		parameters.out = &output;
		parameters.err = &output;
		int const rc = Si::run_process(parameters);
		if (rc != 0)
		{
			throw std::runtime_error("Unexpected CMake return code");
		}
		return {};
	}

	boost::system::error_code cmake_exe::build(
		Si::absolute_path const &build,
		unsigned cpu_parallelism,
		Si::sink<char, Si::success> &output
	) const
	{
		std::vector<std::string> arguments{"--build", "."
#ifndef _WIN32
			//assuming make..
			, "--", "-j", boost::lexical_cast<std::string>(cpu_parallelism)
#endif
		};
#ifdef _WIN32
		boost::ignore_unused_variable_warning(cpu_parallelism);
#endif
		Si::process_parameters parameters;
		parameters.executable = m_exe.to_boost_path();
		parameters.current_path = build.to_boost_path();
		parameters.arguments = std::move(arguments);
		parameters.out = &output;
		parameters.err = &output;
		int const rc = Si::run_process(parameters);
		if (rc != 0)
		{
			throw std::runtime_error("Unexpected CMake return code");
		}
		return {};
	}
}
