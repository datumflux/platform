function import(f)
	return stage.require(f, function (r, i)
		if r ~= nil then
			return r
		end

		r = require(i[1])
		if r ~= nil and i[2] ~= nil then
			if type(r) == "table" then
				return r[ i[2] ]
			end
			return nil
		end
		return r
	end)
end
