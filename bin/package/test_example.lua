thread.new(function ()

---
--
dofile 'luvit-loader.lua' -- Enable require to find lit packages
local p = require('pretty-print').prettyPrint -- Simulate luvit's global p()

-- This returns a table that is the app instance.
-- All it's functions return the same table for chaining calls.
require('weblit-app')

  -- Bind to localhost on port 3000 and listen for connections.
  .bind({
    host = "0.0.0.0",
    port = 3000
  })

  -- Include a few useful middlewares.  Weblit uses a layered approach.
  .use(require('weblit-logger'))
  .use(require('weblit-auto-headers'))
  .use(require('weblit-etag-page'))

  -- This is a custom route handler
  .route({
    method = "GET", -- Filter on HTTP verb
    path = "/greet/:name", -- Filter on url patterns and capture some parameters.
  }, function (req, res)
    p(req) -- Log the entire request table for debugging fun
    res.body = "Hello " .. req.params.name .. "\n"
    res.code = 200
  end)

  -- Actually start the server
  .start()

-- We also need to explicitly start the libuv event loop.
require('luv').run()

end)

--
--
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

--[[
odbc.new({
	["DRIVER"] = "MySQL ODBC 8.0 ANSI Driver", -- /etc/odbcinst.ini에 정의
    ["SERVER"] = "localhost",
    ["DATABASE"] = "DF_DEVEL",
    ["USER"] = "user",
    ["PASSWORD"] = "password",
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
]]

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

local FILE = { "example.mmap", { "b10 i c10" } }

stage.viewof(FILE, 10, function (buffer)
	print("MMAP", buffer.rows, buffer.row_size)
	buffer[1] = { {1, 2}, 2, "01234567890123" }
	buffer[2] = { {3, 4}, 10, "23456789890123" }
	buffer[10] = { {4, 6}, 20, "12123456789123" }
	print("RESULT", stage.viewof({ buffer, { "b10 i c10" }}, 16, 1, function (data)
		print("SCOPE") print_r(data[1])
		data[1] = { {6, 7}, 10, "11156789890123" }
		return "HELLO", "SUCCESS"
	end))
	print("UPDATE") print_r(buffer[2])

	buffer.scope(function ()
		print("RWLOCK")
	end)

	buffer.commit('async')
	--[[ for k, v in ipairs(buffer) do
		print("", k, v)
		print_r(v)
	end ]]
end)

logger.out("HELLO LOG4CXX")
