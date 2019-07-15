## 스크립트를 사용해 개발시 필요한 내용을 기술 합니다.

1. *stage.waitfor()* 에 정의된 함수가 외부에 정의된 변수에 접근할 수 있나요?

   > *STAGE:플랫폼*은 *stage.submit()* 에 연결되는 함수를 제외하고 함수간의 변수 공유가 가능합니다.

   lua 스크립트는 global 영역과 local 영역으로 나누어 집니다. 기본적으로 변수나 함수 선언시 local을 명시하지 않으면 global로 정의됩니다.
   
   ```lua
     local LOCAL_SPACE = {}

     stage.waitfor("test", function (v)
        LOCAL_SPACE[v.k] = v.v
     end)
   ```

   로 정의가 되면 stage.waitfor()에 정의된 함수는 *LOCAL_SPACE* 에 접근할 수 없습니다.

   ```lua
     _G.GLOBAL_SPACE = {}

     stage.waitfor("test", function (v)
        GLOBAL_SPACE[v.k] = v.v
     end)
   ```

   이와 같은 형태는 값이 정상적으로 변경이 됩니다. **_G.** 로 접근이 가능한 범위는 

   **_G.** 로 설정되지 않은 변수는 *stage.addtask()* 또는 *stage.waitfor()* 에 연결된 함수에서 참고할 수 없습니다. (샌드박스 정책)
   만약, 샌드박스 정책을 변경하는 경우에는 **_G.** 설정없이 사용 가능합니다.

   샌드박스 정책은 다음과 같이 변경 가능합니다. 제한을 해제하고 싶은 변수명을 등록하거나 읽기전용 변수를 설정할 수 있습니다. 

   ```lua
     _ENV[""] = {
        "GLOBAL_SPACE", 
        ["MESSAGE"] = "READ-ONLY"
     }
   ```

   샌드박스 정책이 필요한 이유는 *stage.addtask()* 또는 *stage.waitfor()* 에 연결된 함수 내에서 변경되는 값을 공유 때문입니다.

   > 샌드박스 정책과 상관없이 *process.* 형태로 설정되거나 접근되는 변수는 모든 부분(*stage.submit()* 포함)에서 변수가 공유 됩니다.

   * TIP
     1. *stage.addtask()* 에 정의된 함수도 동일한 형태로 접근이 가능합니다.
     1. *stage.submit()* 으로 정의된 함수는 샌드박스로 분리되어 상호 접근이 불가능 합니다.
        > *stage.v()* 또는 *process.* 를 사용해 변수를 공유 할 수 있습니다.
     1. *broker.ready(), broker.join()* 등에서 정의되는 함수도 샌드박스로 분리되어 상호 접근이 불가능 합니다.
   
1. 함수 실행에 많은 시간이 필요한 경우 어떠한 처리 방법이 있나요?

   **첫번째 방법** 으로, *stage.submit()* 는 함수를 쓰레드로 분리하여 실행하는 방법을 통해 실행 지연을 해결합니다.

   주로 데이터를 처리하는데 오랜 시간이 필요한 부분(예를 들면 데이터를 저장하거나 읽어서 처리해야 경우 등)에 많이 사용됩니다.

   격리된 실행환경이라 함수 외부에 존재하는 변수등에 접근할 수 없습니다. 만약 상호 접근이 필요한 경우에는 *stage.v()* 함수를 사용해 공유 공간을 생성할 수 있습니다.

   ```lua
     stage.waitfor("result", function (r)
        print("RESULT", r)
     end)

     -- 0 이 아닌 경우, 동일한 ID에 대한 중복 실행이 제한됩니다.
     stage.submit(0, function (v)
        v.f(v.v)
     end, { 
         ["f"] = function (v)
            print("SUBMIT", v)
            -- 정의된 함수로 결과를 전송합니다.
            stage.signal("result", "SUBMIT OK")
         end,
         "HELLO LUA"
     })
   ```

   **두번째 방법** 인, *stage.submit()* 는 동일한 실행환경에서 쓰레드를 통해 구동이 된다면 다음에 설명되는 *stage.signal()* 기법은 실행 환경과 분리된  다른 실행 환경을 가능하게 합니다.
   
   ```lua
     -- "luajit.so+process" 로 정이된 stage
     stage.waitfor("invoke", function (v)
        -- 반환결과를 되돌려 줍니다.
        --
        -- stage.submit을 사용하지 않는 경우
        --   stage.signal(nil, v.f(v.v))
        --
        stage.submit(0, function (_WAITFOR, v)
            stage.signal(_WAITFOR, v.f(v.v))
        end, _WAITFOR, v)
     end)
   ```

   ```lua
     stage.waitfor("invoke", function (r)
        print("RESULT", r)
     end)

     -- 0 이 아닌 경우, 동일한 ID에 대한 중복 실행이 제한됩니다.
     stage.signal("process:invoke", { 
         ["f"] = function (v)
            print("SUBMIT", v)
            return "SUBMIT OK"
         end,
         "HELLO LUA"
     })
   ```
   > *"=route"* 의 기능을 사용하면 효과적인 로드 분산을 하실 수 있습니다.

   * *stage.single()* 은 다른 STAGE 장비(서버)에 정보를 보내어 실행하게 되므로 현재 구동되는 STAGE 장비(서버)의 CPU등을 사용하지 않습니다.
   * *stage.submit()* 은 같은 STAGE 장비(서버)에서 구동되며 CPU등의 자원을 같이 사용하여 실행 부하가 발생되나 결과 응답이 상대적으로 *stage.signal()* 보다 빠릅니다.
