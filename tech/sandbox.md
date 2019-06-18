**STAGE:플랫폼 for LUA** 은 샌드박스 정책과 독립된 환경에서 스크립트가 실행되는 정책을 가지고 있습니다.

 이는 기존의 정책이 다수의 개발자가 사용하기에 부적합한 구조이기에, 이를 성능과 안정성을 유지하면서 기존의 구조적인 특징을 유지하기 위한 방법으로 다음과 같은 방법을 선택 하였습니다.

> ### 1. coroutine.create()에서 격리되는 전역변수

  lua 스크립트의 coroutine 처리 정책은 로컬변수와 스크립트 실행코드만 분리하고 전역변수는 공유하는 방법을 사용하고 있습니다.

  그러나, 여러 개발자가 개발을 한다면 예측하지 못한 결과를 만들어 낼수도 있어 이에 대한 완전한 분리가 필요합니다.

  스크립트 코드를 보면
   ```lua
    local co = coroutine.create(function ()
        RESULT = 0
        for i=1, 10 do
            print("COROUTINE", i, HELLO)
            RESULT = i
            coroutine.yield()
        end
    end)

    for i=1, 10 do
        HELLO = "HELLO-" .. i
        coroutine.resume(co)
        print("RESULT", RESULT)
    end
   ```

   을, 표준 lua에서 실행해서

   ```console
   ken@ubuntu-vm:~/lua$ bin/lua test.lua 
    COROUTINE	1	HELLO-1
    RESULT	1
    COROUTINE	2	HELLO-2
    RESULT	2
    COROUTINE	3	HELLO-3
    RESULT	3
    COROUTINE	4	HELLO-4
    RESULT	4
    COROUTINE	5	HELLO-5
    RESULT	5
    COROUTINE	6	HELLO-6
    RESULT	6
    COROUTINE	7	HELLO-7
    RESULT	7
    COROUTINE	8	HELLO-8
    RESULT	8
    COROUTINE	9	HELLO-9
    RESULT	9
    COROUTINE	10	HELLO-10
    RESULT	10
   ken@ubuntu-vm:~/lua$ 
   ```
   결과를 얻었습니다. 전역 변수에 접근이 가능하여 변경된 값으로 표시되지만, 
   
   *STAGE:플랫폼 for LUA* 에서 실행한 결과는

   ```console
   ken@ubuntu-vm:~/lua$ docker run -it --rm -v $PWD/stage:/opt/stage datumflux/stage
   ...
    2019-06-17 16:23:00,148 [INFO ] [lua:536] SCRIPT 'start' - 'package/start.lua'
    COROUTINE	1	HELLO-1
    RESULT	nil
    COROUTINE	2	HELLO-1
    RESULT	nil
    COROUTINE	3	HELLO-1
    RESULT	nil
    COROUTINE	4	HELLO-1
    RESULT	nil
    COROUTINE	5	HELLO-1
    RESULT	nil
    COROUTINE	6	HELLO-1
    RESULT	nil
    COROUTINE	7	HELLO-1
    RESULT	nil
    COROUTINE	8	HELLO-1
    RESULT	nil
    COROUTINE	9	HELLO-1
    RESULT	nil
    COROUTINE	10	HELLO-1
    RESULT	nil   
   ...
   ken@ubuntu-vm:~/lua$ 
   ``` 
   coroutine 함수의 실행전에 생성된 변수의 초기값은 읽을 수 있으나 이후에 변경되는 값은 접근할 수 없음을 확인할 수 있습니다.
   
   필요에 따라, 전역변수의 공유가 필요한 상황이라면 **_ENV[""]** 정책을 추가하여, 다음과 같이 변경해 동일한 결과를 얻을 수 있습니다.

   ```lua
    _ENV[""] = { "RESULT", "HELLO" }

    local co = coroutine.create(function ()
        RESULT = 0
        for i=1, 10 do
            print("COROUTINE", i, HELLO)
            RESULT = i
            coroutine.yield()
        end
    end)

    for i=1, 10 do
        HELLO = "HELLO-" .. i
        coroutine.resume(co)
        print("RESULT", RESULT)
    end   
   ```
   
   기존코드의 변경없이 샌드박스 정책만 설정하는 것으로 처리할 수 있으며 다수의 개발자가 콘텐츠를 개발하는데 정확한 결과를 보장합니다.

   ```console
   ken@ubuntu-vm:~/lua$ docker run -it --rm -v $PWD/stage:/opt/stage datumflux/stage
   ...
    2019-06-17 16:26:56,268 [INFO ] [lua:536] SCRIPT 'start' - 'package/start.lua'
    COROUTINE	1	HELLO-1
    RESULT	1
    COROUTINE	2	HELLO-2
    RESULT	2
    COROUTINE	3	HELLO-3
    RESULT	3
    COROUTINE	4	HELLO-4
    RESULT	4
    COROUTINE	5	HELLO-5
    RESULT	5
    COROUTINE	6	HELLO-6
    RESULT	6
    COROUTINE	7	HELLO-7
    RESULT	7
    COROUTINE	8	HELLO-8
    RESULT	8
    COROUTINE	9	HELLO-9
    RESULT	9
    COROUTINE	10	HELLO-10
    RESULT	10
   ...
   ken@ubuntu-vm:~/lua$ 
   ``` 
   협의되지 않은 전역변수에 대한 공유는 제한되므로 변수 사용에 있어 개발의  실수를 줄이는 역할을 수행합니다.

