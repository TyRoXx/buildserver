#ifndef BUILDSERVER_FIND_GIT_HPP
#define BUILDSERVER_FIND_GIT_HPP

#include <silicium/error_or.hpp>
#include <ventura/absolute_path.hpp>

namespace buildserver
{
	Si::error_or<Si::optional<ventura::absolute_path>> find_git();
}

#endif
