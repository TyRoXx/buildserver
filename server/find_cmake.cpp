#include "find_cmake.hpp"
#include "find_executable.hpp"

namespace buildserver
{
	Si::error_or<boost::optional<boost::filesystem::path>> find_cmake_unix()
	{
		return find_executable_unix("cmake", {});
	}
}
