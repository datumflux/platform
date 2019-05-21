stage.signal("route:@", {
    ["id0"] = "call0=ret0"
});

stage.waitfor("ret0", function (v)
    print("RETURN", v, _WAITFOR[0])
end)

stage.waitfor("call0", function (id, v)
    print("CALLBACK", id, v, _WAITFOR[0])
    stage.signal(nil, "FEEDBACK--" .. v)
end)

stage.signal("call0=ret0@A", { "USER_ID", "HI" })
stage.signal("route:*id0", "ROUTE MESSAGE");