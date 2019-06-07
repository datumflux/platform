## *STAGE:플랫폼:* 사용팁

> ### 1. 변수의 값이 변경될때 감지할 수 있는 방법

  **stage.proxy()** 함수를 사용해 변수의 변경을 추적할 수 있습니다.
  
  * 숫자형은 설정된 값의 범위를 넘어가면 호출됩니다.
  * 문자열 또는 boolean은 일치하면 호출 됩니다.
  
  ```lua
  local DATA = {}
  local data = stage.proxy(DATA, {
      ["count"] = {
          20,
          30,
          function (t, edge_values, key, new_value)
              print("CHANGE", key, edge_values, new_value, t[key])
              for k, v in ipairs(edge_values) do
                  print("---", k, v)
              end
          end
      }
  })

  -- data.count = 10
  data.count = 31
  ```

  의 실행 결과는
  ```console
  CHANGE  count   table: 0x55fb3da33a30   31      nil
  ---     1       20.0
  ---     2       30.0
  ```

> ### 2. 대량의 동시 처리 요청을 관리하는 방법

  **conf/stage.json** 설정에서 *ticket* 에 사용하고자 하는 카테고리명과 동시 허용수를 설정합니다.
  
  ```sh
  "=ticket": [
    100000,
    {
       "login": 10
    }
  ]
  ```
  
  설정에서,
  
  * *100000* 은 최대 티켓 수(대기 + 사용)
  * *"login*" 는 카테고리명
  * *10*은 동시 활성화 갯수
    
  를 나타냅니다.
  
  카테고리 그룹은 추가 설정이 가능합니다.
  
  * 스크립트에서
  
  ```lua
  stage.waitfor("ticket_callback", function (v)
      print("TICKET-------", _TICKET, v)
      stage.signal(nil, 3000)
  end)

  stage.waitfor("FEEDBACK_ID", function (arg)
      print("FEEDBACK", _TICKET, arg)
      if type(arg) == "table" then
          for k, v in pairs(arg) do
              if k == "" then
                  print("--USER_DATA", v)
              else
                  print("---", k, v)
              end
          end
      end
  end)

  stage.signal("ticket:login?FEEDBACK_ID", {
      [""] = "HELLO",
      ["ticket_callback"] = 1024
  })
  ```
  
  로 설정하며, 테스트를 위해 "login"의 값을 1로 변경하였습니다.
  
  ```console
  2019-06-07 21:09:51,830 [INFO ] [ticket:69] TICKET 22355: GRANT - 'stage:ticket_callback'
  TICKET-------   @0x55b3295efbb0 1024
  FEEDBACK        nil     table: 0x55b3295f2d50
  --USER_DATA     HELLO
  ---     ticket_callback true
  FEEDBACK        nil     @0x55b3295f0140
  2019-06-07 21:09:54,840 [INFO ] [ticket:81] TICKET 22355: REVOKE - 'stage:ticket_callback'
  2019-06-07 21:09:54,841 [WARN ] [ticket:299] TICKET 22355: CANCEL - 'stage:ticket_callback'
  2019-06-07 21:09:54,841 [INFO ] [ticket:69] TICKET 22355: GRANT - 'stage:ticket_callback'
  TICKET-------   @0x55b3295f0140 4096
  2019-06-07 21:09:57,853 [INFO ] [ticket:81] TICKET 22355: REVOKE - 'stage:ticket_callback'
  2019-06-07 21:09:57,853 [WARN ] [ticket:299] TICKET 22355: CANCEL - 'stage:ticket_callback'
  ```
  결과를 보면, 
  
  1. 처리 요청 이후에 *stage.signal(nil, 3000)* 설정으로 인해 3초 대기
  2. 티켓이 회수되면 다은 대기 중인 요청으로 전환
  
  됩니다.
  
  이러한 형태로, 대량의 요청이 발생되더라도 *STAGE:플랫폼* 은 부하에 대한 처리를 MQ(Message Queue)와 비슷한 형태로 처리가 됩니다.
  
> ### 3. 데이터 처리를 분산하는 방법

  #### 1. 강제 처리: 원격 실행
  
  *STAGE:플랫폼*은 데이터 처리 요청시, 데이터를 처리할 함수를 같이 전달하여 데이터를 처리할 수 있습니다.

  * *STAGE:플랫폼* - rfc
  ```lua
  stage.waitfor("execute", function (f, a, b)
      stage.signal(nil, f(a, b))
  end)
  ```

  * *STAGE:플랫폼* - stage
  ```lua
  stage.waitfor("result", function (v)
      print("RESULT", v)
  end)

  stage.signal("rfc:execute=result@A", {
      function (a, b)
          return a + b
      end,
      10, 20
  })
  ```
  
  으로 하여 처리를 할수 있습니다. 이 경우, *rfc* 의 이름을 가지고 있는 *STAGE:플랫폼*이 여러개인 경우 로드밸런싱 하여 처리 됩니다.
  
  #### 2. 반응형 처리: 선택적 처리
  
  *STAGE:플랫폼*은 **route** 기능을 통해 처리하고자 하는 데이터를 미리 등록하고, 해당되는 메시지만 처리할 수 있습니다.
  
  *1. 강제 처리* 방법의 경우에는 함수를  같이 전달하면서 동일함수를 계속 전달하는 처리가 필요하다는 부담감이 있다면, 이번에 설명하는 기법은 MQ(Message Queue)의 기능 처럼 처리하고자 하는 역할을 미리 등록해 해당하는 처리만 수행하도록 합니다.
  
  * *STAGE:플랫폼* - rfc
  ```lua
  -- 처리 여부에 대한 응답
  stage.waitfor("route:@", function (id, k)
      print("READY CHECK", id, k)
      stage.signal(nil, k) -- 처리가 가능한 상태라면 응답
  end) 
  
  -- 처리 함수
  stage.waitfor("call0", function (id, v)
      print("CALLBACK", id, v, _WAITFOR[0])
      if v ~= nil then
          stage.signal(nil, "FEEDBACK--" .. v)
      end
  end)

  -- 처리 가능한 기능 등록
  stage.signal("route:@", {
      ["id0"] = "call0",
      ["id1"] = "call0"
  });
  ```

  * *STAGE:플랫폼* - stage
  ```lua
  stage.waitfor("ret0", function (v)
      print("RETURN", v, _WAITFOR[0])
  end)

  stage.signal("route:id0=ret0", "ROUTE MESSAGE");  
  ```
  
  의 형태로 등록을 하고, *stage*에서 처리를 요청하면, 처리가 가능한 *rfc* 에서 처리 후 결과를 반환하게 됩니다.
  *rfc* 가 여러개인 경우 로드벨런싱으로 처리 됩니다. 만약, 이때 모든 *rfc* 로 메시지를 보내고자 한다면
  
  ```lua
  stage.signal("route:*id0=ret0", "ROUTE MESSAGE");  
  ```
  
  와 같이 **\*** 를 추가하여 전달하면 됩니다. 이때는 *rfc* 의 *route:@* 를 통해 처리 가능여부를 확인하지 않습니다.
  
