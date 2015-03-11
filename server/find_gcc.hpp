#ifndef BUILDSERVER_FIND_GCC_HPP
#define BUILDSERVER_FIND_GCC_HPP

#include <silicium/absolute_path.hpp>
#include <silicium/error_or.hpp>

namespace buildserver
{
	struct gcc_location
	{
		Si::absolute_path gcc, gxx;
	};

	Si::error_or<Si::optional<gcc_location>> find_gcc_unix();
}

#endif
