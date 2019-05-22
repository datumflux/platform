-- "tcp://0.0.0.0:8081?bson=4k,10k" => process["$ListenPort"]
-- 설정시, "=lua" 설정(start.sh)에서 "ListenPort": "tcp://0.0.0.0:8081?bson=4k,10k" 로 정의하여 사용가능
--
BLACKLIST_IP = {}
MAINTENANCE_TIME = { 0, 0, {} }

stage.waitfor("maintence", function (v)
    if v.cmd == "start" then
        MAINTENANCE_TIME[0] = process.tick
    else if v.cmd == "end" then
        MAINTENANCE_TIME[0] = MAINTENANCE_TIME[1] = 0
    else if v.cmd == "allow" then
        MAINTENANCE_TIME[2][v.ip] = process.tick
    else if v.cmd == "deny" then
        MAINTENANCE_TIME[2][v.ip] = nil
    end
end)

-- 클라이언트와의 통신 방법은 bson으로 처리
local listenPort = broker.ready("tcp://0.0.0.0:8081?bson=4k,10k", function (socket, addr)
    local ip = broker.ntoa(addr, {})

    if MAINTENANCE_TIME[0] ~= MAINTENANCE_TIME[1] then
        local now = process.tick

        -- 유지보수 시간인지 확인
        if (MAINTENANCE_TIME[0] <= now) and 
                ((MAINTENANCE_TIME[1] == 0) or (MAINTENANCE_TIME[1] > now)) then
            -- 유지보수 중에도 접속이 가능한 ip이면...
            if MAINTENANCE_TIME[2][ip.addr] ~= nil then
                return true
            end

            return false
        end
    end

    -- 블랙리스트에 등록된 IP인지 확인
    if BLACKLIST_IP[ip.addr] ~= nil then
        return false
    end

    -- 암호화 설정된 bson을 사용하기 위해서 재 패키징의 처리가 필요합니다.
    -- socket.crypto = "--crypto--key--"
    socket.connect_time = process.tick
    socket.packet_depth = 0
    -- 접속을 해제하고 싶다면, false를 반환 
    return true;
end)

-- 3초 동안 데이터 발생이 없다면 소켓을 닫습니다.
listenPort.expire = 3 * 1000;

-- 전송 대기 데이터를 100msec 주기로 전송합니다.
listenPort.commit = 100;

-- 소켓이 닫히는 경우에 발생
listenPort.close = function (socket)

    if socket.login_time ~= nil then
        stage.signal("*logout", { 
            ["userid"] = socket.userid,
            ["logout_time"] = process.tick
        })
    end

end

-- 데이터가 발생되는 경우 호출 
listenPort.receive = function (socket, data)
    -- 스크립트를 {cmd}.lua 와 같은 형태로 배치
    stage.load(data.cmd, function (r)

        if type(r) == "table" then
            local f = r[socket.packet_depth]
            if type(f) == "function" then
                return f(socket, data)
            else if type(f) == "table" then
                socket.commit(f)
            end
        else if type(r) == "function" then
            return r(socket, data)
        end

    end)
end