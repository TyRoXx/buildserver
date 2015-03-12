#ifndef BUILDSERVER_FIND_EXECUTABLE_HPP
#define BUILDSERVER_FIND_EXECUTABLE_HPP

#include <silicium/error_or.hpp>
#include <silicium/path_segment.hpp>
#include <vector>

namespace buildserver
{

	Si::error_or<Si::optional<Si::absolute_path>> find_file_in_directories(
		Si::path_segment const &filename,
		std::vector<Si::absolute_path> const &directories);

#ifndef _WIN32
	Si::error_or<Si::optional<Si::absolute_path>> find_executable_unix(
		Si::path_segment const &filename,
		std::vector<Si::absolute_path> additional_directories);
#endif
}

#endif
