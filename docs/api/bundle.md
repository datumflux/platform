* [bundle](#bundle-ns)
  * [bundle.proxy](#bundle-proxy)
  * [bundle.viewof](#bundle-viewof)
  * [bundle.json](#bundle-json)
  * [bundle.bson](#bundle-bson)
  * [bundle.random](#bundle-random)

>## <a id="bundle-ns"></a> bundle
>
> bundle는 플랫폼의 기본 기능에 접근할수 있도록 구성되어 있습니다.

>### <a id="bundle-proxy"></a> bundle.proxy(t1, t2 \[, function (v)])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 객체 t1에 대한 변화를 관리합니다.
  * **입력**
    * t1 - 객체
    * t2 - 객체 *t1*의 설정 변화에 대한 값을 지정
    * function (v) - *(생략가능)* *t2* 설정이된 *t1*객체를 처리하는 함수 
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> t1 객체
  * **설명**<br>
    예를 들어, "count 변수가 10, 20, 30 의 경계값을 지날때 마다 특정 처리를 수행하도록 콘텐츠를 구성해야 하는 상황이 있다면 어떻게 해야 하는가?" 에 대한 답을 수행하는 함수입니다.

    * t2의 설정 형식

       * 처리 함수(*F*, *F1*, *F2*) - function (t1, edge_values, column_name, new_value)

       |조건|설정|
       |:---:|:---|
       |값의 변경에 대한 처리|컬럼명 = *F* |
       |특정 조건에 대한 설정|컬럼명 = { V1, V2, ..., *F1*, N1, N2, ..., *F2* }|

       의 형식을 사용합니다.<p>
       *특정 조건에 대한 설정*의 경우에 예를 들어 컬럼명을 A라고 하고 V1, V2, N1, N2의 값이  10, 20, 30, 40 이라고 정이되어 있을때, A의 값이 34로 설정되는 조건을 보면
    
       1. bundle.proxy()의 호출 시점에 t1에 A가 12라고 정의되어 있다면
          > *F1*({20, 30}), *F2*({40})
       1. bundle.proxy()의 호출 시점에 t1에 A가 정의되어 있지 않거나 0인 경우라면
          > *F1*({10, 20, 30}), *F2*({40})

       의 형태로 사용이 됩니다. 참고로, *F* 함수 내에서 t1의 값을 변경하는 경우에는 값 변경에 대한 설정이 되어 있어도 처리되지 않습니다.

  * **예제**
    ```lua
    local T = { 
        value = 10,
        level = 1
    }

    local proxy = bundle.proxy(T, {
        ["value"] = { 10, 20, 30, function (proxy, edges, k, v)
            print("CHANGE VALUE",  k, v)
            for i, p in ipairs(edges) do
                print("", i, p)
                proxy.level = math.floor(p / 10)
            end
        end },
        ["level"] = { 1, 2, 3, function (proxy, edges, k, v)
            print("CHANGE LEVEL", k, v)
            for i, p in ipairs(edges) do
                print("", i, p)
            end
        end }
    })

    proxy.value = 30    -- "CHANGE VALUE" value 30
                        --    1    20
                        -- "CHANGE LEVEL" level 2
                        --    1    2
                        --    2    30
                        -- "CHANGE LEVEL" level 3
                        --    1    3
    ```

>### <a id="bundle-viewof"></a> bundle.viewof(format,[start, rows] \[, function (buffer)])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> format 파일의 start부터 rows 크기의 데이터에 직접 접근할 수 있도록 연결합니다.
  * **입력**
    * format - 파일명과 접근하는 rows의 구조체 정보를 설정
    * start - 파일의 시작 위치 (바이트 단위)
    * rows - format에 지정된 구조체 정보를 rows만큼 읽어 들입니다
    * function (v) - *(생략가능)* *t2* 설정이된 *t1*객체를 처리하는 함수 
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 
    * function (v) 가 정의된 경우 - 함수의 반환값
    * 함수가 정의되지 않은 경우 - buffer
  * **설명**<br>

    bundle.viewof() 함수는 mmap()과 연결되는 함수로, 파일에 저장된 데이터를 변경하거나 읽어오는 처리에 사용됩니다.

    함수 사용시 *start, rows*가 지정되지 않은 경우 접근하는 파일 전체를 연결합니다.

    *format*은 직접 파일명이 지정할 수도 있으며 설정을 지정할 수 도 있습니다. 만약, 직접 파일을 접근하게되면 접근 자료형은 바이트 단위로 설정 됩니다.
    
    만약, 정보를 설정하는 경우에는 table형태로 지정이 되며
    
       |자료형|설정 값|설명|
       |:---:|:--:|:--|
       |string|파일명|접근할 파일명을 지정합니다|
       |buffer|bundle.viewof()|연결된 buffer|
       |table|구조체 정보|연결되는 구조체 정보를 지정합니다.|

    의 정보를 설정할 수 있습니다. 여기에서 정의되는 table 정보는 다음의 형태를 가지게 됩니다.

       |자료형|설정 값|설명|
       |:----:|:-----:|:---|
       |number|int8|개별 단위 접근 처리|
       |string|"xNN"|NN 설정을 통해 데이터 길이를 별도로 설정|
      
    이떄 문자열로 설정되는 상세 설정에 자료형을 표시하는 **x** 는 다음과 같습니다.

       |x 값|C 자료형|LUA 자료형|설명|
       |:-----:|:------:|:--------:|:---|
       |c|char []|string|char 배열형태로 문자열 처리|
       |s|char []|string|'\0'로 끝나는 문자열 처리 (마지막에 '\0'이 포함되므로 실제 저장크기는 NN - 1)|
       |b|int8|number|개별 단위 접근 처리|
       |B|uint8|number|개별 단위 접근 처리|
       |h|int16|number|개별 단위 접근 처리|
       |H|uint16|number|개별 단위 접근 처리|
       |i|int32|number|개별 단위 접근 처리|
       |I|uint32|number|개별 단위 접근 처리|
       |l|int64|number|개별 단위 접근 처리|
       |f|float|number|개별 단위 접근 처리|
       |d|double|number|개별 단위 접근 처리|

    여기에서 정의된 구조체의 크기를 기준으로 rows 만큼 연결합니다.

    정의된 설정으로 C/C++에서 생성된 구조체의 데이터를 가져오는 방법은,

    ```c
    #pragma pack(push, 1)
    struct example {
        char name[10];
        int  age;
    };
    #pragma pack(pop)
    ```
    의 구조체를, 다음과 같이 연결해 사용할 수 있습니다.

    ```lua
    local buffer = bundle.viewof({"example.mmap", { "c10 i" }}, 10)

    buffer[2] = { "HELLO", 100 }

    -- buffer[2]에 바이트단위 접근을 위한 연결을 설정  
    bundle.viewof({buffer, buffer.row_size}, buffer.row_size, 1, function (row)
       row[1] = 20 -- 'H' -> ' '
    end) -- 개별 접근
    ```

    인 경우, 구조체로 정의된  *c10* 와 *i* 로 총 14바이트로 정의된 10개의 rows을 연결합니다. 
    
    연결된 데이터는 배열 형태로 접근을 하며, 시작 번호는 **1** 부터 시작이 됩니다. (lua와의 호환성 처리를 위해 **1** 부터 시작됩니다.)

    > buffer로 연결된 이후로 사용 가능한 함수는 

    * buffer.commit() - 수정된 내용 적용 (동기화)
    * buffer.rollback() - 수정된 내용 폐기
    * buffer.scope(["rw" | "ro",] function () ... end) - 접근 제한 설정 후 처리

    > 이며, 읽을 수 있는 값은

    * buffer.row_size - 구조체에 정의된 크기
    * buffer.rows - 연결된 항목 수

    가 있습니다.

    > 해당 함수는 주로, C/C++ 로 직접 연결이 필요한 데이터에 접근할 때 유용 합니다.

>### <a id="bundle-json"></a> bundle.json(p)
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> JSON과 LUA 테이블간의 데이터를 변환 합니다.
  * **입력**
    * p - JSON 문자열 | JSON 파일명 | JSON으로 변환하고자 하는 테이블
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 변환된 값
  * **설명**<br>
    JSON 형태의 문자열 또는 파일을 파싱하여 lua에서 접근 가능한 테이블로 변환하거나, 
    lua형태의 테이블을 json 형태의 문자열로 변환합니다.

  * **예제**
    ```lua
    local json = bundle.json("stage.json")
    local json_str = bundle.json(json)
    ...
    ```

>### <a id="bundle-bson"></a> bundle.bson(p)
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> BSON과 LUA 테이블간의 데이터를 변환 합니다.
  * **입력**
    * p - BSON 데이터 | BSON으로 변환하고자 하는 테이블
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 변환된 값
  * **설명**<br>
    BSON 데이터를 lua에서 접근 가능한 테이블로 변환하거나, 
    lua형태의 테이블을 BSON 데이터로 변환합니다.

    > BSON으로 변환된 데이터를 print()로 출력하는 경우 정상적으로 확인되지 않습니다.

  * **예제**
    ```lua
    local bson = bundle.bson({name = "HELLO"})
    local bson_str = bundle.bson(bson).name
    ...
    ```

>### <a id="bundle-random"></a> bundle.random(t, [s, e] | [function (random)])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>WELL 512 알고리즘을 통해 랜덤값을  생성합니다.
  * **입력**
    * t - 랜덤 테이블
    * s, e - 범위 값 (생략 가능)
    * function (random) - 그룹 함수 
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 랜덤 값
  * **설명**<br>
    개발중에 랜덤을 사용해야 하는 경우가 많이 있습니다. STAGE.LUA는 스레드를 사용해서 운영할 수 있는 특징을 가지고 있으나 기본적으로 제공하는 함수는 스레드 사용에 안전하지 않습니다.

    이를 위해, 별도의 랜덤함수가 제공되며 해당되는 함수는 기본적으로 제공되는 랜덤함수의 단점을 보완하고 균일한 범위의 랜덤값을 생성할 수 있습니다.

    또한, 사용후 t 로 입력된 랜덤테이블은 최근 데이터로 업데이트 되며 이후 랜덤 함수 호출시 마다 사용해 시드값을 유지할 수 있습니다.
    
  * **예제**
    ```lua
    local R = {}
    print("RANDOM", bundle.random(R, 1, 1000))
    print("RANDOM", bundle.random(R, 1, 1000))
    print("RANDOM", bundle.random(R, 1, 1000))
    ...
    ```

    의 형태를 통해, 랜덤시드값을 유지하면서 랜덤을 생성할 수 있습니다.