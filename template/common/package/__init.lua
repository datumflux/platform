return function ()
    --
    local socket = broker.ready("tcp://0.0.0.0:8801?bson=1k,4k", function (socket, addr)
        --
        print("ACCEPT", socket.id)
        return true
    end)

    socket.expire = 3000 -- 3초 이상 데이터 전달이 없다면 소켓 폐쇄
    socket.commit = 100  -- 100msec 간격 으로 데이터 전송

    socket.close = function (socket)
        print("CLOSE", socket.id)
    end

    socket.receive = function (socket, data, addr)

        if data.message == nil then
            socket.close()
            return
        end

        stage.submit(socket.id, function (socket_id, data, addr)
            local socket = broker.f(socket_id)

            if socket.depth == nil then
                socket.depth = "v0"
            end

            stage.load(socket.depth .. "+" .. data.message, function (f)
                local v = f(socket, data)
                if v ~= nil then
                    socket.commit(v)
                end
            end)

        end, socket.id, data, addr)
    end
    return socket
end