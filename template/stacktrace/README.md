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

    lua5.1.exe: D:\trunk_git\sources\stacktraceplus\test\test.lua:10: attempt to concatenate a nil value
    stack traceback:
    	D:\trunk_git\sources\stacktraceplus\test\test.lua:10: in function <D:\trunk_git\sources\stacktraceplus\test\test.lua:7>
    	(tail call): ?
    	D:\trunk_git\sources\stacktraceplus\test\test.lua:15: in main chunk
    	[C]: ?
		
다음 방식으로 표시됩니다.

    lua5.1.exe: D:\trunk_git\sources\stacktraceplus\test\test.lua:10: attempt to concatenate a nil value
    Stack Traceback
    ===============
    (2)  C function 'function: 00A8F418'
    (3) Lua function 'g' at file 'D:\trunk_git\sources\stacktraceplus\test\test.lua:10' (best guess)
    	Local variables:
    	 fun = table module
    	 str = string: "hey"
    	 tb = table: 027DCBE0  {dummy:1, blah:true, foo:bar}
    	 (*temporary) = nil
    	 (*temporary) = string: "text"
    	 (*temporary) = string: "attempt to concatenate a nil value"
    (4) tail call
    (5) main chunk of file 'D:\trunk_git\sources\stacktraceplus\test\test.lua' at line 15
    (6)  C function 'function: 002CA480'
