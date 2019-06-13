
---
-- local INDEX = stage.load("index")
-- INDEX.New("rank", "index", function (m, k, v)
--    print("CALLBACK", m, k, v)
-- end)
-- :set("KR0001", 1120, {name = "HELLO"})
-- :get("KR0001")
-- :forEach(1, 10)
-- :delete("KR0001")
-- :total()
-- :commit()
---
---
local _INDEX = {}
local MAX_TIER = 4

-- callbaxk(key, result)
function _INDEX:tier(i)
    if i > MAX_TIER then
        error("")
    end
    self.tier = i
    return self
end

function _INDEX:callback(cb)
    self.callbacks = cb
    return self
end


local function methodK(self, method, tier)
    if tier == nil then
        tier = self.tier
    end

    self.i = self.i + 1
    return "#" .. self.i .. "." .. method .. ":" .. self.index .. "/" .. tier;
end

function _INDEX:set(key, score, value, callback)
    local req = {};

    if value ~= nil then
        req.v = value
    end
    if type(score) == "number" then
        req.i = score;
    elseif type(score) == "table" then
        req.I = score
    end

    local k = methodK(self, "set");
    self.values[k] = { [key] = req }
    if callback ~= nil then
        if type(self.callbacks) ~= "table" then
            error("")
        end
        self.callbacks[k] = { callback, function (k, v, cb) cb(k, v) end };;
    end
    return self
end

-- function (key, distance, value)
function _INDEX:get(values, callback)
    local k = methodK(self, "get");

    self.values[k] = values
    if callback ~= nil then
        if type(self.callbacks) ~= "table" then
            error("")
        end
        self.callbacks[k] = { callback, function (k, v, cb) cb(k, v.I, v.v) end};
    end
    return self
end

function _INDEX:forEach(start, count, callback)
    local k = methodK(self, "forEach");
    self.values[k] = { start, count }
    if callback ~= nil then
        if type(self.callbacks) ~= "table" then
            error("")
        end
        self.callbacks[k] = { callback, function (k, v, cb) cb(v.k, v.d, v.i, v.v) end};
    end
    return self
end

function _INDEX:range(key, delta, count, callback)
    local k = methodK(self, "range");
    self.values[k] = { key, delta, count }
    if callback ~= nil then
        if type(self.callbacks) ~= "table" then
            error("")
        end
        self.callbacks[k] = { callback, function (k, v, cb) cb(v.k, v.d, v.i, v.v) end};
    end
    return self
end

function _INDEX:delete(key, callback)
    local k = methodK(self, "del", 0);
    self.values[k] = key
    if callback ~= nil then
        if type(self.callbacks) ~= "table" then
            error("")
        end
        self.callbacks[k] = { callback, function (k, v, cb) cb(k, v) end};
    end
    return self
end

function _INDEX:total(callback)
    local k = methodK(self, "total");
    self.values[k] = 0
    if callback ~= nil then
        if type(self.callbacks) ~= "table" then
            error("")
        end
        self.callbacks[k] = { callback, function (k, v, cb) cb(k, v) end};
    end
    return self
end

--
--
function _INDEX:commit()
    if self.i == 0 then
        return false
    end

    local req_id = self.stage .. ":";

    local callback_count = 0
    for _ in pairs(self.values) do callback_count = callback_count + 1 end
    if (callback_count > 0) and (self.callbacks ~= nil) then

        req_id =  req_id .. stage.waitfor(function (callback, result)
            for k, v in pairs(result) do
                -- print("RESPONSE", k, v)
                if type(callback) == "function" then
                    local s = k:split(".:")
                    if type(v) ~= "table" then
                        callback(s[2], "", v)
                    else
                        for i_k, i_v in pairs(v) do
                            callback(s[2], i_k, i_v)
                        end
                    end
                elseif callback[k] ~= nil and v ~= nil then
                    local cb = callback[k]

                    if type(v) ~= "table" then
                        cb[2](k, v, cb[1])
                    else
                        for i_k, i_v in pairs(v) do
                            cb[2](i_k, i_v, cb[1])
                        end
                    end
                end
            end

            stage.waitfor(_WAITFOR[0], nil)
        end, self.callbacks)
    end

    return stage.signal(req_id, self.values)
end

local INDEX = {}
function INDEX.New(stage_id, index, callback)
    if index == nil then
        index = "index"
    end
    if callback == nil then
        callback = {}
    end
    return setmetatable({
        stage = stage_id, index = index, tier = 1, i = 0, values = {}, callbacks = callback
    }, { __index = _INDEX, __newindex = function (t, k, v)
        local _METHOD = {
            set = function (v)
                return t:set(v[1], v[2], v[3])
            end,
            get = function (v) return t:get(v) end,
            forEach = function (v) return t:forEach(v[1], v[2]) end,
            range = function (v) return t:range(v[1], v[2], v[3]) end,
            delete = function (v) return t:delete(v) end,
            total = function (v) return t:total() end
        }

        if _METHOD[k] ~= nil then
            _METHOD[k](v);
            t:commit()
        end
    end })
end
return INDEX
