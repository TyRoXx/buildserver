#ifndef BUILDSERVER_FIND_GCC_HPP
#define BUILDSERVER_FIND_GCC_HPP

#include <boost/filesystem/path.hpp>
#include <silicium/error_or.hpp>

namespace buildserver
{
	struct gcc_location
	{
		boost::filesystem::path gcc, gxx;
	};

	Si::error_or<boost::optional<gcc_location>> find_gcc_unix();
}

#endif
