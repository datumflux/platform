local function EXP_update(socket, data)
    if data.exp ~= nil then
        local user = bundle.proxy(socket._user, {
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
    local now = os.time()

    ::g_retry::

    local next = {}
    local retry = 0

    for k, v in pairs(data) do
        if socket._challenge[k] ~= nil then
            local CHALLENGE = bundle.proxy(socket._challenge[k], {
                value = { socket._challenge[k].finish, function (_this, edge, k, v)
                    if _this.finish_time == 0 and _this.expire_time > now then -- 이미 완료한 챌린지가 아닌 경우에 처리

                        if _this.retry_count == 0 then
                            _this.finish_time = now -- 완료시간
                        else -- 반복 가능한 챌린지 인 경우
                            _this.value = v - _this.finish
                            _this.retry_count = _this.retry_count - 1
                        end

                        if _this.next_challenge ~= 0 then -- 연결 챌린지가 존재
                            retry = retry + 1
                            next[_this.next_challenge] = _this.next_value
                        end

                        -- TODO: 보상 처리
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
    local IDS = USERS.ID

    local R = {}
    for k, v in pairs(IDS) do
        R[#R + 1] = v[2]
    end
    broker.signal(R, data)
end)

stage.waitfor("$unicast", function (data)
    local v = USERS.NICKNAME[data.target]
    if v ~= nil then
        broker.signal({ v[2] }, data)
    end
end)

local function CHATTING(socket, data)
    data.from = socket._user.nick_name
    if data.target == nil then -- all
        stage.signal("*$boradcast", data)
    else
        local v = USERS.NICKNAME[data.target]
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