

return {
    ["login"] = function (socket, data)
        socket.depth = "v1"

        socket.name = data.name
        stage.v("users", function (v) {
            if v == nil then
                v = {
                    ["socket"] = {},
                    ["name"] = {}
                }
            end

            v.socket[socket.id] = data.name
            v.name[data.name] = socket.id
            return v
        })
        return {
            ["login"] = "success"
        }
    end
}