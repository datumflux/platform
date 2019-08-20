function print_r(arr, indentLevel)
    if(indentLevel == nil) then
        print(print_r(arr, 0))
        return
    end

	if type(arr) == "table" or type(arr) == "userdata" then
    	local str = ""
    	local indentStr = "#"

    	for i = 0, indentLevel do
    	    indentStr = indentStr.."\t"
   	 	end

		for index,value in pairs(arr) do
			if type(value) == "table" then
				str = str..indentStr..index..": \n"..print_r(value, (indentLevel + 1))
			else 
				str = str..indentStr..index..": "..tostring(value).."\n"
			end
		end
    	return str
	else
		return tostring(arr)
	end
end
