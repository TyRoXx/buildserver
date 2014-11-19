local ci = require_host("1.0")
local find_executable = ci:require("find_executable", "1.0")

return ci:implement_package(
	"find-gcc", "1.0",
	"find-gcc-unix", "1.0",
	function (version)
		local gcc = find_executable:find({
			filename = "gcc"
		})
		local gxx = find_executable:find({
			filename = "g++",
			root = ci:parent_path(gcc)
		})
		return {
			gcc = gcc,
			gxx = gxx
		}
	end
)
