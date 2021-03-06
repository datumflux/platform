# STAGE:플랫폼
> 마이크로서비스로 개발되는 애드온 프로그램을 통합하여 관리하는 플랫폼입니다.

마이크로서비스(이하 *stage 서비스*) 는
  - C/C++ SDK를 이용한 개발
  - 스크립트를 이용한 개발
  
을 통해 확장할 수 있습니다.

>## 시작
 
  스크립트를 사용해 콘텐츠를 구현하는데 유용한 API를 설명하기 위한 문서로, 백엔드 플랫폼의 구현에 필요한 지식을 간결하게 설명하고 콘텐츠를 직접 제작하는데 사용할수 있도록 기술하였습니다.
  
  stage 플랫폼은 lua 스크립트를 사용하여 콘텐츠를 구현하도록 하고 있습니다. lua는 문법이 간결하여 프로그래밍 지식에 따른 결과에 차이가 크게 않고 성능과 안정성을 유지하는데 효과적인 특성을 가지고 있습니다. 
 
  stage에 대한 접근 방법을 이해 하기 위해서는 다음의 특징을 먼저 알아야 힐 필요가 있습니다.

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
     > 만약, 공유 변수 공간이 필요하면 *_SANDBOX[""] 설정을 통해* 사용할 수 있습니다.
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

>## 변화

**STAGE:플랫폼** 은 2019년 8월 20일, 오픈소스로 공개가 되며 이를 통해 다양한 서비스가 개발되고 공유되어, 서버 개발에 어려움을 겪고 있는 개발자와 서버 개발을 원하는 다양한 개발자들에게 도움이 되기를 바랍니다.

> STAGE:플랫폼의 라이센스는 LICENSE 파일을 통해 확인을 할수 있습니다.

* #### STAGE:플랫폼은 다음의 서비스로 구성됩니다.  
   > stage 서비스는 BSON을 사용해 통신을 합니다.
    
   - ticket
   - route
   - curl
   - index
   - **lua** - lua 5.3.5
   - **luajit** - luajit 2.0

   > stage.lua(jit) 에 대한 사용 방법과 자세한 설명은 [API 문서](docs/README.md)에서 확인 하실 수 있습니다.    
     
* #### 개발환경

  - Ubuntu 18.04 LTS
  - 라이브러리
    - log4cxx
    - jsoncpp
    - crypto++
    - curl
    - z
    - zip
    - unixodbc
    - jemalloc
    
  - 빌드 도구
    - gcc/g++
    - CMake
  
* #### 빌드

  1. bin/lib 경로 생성
     > lua 라이브러리가 설치될 경로를 생성합니다.
     
     ```console
     $ mkdir bin/lib/lua
     $ mkdir bin/lib/luajit
     ``` 
     
  1. lua/5.3 빌드
     ```console
     $ ./build_lua.sh
     $ ./build_luv.sh
     $ ./build_openssl.sh     
     ``` 
     
  1. lua/jit 빌드
     ```console
     $ ./build_luajit.sh
     $ ./build_luv.sh
     $ ./build_openssl.sh     
     ``` 

  1. [CLion](https://www.jetbrains.com/clion/)을 사용한 개발
  1. Makefile 생성을 통한 개발
  
     ```console
     $ cmake .
     $ make
     ``` 
     를 통해, Makefile을 생성 후 빌드
       
  1. bin/package/start.lua 생성
     디랙토리에 존재하는 test_....lua 파일을 연결하거나, start.lua를 생성합니다.

     ```console
     $ cd package
     $ ln -s test_broker.lua start.lua
     $ cd ..
     $
     $ ./start.sh
     ``` 
     
>## 문의
  * [데이텀플럭스 주식회사](http://datumflux.co.kr)
  