#include "find_executable.hpp"
#include <boost/filesystem/operations.hpp>

namespace buildserver
{
	namespace
	{
		Si::error_or<bool> file_exists(Si::absolute_path const &candidate)
		{
			boost::system::error_code ec;
			boost::filesystem::file_status status = boost::filesystem::status(candidate.to_boost_path(), ec);
			if (status.type() == boost::filesystem::file_not_found)
			{
				return false;
			}
			if (ec)
			{
				return ec;
			}
			return true;
		}
	}

	Si::error_or<Si::optional<Si::absolute_path>> find_file_in_directories(
		Si::path_segment const &filename,
		std::vector<Si::absolute_path> const &directories)
	{
		for (auto const &directory : directories)
		{
			auto candidate = directory / filename;
			auto result = file_exists(candidate);
			if (result.is_error())
			{
				return result.error();
			}
			if (result.get())
			{
				return std::move(candidate);
			}
		}
		return Si::none;
	}

	Si::error_or<Si::optional<Si::absolute_path>> find_executable_unix(
		Si::path_segment const &filename,
		std::vector<Si::absolute_path> additional_directories)
	{
		additional_directories.emplace_back(*Si::absolute_path::create("/bin"));
		additional_directories.emplace_back(*Si::absolute_path::create("/usr/bin"));
		additional_directories.emplace_back(*Si::absolute_path::create("/usr/local/bin"));
		return find_file_in_directories(filename, additional_directories);
	}
}
