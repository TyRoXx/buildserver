#include "find_gcc.hpp"
#include "find_executable.hpp"

namespace buildserver
{
	Si::error_or<boost::optional<gcc_location>> find_gcc_unix()
	{
		auto gcc = find_executable_unix("gcc", {});
		if (gcc.is_error())
		{
			return gcc.error();
		}
		if (!gcc.get())
		{
			return boost::none;
		}
		auto gxx = find_file_in_directories("g++", {gcc.get()->parent_path()});
		return Si::map(std::move(gxx), [&gcc](boost::optional<boost::filesystem::path> gxx_path) -> boost::optional<gcc_location>
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
				return boost::none;
			}
		});
	}
}
