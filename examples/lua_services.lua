return function (require)
	local steps = require("steps", "1.0")
	local trigger = require("trigger", "1.0")
	local git = require("git", "1.0")
	local history = require("history", "1.0")
	local change_triggered = trigger.private_web_trigger("a push happened on the repository")
	local regular_check = trigger.timeout(change_triggered, 3600, function()
		return true
	end)
	local top_message = steps.custom("download the latest commit message", function (input)
		return git.get_commit_message({"https://github.com/TyRoXx/buildserver.git", "master"})
	end)
	local last_message = nil
	local changed_messages = steps.filter("the latest commit message changed", function (current_message)
		if last_message == current_message then
			return false
		end
		last_message = current_message
		return true
	end)
	local messages = regular_check:into(top_message:into(changed_messages))
	local message_archive = history.sequence("the resulting commit messages"):get_push_step()
	return messages:into(message_archive)
end
