local broker = require('broker')

local port = broker.ready("tcp://[::]:8081?text=2k,4k", function (socket, addr)
	local broker = require('broker')

	print("ACCEPT", broker.ntoa(addr))
	return true
end)

port.expire = 10000
port.close = function (socket)
	print("CLOSE", socket.id)
end

stage.waitfor("callback", function (v)
	print("CALLBACK", v)
end)

port.receive = function (socket, data)
	print("RECEIVE", data)
	stage.signal("callback", data)
	return "HELLO\n"
end
print("BROKER SAMPLE")

local bundle = require('bundle')

local b = bundle.bson({
	name = "HELLO"
})
print("decode:", #b)
print("DECODE", bundle.bson(b).name)

--[[ stage.addtask(1000, function (a)
	print("TICK", a, process.tick)
end, "START") ]]
