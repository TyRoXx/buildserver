#ifndef BUILDSERVER_CMAKE_HPP
#define BUILDSERVER_CMAKE_HPP

#include <boost/unordered_map.hpp>
#include <silicium/absolute_path.hpp>

namespace buildserver
{
	struct cmake
	{
		virtual ~cmake();
		virtual boost::system::error_code generate(
			Si::absolute_path const &source,
			Si::absolute_path const &build,
			boost::unordered_map<std::string, std::string> const &definitions
		) const = 0;
		virtual boost::system::error_code build(
			Si::absolute_path const &build,
			unsigned cpu_parallelism
		) const = 0;
	};

	struct cmake_exe : cmake
	{
		explicit cmake_exe(Si::absolute_path exe);
		virtual boost::system::error_code generate(
			Si::absolute_path const &source,
			Si::absolute_path const &build,
			boost::unordered_map<std::string, std::string> const &definitions
		) const SILICIUM_OVERRIDE;
		virtual boost::system::error_code build(
			Si::absolute_path const &build,
			unsigned cpu_parallelism
		) const SILICIUM_OVERRIDE;

	private:

		Si::absolute_path m_exe;
	};
}

#endif