> ### 2. [*stage.submit()*](/datumflux/stage/blob/master/docs/api/stage.md#stage-submit) 을 통한 쓰레드 분리

   대부분의 스크립트 엔진은 쓰레드를 지원하지 않습니다. 가장 큰 이유는 실행속도에 대한 제한과 전역변수에 대한 접근에 있어 오류의 가능성을 포함하고 있기 때문입니다.

   STAGE:플랫폼은 기본적으로 쓰레드를 메시지 처리의 보조역할이 아닌 메인 처리로 사용할 수 있도록 개방되어 있습니다.

   다음의 스크립트 코드는 함수를 쓰레드로 분리하여 실행하는 코드 입니다.
   ```lua
    function hello(msg)
        print("HELLO", msg)
    end 

    HELLO = "HELLO"
    print("MAIN", thread.id)
    stage.submit(0, function ()
        print("THREAD", thread.id, HELLO)
        hello("THREAD")
    end)
   ```

   기본 상태의 실행 결과는
   ```console
   ken@ubuntu-vm:~/lua$ docker run -it --rm -v $PWD/stage:/opt/stage datumflux/stage
   ...
    2019-06-17 16:47:23,001 [INFO ] [lua:536] SCRIPT 'start' - 'package/start.lua'
    MAIN	139999079790144
    THREAD	139998870099712	nil
    2019-06-17 16:47:23,003 [WARN ] [lua:2033] [string "package/start.lua"]:10: attempt to call a nil value (global 'hello')
    Stack Traceback
    ===============
    (2) Lua function 'nil' at line 10 of chunk '"package/start.lua"]'
        Local variables:
        (*temporary) = nil
        (*temporary) = string: "THREAD"
        (*temporary) = string: " (global 'hello')"
        (*temporary) = string: "attempt to call a nil value (global 'hello')"
   ...
   ken@ubuntu-vm:~/lua$ 
   ``` 
   로, 스크립트가 분리되어 실행되고 있음을 확인할 수 있습니다. 

   > TIP: 쓰레드별도 라이브러리 형태의 스크립트를 미리 로딩할 필요가 있다면 "preload/" 폴더에 저장하여 사용할 수 있습니다.

   *coroutine*의 샌드박스 정책 설정과 마찬가지로 **_ENV[""]** 정책 설정을 통해 전역변수나 함수등을 공유할 수 있습니다.

   ```lua
    _ENV[""] = { "*hello", "*HELLO" }

    function hello(msg)
        print("HELLO", msg)
    end 

    HELLO = "HELLO"
    print("MAIN", thread.id)
    stage.submit(0, function ()
        print("THREAD", thread.id, HELLO)
        hello("THREAD")
    end)
   ```

   *coroutine*과의 차이점은 공유 설정이 필요한 함수나 변수명의 정의에 **'\*'** 을 사용한다는 것입니다. 설정 이후 실행 결과를 확인 하면

   ```console
   ken@ubuntu-vm:~/lua$ docker run -it --rm -v $PWD/stage:/opt/stage datumflux/stage
   ...
    2019-06-17 16:49:24,451 [INFO ] [lua:536] SCRIPT 'start' - 'package/start.lua'
    MAIN	140706984142400
    THREAD	140706757666560	HELLO
    HELLO	THREAD
   ...
   ken@ubuntu-vm:~/lua$ 
   ``` 

   과 같이 원하는 결과를 얻을 수 있습니다.

   쓰레드 사용에 있어 개발의 일관성을 최대한 유지하기 위해 *stage.submit()* 함수를 통해 실행한다는 것과 *_ENV[""]* 정책만 추가 되었습니다.

   또한, 동기적인 실행을 통해 처리가 블록되는 현상을 제거하기 위해 모든 처리결과는 비동기로 처리됩니다.

   > #### 쓰레드에서 샌드박스 정책이 설정된 변수와 함수는 스냅샷되어 공유가 됩니다. 
   
   만약, 변수에 대한 무결성의 확인이 필요하다면 *__.변수명 = function (V)* 형태로 접근하여 처리될수 있습니다.

   ```lua
    _ENV[""] = { "*hello", "*HELLO" }

    function hello(msg)
        print("HELLO", msg)
    end 

    __.HELLO = function(V)
      return "HELLO"
    end

    print("MAIN", thread.id)
    stage.submit(0, function ()
        __.HELLO = function (HELLO)
           print("THREAD", thread.id, HELLO)
        end
        hello("THREAD")
    end)
   ```

   이와 같이 접근해 무결성을 유지할 수 있습니다.

