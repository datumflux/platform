
function tprint (tbl, indent)
    if not indent then indent = 0 end
    for k, v in pairs(tbl) do
        formatting = string.rep("  ", indent) .. k .. ": "
        if type(v) == "table" or type(v) == "userdata" then
            print(formatting)
            tprint(v, indent+1)
        else
            if type(v) == "string" then
                local l = string.len(v)
                if l > 20 then
                    v = string.sub(v, 0, 20) .. "..."
                end
            end
            print(formatting .. v)
        end
    end
end

odbc.new("DF_DEVEL", function (adp)
    local rs = adp.execute("SELECT * FROM DEMO");
    local rows = rs.fetch({"user"});
    print("SELECT", rs, rows);
    tprint(rows);

    rows["TEST"] = nil;
    --[[
    rows["TEST"] = {
        ["name"] = "HELLO",
        ["age"] = 10,
        ["memo"] = {
            ["blob"] = "HAHA"
        }
    }; ]]--

    rs.apply(rows, "DEMO", function (ai_k, log)
        print("COMMIT", ai_k, log);
        tprint(ai_k);
        tprint(log);
    end);
end);