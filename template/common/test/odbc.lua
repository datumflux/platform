print("START");

odbc.new({
    ["DRIVER"] = "MySQL ODBC 8.0 ANSI Driver",
    ["SERVER"] = "10.211.55.7",
    ["DATABASE"] = "DF_DEVEL",
    ["USER"] = "datumflux",
    ["PASSWORD"] = "....",
    [".executeTimeout"] = 1000,
    [".queryDriver"] = "mysql"
}, function (adp)
    local rs = adp.execute("SELECT 1 as sum");
    local rows = rs.fetch();
    for k, v in pairs(rows) do
        print("SELECT", k, v);
    end
    return adp;
end)
