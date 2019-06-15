> ## [StackTracePlus](https://github.com/ignacio/StackTracePlus)
> 기본적으로 제공되는 오류정보를 확장하여 상세하게 표시합니다.

> ### 변경 이력  
  1. 2019-06-16 
     > - STAGE:플랫폼의 스크립트 파일 관리방식의 차이로 인해 디버깅 정보에 소스 연결 처리 제거

> ### 설치
  *lib/stacktrace.lua* 와 *preload/stacktrace.lua* 을 STAGE:플랫폼을 설치 경로에 폴더명을 포함하여 복사합니다.
  
  이후, 재시작 하면 기능이 할성화 됩니다.

> ### 설명

기존의 오류 정보가 

 ```console
2019-06-16 01:42:15,035 [WARN ] [lua:2033] [string "package/start.lua"]:25: attempt to call a nil value (field 'v')
stack traceback:
	[string "package/start.lua"]:25: in function <[string "package/start.lua"]:23>
 ```
		
다음 방식으로 표시됩니다.

 ```console
2019-06-16 01:40:49,476 [WARN ] [lua:2033] [string "package/start.lua"]:25: attempt to call a nil value (field 'v')
Stack Traceback
===============
(2) Lua function 'nil' at line 25 of chunk '"package/start.lua"]'
	Local variables:
	 T = table: 0x7fcde400a580  {1:10, 2:20, name:HELLO}
	 (*temporary) = nil
	 (*temporary) = string: "USER"
	 (*temporary) = Lua function 'nil' (defined at line 25 of chunk "package/start.lua"])
	 (*temporary) = string: " (field 'v')"
	 (*temporary) = string: "attempt to call a nil value (field 'v')"
```
