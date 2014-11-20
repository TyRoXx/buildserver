#ifndef BUILDSERVER_FIND_EXECUTABLE_HPP
#define BUILDSERVER_FIND_EXECUTABLE_HPP

#include <silicium/error_or.hpp>
#include <boost/filesystem/path.hpp>
#include <vector>

namespace buildserver
{

	Si::error_or<boost::optional<boost::filesystem::path>> find_file_in_directories(
		boost::filesystem::path const &filename,
		std::vector<boost::filesystem::path> const &directories);

	Si::error_or<boost::optional<boost::filesystem::path>> find_executable_unix(
		boost::filesystem::path const &filename,
		std::vector<boost::filesystem::path> additional_directories);
}

#endif
