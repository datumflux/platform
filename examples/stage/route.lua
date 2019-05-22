stage.waitfor("route:@", function (id, k)
    print("READY CHECK", id, k)
    if id == "id0=ret0" then
        stage.signal(nil, k)
    end
end)

stage.signal("route:@", {
    ["id0"] = "call0=ret1",
    ["id1"] = "call0"
});

stage.waitfor("fail0", function (id, v)
    print("FAIL", id, v, _WAITFOR[0])
end)

stage.waitfor("ret0", function (v)
    print("RETURN", v, _WAITFOR[0])
end)

stage.waitfor("call0", function (id, v)
    print("CALLBACK", id, v, _WAITFOR[0])
    if v ~= nil then
        stage.signal(nil, "FEEDBACK--" .. v)
    end
end)

-- stage.signal("call0=ret0@A", { "USER_ID", "HI" })
stage.addtask(100, function ()
    stage.signal("route:id0=ret0", "ROUTE MESSAGE");
    stage.signal("route:id1!fail0=ret1", "FAIL MESSAGE");
    stage.signal("route:*id0!fail0=ret0", "BROADCAST MESSAGE");
    return 0;
end)
--stage.signal("route+test", "BROADCAST MESSAGE");
-- stage.signal("route:*s_id!f_callback", "ROUTE MESSAGE");