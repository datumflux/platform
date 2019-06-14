
local function LOGIN_user(socket, data)
	print("LOGIN", data)

	if socket._waitfor["login"] ~= nil then
		stage.waitfor(socket._waitfor[1], nil)
		socket._waitfor["login"] = nil
	end

	local callback_id = stage.waitfor(function (_socket, v)
		local socket = broker.f(_socket)
		local callback_id = _WAITFOR[0]:split("=")[1]

		print("TICKET CALLBACK", v, socket._waitfor["login"][1], callback_id)
		if socket._waitfor["login"][1] ~= callback_id then
			return
		end

		socket._waitfor["login"] = nil
		stage.waitfor(_WAITFOR[0], nil)

		local r = odbc.new("DF_DEVEL", function (adp)

			socket._user = { user_id = "KR001", nick_name = "HELLO" }
			socket._challenge = { }
			stage.v("USERS", function (USERS)
				local user_id = socket._user.user_id
				local nick_name = socket._user.nick_name

				USERS.ID[user_id] = { nick_name, socket.id }
				USERS.NICKNAME[nick_name] = { user_id, socket.id }
			end)
			return {
				login = { socket._user, socket._challenge }
			}
		end)
		broker.signal( { _socket }, r)
	end, socket.id)

	socket._waitfor = {login = { callback_id, os.time() }}
	print("TICKET REQUEST", v, socket._waitfor["login"], os.time())
	stage.signal("ticket:login=" .. callback_id, data)
end

return {
	login = LOGIN_user,
}
