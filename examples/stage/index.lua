function tprint (tbl, indent)
    if not indent then indent = 0 end
    for k, v in pairs(tbl) do
        formatting = string.rep("  ", indent) .. k .. ": "
        if type(v) == "table" or type(v) == "userdata" then
            print(formatting)
            tprint(v, indent+1)
        else
            if type(v) == "string" then
                local l = string.len(v)
                if l > 20 then
                    v = string.sub(v, 0, 20) .. "..."
                end
            end
            print(formatting .. v)
        end
    end
end

stage.waitfor("callback", function (v)
    print("CALLBACK", v, _WAITFOR[0], broker.ntoa(_WAITFOR[1]))
    tprint(v)
end)

-- rank stage로 요청을 하며, 처리 결과는 callback으로 전달 받습니다.
stage.signal("rank+callback", {
    ["set:index"] = {
        ["KR0001"] = {
            ["i"] = 10,
            ["v"] = "TEST1"
        },
        ["KR0002"] = {
            ["i"] = 100,
            ["v"] = "TEST2"
        },
        ["KR0003"] = {
            ["i"] = 200,
            ["v"] = "TEST3"
        },
    },
    ["get:index"] = { "KR0001", "KR0002" },
    ["range:index"] = { 100, 100 },
    ["range:index"] = { "KR0001", 1, 100 },
    ["forEach:index"] = { 1, 100 },
});
