build jobs in Lua
=================

```lua
return function (require)
	local steps = require("steps", "1.0")
	local every_minute = steps:clock_trigger("??:??:00 UTC")
	local hello_results = steps:custom(
		"Hello, world!",
		every_minute,
		function (triggers_output, execution)
			execution:info("Hello, world!")
			return "OK!"
		end
	)
	local ok_checker = steps:custom(
		"check for OK to be sure",
		hello_results,
		function (triggers_output, execution)
			if triggers_output ~= "OK!" then
				execution:info("The hello step succeeded, but it returned an unexpected result")
				return nil
			end
			return "looks OK"
		end
	)
	return ok_checker
end
```
