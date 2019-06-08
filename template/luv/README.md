> ## [luv](https://github.com/luvit/luv) 인터페이스 사용

STAGE:플랫폼에 libuv 기능을 사용하기 위한 방법 방법은 다음과 같습니다.

**luv.so** 파일을 다운로드 받아 도커(docker)와 연동되는 디랙토리에 복사 합니다.

```console
$ ls stage/
luv.so     rollback/  index/
logs/      package/   start.lua
$
$ docker run -it --rm -v $PWD/stage:/opt/stage datumflux/stage
```

이후 start.lua에서 **luv** 를 로딩하여 구현을 하면 STAGE:플랫폼과 같이 실행이 됩니다.

단, **luv** 를 정상적으로 사용하기 위해서는 uv.run() 함수의 실행 방법을 다음과 같이 변경해야 합니다.<br>
만약, 기존의 방법으로 실행을 하게 되면 STAGE:플랫폼의 FAIL-OVER정책에 따라 프로세서가 재 시작 됩니다.

```lua
return function ()
  require('luv').run("nowait")
end
```

최종 STAGE:플랫폼과의 연동이 완료된 처리 방식 입니다.

```lua
local uv = require('luv')

-- Create a handle to a uv_timer_t
local timer = uv.new_timer()

-- This will wait 1000ms and then continue inside the callback
timer:start(1000, 0, function ()
  -- timer here is the value we passed in before from new_timer.

  print ("Awake!")

  -- You must always close your uv handles or you'll leak memory
  -- We can't depend on the GC since it doesn't know enough about libuv.
  timer:close()
end)

print("Sleeping");

-- uv.run will block and wait for all events to run.
-- When there are no longer any active handles, it will return
return function ()
  require('luv').run("nowait")
end
```