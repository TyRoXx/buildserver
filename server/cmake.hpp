#ifndef BUILDSERVER_CMAKE_HPP
#define BUILDSERVER_CMAKE_HPP

#include <boost/unordered_map.hpp>
#include <boost/filesystem/path.hpp>
#include <silicium/override.hpp>

namespace buildserver
{
	struct cmake
	{
		virtual ~cmake();
		virtual boost::system::error_code generate(
			boost::filesystem::path const &source,
			boost::filesystem::path const &build,
			boost::unordered_map<std::string, std::string> const &definitions
		) const = 0;
		virtual boost::system::error_code build(
			boost::filesystem::path const &build,
			unsigned cpu_parallelism
		) const = 0;
	};

	struct cmake_exe : cmake
	{
		explicit cmake_exe(boost::filesystem::path exe);
		virtual boost::system::error_code generate(
			boost::filesystem::path const &source,
			boost::filesystem::path const &build,
			boost::unordered_map<std::string, std::string> const &definitions
		) const SILICIUM_OVERRIDE;
		virtual boost::system::error_code build(
			boost::filesystem::path const &build,
			unsigned cpu_parallelism
		) const SILICIUM_OVERRIDE;

	private:

		boost::filesystem::path m_exe;
	};
}

#endif
