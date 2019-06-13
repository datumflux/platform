local function EXP_update(socket, data)
    if data.exp ~= nil then
        local user = stage.proxy(socket._user, {
            exp = { 100, 200, 300, 400, 500, 600, function (_this, edge, k, v)
                local count = 0
                for _i in ipairs(edge) do
                    count = count + 1
                end
                _this.level = _this.level + count
            end },

            level = function (_this, _edge, k, v)
            end
        })

        user.exp = user.exp + data.exp
    end
end

local function CHALLENGE_update(socket, data)
    -- { "challenge_id": value, ... }
    ::g_retry::

    local next = {}
    local retry = 0

    for k, v in pairs(data) do
        if socket._challenge[k] ~= nil then
            local CHALLENGE = stage.proxy(socket._challenge[k], {
                value = { socket._challenge[k].finish, function (_this, edge, k, v)
                    if _this.next ~= 0 then
                        retry = retry + 1
                        next[_this.next] = v - _this.finish
                    end
                end }
            })

            CHALLENGE.value = CHALLENGE.value + v
        end
    end

    if retry > 0 then
        data = next
        goto g_retry
    end
end

stage.waitfor("$broadcast", function (data)
    local IDS = process.USERS.ID

    local R = {}
    for k, v in pairs(IDS) do
        R[#R + 1] = v[2]
    end
    broker.signal(R, data)
end)

stage.waitfor("$unicast", function (data)
    local v = process.USERS.NICKNAME[data.target]
    if v ~= nil then
        broker.signal({ v[2] }, data)
    end
end)

local function CHATTING(socket, data)
    data.from = socket._user.nick_name
    if data.target == nil then -- all
        stage.signal("*$boradcast", data)
    else
        local v = process.USERS.NICKNAME[data.target]
        if v ~= nil then
            broker.signal({ v[2] }, data)
        else
            stage.signal("*$unicast", data)
        end
    end
end

return {
    exp = EXP_update,
    challenge = CHALLENGE_update,
    chat = CHATTING
}