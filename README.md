# STAGE for LUA

> ***계속 업데이트 중입니다.***

*제품에 대한 자세한 정보는 [홈페이지](https://datumflux.co.kr) 에서 확인 하실수 있습니다.*

>## 순서

* [시작](#intro)
* 네임 스페이스
  * [stage](docs/api/stage.md#stage-ns)
  * [log4cxx](docs/api/stage.md#log4cxx-ns)
  * [broker](docs/api/broker.md#broker-ns)
  * [odbc](docs/api/odbc.md#odbc-ns)

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
     |:---:|:---:|:--- |
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
       > stage.signal("stage:message_id", ...) 로 메시지를 보내면 설정된 여러개(정의된 3개) "=lua+stage"는 전달 정책에 따라 하나의 "=lua+stage"에만 전달 됩니다. 모든 "=lua+stage"에 전달을 원하고자 한다면 "*stage:message_id" 로 메시지를 전달하면 됩니다.   

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
       * =route

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
        ["=lua+stage", "=index+rank", "=curl+agent", "=route+route"]
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

