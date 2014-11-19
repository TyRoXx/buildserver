local ci = require_host("1.0")
local cmake_exe = ci:require("find-cmake", "1.0")
local execute = ci:find("exec", "1.0")

return ci:implement_package(
	"cmake", "1.0",
	"cmake-exe", "1.0",
	function (version)
		local cmake = {}
		function cmake:generate(source, build, definitions)
			if type(source) == "table" then
				local args = source
				source = args.source
				build = args.build
				definitions = args.definitions
			end
			local cmdline_args = {
				source
			}
			for key,value in ipairs(definitions) do
				-- TODO: do we have to escape key or value?
				local definition = "-D" .. key .. "=" .. value
				table.insert(cmdline_args, definition)
			end
			execute:run(cmake_exe, build, cmdline_args)
		end
		function cmake:build(build)
			local cpu_parallelism = 1
			if type(build) == "table" then
				local args = build
				build = args.build
				cpu_parallelism = args.cpu_parallelism
			end
			execute:run(cmake_exe, build, {".", "--build", "--", "-j" .. tostring(cpu_parallelism)})
		end
		return cmake
	end
}
