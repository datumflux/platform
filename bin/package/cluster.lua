--%LICENSE DATUMFLUX
--
stage.load("cluster.ready", function (cluster)
	local addr = process["$%cluster"]
	if addr ~= nil then
		return cluster(addr)
	end
end)
--[[
stage.waitfor("callback", function (v)
	print("WAITFOR-CALLBACK", v)
end)

stage.addtask(1, function ()
	local cluster = stage.load("cluster.ready")()
	cluster:signal("test+hello=callback", { math.random(), math.random() })
end)
]]--

-- print("FFI", import("ffi"))

--
-- 1. 변화 감지
--
local T = {
	value = 10,
	level = 1
}

local proxy = stage.proxy(T, {
	["value"] = { 10, 20, 30, function (proxy, edges, k, v)
		print("CHANGE VALUE",  k, v)
		for i, p in ipairs(edges) do
			print("", i, p)
			proxy.level = math.floor(p / 10)
		end
	end },
	["level"] = { 1, 2, 3, function (proxy, edges, k, v)
		print("CHANGE LEVEL", k, v)
		for i, p in ipairs(edges) do
			print("", i, p)
		end
	end }
})

proxy.value = 30

--
-- 2. 변경이력 및 객체 접근
--
odbc.new({
	["DRIVER"] = "MySQL ODBC 8.0 ANSI Driver", -- /etc/odbcinst.ini에 정의
    ["SERVER"] = "localhost",
    ["DATABASE"] = "DF_DEVEL",
    ["USER"] = "datumflux",
    ["PASSWORD"] = "Sskang74@",
    [".executeTimeout"] = 1000,
    [".queryDriver"] = "mysql"
}, function (adp)
	print(adp)
	local rs = adp.execute("SELECT * FROM EXAMPLE")

	--
	-- EXAMPLE의 쿼리 정보를 "id" 순서로 정렬
	local rows = rs.fetch({"id"}, function (rows)
		print("FETCH", rows)
		return rows
	end)

	-- 
	-- 신규 데이터 추가
	-- 특징: "id" 컬럼은 자동증가 컬럼
	rows[0] = {
		["name"] = "TEST",
		["udata"] = {
			"BLOB"
		}
	}

	--
	-- EXAMPLE에 변경된 정보 저장
	rs.apply(rows, "EXAMPLE", function (ai_k, log)
		-- ai_k: 자동 증가된 컬럼 목록
		-- log: 변경 이력
		print("COMMIT", ai_k, log, print_r, _STAGE)
		print_r(ai_k) print_r(log)
	end)
end)

--
-- 2. 객체 접근: 인덱스 변환
--
local index = stage.proxy({}, {
	function (i)
		return (i[2] * 100) + i[1]
	end
})

index[{10, 2}] = 100
print_r(index)

--
-- 3. 접근 제한: 샌드박스
--
_SANDBOX[""] = { "*PUBLIC_VALUE" }

PUBLIC_VALUE = { "HELLO" }
PRIVATE_VALUE = { "SANDBOX" }

stage.submit(0, function ()
	print("PUBLIC", PUBLIC_VALUE[1])
	print("PRIVATE", PRIVATE_VALUE and PRIVATE_VALUE[1] or "<unknown>")
end)

--
-- 3. 접근 제한: 동시 실행 제한 & 특정 채널 메시지
--
stage.waitfor("ticket:@", function (op, result)
	print("RESULT TICKET", op) print_r(result)
end)

stage.waitfor("ticket:odbc", function (message)
	print("TICKET MESSAGE", message)
end)

--[[
print("_STAGE", _STAGE)
for k, v in pairs(_G) do
	print("", k, v)
end
]]
stage.waitfor("limit_callback", function (value)
	stage.signal(nil, 1000)	-- 처리 허용시간 1000 msec 연장
	stage.signal("ticket:=odbc", "HELLO MESSAGE")
	--[[
	print("_WAITFOR", _STAGE)
	for k, v in pairs(_G) do
		print("", k, v)
	end ]]
	print("EXECUTE LIMIT CALLBACK", value, _STAGE) print_r(value)
end)

stage.signal("ticket:odbc=limit_callback", { "HELLO", "START" })

--
-- 4. 방송 및 처리 분산
--
stage.waitfor("route:@", function (id, k)
	print("REQUEST ROUTE", id, k)
	stage.signal(nil,  k)	-- 요청에 대한 가능 회신
end)

stage.waitfor("route_callback", function (id, value)
	print("ROUTE MESSAGE", id, value) print_r(value)
end)

-- 정책 설정
stage.signal("route:@", {
	["chatting"] = "route_callback",
	["notice"] = "route_callback"
})

-- 라우팅 요청
stage.signal("route:chatting?userid", { "HELLO", "MESSAGE" })

local R = {}
print("RANDOM", stage.random(R, 1, 1000))
print("RANDOM", stage.random(R))
print("RANDOM", stage.random(R, function (random)
	return random(1, 1000)
end))
print_r(R)

print("_VERSION", string.find(_VERSION, 'Lua (.*)'))
print("OPENSSL", require("openssl"))

stage.waitfor("curl_callback", function (r)
	print("CURL_CALLBACK", r) -- print_r(r)
end)

stage.signal("curl:curl_callback", "https://m.naver.com")

