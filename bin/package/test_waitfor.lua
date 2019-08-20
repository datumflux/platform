thread.new(function ()
	return "HELLO"
end).waitfor(function (r)
	print("RESULT", r)
end)

stage.waitfor("callback", function (v)
	print("CALLBACK", v, _WAITFOR[-1])
end)
stage.signal("callback", "TEST")

local id = stage.waitfor(function (udata, v)
	print("CALLBACK", _WAITFOR and _WAITFOR[0] or "<none>", udata, v)
	if (_WAITFOR ~= nil) and (_WAITFOR[0] == _WAITFOR[-1]) then
		stage.waitfor(_WAITFOR[-1], nil)
	end
end, "USERDATA")

-- print("WAITFOR", id)
stage.addtask(1000, function (v)
	local f, args = stage.waitfor(v)
	print("CANCEL", v, f, args)
	if f ~= nil then
		stage.call(f, args, "CANCEL")
		stage.waitfor(v, nil)
	end
	return -1
end, id)

stage.signal(id, "HELLO")
--

function actor(ask_id, callback)
	local waitfor_id = stage.waitfor(function (f, ...)
		stage.call(f, ...)
		if _WAITFOR ~= nil then
			stage.waitfor(_WAITFOR[-1], nil)
		end
	end, callback)
	return stage.proxy({ ask_id, waitfor_id }, {
		timeout = function (self, edges, name, value)
			if type(value) == "number" then
			elseif type(value) == "table" then
			end
		end,

		ask = function (self, edges, name, value)
			local id = self[1] .. "=" .. self[2]
			stage.signal(self[1] .. "=" .. self[2], value)
		end
	})
end

local actor = actor "request" function (self, ...)
end

actor.timeout = { 10, "cancel" }
actor.ask "value"


