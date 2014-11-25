#include "find_cmake.hpp"
#include "find_executable.hpp"
#ifdef _WIN32
#include <Shlobj.h>
#endif

namespace buildserver
{
#ifdef _WIN32
	namespace
	{
		struct com_deleter
		{
			void operator()(void *mem) const
			{
				CoTaskMemFree(mem);
			}
		};
	}
#endif

	Si::error_or<boost::optional<boost::filesystem::path>> find_cmake()
	{
#ifdef _WIN32
		wchar_t *programs = nullptr;
		if (S_OK != SHGetKnownFolderPath(FOLDERID_ProgramFilesX86, 0, NULL, &programs))
		{
			throw std::logic_error("COM error handling to do");
		}
		std::unique_ptr<wchar_t, com_deleter> free_programs(programs);
		boost::filesystem::path const programs_path = programs;
		return find_file_in_directories("cmake.exe", { programs_path / "CMake/bin" });
#else
		return find_executable_unix("cmake", {});
#endif
	}
}
