return {
    ["logout"] = function (socket, data)
        socket.close()
    end,

    ["chat"] = function (socket, data)

        -- 사용자를 확인합니다.
        stage.v("users", function (v)

            if data.target == "*" then
                for k, i in pairs(v.socket) do

                    broker.signal(k, {
                        "chat": data.chat,
                        "from": socket.name
                    })

                end
            else if v.name[data.target] ~= nil then
                broker.signal(v.name[data.target], {
                    "chat": data.chat,
                    "from": socket.name
                })
            end

        end)

    end
}