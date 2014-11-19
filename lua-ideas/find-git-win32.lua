local ci = require_host("1.0")
local find_executable = ci:find("find-executable", "1.0")
local logger = ci:require("logger", "1.0")
local win32 = ci:require("win32", "1.0")

return ci:implement_package(
	"find-git", "1.0", --interface
	"find-git-win32", "1.0", --implementation
	function (version)
		local executable = find_executable:find({
			filename = "git",
			hints = {
				win32:path("{PROGRAMS}/Git/bin")
			}
		})
		return executable
	end
)
