local FILE = { "example.mmap", { "b4 i", "s10" } }

stage.viewof(FILE, 10, function (buffer)
	print("MMAP", buffer, buffer.rows, buffer.size)
	buffer[1] = { {1, 2}, 2, "01234567890123" }
	buffer[2] = { {3, 4}, 15, "23456789890123" }
	buffer[10] = { {4, 6}, 20, "12123456789123" }
	--[[
	print("RESULT", stage.viewof({ buffer, { "b4 i c10" }}, buffer.size, 1, function (data)
		print("SCOPE") print_r(data[1])
		data[1] = { {6, 7}, 10, "11156789890123" }
		return "HELLO", "SUCCESS"
	end)) ]]
	local r = buffer[2]

	r[1] = { [3] = 4 }
	r[2] = 20
	print("UPDATE", r) print_r(r)

	buffer.scope(function ()
		print("RWLOCK")
	end)

	buffer.commit('async')
	--[[ for k, v in ipairs(buffer) do
		print("", k, v)
		print_r(v)
	end ]]
	print("END", buffer)
end)

stage.waitfor("route:@", function (id, k)
	print("REQUEST ROUTE", id, k)
	stage.signal(nil,  k .. "?offer_callback")	-- 요청에 대한 가능 회신
end)

stage.waitfor("route_callback", function (id, value)
	print("ROUTE MESSAGE", _WAITFOR[0], id, value) print_r(value)
end)

stage.waitfor("offer_callback", function (id, value)
	print("ROUTE MESSAGE", _WAITFOR[0], id, value) print_r(value)
end)

-- 정책 설정
stage.signal("route:@", {
	["chatting"] = "route_callback",
	["notice"] = "offer_callback"
})

-- 라우팅 요청
stage.signal("route:chatting,notice?userid", { "HELLO", "MESSAGE" })


