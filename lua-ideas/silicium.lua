local ci = require_host("1.0")
local build = ci:require("git-cmake-c++")

return ci:create_package(
	"silicium",
	function ()
		return function ()
			build({
				repository = "https://github.com/TyRoXx/silicium.git",
				commit = "master"
			})
		end
	end
)
