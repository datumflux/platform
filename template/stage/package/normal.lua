
local function LOGIN_user(socket, data)
	print("LOGIN", data)
end

return {
	login = LOGIN_user,
}
