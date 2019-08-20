-- print("HELLO WORLD")
return {
	["hello"] = function (a, b)
		-- print("HELLO", socket, a, b)
		return "HELLO: " .. (a + b)
	end
}
