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

	Si::error_or<Si::optional<Si::absolute_path>> find_cmake()
	{
#ifdef _WIN32
		wchar_t *programs = nullptr;
		if (S_OK != SHGetKnownFolderPath(FOLDERID_ProgramFilesX86, 0, NULL, &programs))
		{
			throw std::logic_error("COM error handling to do");
		}
		std::unique_ptr<wchar_t, com_deleter> free_programs(programs);
		Si::optional<Si::absolute_path> const programs_path = Si::absolute_path::create(programs);
		if (!programs_path)
		{
			return Si::none;
		}
		return find_file_in_directories(
			*Si::path_segment::create("cmake.exe"), {
			*programs_path / Si::relative_path("CMake/bin"),
			*programs_path / Si::relative_path("CMake 2.8/bin")
		});
#else
		return find_executable_unix(*Si::path_segment::create("cmake"), {});
#endif
	}
}
