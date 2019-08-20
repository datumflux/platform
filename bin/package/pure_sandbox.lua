local _G, coroutine = _G, coroutine
local main_thread = coroutine.running() or {} -- returns nil in Lua 5.1
local thread_locals = setmetatable({ [main_thread] = _G }, { __mode = "k" })
local sandbox__metatable = {}

function sandbox__metatable:__index(k)
  local v = __[k]
  if v then
	return v
  else
    local th = coroutine.running() or main_thread
    local t = thread_locals[th]
    if t then
      return t[k]
    else
      return _G[k]
	end
  end
end

function sandbox__metatable:__newindex(k, v)
  if not __(k, function (o)
	  if o ~= nil then
		  return v
	  end
  end) then
    local th = coroutine.running() or main_thread
    local t = thread_locals[th]
    if not t then
      t = setmetatable({ _G = _G }, { __index = _G })
      thread_locals[th] = t
    end
    t[k] = v
  end
end

-- convenient access to thread local variables via the `sandbox` table:
local sandbox = setmetatable({}, sandbox__metatable)
-- or make `sandbox` the default for globals lookup ...
if setfenv then
  setfenv(1, sandbox) -- Lua 5.1
else
  _ENV = sandbox -- Lua 5.2+
end

