#include "find_git.hpp"
#include "find_executable.hpp"

namespace buildserver
{
	Si::error_or<Si::optional<ventura::absolute_path>> find_git()
	{
#ifdef _WIN32
		return buildserver::find_file_in_directories(
			*ventura::path_segment::create("git.exe"),
			{*ventura::absolute_path::create("C:\\Program Files (x86)\\Git\\bin")}
		);
#else
		return buildserver::find_executable_unix(*ventura::path_segment::create("git"), {});
#endif
	}
}
