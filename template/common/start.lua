print(process.id .. ": Initialize... STAGE[" .. process.stage .. "]")

stage.load("__init", function (f)
    f()
end)