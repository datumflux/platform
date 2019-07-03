## STAGE:SDK

STAGE:플랫폼의 stage 개발에 사용됩니다.

#### SDK로 기본 제공되는 범위

  1. 벨런스된 스레드: stage_submit()
  2. 주기적 처리: stage_addtask()
  3. stage간 통신: stage_signal()

를 통해, 기존에 개발된 stage와 통신을 하고 관리 정책을 참여할 수 있습니다.

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
