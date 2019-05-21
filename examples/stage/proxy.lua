-- 지정된 변수의 값이 변경되는 경우에 대한 처리
local T = stage.proxy({}, {
    ["count"] = { 
        10, 20, 30, function (o, m, k, v)
            print("COUNT", o, o[k], m, k, v);
            for i, t in ipairs(m) do
                print("", i, t);
            end
            print("END");
        end
    },
    ["hello"] = function (o, m, k, v)
        print("HELLO", o, o[k], m, k, v)
    end
})(function (v)
    print("==> INDEX", v.count);

    v.count = 9;
    v.count = 19;

    v.hello = "hello";
end);

for k, v in pairs(T) do
    print("==>", k, v);
end
--[[
-- 변수가 추가되는 경우에 대해 callback 처리
--
local T = stage.proxy({}, function (o, m, k, v)
    print("HELLO", o, o[k], m, k, v)
end)(function (v)
    v[#v + 1] = 10;
    for i, p in ipairs(v) do
        print("==>", i, p);
    end
end);
]]
