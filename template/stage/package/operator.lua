local _PROTOCOL = stage.load("user", function (f)
    local _PROTOCOL = {}

    if type(f) == "table" then
        for k, v in pairs(f) do
            _PROTOCOL[k] = v
        end
    end
    return _PROTOCOL
end)

return _PROTOCOL