local signal = function()
	local subscribers = {}
	local notify_all = function(...)
		for _,subscriber in ipairs(subscribers) do
			subscriber(unpack(arg))
		end
	end
	return {
		subscribe = function(subscriber)
			table.insert(subscribers, subscriber)
		end
	}, notify_all
end

local describe = function(description, identifier, step)
	return {
		description = description,
		identifier = identifier,
		kind = "description",
		subscribe = function(...)
			return step:subscribe(unpack(arg))
		end
	}
end

local map = function(start)
	local step, notify_all = signal()
	step.kind = "map"
	step.run = function(...)
		notify_all(start(unpack(arg)))
	end
	return step
end

local timeout = function(input, duration, make_timeout_value)
	local step, notify_all = signal()
	step.kind = "timeout"
	local timer = nil
	local set_timer = nil
	set_timer = function()
		timer = create_timer(duration)
		timer:async_get_one(function()
			notify_all(make_timeout_value())
			set_timer()
		end)
	end
	input:subscribe(function(...)
		if timer then
			timer:stop()
		end
		notify_all(unpack(arg))
		set_timer()
	end)
	return step
end

local sequence = function(step_array)
	local step, notify_all = signal()
	step.kind = "sequence"
	local previous_step = nil
	for _,step in ipairs(step_array) do
		if previous_step ~= nil then
			previous_step:subscribe(function(...)
				step:run(unpack(arg))
			end)
		end
		previous_step = step
	end
	previous_step:subscribe(function(...)
		notify_all(unpack(arg))
	end)
	return step
end

local spawn = function(step)
	step:subscribe(function()
		-- ignore any output
	end)
end

return function (require)
	local trigger = require("trigger", "1.0")
	local git = require("git", "1.0")
	local history = require("history", "1.0")

	-- There is a commit hook that has to be configured in detail by the
	-- administrator.
	local change_triggered = describe(
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
	)

	-- Trigger the build one hour after the last commit hook trigger
	-- because that is better than nothing when the hook breaks somehow.
	local regular_check = describe(
		"when there is no push for an hour",
		"200",
		timeout(
			change_triggered,
			3600,
			function() return true end
		)
	)

	-- Returns the latest commit message on the master branch.
	-- Does not take any input other than data from github.com.
	local top_message = describe(
		"download the latest commit message",
		"300",
		map(function ()
			return git.get_commit_message({"https://github.com/TyRoXx/buildserver.git", "master"})
		end)
	)

	-- Remembers the last message that was downloaded and discards any
	-- messages that are equal to the previous one.
	-- You may want to do that for example when you poll remote state.
	local changed_messages = describe(
		"the latest commit message changed",
		"400",
		history.persistent_filter(function (current_message, last_message)
			return (last_message ~= current_message), current_message
		end)
	)

	spawn(sequence {
		-- the first element can be either a trigger or an existing step
		regular_check,
		-- the following elements have to be steps
		top_message,
		-- the output of the last step will be the output of the sequence
		changed_messages
	})
end
