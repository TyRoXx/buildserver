return function(require)
	local buildserver = require("buildserver", "1.0")

	-- "observable" step
	local pushed = buildserver:private_web_trigger("repository-pushed")

	-- "observable" step
	local pushed_or_timed_out = buildserver:timeout(pushed, 3600)

	local the_what = buildserver:exclusive_order{
		buildserver:map("get the current commit message", function()
			local message = git.get_commit_message({"https://github.com/TyRoXx/buildserver.git", "master"})
			return message
		end),
		buildserver:stateful_filter("filter redundant messages", function(message, previous_message)
			return (message ~= previous_message), message
		end),
		append_to_sink
	}

	buildserver:connect_trigger_and_steps(pushed_or_timed_out, the_what)
end
