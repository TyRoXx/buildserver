local ci = require_host("1.0")
local find_executable = ci:require("find_executable", "1.0")

return ci:implement_package(
	"find-cmake", "1.0",
	"find-cmake-unix", "1.0",
	function (version)
		local cmake = find_executable:find({
			filename = "cmake"
		})
		return cmake
	end
)
