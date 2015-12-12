#ifndef BUILDSERVER_FIND_EXECUTABLE_HPP
#define BUILDSERVER_FIND_EXECUTABLE_HPP

#include <silicium/error_or.hpp>
#include <ventura/absolute_path.hpp>
#include <vector>

namespace buildserver
{

	Si::error_or<Si::optional<ventura::absolute_path>> find_file_in_directories(
		ventura::path_segment const &filename,
		std::vector<ventura::absolute_path> const &directories);

#ifndef _WIN32
	Si::error_or<Si::optional<ventura::absolute_path>> find_executable_unix(
		ventura::path_segment const &filename,
		std::vector<ventura::absolute_path> additional_directories);
#endif
}

#endif
