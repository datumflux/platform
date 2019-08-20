--[[
--{
--  "k": { return_id, value }
--}
]]--
return function (socket, data, addr)
	-- print("RECEIVE", broker.ntoa(socket[1]))
	-- print_r(data)
	if type(data) == "table" then
		local result = {}
		local notify = 0

		for k, v in pairs(data) do
			-- "test.curl+callback"
			if type(k) ~= "string" then
			elseif string.len(k) == 0 then
				local accept = _CLUSTER_ACCEPT[socket.id]

				if #accept == 0 then
					accept[#accept + 1] = v
					if _CLUSTER_STAGE[v] == nil then
						_CLUSTER_STAGE[v] = {}
					end
					_CLUSTER_STAGE[v][socket.id] = process.tick
				end
			elseif v[1] ~= nil then
				-- print("CALLBACK", k, socket, v[2])
				local r = stage.call(k, v[2])
				-- print("RESULT", r)
				if r == nil then
				elseif string.len(v[1]) > 0 then
					result[ v[1] ] = { nil, r }
					notify = notify + 1
				end
			else
				-- print("SIGNAL", k, v[2])
				stage.signal(k, v[2])
			end
		end
		-- print("NOTIFY", notify)
		if notify > 0 then
			-- print_r(result)
			return result
		end
	end
end
