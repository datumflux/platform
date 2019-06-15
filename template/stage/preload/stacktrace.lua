stage.load("stacktrace", function (T)
	print("STACKTRACE", T)
	debug.traceback = T.stacktrace
end)
