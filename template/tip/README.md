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
  
  1. 처리 요청 이후에 *stage.signal(nil, 3000)* 설정으로 인해 3초가 대기
  2. 티켓이 회수되면 다은 대기 중인 요청으로 전환 됩니다.
    
> ### 3. 데이터 처리를 분산하는 방법

  데이터를 처리하기 위해 다른곳에서 실행중인 백엔드 프로그램으로 메시지를 요청합니다. 그런데, 데이터를 처리할 수 있는 기능이 없다면 데이터를 분산하는 의미가 없어 집니다.
  
  *STAGE:플랫폼*은 이러한 경우 데이터를 처리하기 위한 함수를 같이 전달하여 처리 할수 있습니다.

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
  
