-- stage.signal()

-- CASE.1: callback 함수의 user argument를 포함하는 경우
stage.waitfor("hello", function (arg, v)
  print("HELLO", arg[0], arg[1], v.msg)
end, { "arg1", "arg2" })

-- CASE.2: "process" 로 정의된 lua에서 전달되는 signal만 받고자 하는 경우
stage.waitfor("process:hello", function (arg, v)
  print("HELLO", arg[0], arg[1], v.msg)
end, { "from", "PROCESS" })

--
stage.signal("hello", { ["msg"] = "CALLBACK" })
stage.signal("process+hello", { ["msg"] = "CALLBACK" })
