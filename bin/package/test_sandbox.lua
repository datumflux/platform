print("SANDBOX", _SANDBOX)
--[[
if setfenv then
  setfenv(1, getmetatable(_SANDBOX)) -- Lua 5.1
else
  _ENV = _SANDBOX -- Lua 5.2+
end
]]--
_SANDBOX[""] = { "*HELLO", "TEST" }

stage.waitfor("callback", function (v)
	print("============CALLBACK", v)
	if type(v) == "table" then
		for k, i in pairs(v) do
			print("", k, i)
		end
	end
end)

HELLO = "HI HELLO"
print("NORMAL _SANDBOX============================")
_G.DIRECT = "1234"
PRIVATE = "1122"
for k, v in pairs(_SANDBOX) do
	print("", k, v)
end

print("NORMAL _G============================")
print("_G", _VERSION, _G)
for k, v in pairs(_G) do
	print("", k, v)
end

print("============================")
TEST="PUBLIC"
co = coroutine.create(function (a,b,c)
   print("COROUTINE _G", a,b,c, HELLO, TEST)
   --[[ for k, v in pairs(_G) do
  	  print("", k, v)
   end ]]
   coroutine.yield()
   -- NAME = "HELLO"
   print("COROUTINE _SANDBOX", _VERSION)
   --[[ for k, v in pairs(_SANDBOX) do
   	  print("", k, v)
   end ]]--

   co2 = coroutine.create(function (a,b,c)
	   print("COROUTINE _SANDBOX2", _VERSION)
	   -- PRIVATE = "TEST"
	   --[[ for k, v in pairs(_SANDBOX) do
		  print("", k, v)
	   end ]]
	   stage.signal("callback", "HI TEST")
	end)
	coroutine.resume(co2, 1, 2, 3)

   PRIVATE = "NAME11"
   print("COROUTINE _SANDBOX", _VERSION)
   --[[ for k, v in pairs(_SANDBOX) do
   	  print("", k, v)
   end ]]
   print("co", a,b,c, HELLO, PRIVATE)
end)
PRIVATE = "2233"
TEST="CHANGE"
coroutine.resume(co, 1, 2, 3)
coroutine.resume(co, 3, 4, 5)
print("============================")
--[[ print("_SANDBOX", _VERSION)
for k, v in pairs(_SANDBOX) do
	print("", k, v)
end ]]--

local G = __[1]
print("_SANDBOX", G)
G.SAMPLE = "SAMPLE"

thread.new(function (a, b)
	print("CALL", a, b, __[1].SAMPLE)
	return a + b
end, 10, 20).waitfor(function (r)
	print("RESULT", r)
	for k, v in pairs(__[1]) do
		print("", k, v)
	end
end)

