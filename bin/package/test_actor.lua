--[[
function stage.actor(ask_id, callback, timeout)
	local proxy = { ask_id, callback, timeout and timeout or 100}

	proxy[ask_id] = function (proxy, arg, ...)
		-- print("CALL", proxy[2])
		local id = stage.waitfor(function (f, arg, ...)
			-- print("WAITFOR", f, arg)
			if _WAITFOR ~= nil then
				local id = _WAITFOR[-1]

				stage.waitfor(id, nil)
				stage.call(f, arg, ...)
			else
				-- cancel
				stage.call(f, arg)
			end
		end, proxy[2], arg)

		if proxy[3] > 0 then
			stage.addtask(proxy[3], function (id)
				local f, arg = stage.waitfor(id)
				if f ~= nil then
					stage.waitfor(id, nil)
					-- print("CANCEL", id, f, arg) print_r(arg)
					stage.call(f, arg)
				end
				return -1;
			end, id)
		end
		return stage.signal(proxy[1] .. "=" .. id, ...)
	end
	return stage.proxy(proxy, { "actor" })
end ]]

local v = stage.actor("callback", function (r)
	print("RETURN", self, r and r or "<cancel>")
end)

stage.waitfor("callback", function (r)
	print("CALLBACK",  r)
	return "HELLO"
end)

print(v)
v:callback({}, "HI")