> ### 3. [*stage.waitfor()*](/datumflux/stage/blob/master/docs/api/stage.md#stage-waitfor) 과 [*stage.signal()*](/datumflux/stage/blob/master/docs/api/stage.md#stage-signal) 을 이용한 비동기 결과 반환

  STAGE:플랫폼에서 처리 결과를 받기위해서는 *stage.waitfor()* 와 *stage.signal()* 을 사용하는 방법이 있습니다.

  스크립트의 실행방식 처럼 순서대로 실행한다면 개발에 대한 편의성은 확보할 수 있으나 성능상의 이슈는 해결하기 어렵게 됩니다. 
  
  스크립트 언어는 성능의 문제를 해결하기 위해 비동기 인터페이스를 제공하고 있으나 lua의 경우는 기본적으로 제공되는 처리가 존재하지 않아 별도로 제공됩니다. *(javascript의 경우에는 Promise를 통해 지원)*

  처리결과를 비동기로 받기 위한 방법을 쓰레드의 사용예제를 통해 확인해 보도록 하겠습니다.

   ```lua
    _ENV[""] = { "*HELLO" }

    function hello(msg)
        print("HELLO", msg)
    end 

    stage.waitfor("hello", hello)

    HELLO = "HELLO"
    print("MAIN", thread.id)
    stage.submit(0, function ()
        print("THREAD", thread.id, HELLO)
        stage.signal("hello", "THREAD")
    end)
   ```
   
   메시지를 받는 *stage.waitfor()* 와 메시지를 보내는 *stage.signal()* 로 이루어져 있습니다.

   실행 결과는 다음과 같습니다.
   ```console
   ken@ubuntu-vm:~/lua$ docker run -it --rm -v $PWD/stage:/opt/stage datumflux/stage
   ...
    2019-06-17 17:03:47,779 [INFO ] [lua:536] SCRIPT 'start' - 'package/start.lua'
    MAIN	140199838099008
    THREAD	140199620015872	HELLO
    HELLO	THREAD
   ...
   ken@ubuntu-vm:~/lua$ 
   ``` 

   사용에 있어 주의가 필요한 부분은 *stage.waitfor()* 함수가 실행되는 위치와 방법에 있습니다. 
   
   *stage.waitfor()* 함수는 메인 스크립트가 실행되는 스크립트에서 실행이 되며 메시지가 발생된 순서대로 실행하는 특징을 가지고 있습니다.

   다음의 스크립트는 *stage.waitfor()* 를 쓰레드 내에서 정의하고, 메인 스크립트에 정의된 *RESULT* 함수에 접근 여부를 확인하기 위한 예제 입니다.

   ```lua
    _ENV[""] = { "*HELLO" }

    RESULT = "MAIN"
    HELLO = "HELLO"
    print("MAIN", thread.id)
    stage.submit(0, function ()
        function hello(msg)
            print("HELLO", msg, RESULT)
        end 

        stage.waitfor("hello", hello)
        print("THREAD", thread.id, HELLO)
        stage.signal("hello", "THREAD")
    end)
   ```

   실행결과를 통해, 메인 스크립트에 정의된 *RESULT* 변수에 접근이 가능함을 확인할 수 있습니다.

   ```console
   ken@ubuntu-vm:~/lua$ docker run -it --rm -v $PWD/stage:/opt/stage datumflux/stage
   ...
    2019-06-17 17:10:15,593 [INFO ] [lua:536] SCRIPT 'start' - 'package/start.lua'
    MAIN	140687938229824
    THREAD	140687787288320	HELLO
    HELLO	THREAD	MAIN
   ...
   ken@ubuntu-vm:~/lua$ 
   ``` 

   이 처럼, STAGE:플랫폼은 간단한 접근 방식을 유지하면서 성능과 효과적인 개발을 위해 단순화된 처리를 하고 있습니다.

   > *TIP* stage.signal()을 통해 원격에서 실행중인 *STAGE:플랫폼* 과 함수와 메시지를 공유할 수 있습니다.

   > #### 업데이트를 통해 쓰레드용 결과 반환 방법이 추가

   ```lua
    _ENV[""] = { "*HELLO" }

    RESULT = "MAIN"
    HELLO = "HELLO"
    print("MAIN", thread.id)
    local to = thread.new(function ()
        print("THREAD", thread.id, HELLO)
        thread.signal("THREAD")
        return "FINISH" -- thread.single("FINISH")        
    end)

    to.waitfor(function (msg)
        print("HELLO", msg, RESULT)
    end)
   ```
   형태로 구성을 하게 됩니다. 
   
   실행 결과는
   ```console
   ken@ubuntu-vm:~/lua$ docker run -it --rm -v $PWD/stage:/opt/stage datumflux/stage
   ...
    2019-06-18 23:09:17,046 [INFO ] [lua:535] SCRIPT 'start' - 'package/start.lua'
    MAIN	140255821305408
    THREAD	140255645185792	HELLO
    HELLO	THREAD	MAIN
    HELLO	FINISH	MAIN
   ...
   ken@ubuntu-vm:~/lua$ 
   ``` 
   
   이처럼, 쓰레드의 결과를 받기위한 처리가 간략하게 단방향의 형태로만 처리가 됩니다. *(양방향 처리 구성이 가능하게 설정하는 경우 처리방식에 따라 기능을 처리하지 못하는 **DEAD-LOCK 상황** 이 발생될 수 있어 제한됩니다.)*
   
   기존의 *stage.waitfor()* 와 다르게 *to.waitfor()* 로 연결된 함수는 *thread.new()* 함수의 처리가 완료되면 제거됩니다. 

   > *thread.signal()* 과 *to.waitfor()* 함수는 *thread.new()* 로 연결된 함수에만 적용 됩니다.

