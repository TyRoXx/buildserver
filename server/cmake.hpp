#ifndef BUILDSERVER_CMAKE_HPP
#define BUILDSERVER_CMAKE_HPP

#include <boost/unordered_map.hpp>
#include <ventura/absolute_path.hpp>
#include <silicium/sink/sink.hpp>
#include <silicium/success.hpp>

namespace buildserver
{
	struct cmake
	{
		virtual ~cmake();
		virtual boost::system::error_code
		generate(ventura::absolute_path const &source, ventura::absolute_path const &build,
		         boost::unordered_map<Si::os_string, Si::os_string> const &definitions,
		         Si::Sink<char, Si::success>::interface &output) const = 0;
		virtual boost::system::error_code build(ventura::absolute_path const &build, unsigned cpu_parallelism,
		                                        Si::Sink<char, Si::success>::interface &output) const = 0;
	};

	struct cmake_exe : cmake
	{
		explicit cmake_exe(ventura::absolute_path exe);
		virtual boost::system::error_code
		generate(ventura::absolute_path const &source, ventura::absolute_path const &build,
		         boost::unordered_map<Si::os_string, Si::os_string> const &definitions,
		         Si::Sink<char, Si::success>::interface &output) const SILICIUM_OVERRIDE;
		virtual boost::system::error_code build(ventura::absolute_path const &build, unsigned cpu_parallelism,
		                                        Si::Sink<char, Si::success>::interface &output) const SILICIUM_OVERRIDE;

	private:
		ventura::absolute_path m_exe;
	};
}

#endif
