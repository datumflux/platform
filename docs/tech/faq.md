# FAQ

> ## 처리지연을 관리하기 위한 방법이 있습니까?

  **네. 이미 여러분은 사용하고 계십니다.**
  
  처리지연은 의미 그대로 요청에 의한 처리가 지연되면서 발생되는 문제로 응답속도가 처리지연의 평가 요소가 되기도 합니다.

  단순한 처리만 존재한다면 문제가 없지만, 현실적으로는 불가능하기에 원인을 확인해 보면

  * 동시접근 제한이 필요한 데이터 또는 함수에 처리가 집중되는 경우
  * 데이터베이스와 같은 외부 처리에 의존적으로 개발되는 경우
  
  입니다. 
  
  로그인 과정을 간단한 예로 들어보면 사용자는 로그인을 요청하는 순간 백엔드에서는 기존에 사용자가 이용중인지 확인하고, 없다면 데이터베이스에 정보를 확인 후 사용자 정보를 저장하게 됩니다.
  
  > 처리 순서를 간략히 정리하면

  1. 사용자 정보를 저장하기 위해 접근 메모리에 락(동시 접근 제한)을 설정
  2. 데이터베이스에 사용자 정보 요청
  3. 전달 받은 정보를 메모리에 저장
  4. 로그인 완료 전달

  이 과정에서, 기존의 사용자가 데이터베이스에 업데이트를 요청하는 과정에서 트랜젝션을 통해 업데이트를 하면서 1초가 소요되었다면, **2. 데이터베이스** 의 지연이 발생되고 **1. 사용자 정보 접근**이 제한되는 상황이 발생되면서 모든 콘텐츠 처리는 데드락과 같은 상황(정지)이 발생됩니다.

  극단적으로 보이는 경우지만 실제로 발생되는 상황입니다.

  *STAGE:플랫폼* 은 콘텐츠 개발 시 동시접근을 제한하기 보다는 효과적으로 사용하는 방법을 선택했습니다. 

   - 동시접근이 필요한 데이터는 스냅샷형태로 접근
   - ticket: 동시접근이 필요한 처리에 대한 동시 실행 수 제한
        ```lua
        stage.waitfor("login_callback", function (a)
            print("LOGIN_CALLBACK", a)
        end)

        stage.signal("ticket:login=login_callback", "CHECK")
        ```
        을 통해, **전체 서비스의 동시실행을 제한** 합니다.

   - stage.submit(): 동시실행 제한 설정
        ```lua
        local function run(v)
            print("THREAD RUN", v)
            sleep(2)
        end

        stage.submit(1234, run, "HELLO")
        stage.submit(0, run, "RUN") -- 실행
        stage.submit(1234, run, "WORLD") -- 1234가 실행 중이므로 완료 대기 후 실행
        ```

        *stage.submit()* 실행시 *1234* 고유번호를 지정하여 **프로세서 내에서 동시 실행을 제한** 합니다. -- 고유번호는 임의대로 지정 가능합니다.
        
   - stage.signal(): 외부 실행을 통해 기능을 분산

        ```lua
        stage.waitfor("result", function (r)
           print("RESULT:", r)
        end)

        stage.signal("stage:callback=result", function(v)
            print("THREAD RUN", v)
            return "RESULT:" .. v
        end, "HELLO")
        ```

        을 통해 다른 stage로 함수를 전달해 실행 후 결과만 전달 받습니다.
  
  동시처리에 대한 간섭을 제한하는 과정을 통해 지연이 발생되지 않도록 관리합니다.
   
  > 개발자는 *STAGE:플랫폼*을 사용해 개발하는 과정중에, 처리지연에 대한 관리방법을 사용하게되며 대응(Scale-Up)이 가능한 상황도 이해 하게 됩니다.

> ## 처리지연이 발생되면 *STAGE:플랫폼* 은 어떤 대처를 하나요?

  **물리적인 자원의 해결없이 처리지연을 해결하는 방안은 처리 요청 수를 줄이는 방법입니다.**

  *STAGE:플랫폼*에는 하드웨어의 성능에 따라 유동적으로 결정되는 임계영역이 존재합니다. 해당 위치가 안정적인 응답속도를 유지하기 위한 최소한의 실행 간격으로 해당 수치 이하를 유지하도록 설계되었습니다.
  
  만약, 임계영역을 초과하는 상황이 발생되기 시작하면 *STAGE:플랫폼*은 플랫폼간 네트워킹에서 이탈하고 내부 처리에 집중하게 합니다. -- 오직 *STAGE:플랫폼*은 내부에서 필요한 기능에만 집중하며 안정화 될때까지 상태를 유지합니다.

  * 플랫폼 네트워킹에서 이탈
  * stage.addtask() 처리 중지
  
  > 임계영역은 메시지 처리요청으로 인해 발생되는 지연을 측정하는 방식으로, stage.json 설정의 **#route** 설정 값에 따라 민감도가 변경됩니다. (수치가 작을 수로 민감해 집니다.)

