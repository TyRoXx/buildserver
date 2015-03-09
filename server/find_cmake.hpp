#ifndef BUILDSERVER_FIND_CMAKE_HPP
#define BUILDSERVER_FIND_CMAKE_HPP

#include <silicium/error_or.hpp>
#include <boost/filesystem/path.hpp>

namespace buildserver
{
	Si::error_or<Si::optional<boost::filesystem::path>> find_cmake();
}

#endif
