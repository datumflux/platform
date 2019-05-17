# STAGE for LUA

>## 순서

* [시작](#intro)
* 네임 스페이스
  * [stage](#stage-ns)
    * [stage.once](#stage-once)
    * [stage.load](#stage_load)
    * [stage.addtask](#stage-addtick)
    * [stage.submit](#stage-submit)
    * [stage.signal](#stage-signal)
    * [stage.waitfor](#stage-waitfor)
    * [stage.proxy](#stage-proxy)
    * [stage.v](#stage-v)
  * [log4cxx](#log4cxx-ns)
  * [broker](#broker-ns)
    * [broker.ready](#broker-ready)
    * [broker.join](#broker-join)
    * [broker.close](#broker-close)
    * [broker.signal](#broker-signal)
    * [broker.f](#broker-f)
    * [broker.ntoa](#broker-ntoa)
    * [broker.aton](#broker-aton)
    * [broker.ifaddr](#broker-ifaddr)<p>
    * [socket.commit](#socket-commit)
    * [socket.close](#socket-close)
  * [odbc](#odbc-ns)
    * [odbc.new](#odbc-new)
    * [odbc.cache](#odbc-cache)
    * [odbc.purge](#odbc-purge)<p>
    * [adapter.execute](#adapter-execute)
    * [adapter.apply](#adapter-apply)
    * [adapter.begin](#adapter-begin)
    * [adapter.commit](#adapter-commit)
    * [adapter.rollback](#adapter-rollback)
    * [adapter.close](#adapter-close)<p>
    * [resultset.fetch](#resultset-fetch)
    * [resultset.erase](#resultset-erase)
    * [resultset.apply](#resultset-apply)

* 프로퍼티
  * [process](#process-property)
  * [thread](#thread-property)

* [LUA 메뉴얼](http://www.lua.org/manual/5.3/)

>## 시작
 
  스크립트를 사용해 콘텐츠를 구현하는데 유용한 API를 설명하기 위한 문서로, 백엔드 플랫폼의 구현에 필요한 지식을 간결하게 설명하고 콘텐츠를 직접 제작하는데 사용할수 있도록 기술하였습니다.
  
  stage 플랫폼은 lua 스크립트를 사용하여 콘텐츠를 구현하도록 하고 있습니다. lua는 문법이 간결하여 프로그래밍 지식에 따른 결과에 차이가 크게 않고 성능과 안정성을 유지하는데 효과적인 특성을 가지고 있습니다. 
 
  stage에 대한 접근 방법을 이해 하기 위해서는 다음의 특징을 먼저 확인할 필요가 있습니다.

  > 특징
  1. **분산 실행** - 스크립트는 다른 프로세서 또는 다른 장비에서 실행될수 있습니다.
  1. **샌드 박스** - 스크립트는 쓰레드별로 독립된 공간에서 실행 됩니다.
  1. **선택적 동시 실행** - 동시 실행이 필요한 경우 함수를 분리하여 실행할수 있습니다.
  1. **비동기 실행** - 메시지에 대한 콜백 함수를 등록하고, 외부에서 메시지가 발생되면 등록된 함수가 실행되는 방식으로 처리 결과는 비동기 요청 프로세서(서버)로 반환됩니다.
     > 필요에 따라 결과를 반환 받지 않을수 있습니다.

  을 가지고 있음을 인지해야 합니다. <p>
  
  또한, 백엔드 플렛폼의 특징을 그대로 가지고 콘텐츠 구현을 하기 위해서 다음과 같은 제한도 가지고 있습니다. -- 이러한 제한은, stage의 특성을 유지하면서 무결성과 안정성을 유지하기 위함입니다.

  > 제한
  1. 콜백 함수는 **샌드 박스**에서 실행되므로 함수 외부에 정의된 다른 함수 및 변수에 접근 할수 없습니다.
     > 만약, 공유 변수 공간이 필요하면 stage.v() 함수를 사용할 수 있습니다.
  1. 메인 스크립트를 제외한 스크립트는 동시에 사용될 수 있어 중복 실행이 가능합니다.
     > 실행을 제한하기 위해서는 stage.once() 함수를, 동시 실행을 위해서는 stage.submit()을 사용할수 있습니다.
  1. **분산 실행**은 비동기로 요청이 되며 결과가 회신되지 않을수도 있습니다.
     > stage.signal()을 통해 메시지 처리를 요청할수 있습니다. 
  1. 서버간 **비동기 실행**을 위해 등록되는 콜백 함수는 **동시 실행**되지 않습니다.
     > stage.addtask()도 동시 실행되지 않습니다.
  1. **동기 실행**에 대한 명령이 수행되면 처리가 종료될때 까지 다른 명령을 실행하지 못하도록 **샌드 박스**에 격리됩니다.

  제한된 기능은 **쓰레드** 에서 효과적인 운영을 위해 설정한 기능들로 콘텐츠 구현에서 유용하게 사용될수 있습니다.
  
  > 참고
  * **쓰레드** - 스크립트를 동시에 실행하기 위해 사용하는 처리 기술로, CPU의 수만큼 동시 실행이 가능합니다.
  * **프로세서** - 실행되는 프로그램
  * **샌드 박스** - 외부와 분리되어 보호되는 실행 공간으로 안정성 유지를 위해 설정
  * **동기 실행** - 명령의 실행이 완료될때 까지 기다리는 형태의 실행방식
  

  ### **시작하기**

  1. stage의 환경 설정

     |이름|자료형|역할|
     |:==:|:====:|:===|
     |#services|object|프로세서간 공유가 필요한 네트워크 주소|
     |#license|object|라이센스 설정 정보|
     |#router|array|라우팅 정보 설정|
     |#cluster|array|서버간 클러스터링 연결 정보|
     |#threads|array|쓰레드 설정 정보|
     |#startup|array|사용하고자 하는 stage plugin|

     * #services <br>
       리눅스 환경에서 구동되는 플랫폼은 외부 접속을 허용하는 네트워크 포트에 대해 여러 프로세서가 연결을 나누어 받을 수 있는 방법을 제공합니다. #PREFORK

       예를 들어, 다음과 같은 설정을 통해

       ```json
         "#startup":[
           ["=lua+stage"],
           ["=lua+stage"],
           ["=lua+stage"]
         ]
       ```
       으로 구성된 설정에서

       ```lua
         broker.ready("tcp://0.0.0.0:8081?...", function (socket, addr)
         ...
       ```
       연결을 받도록 지정해 두었다면, 8081 포트로 연결되는 접속을 *["=lua+stage"]* 로 설정된 프로세서가 나누어 받을 수 있습니다.

     * #router <br>
       메시지 교환에 필요한 값을 설정합니다<br>
       1. 첫번째 항목은 메시지 수신 가능상태에 대한 동기화 시간을 설정합니다.
          > *설정시간* 은 msec로 설정이 됩니다.<br>

          > 서버에 연결된 stage가 *설정시간* 동안 동기화 되지 않으면 stage처리 권한이 제거됩니다. -- 제거되는 경우, "#cluster"에서 *"=서버주소:포트번호"* 로 연결된 다른 서버로 전달됩니다. 

     * #cluster <br>
       클러스터 설정은 3가지 설정을 가질수 있습니다.<br>
       1. 다른 서버에서 정보를 받을수 있도록 설정 <br>
          - 다른 위치에 존재하는 서버가 보낸 정보를 받을 수 있습니다.
          > *포트번호* 를 지정합니다.<br>
          > 다음 설정에 존재하는 *"포트번호"* 는 해당 값을 의미합니다.

       1. 연결할 상대방 서버 주소를 지정합니다 <br>
          - 다른 서버로 정보를 보낼 수 있습니다.
          > *"=서버주소:포트번호"* 를 지정합니다.

       1. 백업 클러스터 연결을 구성합니다.<br>
          - 현재 서버에 stage_id가 존재하지 않거나 문제가 있다면 연결된 서버에 처리 요청을 전송합니다.
          > *"=서버주소:포트번호"* 를 지정합니다. <br>
          > 해당 설정된 서버는 stage_id가 존재하지 않는 경우에만 전달 됩니다.

     * #startup <br>
       활성화 시킬 stage를 지정합니다.

       기본으로 제공되는 stage는

       * =lua
       * =index
       * =curl

       입니다. *SDK API를 사용해 커스터마이징된 stage 추가 가능*<p>
       외부 요청에 대한 처리를 하기위해서는 stage_id를 지정해야 하는데, id는 stage명 뒤에 추가되는 *"+이름"* 형태로 지정을 합니다. 

       1. 항목별로 독립된 프로세서로 구성됩니다.
          > 프로세서에 문제가 발생되어 종료되는 경우, 자동 재 시작됩니다.
       1. 프로세서에 정의되는 stage_id는 중복되지 않도록 주의 하셔야 합니다. (처리 하지 못하는 데이터가 전달될 수 있습니다)
          > 예) ["=lua+stage", "=index+stage"]
       
  ```sh
#!/bin/sh

exec ./single -c - $* << EOF
{
    "#services": {
        "tcp":["0.0.0.0:8081"]
    },
    "#license": { "DATUMFLUX CORP.": ["xxxxxxxxxxxxxxxxxxxxx"] },
    "#router": [ 1500 ],
    "#cluster": [ 18081, "127.0.0.1:28081"],
    "#threads": [ 10, 10000 ],
    "#startup":[ 
        ["=lua+stage", "=index+rank", "=curl+agent"]
    ],

    "=curl+agent": {
        "user-agent": "STAGE Agent/1.0"
    },

    "=lua+stage": {
        "%preload": "preload",
        "%package": [ "devel", "v1" ],
        "%odbc":  {
            "DF_DEVEL": {
                "DRIVER": "MySQL ODBC 8.0 Unicode Driver",
                "SERVER": "...",
                "DATABASE": "...",
                "USER": "...",
                "PASSWORD": "..."
            }
        },
        "=lua+stage": "start"
    },

    "=index+rank": "score/"
}
EOF
  ```

  2. 메인 스크립트 작성 

  ```lua
-- v1/start.lua
-- 외부 연결을 준비 (필요에 따라 수정할수 있습니다.)
local sp = broker.ready("tcp://0.0.0.0:8081?packet=4k,10k", function (socket, addr)

    -- 데이터를 받으면 호출되는 함수를 등록한다.
    return function (socket, data, addr)

        -- 처리를 쓰레드로 실행하도록 전환 한다.
        stage.submit(socket.id, function (socket_id, data, addr))

            local socket = broker.f(socket_id)
            stage.load("packet+" .. data.message, function (f)
                local v = f(socket, data)
                if v ~= nil then
                    socket.commit(v, addr);
                end
            end)

        end, socket.id, data, addr);
    end
end);

sp.expire = 3000; -- 3초 동안 데이터 발생이 없으면 닫기
sp.close = function (socket)

    -- 클라이언트 접속이 해제될때 호출된다.
    stage.load("packet+__close", function (f)
        f(socket);
    end)
end


log4cxx.out("READY")
return function () -- IDLE 함수 등록
    return 1000; -- 다음 호출 시간 예약 (0 또는 반환이 없는 경우 중지)
end;
  ```
  3. 패킷 스크립트 작성 

  ```lua
-- v1/packet.lua
--

return {
    ["login"] = function (socket, data)
    end
}
  ```

>## <a id="stage-ns"></a> stage
>
> stage는 플랫폼의 기본 기능에 접근할수 있도록 구성되어 있습니다.

>#### <a id="stage-once"></a> stage.once(s, function ())

  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> s 가 실행된 적이 없는 경우에만 function()을 실행
  * **입력**
    * s - 구분하고자 하는 실행 id
    * function () - 실행하고자 하는 함수 (실행함수가 지정되지 않은 경우 s 정보를 제거)
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>
    stage 플랫폼에서 구동되는 스크립트 중에, 메인 스크립트를 제외한 모든 스크립트는 동시에 사용될수 있습니다. 이러한 경우 동시 실행되면 안되는 경우가 존재할수 있습니다. 이러한 경우 stage.once()를 사용하여 실행을 제한할수 있습니다.
  * **예제**
    ```lua
       stage.once("#1", function ()
          -- 처음 실행인 경우에만 호출
          broker.ready("tcp://...", function (socket, addr)
             ...
          end)

       end)
    ```

    ```lua
       stage.once("#1", nil) -- 실행 여부 제거
    ```
  
>#### <a id="stage-load"></a> stage.load(s \[, function (v)])

  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> s 스크립트를 로딩
  * **입력**
    * s - 로딩할 스크립트 이름 (파일명+아이디)
    * function (v) - 로딩이 완료되면 실행되는 함수
  * **반환** 
    * function (v)가 사용 여부
      * Y - 로딩된 스크립트
      * N - function (v)의 반환 값
  * **설명**<br>
    스크립트 파일을 읽어 실행 가능한 상태로 준비하게 됩니다. 만약, 이미 로딩된 적이 있다면 캐싱되어 있는 정보를 반환하게 됩니다. (단, 파일의 크기 또는 수정날짜가 변경된 경우에는 다시 로딩합니다.)

    *로딩할 스크립트 이름*은 "파일명\[+아이디]"의 형태로 사용이 되며 해당 의미는 다음과 같습니다.

    ```lua
      # source.lua
      print("HELLO STAGE")
      return {
          ["id"] = function (v)
             print("LOADING", v)
          end
      }
    ```

    * "source"<span style="white-space: pre;">&#9;&#9;</span>return에 정의된 {...} 반환
    * "source+id"<span style="white-space: pre;">&#9;</span>\["id"]에 정의된 function()을 반환

  * **예제**
    ```lua
       stage.load("source+id")("SOURCE");
    ```

    ```lua
       stage.load("source+id", function (v)
          v("SOURCE")
       end)
    ```

    는 동일한 결과를 보여 줍니다.

>#### <a id="stage-addtask"></a> stage.addtask(i, function (v...)\[, v...])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> *i* msec 이후에 function()을 실행합니다.
  * **입력**
    * i - 함수를 실행할 지연 시간 (msec)
    * function () - 실행하고자 하는 함수
    * v... - function()에 전달하고자 하는 값
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> task 정보
  * **설명**<br>

    일정 시간이 지난 이후 동작해야 하는 함수를 만들거나, 처리를 일시적으로 지연시켜야 하는경우 사용될수 있는 함수입니다.
    
    * function ()의 반환값은 다시 함수를 실행하기 위한 시간을 의미합니다.
      > 값을 반환하지 않거나 0 이하의 값을 반환하면 더 이상 실행되지 않습니다.

    * 실행 취소
        ```lua
            local task = stage.addtask(0, function (v)
                ...
            end, ...)

            stage.addtask(task, nil)
        ```

  * **예제**
    ```lua
       stage.addtask(0, function (v)
          print("RANDOM", v[math.random(0, 2)])
          return 1000; --- 1초 간격으로 재 실행
       end, { 10, 20, 30 })
    ```

  * 참고
    * **msec** - msec는 1/1000초를 의미하며 1000 msec 가 1초 입니다.

>#### <a id="stage-submit"></a> stage.submit(i, function (v...)\[, v...])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> *i* 고유 값이 중복되지 않도록 function()을 실행합니다.
  * **입력**
    * i - 고유 값 (0 - 중복실행 허용)
    * function () - 실행하고자 하는 함수
    * v... - function()에 전달하고자 하는 값
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>

    비슷한 기능을 하는 stage.addtask()와 비교하면 다음과 같은 차이가 있습니다.

    |&nbsp;|stage.addtask()|stage.submit()|
    |:=:|:=:|:=:|
    |*i*|지연시간|고유 값|
    |중복 설정|허용 안됨|허용됨|
    |실행 취소|O|X|

    두 기능의 차이는 **동시 실행**의 여부입니다. addtask()는 설정된 함수를 순서대로 실행하는 방식이라면 submit()은 실행 가능한 상태가 되면 **동시 실행**을 합니다.
    
    단, *i*의 값이 0이 아닌 경우에는 동일한 *i*값을 가지는 콜백의 동시 실행을 허용하지 않습니다.
    > 이러한 처리는 중복 처리를 허용하면 안되는 실행(데이터를 저장 하거나, 소켓별 처리)에 사용됩니다.

    * function ()의 반환값은 다시 함수를 실행하기 위한 시간을 의미합니다.
      > 값을 반환하지 않거나 0 이하의 값을 반환하면 더 이상 실행되지 않습니다.

  * **예제**
    ```lua
       stage.submit(0, function (v)
          print("RANDOM", v[math.random(0, 2)])
          return 1000; --- 1초 간격으로 재 실행
       end, { 10, 20, 30 })
    ```

>#### <a id="stage-signal"></a> stage.signal(s1, v \[, s2])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 메시지 v를 *s1* 에게 전달할때, s2에 위치한 프로세서(서버)로 메시지를 전송합니다.
  * **입력**
    * s1 - 메시지 ID
    * v - 메시지
    * s2 - *(생략 가능)* stage.aton() 또는 전달받은 프로세서(서버)의 정보
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 처리 결과
  * **설명**<br>
     해당 함수는 메시지를 전달하기 위한 함수로 서버간의 이동을 포함하여 폭 넓게 사용됩니다.

     예를 들어,

     * 내부에 정의된 함수를 실행하거나
     * 다른 서버에 실행 명령을 전송
     
     의 용도로 사용하여 메시지의 분산 처리가 가능합니다.

     * *s1* 이 nil로 설정되면 waitfor()에 대한 응답을 전달 합니다.
       > 만약, waitfor()에 등록된 함수가 아닌 경우 오류로 처리됩니다.

     * *s1* 의 설정은 *target_stage_id* 를 설정하여 특정 영역에 전달할수 있습니다.
        > *target_stage_id*가 자신인 경우 생략 가능합니다.<br>
        > *":메시지ID"* 로 사용되는 경우 외부 라우팅을 거치지 않고 내부 전달 메시지로 처리 됩니다. (해당 경우 동기처리 방식으로 메시지를 처리합니다.) <br>

           target_stage_id:메시지ID

        형태로 지정 됩니다.

        ```lua
        stage.waitfor("local_callback", function (v)
           print("STEP #2 ", v.msg)
        end)

        ...
        print("STEP #1")
        stage.signal(":local_callback", {
          ["msg"] = "HELLO"
        })
        print("STEP #3")
        ```

     * 요청하는 stage_id를 포함하여 전달하고자 하는 경우에는 *+* 를 사용해 *s1* 을 구성할수 있습니다.
     
          target_stage_id+메시지ID

       이런 경우, stage.waitfor()에 동일 메시지ID라도 데이터를 요청한 stage 별로 분리 하여 처리할수 있습니다.

        ```lua
        stage.waitfor("stage:callback", function (v)
           print("STAGE ", v.msg)
        end)

        stage.waitfor("process:callback", function (v)
           print("PROCESS ", v.msg)
        end)

        -- stage에세 전송 합니다.
        stage.signal("stage+callback", {
          ["msg"] = "HELLO"
        })

        ```         

     * 메시지를 모든 stage에 보낸다거나, 동일 서버에 존재하는 stage에만 보내는 등의 처리를 하고자 할때 메시지ID에 다음과 같은 키워드를 추가하면 됩니다.
        > **기본 정책**은, 동일한 stage를 가지고 있는 서비스가 있다면 *순차적으로 사용*되도록 처리가 되어 있습니다.

        > 예를 들어, A라는 이름의 stage를 A-1, A-2, A-3라고 가정하면, *기본 정책* 으로는 처음에 A-1에 메시지를 전달하고 다음에는 A-2에 전달, 그리고 A-3 이후에 다시 A-1으로 보내면서 순서대로 메시지를 순환처리합니다. 
        
        > 만약, A-2에 부하가 많아 지연이 되면 메시지 처리 순서에서 A-2를 건너뛰고 처리될수도 있습니다.

        1. 동일한 stage에 모두 전달
           > 유일하게 동일 이름을 가지고 있는 모든 stage에 메시지를 전달합니다.

           *\*target_stage_id:메시지ID* 또는 *\*메시지ID*

        1. 동일 프로세서 그룹에 있는 stage를 순차적으로 사용

           *\=target_stage_id:메시지ID* 또는 *\=메시지ID*

        1. 다른 네트워크(서버)에 있는 stage를 순차적으로 사용

           *\+target_stage_id:메시지ID* 또는 *\+메시지ID*

     * 또 다른 경우로, 메시지의 처리를 순서대로 처리를 진행해야 경우에는
       > *명령ID* 는 고유한 값으로, *명령ID* 가 같은 경우에는 동일한 순서를 요청하는 처리로 인식합니다.<br>

           >명령ID.반환_메시지ID

       > *반환_메시지ID* 를 전달 받아 시작이 된 경우 동일한 *>명령ID.\** 를 재 전송하여 처리 시간을 연장 할수 있습니다.

        ```lua
        stage.waitfor("lock_callback", function (v)
           print("CALLBACK: ", v.msg)
           stage.signal("<LOCK.*", nil)
        end)

        ...
        stage.signal(">LOCK.lock_callback", {
          ["msg"] = "HELLO"
        })
        ```

       를 *s1*으로 사용하고, 처리가 가능한 순서가 되면 *반환_메시지ID* 가 호출되어 처리를 진행할수 있습니다. 
       
       처리가 완료된 이후에는 

           <명령ID.*

       를 *s1*에 설정하여 전송하여 사용이 완료되었음을 알려 주어야 합니다. 그러지 않는 경우 타임아웃(100 msec)이후 자동 해제 됩니다.

       > **주)** 해당 처리는 *#startup* 에 연결된 stage만 영향을 받습니다.

       * TIP
       ```lua
         stage.waitfor("callback", function (v)
           v.f(v.arg);
         end)

         stage.signal("callback", {
           ["f"] = function (v)
             print("CALLBACK", v)
           end,
           ["arg"] = "HELLO"
         })
       ```
       와 같이 함수를 원격으로 실행 할수도 있습니다. 즉, 처리를 위해 원격에 함수를 구현할 필요 없이 필요에 따라 실행 함수를 전송해 처리를 할수 있습니다.

     *stage_id* 의 설정은
     ```json
        "#startup":[ 
          ["=lua+stage"],
          ["=lua+process"],
          ["=index+rank"],
          ["=curl+agent"]
        ],
     ```
     처럼, stage 이후에 (**+** 문자) 지정된 문자열을 나타냅니다.<br>
     만약, 지정되지 않으면 다른 stage에서 메시지를 보낼 수 없습니다. (단, 요청에 대한 응답은 제외 입니다)

     > *["=lua+stage"]* 와 *["=lua+process"]* 는 별도의 프로세서로 분리되어 실행됩니다.

     > *["=lua+stage", "=index+rank"]* 라고 정의되면 하나의 프로세서에 두개의 stage가 실행됩니다.

     * index로 메시지 전달
       > index는 데이터를 실시간으로 처리하는 기능을 합니다.

       요청은 "명령:색인파일/TIER" 형태로 요청을 합니다.

       *TIER* 가 생략되면 1이 기본으로 선택됩니다.

        ```lua
        stage.signal("rank:callback", { 
            --[[ ["set:index"] = {
                ["KR0001"] = {
                    ["i"] = 10,
                    ["v"] = "TEST1"
                },
                ["KR0002"] = {
                    ["i"] = 100,
                    ["v"] = "TEST2"
                },
                ["KR0003"] = {
                    ["i"] = 200,
                    ["v"] = "TEST3"
                },
            }, ]]--
            ["get:index"] = { "KR0001", "KR0002" },
            ["range:index"] = { 100, 100 },
            -- ["range:index"] = { "KR0001", 1, 100 },
            ["forEach:index"] = { 1, 100 },
        });
        ```

       * 데이터 등록

          1. 요청
          > 이미 존재하는 상태에서 데이터를 설정하면, 기존 데이터를 갱신합니다.
          ```lua
          "set": {
            "ID": {       -- 고유값
              "v": VALUE  -- 보관이 필요한 데이터
              "i": score, -- 선택된 score에 설정
              "I": {      -- "i"를 사용하지 않고, 각 slot에 직접 설정
                1: score,
                2: score,
                3: score,
                4: score
              }
            },
            ...
          }
          ```
          
          2. 반환
          ```lua
          "set": {
            "ID": DISTANCE -- 0인 경우 score의 변경 없음
          }

       * 데이터 삭제
         1. 요청
          ```lua
          "del": [ ID, ...]
          ```

         2. 반환<br>
           없음

       * 데이터 조회
         1. 요청
          ```lua
          "get": [ ID, ...]
          ```

         2. 반환<br>
          ```lua
          "get": {
            ID: {
              "v": VALUE,
              "I": {
                1: {
                  "i" SCORE,
                  "d": DISTANCE
                },
                ...
              }
            }
          }
          ```

       * 범위 조회
         1. 요청
          ```lua
          "range": [ SCORE, COUNT]  -- SCORE부터 COUNT만큼 데이터를 얻는다.
          "range": [ ID, DELTA, COUNT] -- ID의 SCORE를 기준으로 (X - DELTA)부터 COUNT만큼 데이터를 얻는다. (COUNT가 정의되지 않는 경우 ID를 중간으로 하여 DELTA만큼 얻는다.)

          "forEach": [ START, COUNT ] -- START순위부터 얻는다.
          ```

         2. 반환<br>
          ```lua
          "range": [
            {
              "k": ID,
              "v": VALUE,
              "i": SCORE,
              "d": DISTANCE
            },
            ...
          ]
          ```

          ```lua
          "forEach": [
            {
              "k": ID,
              "v": VALUE,
              "i": SCORE,
              "d": DISTANCE
            },
            ...
          ]
          ```

     * curl로 메시지 전달

        HTTP(S)를 통해 외부와 연동이 필요한 경우 사용할 수 있습니다.

        기본적인 사용법은 다음과 같습니다.

        ```lua
        stage.signal("agent:callback", {
            ["url"] = "http://www.naver.com"
        });
        ```

        1. 요청

          |컬럼명|자료형|설명|
          |:====:|:====:|:===|
          |url|string|요청하는 URL|
          |header|object| { NAME = VALUE } 형태로 헤더 설정|
          |data|string, object| POST로 전송할 데이터|
          |timeout|number|처리 제한 시간|
          |extra|object|결과 전송시 받을 데이터|

        2. 반환
          |컬럼명|자료형|설명|
          |:====:|:====:|:===|
          |url|string|요청된 URL|
          |redirect_url|string|리다이렉트된 URL|
          |effective_url|string|처리된 URL|
          |status|number|상태 값 (200 - OK)|
          |cookie|array|전송된 쿠키값 리스트|
          |extra|object|결과 전송시 받을 데이터|
          |data|object|전달 받은 데이터|

          * data 정보
            |컬럼명|자료형|설명|
            |:====:|:====:|:===|
            |type|string|데이터 형태|
            |length|number|data의 길이|
            |download_speed|number|다운로드 속도|
            |content|string|데이터|
            |charset|string|데이터의 Character Set|
            |result|number|처리 결과|
            |message|string|처리 결과|
            |time|number|처리된 경과 시간|

  * **예제**
    ```lua
       ...
       stage.waitfor("message", function (_self, v)
          print("MESSAGE", v)
       end, {...})
       ...
       stage.signal("message", "HELLO")
    ```

  * 참고
    * [stage.waitfor](#stage-waitfor)


>#### <a id="stage-waitfor"></a> stage.waitfor(\[s,] function (v..., data...) \[, v...])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 메시지를 *s*를 처리 할수 있는 함수를 등록합니다.
  * **입력**
    * s - *(생략 가능: 생략시 자동 생성)* 메시지ID
    * function() - 실행 함수
    * v... - *(생략 가능)* function()에 전달할 값
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 메시지 ID
  * **설명**<br>
    stage.signal()로 전달되는 메시지를 처리할수 있는 함수를 등록합니다. 

    만약, *s* 가 생략되는 경우 메시지ID가 자동으로 생성됩니다. 자동 생성은 고정된 형태의 값이 아니라 임시적으로 사용 후 처리 함수를 제거하기 위한 용도로 사용됩니다.

    설정되는 메시지ID(*s*)는 그룹 구분자를 포함하여 설정할수 있습니다. 그룹의 구분자는

    1. 메시지ID를 그룹으로 구분하는 *"."*
    1. 메시지ID를 발생시킨 stage 그룹을 구분하는 *":"*

    를 포함하여 설정할수 있습니다. 
    
    설정되는 실행함수 내에서는 *_WAITFOR* 로 정의되는 전역 변수를 가지고 있습니다. 해당 변수에는 메시지의 발생 정보를 포함하고 있습니다.

       |순서|자료형|설명|
       |:==:|:====:|:===|
       |0|string|메시지ID|
       |1|object|메시지를 발생시킨 서버 정보|
       |2|string|메시지를 반환 받을 stage|

       > *_WAITFOR[2]* 가 nil 인 경우는 메시지의 반환이 필요없는 요청입니다.
    * **TIP**
      
      설정되는 메시지ID(*s*)는 발생된 메시지를 처리하는 순서를 가지고 있습니다. 해당되는 처리규칙이 발견되면 더 이상 확인하지 않습니다.

      1. 메시지ID
      1. "*.*" 로 구분되어 등록된 그룹
      1. "*:*" 로 구분되는 stage 그룹 
      > stage 그룹 문자열에는 포함된 "*.*" 구분자는 무시됩니다.

      예를 들어,<br> 
      발생된 메시지ID가 *"stage:group.msg.callback"* 이라고 가정하면, 

      1. *"stage:group.msg.callback"*
      1. *"stage:group.msg."*
      1. *"stage:group."*
      1. *"stage:"*

      순서로 확인을 하게됩니다.

    * 등록 취소
        ```lua
            local task = stage.waitfor(function (v)
                ...
            end, ...)

            stage.waitfor(task, nil)
        ```

  * **예제**
    ```lua
       ...
       stage.waitfor("message", function (_self, v)
          print("MESSAGE", v)
       end, {...})
       ...
       stage.signal("message", "HELLO")
    ```

  * 참고
    * [stage.signal](#stage-signal)

>### <a id="stage-proxy"></a> stage.proxy(t1, t2 \[, function (v)])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 객체 t1에 대한 변화를 관리합니다.
  * **입력**
    * t1 - 객체
    * t2 - 객체 *t1*의 설정 변화에 대한 값을 지정
    * function (v) - *(생략가능)* *t2* 설정이된 *t1*객체를 처리하는 함수 
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> t1 객체
  * **설명**<br>
    예를 들어, "count 변수가 10, 20, 30 의 경계값을 지날때 마다 특정 처리를 수행하도록 콘텐츠를 구성해야 하는 상황이 있다면 어떻게 해야 하는가?" 에 대한 답을 수행하는 함수입니다.

    * t2의 설정 형식

       * 처리 함수(*F*, *F1*, *F2*) - function (t1, edge_values, column_name, new_value)

       |조건|설정|
       |:==:|:==:|
       |값의 변경에 대한 처리|컬럼명 = *F* |
       |특정 조건에 대한 설정|컬럼명 = { V1, V2, ..., *F1*, N1, N2, ..., *F2* }|

       의 형식을 사용합니다.<p>
       *특정 조건에 대한 설정*의 경우에 예를 들어 컬럼명을 A라고 하고 V1, V2, N1, N2의 값이  10, 20, 30, 40 이라고 정이되어 있을때, A의 값이 34로 설정되는 조건을 보면
    
       1. stage.proxy()의 호출 시점에 t1에 A가 12라고 정의되어 있다면
          > *F1*({20, 30}), *F2*({40})
       1. stage.proxy()의 호출 시점에 t1에 A가 정의되어 있지 않거나 0인 경우라면
          > *F1*({10, 20, 30}), *F2*({40})

       의 형태로 사용이 됩니다. 참고로, *F* 함수 내에서 t1의 값을 변경하는 경우에는 값 변경에 대한 설정이 되어 있어도 처리되지 않습니다.

  * **예제**
    ```lua
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
            v.hello = "hello";
        end);
    ```

>#### <a id="stage-v"></a> stage.v(s \[, v | function (v)])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> *s*로 정의되는 값 *v*를 관리합니다.
  * **입력**
    * s - *v*의 이름
    * v - *(생략 가능)* 관리하는 값
    * function (v) - *(생략 가능)* *s*에 값을 처리하는 함수
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> *s*의 값
  * **설명**<br>
    
    stage.v()를 통해 관리되는 함수는 **샌드 박스**의 유무와 상관없이 접근할수 있습니다.

    *v* 또는 *function (v)* 가 정의되지 않은 경우에는 *s*의 값을 반환합니다.
    *v* 가 지정된 경우에는 
       * *v* 가 nil인 경우에는 *s*가 삭제 됩니다.
       * 그 외의 경우에는 *s*에 설정된 값이 지정됩니다.

    *function (v)* 은 기존 값이 v로 전달이 되며 함수의 반환값으로 변경됩니다.

    * *function (v)* 의 반환값은 무조건 *s*의 값으로 변경 됩니다.(nil 포함)
    * 값 삭제
        ```lua
            stage.v(name, nil)
        ```

  * **예제**
    ```lua
       ...
       stage.v("name", function (v)
          ...
          return v + "_HELLO";
       end);
    ```

>## <a id="log4cxx-ns"></a> log4cxx.*level*(s)
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 로그를 출력합니다.
  * **입력**
    * s - 로그 메시지
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>

    로그 메시지 *s*에 다음과 같은 문장이 포함되는 경우 변경되어 출력합니다.

    | 문장 | 변경되는 값 |
    |:======|==|
    | %l  |**%F [%M:%L]** |
    | %F  |  파일 이름 |
    | %M  |  함수 이름 |
    | %L   | 위치 (라인위치) |
    | %{*ENV*}&nbsp; |   환경변수 |
    | %{=log4cxx:level}&nbsp; | 출력 정보 변경  |

    *%{=log4cxx:level}* 을 사용하여, 출력시 로그 레벨을 변경할수 있습니다.
    기본적으로 제공되는 로그 레벨은 다음과 같습니다. (log4cxx.out의 기본 로그 레벨은 info와 동일합니다)

    | 로그 레벨 | 설명 |
    |======|======|
    | info | 정보 (out)|
    | warn | 경고 |
    | error | 오류 |
    | trace | 추적 |

    로그 레벨명을 기준으로 log4cxx.**level**(s) 함수가 정의되어 있으며, 해당 로그 레벨에 적합한 함수를 사용하는 방법과 로그 출력 전에 *%{=log4cxx:level}* 를 지정하여 변경하는 방법이 있습니다. 필요에 따라서 사용할 수 있습니다.

    >##### log4cxx.out(s)
    >##### log4cxx.info(s)
    >##### log4cxx.warn(s)
    >##### log4cxx.error(s)
    >##### log4cxx.trace(s)

  * **예제**
    ```lua
    -- 해당 로그를 bot의 그룹에 warn 레벨로 출력하라고 설정  
    log4cxx.out("%{PWD} %l - " .. msg, "%{=bot:warn}");
    ```

>## <a id="broker-ns"></a> broker

 stage는 서버간의 통신을 위한 영역이라면, broker는 클라이언트 또는 타 서버와의 연동을 위한 처리를 제공합니다.

 기본적인 통신 방식은 BSON을 통해 데이터를 전달 받을수 있도록 구성되어 있습니다.

>#### <a id="broker-ready"></a> broker.ready(s, function (socket))
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> *s*로 정의된 네트워크 연결을 대기 상태로 설정합니다.
  * **입력**
    * s - 설정 정보
    * function (socket) - 연결이 발생될때 호출될 콜백 함수
  * **반환** 
    * *i* <span style="white-space: pre;">&#9;&#9;</span> 오류 번호 
    * *socket* <span style="white-space: pre;">&#9;</span> 연결 정보 
  * **설명**<br>
    
    *s*는 uri 형태의 정보를 가지고 있습니다.
      | 네트워크 |  네트워크 |  패킷 설정 | 비고 |
      |=========:|:=========:|:==========|:=====:|
      |   tcp:// | *IP*:*PORT*   | ?packet=*IN*:*OUT* | 캐싱 사용 가능 |
      |   udp:// | *IP*:*PORT*   | - | 캐싱 무시 |

      > udp의 경우 *IP*가 멀티케스트 대역을 지원 합니다.      
      > *IP*는 IPv4와 IPv6를 지원합니다.

    *packet*에 대해

       - 32비트 자료형(INTEGER)으로 데이터의 크기를 [리틀 엔디언](https://ko.wikipedia.org/wiki/%EC%97%94%EB%94%94%EC%96%B8) 으로 저장합니다.
       - BSON 버퍼인 경우 별도의 처리 없이 사용 가능합니다.
         > BSON으로 전송되는 경우 직접 접근이 가능합니다.

    > 지정된 *PORT*가 설정("#service")에 지정된 값인 경우 여러 프로세서가 공유 가능

    > *socket*에서 다음의 추가 정보를 읽을수 있습니다.

      | 이름 | 자료형 | 설명 |
      |:====:|:======:|:=====|
      |id|i|소캣 번호|
      |crypto|b|암호화 유무|전달되는 데이터의 암호화 유무를 지정|
      |errno|i|오류 번호|
      |addrs|t|연결 주소 [로컬, 원격]|
      |-|v|사용자 정의|

    > *socket*에 다음 정보를 설정할수 있습니다.

      | 이름 | 자료형 | 설명 |
      |:====:|:======:|:=====|
      |expire|i|데이터가 발생되지 않을때 소켓을 닫을 시간 (msec)|
      |commit|i|데이터 전송 간격 (msec)|
      |crypto|s|암호화 공개키(CBC/RIJNDAEL 알고리즘)|
      |crypto|nil|암호화 OFF|
      |crypto|t|암호화 설정 ["알고리즘/블록운영/패딩방식" = 공개키 ]|
      |close|f|close 발생시 호출할 콜백 함수|
      |-|v|사용자 정의|

      * 알고리즘
        * [AES](https://ko.wikipedia.org/wiki/%EA%B3%A0%EA%B8%89_%EC%95%94%ED%98%B8%ED%99%94_%ED%91%9C%EC%A4%80)
        * [DES](https://ko.wikipedia.org/wiki/%EB%8D%B0%EC%9D%B4%ED%84%B0_%EC%95%94%ED%98%B8%ED%99%94_%ED%91%9C%EC%A4%80)
        * [SEED](https://ko.wikipedia.org/wiki/SEED)
        * [RIJNDAEL](https://ko.wikipedia.org/wiki/%EA%B3%A0%EA%B8%89_%EC%95%94%ED%98%B8%ED%99%94_%ED%91%9C%EC%A4%80)

      * [블록 운영](https://ko.wikipedia.org/wiki/%EB%B8%94%EB%A1%9D_%EC%95%94%ED%98%B8_%EC%9A%B4%EC%9A%A9_%EB%B0%A9%EC%8B%9D)
        * [CBC](https://ko.wikipedia.org/wiki/%EB%B8%94%EB%A1%9D_%EC%95%94%ED%98%B8_%EC%9A%B4%EC%9A%A9_%EB%B0%A9%EC%8B%9D#%EC%95%94%ED%98%B8_%EB%B8%94%EB%A1%9D_%EC%B2%B4%EC%9D%B8_%EB%B0%A9%EC%8B%9D_(CBC))
        * [CFB](https://ko.wikipedia.org/wiki/%EB%B8%94%EB%A1%9D_%EC%95%94%ED%98%B8_%EC%9A%B4%EC%9A%A9_%EB%B0%A9%EC%8B%9D#%EC%95%94%ED%98%B8_%ED%94%BC%EB%93%9C%EB%B0%B1_(CFB))

      * [패딩 방식](https://cryptosys.net/pki/manpki/pki_paddingschemes.html)
        * PKCS5Padding
        * PKCS1Padding
        * NoPadding

  * **예제**
    ```lua
       ...
       broker.ready("tcp://0.0.0.0:8081?packet=10k,40k", function (socket)
          ...
       end);
    ```

>#### <a id="broker-join"></a> broker.join(s, function (socket, s))
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> *s*로 정의된 네트워크 연결을 진행 합니다.
  * **입력**
    * s - 설정 정보
    * function (socket, s) - 연결이 완료될때 호출될 콜백 함수
  * **반환** 
    * *i* <span style="white-space: pre;">&#9;&#9;</span> 오류 번호 
    * *socket* <span style="white-space: pre;">&#9;</span> 연결 정보 
  * **설명**<br>
    
    *s*는 uri 형태의 정보를 가지고 있습니다. -- 연결 지향성이라 tcp만 허용됩니다
      | 네트워크 |  네트워크 |  패킷 설정 | 비고 |
      |=========:|:=========:|:==========|:=====:|
      |   tcp:// | *IP*:*PORT*   | ?packet=*IN*:*OUT* | 캐싱 사용 가능 |

    > *packet*에 대해

       - 32비트 자료형(INTEGER)으로 데이터의 크기를 [리틀 엔디언](https://ko.wikipedia.org/wiki/%EC%97%94%EB%94%94%EC%96%B8) 으로 저장합니다.
       - BSON 버퍼인 경우 별도의 처리 없이 사용 가능합니다.
         > BSON으로 전송되는 경우 직접 접근이 가능합니다.

    > 지정된 *PORT*가 설정("#service")에 지정된 값인 경우 여러 프로세서가 공유 가능

    > *socket*에서 다음의 추가 정보를 읽을수 있습니다.

      | 이름 | 자료형 | 설명 |
      |:====:|:======:|:=====|
      |id|i|소캣 번호|
      |crypto|b|암호화 유무|전달되는 데이터의 암호화 유무를 지정|
      |errno|i|오류 번호|
      |addrs|t|연결 주소 [로컬, 원격]|
      |-|v|사용자 정의|

    > *socket*에 다음 정보를 설정할수 있습니다.

      | 이름 | 자료형 | 설명 |
      |:====:|:======:|:=====|
      |expire|i|데이터가 발생되지 않을때 소켓을 닫을 시간 (msec)|
      |commit|i|데이터 전송 간격 (msec)|
      |crypto|s|암호화 공개키(CBC/RIJNDAEL 알고리즘)|
      |crypto|nil|암호화 OFF|
      |crypto|t|암호화 설정 [ "알고리즘/블록운영/패딩방식" = 공개키 ] |
      |close|f|close 발생시 호출할 콜백 함수|
      |-|v|사용자 정의|

      * 알고리즘
        * [AES](https://ko.wikipedia.org/wiki/%EA%B3%A0%EA%B8%89_%EC%95%94%ED%98%B8%ED%99%94_%ED%91%9C%EC%A4%80)
        * [DES](https://ko.wikipedia.org/wiki/%EB%8D%B0%EC%9D%B4%ED%84%B0_%EC%95%94%ED%98%B8%ED%99%94_%ED%91%9C%EC%A4%80)
        * [SEED](https://ko.wikipedia.org/wiki/SEED)
        * [RIJNDAEL](https://ko.wikipedia.org/wiki/%EA%B3%A0%EA%B8%89_%EC%95%94%ED%98%B8%ED%99%94_%ED%91%9C%EC%A4%80)

      * [블록 운영](https://ko.wikipedia.org/wiki/%EB%B8%94%EB%A1%9D_%EC%95%94%ED%98%B8_%EC%9A%B4%EC%9A%A9_%EB%B0%A9%EC%8B%9D)
        * [CBC](https://ko.wikipedia.org/wiki/%EB%B8%94%EB%A1%9D_%EC%95%94%ED%98%B8_%EC%9A%B4%EC%9A%A9_%EB%B0%A9%EC%8B%9D#%EC%95%94%ED%98%B8_%EB%B8%94%EB%A1%9D_%EC%B2%B4%EC%9D%B8_%EB%B0%A9%EC%8B%9D_(CBC))
        * [CFB](https://ko.wikipedia.org/wiki/%EB%B8%94%EB%A1%9D_%EC%95%94%ED%98%B8_%EC%9A%B4%EC%9A%A9_%EB%B0%A9%EC%8B%9D#%EC%95%94%ED%98%B8_%ED%94%BC%EB%93%9C%EB%B0%B1_(CFB))

      * [패딩 방식](https://cryptosys.net/pki/manpki/pki_paddingschemes.html)
        * PKCS5Padding
        * PKCS1Padding
        * NoPadding

  * **예제**
    ```lua
       ...
       broker.join("tcp://192.168.1.10:8081?packet=10k,40k", function (socket)
          ...
          print("NTOA: ", k, broker.ntoa(socket.addrs.remote));
          socket.commit("GET / HTTP/1.0\r\n\r\n");
       end);
    ```

>#### <a id="broker-close"></a> broker.close([i | { i, ... }])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 네트워크 연결을 페쇄합니다.
  * **입력**
    * i - 소켓 번호
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>
    
    연결된 네트워크를 페쇄합니다.

  * 참고
    * [broker.ready](#broker-ready)
    * [broker.join](#broker-join)
    * [socket.close](#socket-close)

  * **예제**
    ```lua
       ...
       broker.close(socket.id);
    ```

>#### <a id="broker-f"></a> broker.f(i)
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 소켓번호를 연결 정보로 전환합니다.
  * **입력**
    * i - 소켓 번호
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 연결 정보
  * **설명**<br>
    
    **샌드 박스**로 실행되는 콜백 함수로는 연결 정보를 전달할 수 없습니다.<br> 이러한 경우에는 소켓번호로 전달 후 다시 연결 정보로 전환하여 사용하는 방법을 제공하고 있습니다. 

    ```lua
       ...
       stage.submit(socket.id, function (socket_id, data)
          local socket = broker.f(socket_id)
          ...
       end, socket.id, data)
    ```
    의 형태로 사용될수 있습니다.

  * 참고
    * [broker.ready](#broker-ready)
    * [broker.join](#broker-join)
    * [broker.close](#broker-close)

  * **예제**
    ```lua
       ...
       local socket = broker.f(socket_id);
    ```

>#### <a id="broker-signal"></a> broker.signal(\[i | s | { i, s... }], v)
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 메시지를 전달합니다.
  * **입력**
    * i - 소켓 번호에 메시지가 발생된 것 처럼 전달합니다.
    * s - 연결 주소 (broker.aton())에 메시지를 전달합니다.
    * v - 메시지
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>
    
    *i*의 경우는 broker.signal()을 통해 메시지가 발생된것 처럼 처리하게 됩니다. 
    
    만약, *i*에 메시지를 보내고자 한다면
      * broker.signal({ *i* }, ...)
      * broker.f(*i*).commit(...)

    를 사용할수 있습니다.

    *s*는 udp로 연결된 서버에 데이터를 보내고자 할때 다음과 같이 사용할수 있습니다.    
    ```lua
       ...
       local addr = broker.aton("udp://224.0.0.1:8082", {})[1];

       broker.signal(addr, { "한글 테스트" });
    ```

  * 참고
    * [broker.ready](#broker-ready)
    * [broker.join](#broker-join)
    * [broker.close](#broker-close)
    * [broker.aton](#broker-aton)

  * **예제**
    ```lua
       ...
       -- socket에서 logout 메시지가 발생된것으로 보냅니다.
       broker.signal(socket.id, {
           ["logout"] = ""
       })
    ```
>#### <a id="broker-ntoa"></a> broker.ntoa(s \[, t])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 네트워크 주소를 문자열로 변환합니다.
  * **입력**
    * s - 연결 주소
    * t - 테이블에 서버 연결 주소를 추가합니다
  * **반환** 
    * s <span style="white-space: pre;">&#9;&#9;</span> 문자열로 반환
    * t <span style="white-space: pre;">&#9;&#9;</span> 테이블로 반환
  * **설명**<br>

    *t*를 사용하는 경우에 다음의 항목이 추가됩니다.
       |이름|자료형|값|비고|
       |:==:|:====:|:=:|:=:|
       |family|i| 네트워크 형태 | 2 - IPv4, 10 - IPv6 |
       |addr|s|연결 주소| |
       |port|i|포트 번호| |
       |scope_id|i|IPv6의 scope id| |

    *t*가 정이되지 않은 경우에는
       - IPv4 <span style="white-space: pre;">&#9;&#9;</span> "IP:PORT" 
       - IPv6 <span style="white-space: pre;">&#9;&#9;</span> "[IP]:PORT"

    형태로 반환됩니다.   

  * 참고
    * [broker.aton](#broker-aton)

  * **예제**
    ```lua
       ...
       print("IP", broker.ntoa(addr));
    ```

>#### <a id="broker-aton"></a> broker.aton(\[s | i\] \[, t])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 문자열을 네트워크 주소로 변환합니다.
  * **입력**
    * s - 연결 주소
    * i - 네트워크 형태 (2 - IPv4, 10 - IPv6)
    * t - 설정하고자 하는 값
  * **반환** <span style="white-space: pre;">&#9;&#9;&#9;</span> 네트워크 주소
  * **설명**<br>

    *t*에 설정된 값으로 변경 합니다.
       |이름|자료형|값|비고|
       |:==:|:====:|:=:|:=:|
       |addr|s|연결 주소| |
       |port|i|포트 번호| |
       |scope_id|i|IPv6의 scope id| |

  * 참고
    * [broker.ntoa](#broker-ntoa)

  * **예제**
    ```lua
       ...
       local addr = broker.aton("udp://224.0.0.1:8082", {})[1];
    ```

>#### <a id="broker-ifaddr"></a> broker.ifaddr(s)
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 네트워크 장치의 주소를 얻습니다.
  * **입력**
    * s - 얻고자 하는 장치명
  * **반환** <span style="white-space: pre;">&#9;&#9;&#9;</span> 장치 정보
  * **설명**<br>

    네트워크 장치(랜카드)에 할당된 주소를 얻습니다.

    만약, *s*가 지정되는 경우 해당 장치의 주소를 얻습니다. (*s* 문자열이 포함된 장비를 모두 얻습니다.)

  * 참고
    * [broker.ntoa](#broker-ntoa)

  * **예제**
    ```lua
       ...

       local devices = broker.ifaddr();
       for k, v in pairs(devices) do
            print("DEVICE: " .. k);
            for v_i, v_v in pairs(v) do
                local addr = broker.ntoa(v_v, {});
                print("", "CHECK -", addr.family, addr.addr);
                print("", v_i, broker.ntoa(broker.aton(v_v, { ["port"] = 8080 })));
            end
       end
    ```

>### <a id="socket-ns"></a> socket

>#### <a id="socket-commit"></a> socket.commit(v \[, s])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 메시지 *v*를 *s*로 보냅니다.
  * **입력**
    * v - 보내고자 하는 메시지
    * s - *(생략 가능)* 메시지를 받을 네트워크 주소 (broker.aton())
  * **반환** <span style="white-space: pre;">&#9;&#9;&#9;</span> 없음
  * **설명**<br>

    *socket* 으로 메시지를 전달합니다.

  * 참고
    * [broker.ready](#broker-ready)
    * [broker.join](#broker-join)
    * [broker.signal](#broker-signal)
    * [broker.ntoa](#broker-ntoa)

  * **예제**
    ```lua
       ...
       socket.commit({
           ["message"] = {
               ["SYS"] = "공지사항: 임시 점검 예정입니다."
           }
       })
    ```

>#### <a id="socket-close"></a> socket.close()
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 네트워크 연결을 페쇄합니다.
  * **입력** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>
    
    연결된 네트워크를 페쇄합니다.

  * 참고
    * [broker.ready](#broker-ready)
    * [broker.join](#broker-join)
    * [broker.close](#broker-close)

  * **예제**
    ```lua
       ...
       socket.close();
    ```

>## <a id="odbc-ns"></a> odbc

>#### <a id="odbc-scope"></a> odbc.new(\[s | t] \[, function (adapter)])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> odbc를 통해 데이터베이스와 연결합니다. 
  * **입력** 
    * s - 문자열 형태의 연결 정보
    * t - 설정 형태의 연결 정보
    * function (adapter) - 연결이 완료되면 호출되는 함수
  * **반환**
    * f가 정의된 경우 - f 의 반환 값
    * f가 정의되지 않은 경우 - adapter 
  * **설명**<br>
    
    *s*로 설정되는 연결정보는 *ODBC Connection String* 또는 설정 파일의 *%odbc*에 정의된 이름을 지정합니다.

      |데이터베이스|참고 사이트 |
      |:==========:|:==========:|
      | mysql| [Connector/ODBC](https://dev.mysql.com/doc/connector-odbc/en/connector-odbc-configuration-connection-without-dsn.html)|
      | mssql| [DSN 및 연결 문자열 키워드 및 특성](https://docs.microsoft.com/ko-kr/sql/connect/odbc/dsn-connection-string-attribute?view=sql-server-2017)|

    *t*는 *ODBC Connect String*을 *%odbc*에 정의되는 정보 형태로 설정한 값 입니다.
    > ODBC Connect String에 정의된 이름을 그대로 사용하시면 됩니다.

    다음에 나열되는 설정은 추가설정입니다.

      | 이름 | 자료형 | 설명 |
      |:====:|:======:|:=====|
      | .executeTimeout | i | SQL 실행 타임 아웃 값 (msec) |
      | .connectionTimeout | i | 데이터베이스 연결 타임 아웃 값 (msec) |
      | .loginTimeout | i | 로그인 타임 아웃 값 (msec) |
      | .queryDriver | s | 데이터베이스 유형 (mysql, mssql) |

    ```lua
       local adp = odbc.new({
         ["DRIVER"] = "MySQL ODBC 8.0 ANSI Driver", -- /etc/odbcinst.ini에 정의
         ["SERVER"] = "localhost",
         ["DATABASE"] = "DF_DEVEL",
         ["USER"] = "test",
         ["PASSWORD"] = "test&&",
         [".executeTimeout"] = 1000,
         [".queryDriver"] = "mysql"
       })
    ```
    데이터베이스의 유형 (*.queryDriver*)는 쿼리에 컬럼의 이름을 표시하는 방법과 자동증가된 값을 얻는 방법등의 차이를 구분하기 위해 사용되는 값 입니다.

      *mysql*은 자동증가된 값을 얻기 위해 *"SELECT LAST_INSERT_ID"* 를 사용하지만,<br>
      *mssql*은 "SELECT SCOPE_IDENTITY()"를 사용합니다.

      > 추가적인 지원이 필요하신 경우 support@datumflux.co.kr 로 요청 부탁드립니다.

    *function (adapter)* 가 정의되어 있다면, 데이터베이스에 연결후 해당 함수를 호출합니다.

  * 참고
    * [odbc.cache](#odbc-cache)
    * [odbc.purge](#odbc-purge)

  * **예제**
    ```lua
        function tprint (tbl, indent)
            if not indent then indent = 0 end
            for k, v in pairs(tbl) do
                formatting = string.rep("  ", indent) .. k .. ": "
                if type(v) == "table" or type(v) == "userdata" then
                    print(formatting)
                    tprint(v, indent+1)
                else
                    print(formatting .. v) 
                end 
            end 
        end

        odbc.new("DF_DEVEL", function (adp)
            local rs = adp.execute("SELECT * FROM DEMO");
            local rows = rs.fetch({"user"});
            print("SELECT", rs, rows);
            tprint(rows);

            -- rows["TEST"] = nil;
            rows["TEST"] = {        -- user varchar(40)
                ["name"] = "HELLO", -- name varchar(80)
                ["age"] = 10,       -- age int
                ["memo"] = {        -- memo blob
                    ["blob"] = "HAHA"
                }
            };

            rs.apply(rows, "DEMO", function (ai_k, log)
                print("COMMIT", ai_k, log);
                tprint(ai_k);
                tprint(log);
            end);
        end);
    ```

>#### <a id="odbc-cache"></a> odbc.cache(s \[, function ()])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  *s*로 캐싱된 데이터가 없는 경우 *function*을 호출합니다.
  * **입력** 
    * s - 고유 이름
    * function () - 데이터가 없는 경우 호출됩니다.
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 캐싱된 값
  * **설명**<br>    
    
    반복되는 동일한 요청으로 인해 데이터베이스의 사용률에 높아지게 되면 다른 처리에 지연이 발생됩니다. 이러한 상황이 발생되지 않도록 동일한 요청에 대한 데이터를 캐싱하여 데이터베이스의 부하를 줄일수 있습니다.
    
    *odbc.cache*는 *function()* 의 반환값을 값을 저장하게 되는데 저장 가능한 데이터는

    * resultset.fetch()의 결과
    * lua 자료형 (문자열, 숫자, 테이블 등)

    을 저장하며, 캐싱된 데이터를 제거하기 위해서는 *odbc.purge*를 사용해 제거 할수 있습니다.
    
  * 참고
    * [odbc.new](#odbc-new)
    * [odbc.purge](#odbc-purge)

  * **예제**
    ```lua
       ...
        odbc.cache("IDLE", function ()

            return odbc.new(function (adp)
              local rs = adp.execute("SELECT 1 as sum");
              local rows = rs.fetch();
              for k, v in pairs(rows) do
                  print("SELECT", k, v);
              end
              return rows;
           end
        end);
    ```

>#### <a id="odbc-purge"></a> odbc.purge([s, t])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  데이터를 제거합니다.
  * **입력** 
    * s - 고유 이름
    * t - 캐싱된 고유 이름 목록
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>    
    
    *odbc.cache*를 통해 저장된 데이터를 제거하는 함수로, *s* 또는 *t*를 정의하지 않고 호출하는 경우 저장된 모든 캐싱 데이터를 제거합니다.
        
  * 참고
    * [odbc.new](#odbc-new)
    * [odbc.cache](#odbc-cache)

  * **예제**
    ```lua
       ...
        odbc.purge({ "IDLE" })
    ```

>### <a id="adapter-ns"></a> adapter

>#### <a id="adapter-execute"></a> adapter.execute(\[s | t] \[, v... ])

  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 데이터베이스로 명령을 전달합니다.
  * **입력** 
    * s - SQL 명령
    * t - SQL 명령과 사용되는 자료형을 지정
    * v... - SQL에 전달하는 값
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> resultset
  * **설명**<br>    
    
    *s*는 SQL 명령으로 "SELECT...", "UPDATE...", "INSERT...", "CALL ..." 등의 명령을 포함합니다. 
    
    이 중에, SELECT, UPDATE, INSERT등에서 자료형을 명확히 선언해야 하는 경우가 있을수 있습니다. 이러한 경우 사용되는 방법이 *t*를 사용하는 방법입니다.
    -- 스크립트 언어의 특성상 대표되는 자료형만 지원하고 있어 이를 명시하기 위한 방법입니다.
    
    두 명령의 차이를 보면

    * *"SELECT * FROM USER WHERE user_id = ? AND password = ?"*
    * { *"SELECT * FROM USER WHERE user_id = ? AND password = ?"*, "C", "C" }&nbsp; 

    로 구분할수 있습니다. <br>

    ```lua
       local rs = adp.execute({
         "SELECT * FROM USER WHERE user_id = ? AND password = ?", "C", "C" 
         }, "USER_ID", "PASSWORD")
    ```

    *t*는 정의할때 처음 위치에는 SQL 명령을 다음 위치 부터는 SQL 명령에 포함된 ? 에 지정할 SQL 자료형을 설정하면 됩니다.

    | format | SQL TYPE | 값 형태 |
    |:========:|:=========:|:=====:|
    | W | WVARCHAR | 제한없음 |
    | w | WCHAR | 제한없음 |
    | C | VARCHAR | 제한없음 |
    | c | CHAR | 제한없음 |
    | B | BINARY | 제한없음 |
    | b | TINYINT | 제한없음 |
    | s | SHORT | 제한없음 |
    | i | INT | 제한없음 |
    | l | LONG | 제한없음 |
    | T | UNSIGNED TINYINT | 제한없음 |
    | S | UNSIGNED SHORT | 제한없음 |
    | I | UNSIGNED INT | 제한없음 |
    | L | UNSIGNED LONG | 제한없음 |
    | f | FLOAT | 제한없음 |
    | d | DOUBLE | 제한없음 |
    | D | DATETIME | 문자열의 경우 "YYYY-MM-DD HH:MM:SS" |

  * 참고
    * [odbc.new](#odbc-new)
    * [odbc.cache](#odbc-cache)

  * **예제**
    ```lua
       ...
        odbc.cache("IDLE", function ()

            return odbc.new(function (adp)
              local rs = adp.execute("SELECT 1 as sum");
              local rows = rs.fetch();
              for k, v in pairs(rows) do
                  print("SELECT", k, v);
              end
              return rows;
           end
        end);
    ```

>#### <a id="adapter-apply"></a> adapter.apply(row \[, s] \[, function (t1, t2)])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  변경된 항목을 적용합니다.
  * **입력** 
    * row - resultset.fetch()를 통해 반환된 객체와 연결된 객체
    * s - *(생략 가능)* 적용할 테이블 명
    * function (t1, t2) - *(생략 가능)* 변경 정보를 전달할 함수
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> function (t1, t2)의 반환값
  * **설명**<br>    
    
    SELECT 이후 *resultset.fetch*를 사용해 얻은 결과를 수정한 경우, 해당 내용을 데이터베이스에 적용해야 하는경우 사용될 수 있습니다.
    > row는 *resultset.fetch()*의 결과 뿐만 아니라 row[*KEY*] 로 반환된 객체도 가능합니다. (값은 불가능)

    변경된 내용은 값의 변경 유무에 따라서 INSERT,UPDATE,DELETE로 처리가 됩니다.<br>
    그에 대한 정보는 *function (t1, t2)* 로 전달이 되며 *t1*은 자동증가되는 컬럼이 추가되는 경우 추가된 값을,
    *t2*는 다음 테이블의 변경된 로그 정보를 가지게 됩니다.

    |컬럼명|자료형|설명|
    |:====:|:====:|:===|
    |table|s|테이블명|
    |action|i|2: 업데이트, 4: 추가, 8: 삭제  |
    |where|t| { "COLUMN": 값, ... }&nbsp; |
    |value|t| { "o": 이전값, "n": 새로운값, "t": SQL_TYPE }&nbsp;|

    전달 되는 정보를 바탕으로 로깅을 할수 있습니다.

    * **주**

      1. 데이터베이스 종류에 따라 테이블 명을 확인할수 없어 명시가 필요한 경우가 있습니다.
         > MS-SQL을 사용하는 경우에는 테이블 명을 명시해야 합니다.
      1. SELECT 쿼리에서 JOIN 쿼리는 테이블 명을 명시해야 합니다.
      1. 적용되는 컬럼이 *s* (테이블명)에 포함되지 않은 경우 SQL오류가 발생될수 있습니다.

  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.execute](#adapter-execute)
    * [resultset.apply](#resultset-apply)

  * **예제**
    ```lua
       ...
        local rows = odbc.cache("USER")
        if rows ~= nil then
          ...
          odbc.new("DF_DEVEL", function (adp)
            adp.apply(rows, "USERS")
          end)
        end
    ```

>#### <a id="adapter-begin"></a> adapter.begin( \[function (adapter)] )
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  트랜젝션을 시작합니다.
  * **입력** 
    * function (adapter) - *(생략 가능)* 트랜젝션 처리 후 호출되는 콜백 함수
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> function의 반환
  * **설명**<br>    
    
    데이터베이스의 트랜젝션을 시작합니다. 

    만약, *function (adapter)* 를 사용하는 경우 *adapter.commit()* 또는 *adapter.rollback()* 의 처리를 *function (adpater)* 의 반환값으로 대체 가능합니다.
        
    ```lua
       ...
        adp.begin(function (adp)
           ...
           return true; -- true: commit, false: rollback
        end)
    ```

    * **주**    
      1. 트랜젝션이 설정되면 동일한 데이터베이스에 접근에 제한이 됩니다.
      1. 트랜젝션 설정 후 처리 시간이 지연되면 데이터베이스의 사용률이 올라가 문제가 발생될수 있습니다.
      1. 트랜젝션이 설정되면 이후 UPDATE, INSERT등 SQL명령은 *adapter.commit()* 이후 데이터베이스에 적용 됩니다.
         > *adapter.rollback()* 은 트랜젝션 이후의 SQL명령을 취소합니다.

  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.commit](#adapter-commit)
    * [adapter.rollback](#adapter-rollback)

  * **예제**
    ```lua
       ...
       adp.begin()
    ```

>#### <a id="adapter-commit"></a> adapter.commit()
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  트랜젝션 중에 처리된 변경 내용을 적용합니다.
  * **입력** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>    
    
    *adapter.begin()* 이후에 적용된 SQL명령을 데이터베이스에 적용 합니다.
    
  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.begin](#adapter-begin)
    * [adapter.rollback](#adapter-rollback)

  * **예제**
    ```lua
       ...
       adp.commit()
    ```

>#### <a id="adapter-rollback"></a> adapter.rollback()
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  트랜젝션 중에 처리된 변경 내용을 취소합니다.
  * **입력** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>    
    
    *adapter.begin()* 이후에 적용된 SQL명령을 취소 합니다.
    
  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.begin](#adapter-begin)
    * [adapter.commit](#adapter-commit)

  * **예제**
    ```lua
       ...
       adp.rollback()
    ```

>#### <a id="adapter-close"></a> adapter.close()
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 데이터베이스 연결을 닫습니다.
  * **입력** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>    
    
    *odbc.new()* 로 연결된 정보를 해제 합니다. 
    > 해당 함수를 호출하지 않아도 처리된 함수를 벗어나면 자동으로 해제됩니다.
    
    해제 될때, 완료되지 않은 트랜젝션은 *adapter.commit()* 이 됩니다.
    
  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.commit](#adapter-commit)

  * **예제**
    ```lua
       ...
       adp.close()
    ```

>### <a id="resultset-ns"></a> resultset

>#### <a id="resultset-fetch"></a> resultset.fetch( \[t | s] \[, function (row)])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 실행된 명령에서 결과를 얻습니다.
  * **입력** 
    * t - 결과의 정렬을 원하는 컬럼 목록
    * s - 정렬을 원하는 컬럼명
    * function (row) - *(생략 가능)* 결과에 대한 콜백 함수
  * **반환** 
    * f가 정의된 경우 - f 의 반환 값
    * f가 정의되지 않은 경우 - row 
  * **설명**<br>    

    *adapter.execute* 에서 실행된 결과를 얻기 위한 함수로, 주로 "SELECT"이후에 사용되는 명령입니다. 해당 명령을 통해 얻어진 결과를 테이블 형태로 직접 접근하도록 변환합니다.

    > *s* 또는 *t*에 지정될수 있는 컬럼은 SELECT에서 얻어진 컬럼 명이어야 합니다.    
    > *s* 또는 *t*의 컬럼명에 *$*를 지정하게 되면 ROW 일련번호를 기준으로 결과를 얻어 옵니다. (0, 1, 2...)
    
    만약, *s* 또는 *t*가 지정되지 않는 경우 마지막 정보만 반환됩니다.

    row의 컬럼명은 SELECT를 통해 얻어진 컬럼을 사용합니다.
    > 만약, *adp.execute("SELECT 1 as B")* 로 요청을 하면 컬럼명은 "B"가 됩니다.
    
    ```lua
       ...
       local rows = rs.fetch({ "userid", "$" });

       print("USERNAME", rows["USER_ID"][0].username)
    ```

    반환된 row 정보는 테이블에 접근하듯이 사용할수 있으며 값을 추가 하거나, 수정 및 삭제를 할수 있습니다. 
    > 해당 처리 후 *resultset.apply* 또는 *adapter.apply*를 통해 데이터베이스에 적용 할수 있습니다.

  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.execute](#adapter-execute)
    * [adapter.apply](#adapter-apply)
    * [resultset.apply](#resultset-apply)

  * **예제**
    ```lua
       ...
       local rs = adp.execute("SELECT 1 as sum");
       local rows = rs.fetch();
       for k, v in pairs(rows) do
            print("SELECT", k, v);
       end       
    ```

>#### <a id="resultset-erase"></a> resultset.erase(row, [s | t])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 얻어진 결과에서 *t* 를 삭제합니다.
  * **입력** 
    * row - resultset.fetch()의 반환 객체에 연결된 객체
    * s - 삭제를 원하는 객체의 값
    * t - 삭제를 원하는 객체가 위치한 값의 진입 순서
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 제거된 갯수
  * **설명**<br>    

    *s* 는 row에서 직접 접근이 가능한 값을 지정하며,<br>
    *t* 는 row에서 여러 단계에 걸쳐서 접근해야 하는 위치의 값을 지정하는데 사용됩니다.

    > row는 *resultset.fetch()* 의 결과 뿐만 아니라 row[*KEY*] 로 반환된 객체도 가능합니다. (값은 불가능)

       예를 들어,
       | 0 | 1 | 2 | 3 | ... |
       |:=:|:=:|:=:|:=:|:=:|
       | A0 | B0 | C0 | D0 | ... |
       | A0 | B0 | C1 | D1 | ... |

       로 저장된 값에서 D1을 삭제하기 위해서는 
       ```lua
          rs.erase(rows, { "A0", "B0", "C1", "D1" })
       ```

       "B0"의 모든 하위 값을 삭제하고자 한다면
       ```lua
          rs.erase(rows, { "A0", "B0" })
       ```
       
       으로 삭제 할수 있습니다.

       물론, 삭제하는데 있어 
       ```lua
          rows.A0.B0.C1.D1 = nil
          -- 동일한 rows["A0"]["B0"]["C1"]["D1"] = nil
       ```

       이나, 
       ```lua
          rows.A0.B0 = nil
          -- 동일한 rows["A0"]["B0"] = nil
       ```
       도 동일한 결과를 같습니다.

       해당 함수를 존재 이유는 하위 객체 접근에 대한 편의성을 위해 제공됩니다.

  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.execute](#adapter-execute)
    * [adapter.apply](#adapter-apply)
    * [resultset.apply](#resultset-apply)

  * **예제**
    ```lua
       ...
       local rs = adp.execute("SELECT 1 as sum");
       local rows = rs.fetch();
       for k, v in pairs(rows) do
            print("SELECT", k, v);
       end       
    ```

>#### <a id="resultset-apply"></a> resultset.apply(row \[, table] \[, function (t1, t2)])

  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  변경된 항목을 적용합니다.
  * **입력** 
    * row - resultset.fetch()를 통해 반환된 객체와 연결된 객체
    * s - *(생략 가능)* 적용할 테이블 명
    * function (t1, t2) - *(생략 가능)* 변경 정보를 전달할 함수
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> function (t1, t2)의 반환값
  * **설명**<br>    
    
    SELECT 이후 *resultset.fetch*를 사용해 얻은 결과를 수정한 경우, 해당 내용을 데이터베이스에 적용해야 하는경우 사용될 수 있습니다.
    > row는 *resultset.fetch()* 의 결과 뿐만 아니라 row[*KEY*] 로 반환된 객체도 가능합니다. (값은 불가능)

    변경된 내용은 값의 변경 유무에 따라서 INSERT,UPDATE,DELETE로 처리가 됩니다.<br>
    그에 대한 정보는 *function (t1, t2)* 로 전달이 되며 *t1*은 자동증가되는 컬럼이 추가되는 경우 추가된 값을,
    *t2*는 다음 테이블의 변경된 로그 정보를 가지게 됩니다.

    |컬럼명|자료형|설명|
    |:====:|:====:|:===|
    |table|s|테이블명|
    |action|i|2: 업데이트, 4: 추가, 8: 삭제  |
    |where|t| { "COLUMN": 값, ... }&nbsp; |
    |value|t| { "o": 이전값, "n": 새로운값, "t": SQL_TYPE }&nbsp;|

    전달 되는 정보를 바탕으로 로깅을 할수 있습니다.

    * **주**

      1. 데이터베이스 종류에 따라 테이블 명을 확인할수 없어 명시가 필요한 경우가 있습니다.
         > MS-SQL을 사용하는 경우에는 테이블 명을 명시해야 합니다.
      1. SELECT 쿼리에서 JOIN 쿼리는 테이블 명을 명시해야 합니다.
      1. 적용되는 컬럼이 *s* (테이블명)에 포함되지 않은 경우 SQL오류가 발생될수 있습니다.

  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.execute](#adapter-execute)
    * [adapter.apply](#adapter-apply)

  * **예제**
    ```lua
       ...
        rs.apply(rows["userid"], "USERS")
    ```

>## <a id="process-property"></a> process

  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  구동중인 프로세서의 정보를 확인합니다.
  * **설명**<br>    
    프로세서에 대한 정보를 확인할수 있습니다.  

    * 읽을수 있는 정보
    |이름|자료형|설명|
    |:==:|:====:|:===|
    |id|i|프로세서 아이디|
    |tick|d|초단위 시간|
    |args|t|프로그램 실행 인자|
    |stage|i|스테이지 번호|
    |%*NAME*|v|환경설정에 정의된 값|
    |$*NAME*|v|설정 파일에 정의된 값|
    |*NAME*|v|사용자 정의 값|

    * 기록할수 있는 정보
    |이름|자료형|설명|
    |:==:|:====:|:===|
    |title|s|프로세서의 이름 변경(ps로 표시되는)|
    |$|s|설정 정보를 변경(JSON 형식)|
    |*NAME*|v|사용자 정의 값

  * 참고
    * [thread](#thread-property)

  * **예제**
    ```lua
      print("LANG", process["%LANG"])
    ```

>## <a id="thread-property"></a> thread
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  구동중인 쓰레드 정보를 확인합니다.
  * **설명**<br>    
    현재 구동중인 쓰레드에 대한 정보를 확인할수 있습니다.  

    * 읽을수 있는 정보
    |이름|자료형|설명|
    |:==:|:====:|:===|
    |id|i|쓰레드 아이디|
    |tick|d|초단위 시간|
    |*NAME*|v|사용자 정의 값|

    * 기록할수 있는 정보
    |이름|자료형|설명|
    |:==:|:====:|:===|
    |*NAME*|v|사용자 정의 값

  * 참고
    * [process](#process-property)

  * **예제**
    ```lua
    ```
