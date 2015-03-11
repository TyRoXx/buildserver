#ifndef BUILDSERVER_FIND_CMAKE_HPP
#define BUILDSERVER_FIND_CMAKE_HPP

#include <silicium/error_or.hpp>
#include <silicium/absolute_path.hpp>

namespace buildserver
{
	Si::error_or<Si::optional<Si::absolute_path>> find_cmake();
}

#endif
