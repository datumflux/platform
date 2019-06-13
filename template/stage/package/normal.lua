
local function LOGIN_user(socket, data)
	print("LOGIN", data)
	stage.v("USERS", function (USERS)
		local user_id = socket._user.user_id
		local nick_name = socket._user.nick_name

		USERS.ID[user_id] = { nick_name, socket.id }
		USERS.NICKNAME[nick_name] = { user_id, socket.id }
	end)
	return {
		login = { socket._user, socket._challenge }
	}
end

return {
	login = LOGIN_user,
}
