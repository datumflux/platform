_SANDBOX[""] = { "*EXAMPLE" }

stage.waitfor("callback", function (a, b, c)
	print("WAITFOR", a, b, c)
end)

EXAMPLE = { 10, 20 }
print("EXAMPLE", EXAMPLE)
for k, v in pairs(EXAMPLE) do
	print("", k, v)
end

print("SANDBOX")
__.TEST = "HELLO"
for k, v in pairs(_SANDBOX) do
	print("", k, v)
end
stage.signal("callback", 10, 20, { "HELLO" } )

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
	rs.columns(function (columns)
		print("COLUMNS", columns)
		for k, v in pairs(columns) do
			print("", k, v)
			for v_k, v_v in pairs(v) do
				print("", "", v_k, v_v)
			end
		end
	end)

	local rows = rs.fetch({"id"}, function (rows)
		print("FETCH", rows)
		return rows
	end)

	print("ROWS", rows(20)) -- id == 20
	print("ROWS", rows({19, 20}, function (r)
		local total = 0
		local success = 0
		for k, v in pairs(r) do
			if v ~= nil then
				success = success + 1
			end
			total = total + 1
		end
		return total == success
	end)) -- id = [19, 20]
	print("ROWS", #rows()) -- id all
	print("ROWS", rs.near(rows, 20)) -- rows id == 20
	for k, v in pairs(rows) do
		print("", k, v)
	end

	if #rows() == 0 then
		rows[0] = {
			["name"] = "TEST",
			["udata"] = {
				"BLOB"
			}
		}
		rs.apply(rows, "EXAMPLE", function (ai_k, log)
			print("COMMIT", ai_k, log)
			for k, v in ipairs(ai_k) do
				print("", k, v)
			end
		end)
	end
end)

local proxy = stage.proxy({}, {
	[""] = {
		"index:{10,10}",
		function (k, i)
			print("INDEX", k, i)
			return k[1]
		end,

		["*"] = function (t, edges, k, v)
			print("NAME CHANGE", t, edges, k, v)
		end,
		["#"] = function (t, edges, k, v)
			print("INDEX CHANGE", t, edges, k, v)
		end
	},
	test = { 10, 20, function (t, edges, k, v)
		print("TEST", edges, k, v)
		for k, v in pairs(edges) do
			print("", k, v)
		end
	end}
})

proxy.test = 0
proxy.test = 40
for i = 1, 30, 1 do
	proxy.test = proxy.test + 1
end

proxy.name = "HELLO"
proxy[10] = "STAGE"
proxy[{"HELLO", 20}] = "HAHA"

print("PROXY", proxy)
for k, v in pairs(proxy) do
	print("", k, v)
end

stage.waitfor("ticket:@", function (result_id, result)
	print("TICKET CONTROL", result_id, result)
end)

stage.waitfor("ticket_callback", function (value)
	print("TICKET CALLBACK", value)
end)
stage.waitfor("fail_callback", function (id, value)
	print("FAILED CALLBACK", id, value)
	for k, v in pairs(value) do
		print("", k, v)
	end
end)

stage.signal("ticket:odbc?fail_callback", {
	[""] = "USER DATA",
	ticket_callback = "HELLO"
})
