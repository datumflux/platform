stage.waitfor("get_callback", function (result)
	print("GET CALLBACK", result)
	print_r(result)
end)

--[[
stage.addtask(100, function ()
	local v = 1234 -- math.random(1, 1000000)

	-- print("INDEX", v)
	stage.signal("index:get_callback=g?index", string.format("KR%d", v) );
end)
]]--


stage.waitfor("set_callback", function (result)
	print("SET CALLBACK", result)
	print_r(result)
end)

_SANDBOX[""] = { "count" }

count = 0
stage.addtask(100, function ()
	local value = {}

	for i=1, 500 do
		local v = math.random(1, 100000000)

		count = count + 1
		value[string.format("KR%d", v)] = {
			i = math.random(1, 1000000),
			v = {
				10, 20, 30, 40
			}
		}
	end
	-- print("INDEX", v)
	stage.signal("index:s?index", value)
	--[[ stage.signal("index:insert_callback", {
		["s?index"] = {
			[string.format("KR%d", v)] = {
				i = math.random(1, 1000000),
				v = {
					10, 20, 30, 40
				}
			}
		}
	}) ]]

	if count > 200000 then
		return -1
	end
end)


stage.waitfor("delete_callback", function (result)
	print("DELETE CALLBACK", result)
	print_r(result)
end)

--[[
stage.addtask(100, function ()
	local v = math.random(1, 100000000)

	-- print("INDEX", v)
	stage.signal("index:delete_callback=d?index", string.format("KR%d", v) );
	-- stage.signal("index:delete_callback", {
	--	["d?index"] = string.format("KR%d", v)
	-- });
end)
]]

stage.waitfor("@", function (...)
	local arg = {...}
	print(_WAITFOR[0], _WAITFOR[2], arg) print_r(arg)
end)

--[[ stage.waitfor("total_callback", function (result)
	print("TOTAL CALLBACK", result)
end) ]]--

stage.addtask(61000, function ()
	stage.signal("index:t?index=total_callback", 0)
end)

stage.waitfor("forEach_callback", function (result)
	print("FOREACH CALLBACK", result)
	stage.signal("index:delete_callback=d?index", result[2].k)
	print_r(result)
end)

-- stage.signal("index:forEach_callback=f?index", { 1, 10 })
-- stage.signal("index:forEach_callback=r?index", { "KR994482", 2, 5 })
--

--[[
local indexForEach = stage.proxy({"index:forEach_callback="}, {
	"index",
	function (k) return k end,
	[""] = function (t, o, k, v)
		-- print("CHANGE", t, o, k, v, t[1])
		stage.signal(t[1] .. k, v)
		return false
	end
})

indexForEach["f?index"] = { 1, 10 }
]]
--[[
local t_v = stage.proxy({}, {
	name = { 10, 20, 30, 40, function (t, edges, k, v)
		print("CHANGE", t, edges, k, v)
		print_r(edges)
	end}
})
t_v.name = 32
for k, v in pairs(t_v) do
	print("", k, v)
end
]]--
