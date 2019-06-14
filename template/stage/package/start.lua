
-- SANDBOX 정책에 상관없이 자료형이 공유되도록 설정
_ENV[""] = {
}

-- stage.submit()과 broker.ready()에 연결된 함수에서 사용하기 위한 변수를 설정
__("BLACKLIST", {})
__("MAINTENANCE_TIME", { 0, 0, {} })
__("CONCURRENT", {
	REPORT = {},
	start = 0
});

__("updateConcurrentReport", function ()

	return function( index, now)
		__("CONCURRENT", function (CONCURRENT)

			if now == nil then
				now = os.time()
			end

			local tm = os.date("*t", now)
			local today_index = (tm.hour * 60) + tm.min
			if CONCURRENT.REPORT[today_index] == nil then
				CONCURRENT.REPORT[today_index] = { 0, 0, 0 }
			end
			CONCURRENT.REPORT[today_index][index] = CONCURRENT.REPORT[today_index][index] + 1
			return CONCURRENT
		end)
	end
end)

--
stage.addtask(60 * 1000, function ()
	local tm = os.date("*t", now)
	local today_index = ((tm.hour * 60) + tm.min) - 1
	if today_index < 0 then
		today_index = 1357 -- 23 Hour, 59 Min
	end

	local REPORT = CONCURRENT.REPORT[today_index]
	if REPORT ~= nil then
		log4cxx.out("CONCURRENT: " .. today_index .. " [", REPORT[1], REPORT[2], REPORT[3], "]")
		--[[ odbc.new("DF_DEVEL", function (adp)
			adp.execute("UPDATE USER SET logout_time = ? WHERE user_id = ?", { os.time(), socket._user.user_id })
		end) ]]--
	end
end)

stage.waitfor("$black", function (ips)
	__("BLACKLIST", function (BLACKLIST)
		for k, v in ipairs(ips) do
			if v == 0 then
				BLACKLIST[k] = nil
			else
				BLACKLIST[k] = v
			end
		end
		return BLACKLIST
	end)
end)

stage.waitfor("$maintenance", function (op, value)
	__("MAINTENANCE_TIME", function (MAINTENANCE_TIME)

		if op == "set" then
			if type(value) == "table" then
				MAINTENANCE_TIME = value
			elseif type(value) == "number" then
				MAINTENANCE_TIME[2] = os.time() + value
			end
			--
			if MAINTENANCE_TIME[1] > MAINTENANCE_TIME[2] then
				-- 점검이 설정되면 기본적으로 1일을 설정
				MAINTENANCE_TIME[2] = MAINTENANCE_TIME[1] + 86400
			end
		elseif op == "add" then
			if type(value) == "string" then
				MAINTENANCE_TIME[3][value] = 0
			elseif type(value) == "table" then
				for k, v in ipairs(value) do
					MAINTENANCE_TIME[3][k] = 0
				end
			end
		elseif op == "remove" then
			if type(value) == "string" then
				MAINTENANCE_TIME[3][value] = nil
			elseif type(value) == "table" then
				for k, v in ipairs(value) do
					MAINTENANCE_TIME[3][k] = nil
				end
			end
		elseif op == "reset" then
			MAINTENANCE_TIME = { 0, 0, {}}
		end
		return MAINTENANCE_TIME
	end)

end)

print(process.id .. ": Initialize... STAGE[" .. process.stage .. "]")
--
-- 테스트를 위해 패킷 유형을 text로 설정하였습니다. 실제 서비스에서는 bson으로 변경 필요합니다.
--
__("USERS", {
	ID = {},
	NICKNAME = {}
})

local port = broker.ready("tcp://0.0.0.0:8081?text=2k,8k", function (socket, addr)
	local ip = broker.ntoa(addr):split()[1] -- "IP:PORT" => "IP"
	local now = os.time()

	print("ACCEPT", BLACKLIST, MAINTENANCE_TIME)
	if BLACKLIST[ip] ~= nil then
		local allow = BLACKLIST[ip]
		if allow > os.time() then
			log4cxx.out("BLACKLIST: DISALLOW - " .. ip)
			return false
		end
	end

	if MAINTENANCE_TIME[1] < MAINTENANCE_TIME[2] then
		if now > MAINTENANCE_TIME[2] then -- 점검시간 마침
		elseif now > MAINTENANCE_TIME[1] then -- 점검시간 시작
			if MAINTENANCE_TIME[3][ip] ~= nil then
				log4cxx.out("MAINTENANCE ALLOW - " .. ip)
				return true
			end
			return false
		end
	end

	updateConcurrentReport(1, now)
	socket._waitfor = {}
	print("LISTEN", ip)
end)

port.expire = 30000
port.close = function (socket)

	updateConcurrentReport(2)
	for k, v in pairs(socket._waitfor) do
		log4cxx.out("CANCEL - " .. k, v[1], v[2])
		stage.waitfor(v[1], nil) -- { callback_id, start_time }
	end

	if socket._user ~= nil then
		local user_id = socket._user.user_id

		__("USERS", function (USERS)
			local v = USERS.ID[user_id] -- [ nickname, socket_id ]

			USERS.NICKNAME[ v[1] ] = nil -- [ user_id, socket_id ]
			USERS.ID[user_id] = nil
			return USERS
		end)

		log4cxx.out("LOGOUT", socket._user.user_id)
		odbc.new("DF_DEVEL", function (adp)
			adp.execute("UPDATE USER SET logout_time = ? WHERE user_id = ?", { os.time(), socket._user.user_id })
		end)
	end
end

port.receive = function (socket, _data)
	--[[
	{
	   "cmd": value
	}
	]]
	updateConcurrentReport(3)
	stage.submit(socket.id, function (_socket, _data)
		local socket = broker.f(_socket)
		local data = stage.json(_data)

		--[[
		print("RECEIVE: ", socket, data)
		for k, v in pairs(data) do
			print("----", k, v)
		end
		]]--
		for k, v in pairs(data) do

			local user_level = "normal"
			if socket._user ~= nil then -- 로그인정보 생성 전
				user_level = socket._user.level
				if user_level == 0 then
					user_level = "user"
				elseif user_level >= 90 then
					user_level = "operator"
				end
			end

			stage.load(user_level .. "+" .. k, function (f)
				local r = f(socket, v)
				if r ~= nil then
					socket.commit(r)
				end
			end)
		end
	end, socket.id, _data)
end

--[[
local V = stage.proxy({}, {
	count = function (_this, edge, k, v)
		print("COUNT: UPDATE - ", k, v)
		if _this.count == nil then
			_this.count = 1
		end
		_this.count = _this.count + 1
	end
})

V.count = 10
print("COUNT: ", V.count)
]]--
return function ()
    require('luv').run('nowait')
end
