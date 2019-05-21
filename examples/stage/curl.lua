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

stage.signal("agent:callback", {
    ["url"] = "http://www.naver.com"
});
