local ci = require_host("1.0")
local git = ci:require("git", "1.0")
local cmake = ci:require("cmake", "1.0")
local cxx = ci:require("c++", "1.0")
local workspace = ci:require("workspace", "1.0", {min_available = ci:gibibytes(5)})
local logger = ci:require("logger", "1.0")
local cpu = ci:require("cpu", "1.0")

return ci:create_package(
	"git-cmake-c++",
	function ()
		return function (options)
			local repository = options:repository()
			local commit = options:commit()
			local src_dir = workspace:create_directory("src")
			logger("cloning ", repository, " ", commit)
			git:clone({
				from = repository,
				to = src_dir,
				commit = commit
			})
			logger("cloned")
			logger("running CMake")
			local build_dir = workspace:create_directory("build")
			cmake:generate({
				source = src_dir,
				build = build_dir,
				definitions =
				{
					CMAKE_CXX_COMPILER = cxx:executable(),
					CMAKE_C_COMPILER = cxx:c_executable()
				}
			})
			logger("building with CMake")
			cmake:build({
				build = build_dir,
				cpu_parallelism = cpu:hardware_parallelism()
			})
		end
	end
)
