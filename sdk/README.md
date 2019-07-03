## STAGE:SDK

STAGE:플랫폼의 stage 개발에 사용됩니다.

#### SDK로 기본 제공되는 범위

  1. 벨런스된 스레드: stage_submit()
  2. 주기적 처리: stage_addtask()
  3. stage간 통신: stage_signal()

를 통해, 기존에 개발된 stage와 통신을 하고 관리 정책을 참여할 수 있습니다.

#### 빌드 환경

  - Ubuntu 18.04.2 LTS

  Windows 10 이상을 사용하시는 경우에는 [Windows Subsystem for Linux](https://docs.microsoft.com/ko-kr/windows/wsl/install-win10) 을 사용하는 방법도 있습니다.

  배포판(Linux) 설치 이후
  ```console
  $ sudo apt-get install g++ gdb
  ```

#### 제한 사항

  STAGE:플랫폼은 prefork 방식을 사용하고 있어, 직접적인 gdb(디버거) 접근이 불가능합니다.

  프로세서로 연결을 하더라도 직접적인 확인이 어려운 부분이 있어, 실행전에

  ```console
  $ ulimit -c unlimited
  ```

  설정 후, 크래쉬가 발생될때 생기는 core 파일을 통해 디버깅을 진행할 수 있습니다.

  ```console
  $ gdb single core
  ```

#### 적용

개발된 stage는 **build.sh** 를 통해 애드온으로 빌드가 되며,
빌드된 stage는 stage.json에 다음과 같은 형식으로 정의하여 사용할 수 있습니다.

```json
{
	"#startup":[ 
		["./examples.so+example"]
	],

	"./examples.so": {
	}
}
```
