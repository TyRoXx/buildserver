local ci = require_host("1.0")
local git_executable = ci:find("find-git", "1.0")
local execute = ci:find("exec", "1.0")
local logger = ci:require("logger", "1.0")

return ci:implement_package(
	"git", "1.0", --interface
	"git-exe", "1.0", --implementation
	function (version)
		local git = {}
		function git:clone(from, to, commit)
			if type(from) == "table" then
				local arguments = from
				from = arguments.from
				to = arguments.to
				commit = arguments.commit
			end
			logger("cloning")
			execute:run(git_executable, nil, {
				from,
				to
			})
			logger("checking the interesting revision out")
			execute:run(git_executable, to, {
				"checkout",
				commit
			})
		end
		return git
	end
)
