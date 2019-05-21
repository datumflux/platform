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

* 프로퍼티
  * [process](#process-property)
  * [thread](#thread-property)


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
    |:---:|:---:|:---:|
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

     * 메시지를 보내는 현재 stage_id를 포함하여 전달하고자 하는 경우에는 *+* 를 사용해 *s1* 을 구성할수 있습니다.
     
          target_stage_id+메시지ID

       > "target_stage_id:current_stage_id:메시지ID" 로 전달 됩니다.

       target_stage에서는 stage.waitfor()를 "current_stage_id:메시지ID"로 대기 설정하여 메시지를 처리 할 수 있습니다. 
       
        ```lua
        stage.waitfor("stage:callback", function (v)
           print("STAGE ", v.msg)
        end)

        stage.waitfor("process:callback", function (v)
           print("PROCESS ", v.msg)
        end)

        -- stage에세 전송 합니다.
        stage.signal("process+callback", {
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
          ["=curl+agent"],
          ["=route+route"]
        ],
     ```
     처럼, stage 이후에 (**+** 문자) 지정된 문자열을 나타냅니다.<br>
     만약, 지정되지 않으면 다른 stage에서 메시지를 보낼 수 없습니다. (단, 요청에 대한 응답은 제외 입니다)

     > *["=lua+stage"]* 와 *["=lua+process"]* 는 별도의 프로세서로 분리되어 실행됩니다.

     > *["=lua+stage", "=index+rank"]* 라고 정의되면 하나의 프로세서에 두개의 stage가 실행됩니다.

     * index로 메시지 전달
       점수(스코어)를 기준으로 실시간 순위를 처리해야 하는 경우가 있습니다. 이러한 처리를 위한 기능으로, 일반적으로 스코어보드라고 합니다.<br>

       사용 방법은 비교적 간단하게 정의되어 있으며 그에 대한 접근 방법은 다음과 같습니다.

       > index는 데이터를 실시간으로 처리하는 기능을 합니다.

       요청은 "명령:색인파일/TIER" 형태로 요청을 합니다. TIER는 값을 저장할 수 있는 영역으로 객체 당 4개를 가질 수 있습니다. (숫자로 저장되는 값은 큰 값이 상위로 정렬이 됩니다)

       *TIER* 가 생략되면 1이 기본으로 선택됩니다. 범위는 1 ~ 4로 설정 할 수 있습니다. 

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

        HTTP(S)를 통해 외부와 연동이 필요한 경우 사용할 수 있습니다. RESTful과 같은 인페이스 연동과 같은 경우가 대표적인 경우로 주로, 결제에 대한 정보 또는 푸쉬에 대한 데이터 연동을 위한 경우가 이에 해당합니다.
        > 최근 Google또는 NAVER 같은 많은 회사에서 RESTful형태의 API를 통해 다양한 서비스를 제공하고 있습니다.

        기본적인 사용법은 다음과 같습니다.

        ```lua
        stage.signal("agent:callback", {
            ["url"] = "http://www.naver.com"
        });
        ```

        1. 요청

          |컬럼명|자료형|설명|
          |:---:|:---:|:---|
          |url|string|요청하는 URL|
          |header|object| { NAME = VALUE } 형태로 헤더 설정|
          |data|string, object| POST로 전송할 데이터|
          |timeout|number|처리 제한 시간|
          |extra|object|결과 전송시 받을 데이터|

        2. 반환

          |컬럼명|자료형|설명|
          |:---:|:---:|:---|
          |url|string|요청된 URL|
          |redirect_url|string|리다이렉트된 URL|
          |effective_url|string|처리된 URL|
          |status|number|상태 값 (200 - OK)|
          |cookie|array|전송된 쿠키값 리스트|
          |extra|object|결과 전송시 받을 데이터|
          |data|object|전달 받은 데이터|

          * data 정보

            |컬럼명|자료형|설명|
            |:---:|:---:|:---|
            |type|string|데이터 형태|
            |length|number|data의 길이|
            |download_speed|number|다운로드 속도|
            |content|string|데이터|
            |charset|string|데이터의 Character Set|
            |result|number|처리 결과|
            |message|string|처리 결과|
            |time|number|처리된 경과 시간|

     * route로 메시지 전달

       전달되는 메시지를 필터링하여 전달 받을 수 있습니다.<br/>

       > "route" stage에 *"subscript_id"* 로 전달하면, 연결된 stage로 전달합니다. 메시지를 전달 할수 없는 상황이면 메시지를 소멸 됩니다.<br>
       > -- 만약, 해당 경우에 대한 callback 설정이 되어 있다면 결과를 반환 합니다. <p/>
       > 메시지를 발생시키는 stage를 A, 메시지를 받으려고 "route"에 등록한 stage를 B라고 할때, B가 받은 메시지의 반환 주소는 A로 설정이 됩니다.

       1. 라우팅 등록

        > 설정되는 "callbac_id"는 중복이 허용됩니다.
        > *stage.waitfor()* 로 설정되는 callbac_id를 의미합니다.

        등록되는 callback 함수는 첫번째 인자로 요청한 subscribe_id가 전달됩니다.

        메시지를 처리하는 callback_id의 반환 결과를 return_id로 받을 수 있도록 설정 할 수 있습니다.

        ```lua
          -- route에서 메시지 처리여부를 확인하기 위해 호출됨.
          stage.waitfor("route:@", function (id, k)
             -- id에 대해 처리를 원하는 경우
             stage.signal(nil, k)
          end)

          stage.waitfor("callback_id", function (id, arg)
             ...
          end)

          stage.signal("route:@", {
            "subscribe_id_0": "callbac_id",
            "subscribe_id_1": "callbac_id",
            "subscribe_id_2": "callbac_id",
            "subscribe_id...": "callbac_id",
          })
        ```

       2. 라우팅 일시 정지/해제

        > 만약, false로 해당 "subscribe_id"를 설정하면 다시 true로 설정하기 전까지 메시지를 받을 수 없습니다.<br/>
        > msec를 설정하는 경우, 해당 시간이 경과 한 이후에 다시 활성화 됩니다.

        ```lua
          stage.signal("route:@", {
            "subscribe_id": true | false | msec
          })

          stage.signal("route:@", msec)
        ```

       3. 라우팅 요청 해제

        ```lua
          stage.signal("route:@", {
            "subscribe_id": nil
          })

          stage.signal("route:@", nil)
        ```

       4. 라우팅 메시지 전달

        ```mermaid
        graph LR;
          START --> A(route:subscribe_id);
          A --> END
          A --> B(=return_callback)
          B --> C
          C --> END
          A --> C(!fail_callback)
          C --> B
          B --> END
        ```

        ```lua
          stage.signal("route:subscribe_id", value)
        ```

        subscribe_id 수신하고자 하는 stage가 여러 개인 경우 받을 수 있는 조건이 되는 하나의 stage에 전달됩니다.<p/> 
        만약, 모든 stage에 전달하고자 한다면 *"route:*subscribe_id"* 로 전달 하면 됩니다.

        ```mermaid
        sequenceDiagram
          participant 요청 stage
          participant route stage
          participant 처리1 stage
          participant 처리2 stage

          요청 stage ->> route stage: "route:subscribe_id"
          opt 직접 전달 요청
             route stage -->> 처리1 stage: "처리 가능?"
             route stage -->> 처리2 stage: "처리 가능?"
             처리1 stage -->> route stage: "처리 가능"
          end
          route stage ->> 처리1 stage: "처리 요청"
          처리1 stage ->> 요청 stage: "처리 결과"
        ```

        > *"!fail_callback_id"* 는 메시지를 받을 수 있는 stage가 없는 경우 반환되는 callback_id 입니다. 

       5. 라우팅 메시지 유지

        ```lua
          stage.signal("route:@", true)
        ```

        최소 30초 이내에 주기적으로 전달되어야 합니다. 만약, 전달되지 않는다면 route에서 제거됩니다.

  * **예제**
    ```lua
       stage.waitfor("fetch", function (id, v) 
          ...
          stage.signal(nil, "FEEDBACK")
       end)

       stage.waitfor("r__fail", function (id, v)
          print("ROUTE FAIL", id, v)
       end)

       ...
       stage.waitfor("message", function (_self, v)
          print("MESSAGE", v)
       end, {...})
       ...
       stage.signal("message", "HELLO")

       stage.waitfor("route:@", function (id, v)
         -- 만약, "route:" 에서 보내는 데이터를 처리되길 원하지 않는 경우에는 stage.signal()로 반환하지 않으면 됩니다.
         stage.signal(nil, v)
       end)

       stage.signal("route:@", {
          ["login"] = "fetch"
       })
       stage.signal("route:login!r__fail", "HELLO")
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
       |:---:|:---:|:---|
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
       |:---:|:---|
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
    |:---:|:---|
    | %l  |**%F [%M:%L]** |
    | %F  |  파일 이름 |
    | %M  |  함수 이름 |
    | %L   | 위치 (라인위치) |
    | %{*ENV*}&nbsp; |   환경변수 |
    | %{=log4cxx:level}&nbsp; | 출력 정보 변경  |

    *%{=log4cxx:level}* 을 사용하여, 출력시 로그 레벨을 변경할수 있습니다.
    기본적으로 제공되는 로그 레벨은 다음과 같습니다. (log4cxx.out의 기본 로그 레벨은 info와 동일합니다)

    | 로그 레벨 | 설명 |
    |:---:|:---|
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

>## <a id="process-property"></a> process

  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  구동중인 프로세서의 정보를 확인합니다.
  * **설명**<br>    
    프로세서에 대한 정보를 확인할수 있습니다.  

    * 읽을 수 있는 정보
    
    |이름|자료형|설명|
    |:---:|:---:|:---|
    |id|i|프로세서 아이디|
    |tick|d|초단위 시간|
    |args|t|프로그램 실행 인자|
    |stage|i|스테이지 번호|
    |%*NAME*|v|환경설정에 정의된 값|
    |$*NAME*|v|설정 파일에 정의된 값|
    |*NAME*|v|사용자 정의 값|

    * 기록할 수 있는 정보
    
    |이름|자료형|설명|
    |:---:|:---:|:---|
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

    * 읽을 수 있는 정보
    
    |이름|자료형|설명|
    |:---:|:---:|:---|
    |id|i|쓰레드 아이디|
    |tick|d|초단위 시간|
    |*NAME*|v|사용자 정의 값|

    * 기록할 수 있는 정보
    
    |이름|자료형|설명|
    |:---:|:---:|:---|
    |*NAME*|v|사용자 정의 값

  * 참고
    * [process](#process-property)

  * **예제**
    ```lua
      print(process.id .. ": Initialize... STAGE[" .. process.stage .. "]");
    ```

