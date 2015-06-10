#include "find_executable.hpp"
#include <boost/filesystem/operations.hpp>

namespace buildserver
{
	Si::error_or<Si::optional<Si::absolute_path>> find_file_in_directories(
		Si::path_segment const &filename,
		std::vector<Si::absolute_path> const &directories)
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
		return Si::none;
	}

#ifndef _WIN32
	Si::error_or<Si::optional<Si::absolute_path>> find_executable_unix(
		Si::path_segment const &filename,
		std::vector<Si::absolute_path> additional_directories)
	{
		additional_directories.emplace_back(*Si::absolute_path::create("/bin"));
		additional_directories.emplace_back(*Si::absolute_path::create("/usr/bin"));
		additional_directories.emplace_back(*Si::absolute_path::create("/usr/local/bin"));
		return find_file_in_directories(filename, additional_directories);
	}
#endif
}
