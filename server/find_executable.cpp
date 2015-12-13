#include "find_executable.hpp"
#include <ventura/file_operations.hpp>
#include <boost/filesystem/operations.hpp>

namespace buildserver
{
	Si::error_or<Si::optional<ventura::absolute_path>>
	find_file_in_directories(ventura::path_segment const &filename,
	                         std::vector<ventura::absolute_path> const &directories)
	{
		for (auto const &directory : directories)
		{
			auto candidate = directory / filename;
			auto result = ventura::file_exists(candidate);
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
	Si::error_or<Si::optional<ventura::absolute_path>>
	find_executable_unix(ventura::path_segment const &filename,
	                     std::vector<ventura::absolute_path> additional_directories)
	{
		additional_directories.emplace_back(*ventura::absolute_path::create("/bin"));
		additional_directories.emplace_back(*ventura::absolute_path::create("/usr/bin"));
		additional_directories.emplace_back(*ventura::absolute_path::create("/usr/local/bin"));
		return find_file_in_directories(filename, additional_directories);
	}
#endif
}
