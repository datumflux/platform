function string:dump()
    for byte=1, #self, 16 do
        local chunk = self:sub(byte, byte+15)
        io.write(string.format('%08X  ',byte-1))
        chunk:gsub('.', function (c) io.write(string.format('%02X ',string.byte(c))) end)
        io.write(string.rep(' ',3*(16-#chunk)))
        io.write(' ',chunk:gsub('%c','.'),"\n")
    end
end
