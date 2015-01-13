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
	local top_message = steps.custom("download the latest commit message", function ()
		return git.get_commit_message({"https://github.com/TyRoXx/buildserver.git", "master"})
	end)

	-- Remembers the last message that was downloaded and discards any
	-- messages that are equal to the previous one.
	-- You may want to do that for example when you poll remote state.
	local last_message = nil
	local changed_messages = steps.filter("the latest commit message changed", function (current_message)
		if last_message == current_message then
			return false
		end
		last_message = current_message
		return true
	end)

	-- A persistent sequence of values that we can append our results to.
	-- The administrator will have to configure some backing storage.
	-- Here we are not interested in any details.
	-- The name is a description and the primary identifier - relative
	-- to this build job file - of the history.
	-- A job can have multiple histories of course.
	local message_archive = history.sequence("the resulting commit messages"):get_push_step()

	-- `combined` builds a pipe from triggers and steps. The output of
	-- the previous one will be used as input for the next one.
	-- The pipe is also a step.
	return steps.combined {
		-- the first element can be either a trigger or an existing step
		regular_check,
		-- Pipes will try to parallelize work by default, but sometimes
		-- that is not what we want. In this case we want to archive the
		-- messages in chronological order. The order is maintained by
		-- making sure that a message has been fully stored before
		-- attempting to retrieve the next one.
		steps.sequential {
			top_message,
			changed_messages,
			message_archive
		}
	};
end
