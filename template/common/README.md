## 일반적인 형태의 서버 스크립트

#### ODBC 연결 구성
  1. MySQL 8.x

#### 파일 설명
  1. **test**  테스트 목적의 스크립트 저장 폴더

     |파일명|기능|
     |:----:|:---|
     |**odbc.lua**|odbc 연결 테스트를 위한 스크립트|

  2. **package**  스크립트 패키지
     |파일명|기능|
     |:----:|:---|
     |**__init.lua**|초기화 수행 스크립트|
     |**v0.lua**|login 전 단계에 수행 가능한 패킷 스크립트|
     |**v1.lua**|login 이후 수행 가능한 패킷 스크립트|

#### TIP

   1. 스크립트 그룹화

      **__init.lua** 에서 사용된 코드 중 *stage.load(socket.depth .. "+" .. data.message)* 의 처리 부분이 있습니다. 이 부분은 하나의 스크립트 파일에 정의된 여러 함수를 연결하는 방법을 나타내고 있습니다.
      
      즉, **v1.lua**의 소스를 보면,

       ```lua
        return {
            ["logout"] = function (socket, data)
                socket.close()
            end,

            ["chat"] = function (socket, data)
                ...
            end
        }
       ```

      로 정의가 되어 있습니다.

      *socket.depth* 는 **v1.lua**를 나타내며, *data.message* 는 스크립트에 정의된 *"logout"* 또는 *"char"* 과 연결합니다.

      물론, 정의되어 있지 않다면 이후에 정의된 function()은 호출되지 않습니다.

    2. *stage.v()* 함수의 역할

       데이터를 받으면 *stage.submit()* 함수를 통해 쓰레드로 연결해 처리를 합니다. 이러한 상황에서 공유가 필요한  변수를 저장해야 하는 경우에 *stage.v()* 함수를 사용합니다.

       해당 함수는 다른 쓰레드에서 동일한 방법으로 접근하는 경우에 접근 제한이 설정될수 있습니다.
