local signal = function()
	local subscribers = {}
	local notify_all = function(...)
		local current_subscribers = subscribers
		subscribers = {}
		for _,subscriber in ipairs(current_subscribers) do
			subscriber(unpack(arg))
		end
	end
	return {
		subscribe = function(subscriber)
			table.insert(subscribers, subscriber)
		end
	}, notify_all
end

local subscribe_forever = function(signal, handler)
	local renew = nil
	local subscriber = function(...)
		renew()
		handler(unpack(arg))
	end)
	renew = function()
		signal:subscribe(subscriber)
	end
	renew()
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
	subscribe_forever(input, function(...)
		if timer then
			timer:stop()
		end
		set_timer()
		notify_all(unpack(arg))
	end)
	return step
end

local pipe = function(step_array)
	local step, notify_all = signal()
	step.kind = "pipe"
	local previous_step = nil
	for _,step in ipairs(step_array) do
		if previous_step ~= nil then
			subscribe_forever(
				previous_step, 
				function(...)
					step:run(unpack(arg))
				end
			)
		end
		previous_step = step
	end
	subscribe_forever(
		previous_step,
		function(...)
			notify_all(unpack(arg))
		end
	)
	return step
end

local critical_section = function(head, tail)
	local step, notify_all = signal()
	step.kind = "critical_section"
	local enter = nil
	enter = function()
		head:subscribe(function(...)
			tail:subscribe(function(...)
				enter()
				notify_all(unpack(arg))
			end)
			tail:run(unpack(arg))
		end)
	end
	enter()
	return step
end

local spawn = function(step)
	subscribe_forever(step, function(...)
		-- ignore any output of the step
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

	spawn(
		pipe{
			-- the first element can be either a trigger or an existing step
			regular_check,
			critical_section(
				-- the following elements have to be steps
				top_message,
				-- the output of the last step will be the output of the sequence
				changed_messages
			)
		}
	)
end
