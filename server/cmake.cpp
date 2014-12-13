#include "cmake.hpp"
#include <silicium/process.hpp>
#include <silicium/sink/iterator_sink.hpp>
#include <silicium/sink/virtualized_sink.hpp>
#include <boost/lexical_cast.hpp>

namespace buildserver
{
	cmake::~cmake()
	{
	}


	cmake_exe::cmake_exe(
		boost::filesystem::path exe)
		: m_exe(std::move(exe))
	{
	}

	boost::system::error_code cmake_exe::generate(
		boost::filesystem::path const &source,
		boost::filesystem::path const &build,
		boost::unordered_map<std::string, std::string> const &definitions
	) const
	{
		std::vector<std::string> arguments;
		arguments.emplace_back(source.string());
		for (auto const &definition : definitions)
		{
			//TODO: is this properly encoded in all cases? I guess not
			auto encoded = "-D" + definition.first + "=" + definition.second;
			arguments.emplace_back(std::move(encoded));
		}
		Si::process_parameters parameters;
		parameters.executable = m_exe;
		parameters.current_path = build;
		parameters.arguments = std::move(arguments);
		auto output = Si::virtualize_sink(Si::null_sink<char, Si::success>());
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
		boost::filesystem::path const &build,
		unsigned cpu_parallelism
	) const
	{
		std::vector<std::string> arguments{"--build", "."
#ifndef _WIN32
			//assuming make..
			, "--", "-j", boost::lexical_cast<std::string>(cpu_parallelism)
#endif
		};
		Si::process_parameters parameters;
		parameters.executable = m_exe;
		parameters.current_path = build;
		parameters.arguments = std::move(arguments);
		std::string output;
		auto output_sink = Si::virtualize_sink(Si::make_container_sink(output));
		parameters.out = &output_sink;
		parameters.err = &output_sink;
		int const rc = Si::run_process(parameters);
		if (rc != 0)
		{
			throw std::runtime_error("Unexpected CMake return code");
		}
		return {};
	}
}
