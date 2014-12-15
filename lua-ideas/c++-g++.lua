local ci = require_host("1.0")
local gcc = ci:require("find-gcc", "1.0")

return ci:implement_package(
	"c++", "1.0",
	"c++-g++", "1.0",
	function (version)
		return {
			c = gcc.gcc,
			cxx = gcc.gxx
		}
	end
)
