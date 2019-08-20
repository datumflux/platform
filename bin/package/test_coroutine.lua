_SANDBOX[""] = { "GLOBAL_V" }

print("SANDBOX Sample")

local co = coroutine.create(function (value1,value2)
   local tempvar3 = 10
   print("coroutine section 1", value1, value2, tempvar3)
	
   local tempvar1 = coroutine.yield(value1+1,value2+1)

   print("BEGIN SANDBOX", _SANDBOX, GLOBAL_V)
   tempvar3 = tempvar3 + value1
   print("coroutine section 2",tempvar1 ,tempvar2, tempvar3)
	
   local tempvar1, tempvar2= coroutine.yield(value1+value2, value1-value2)
   tempvar3 = tempvar3 + value1
   print("coroutine section 3",tempvar1,tempvar2, tempvar3)

   print("END SANDBOX", _SANDBOX, GLOBAL_V)
   return value2, "end"	
end)

print("main", coroutine.resume(co, 3, 2))
GLOBAL_V = "Hello"
print("main", coroutine.resume(co, 12,14))
GLOBAL_V = "World"
print("main", coroutine.resume(co, 5, 6))
print("main", coroutine.resume(co, 10, 20))
