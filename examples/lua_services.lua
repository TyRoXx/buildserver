return function (require)
	local steps = require("steps", "1.0")
	local trigger = require("trigger", "1.0")
	local git = require("git", "1.0")
	local history = require("history", "1.0")

	-- There is a commit hook that has to be configured in detail by the
	-- administrator.
	local change_triggered = steps.describe(
		-- The description can be changed at any time.
		"a push happened on the repository",
		-- This is the unique identifier (relative to this script) of
		-- the trigger or step.
		-- You can change name and even the type of the trigger or
		-- step and still keep its history if you keep the ID.
		-- You can also change relationships between steps at any time.
		"100",
		trigger.private_web_trigger(
			-- Optional ID of the web trigger. The default would be the
			-- unique identifier. This ID has to be unique among all the
			-- web triggers in this script. 
			"repository-pushed"
		)
	);

	-- Trigger the build one hour after the last commit hook trigger
	-- because that is better than nothing when the hook breaks somehow.
	local regular_check = steps.describe(
		"when there is no push for an hour",
		"200",
		trigger.timeout(
			change_triggered,
			3600,
			function() return true end
		)
	);

	-- Returns the latest commit message on the master branch.
	-- Does not take any input other than data from github.com.
	local top_message = steps.describe(
		"download the latest commit message",
		"300",
		steps.map(function ()
			return git.get_commit_message({"https://github.com/TyRoXx/buildserver.git", "master"})
		end)
	);

	-- Remembers the last message that was downloaded and discards any
	-- messages that are equal to the previous one.
	-- You may want to do that for example when you poll remote state.
	local changed_messages = steps.describe(
		"the latest commit message changed",
		"400",
		history.persistent_filter(function (current_message, last_message)
			return (last_message ~= current_message), current_message
		end)
	);

	return steps.sequence {
		-- the first element can be either a trigger or an existing step
		regular_check,
		-- the following elements have to be steps
		top_message,
		-- the output of the last step will be the output of the sequence
		changed_messages
	};
end
