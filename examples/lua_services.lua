return function (require)
	local steps = require("steps", "1.0")
	local trigger = require("trigger", "1.0")
	local git = require("git", "1.0")
	local history = require("history", "1.0")

	-- There is a commit hook that has to be configured in detail by the
	-- administrator.
	-- The name is description and primary identifier for the administrator.
	local change_triggered = trigger.private_web_trigger("a push happened on the repository")

	-- Trigger the build one hour after the last commit hook trigger
	-- because that is better than nothing when the hook breaks somehow.
	local regular_check = trigger.timeout(change_triggered, 3600, function()
		return true
	end)

	-- Returns the latest commit message on the master branch.
	-- Does not take any input other than data from github.com.
	local top_message = steps.map("download the latest commit message", function ()
		return git.get_commit_message({"https://github.com/TyRoXx/buildserver.git", "master"})
	end)

	-- Remembers the last message that was downloaded and discards any
	-- messages that are equal to the previous one.
	-- You may want to do that for example when you poll remote state.
	local changed_messages = history.persistent_filter("the latest commit message changed", function (current_message, last_message)
		return (last_message ~= current_message), current_message
	end)

	return history.maximum(
		steps.sequential {
			-- the first element can be either a trigger or an existing step
			regular_check,
			top_message,
			changed_messages
		}
	);
end
