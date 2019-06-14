print("Hello STAGE:Platform")

stage.waitfor("callback", function (v)
	print("CALLBACK", process.tick, "--", v)
end)

stage.addtask(1000, function ()
	print("TIMER", process.tick)
	stage.signal("callback", "HELLO CALLBACK: " .. process.tick)
end)

_("GLOBAL", function (GLOBAL)
	print("GLOBAL", GLOBAL)
	if GLOBAL == nil then
		GLOBAL = os.time()
	end
	return GLOBAL
end)

print("---GLOBAL", GLOBAL)

stage.submit(0, function (v)
	print("THREAD---GLOBAL", GLOBAL)
	print("THREAD", process.tick)
end)

local port = broker.ready("tcp://0.0.0.0:8081?text=2k,4k", function (socket, addr)
	print("ACCEPT", socket.id, broker.ntoa(addr))
	return function (socket, data)
		print("RECEIVE", data)

		stage.signal("callback", data)
	end
end)

port.expire = 10 * 1000
port.close = function (socket)
	print("CLOSE", socket.id)
end

