#include "find_gcc.hpp"
#include "find_executable.hpp"

namespace buildserver
{
#ifndef _WIN32
	Si::error_or<Si::optional<gcc_location>> find_gcc_unix()
	{
		auto gcc = find_executable_unix(*Si::path_segment::create("gcc"), {});
		if (gcc.is_error())
		{
			return gcc.error();
		}
		if (!gcc.get())
		{
			return Si::none;
		}
		auto gcc_dir = parent(*gcc.get());
		assert(gcc_dir);
		auto gxx = find_file_in_directories(*Si::path_segment::create("g++"), {*gcc_dir});
		return Si::map(
			std::move(gxx),
			[&gcc](Si::optional<Si::absolute_path> gxx_path) -> Si::optional<gcc_location>
			{
				if (gxx_path)
				{
					gcc_location result;
					result.gcc = std::move(*gcc.get());
					result.gxx = std::move(*gxx_path);
					return result;
				}
				else
				{
					return Si::none;
				}
			}
		);
	}
#endif
}
