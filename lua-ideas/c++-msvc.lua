local ci = require_host("1.0")
local find_executable = ci:require("find-executable", "1.0")
local win32 = ci:require("win32", "1.0")

return ci:implement_package(
	"c++", "1.0",
	"c++-msvc", "1.0",
	function (version)
		local cl = find_executable:find({
			filename = "cl",
			hints = {
				win32:path("{PROGRAMS}/Microsoft Visual Studio 12.0/VC/bin")
			}
		})
		return {
			c = cl,
			cxx = cl
		}
	end
}
