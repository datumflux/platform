
local function LOGIN_user(socket, data)
	print("LOGIN", data)

	socket("_waitfor", function (_waitfor)
		if _waitfor["login"] ~= nil then
			stage.waitfor(_waitfor[1], nil)
			_waitfor["login"] = nil
		end
		return _waitfor
	end)

	local callback_id = stage.waitfor(function (_socket, v)
		local socket = broker.f(_socket)
		local callback_id = _WAITFOR[0]:split("=")[1]

		local success = false
		socket("_waitfor", function (_waitfor)

			print("TICKET CALLBACK", v, _waitfor["login"][1], callback_id)
			if _waitfor["login"][1] == callback_id then
				success = true

				_waitfor["login"] = nil
				stage.waitfor(callback_id, nil)
			end
			return _waitfor
		end)

		if success == false then
			return
		end

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

	socket("_waitfor", function (_waitfor)
		_waitfor["login"] = { callback_id, os.time() }
		return _waitfor
	end)
	print("TICKET REQUEST", v, socket._waitfor["login"], os.time())
	stage.signal("ticket:login=" .. callback_id, data)
end

return {
	login = LOGIN_user,
}
