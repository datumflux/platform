* [odbc](#odbc-ns)
  * [odbc.new](#odbc-new)
  * [odbc.cache](#odbc-cache)
  * [odbc.purge](#odbc-purge)<p>
  * [adapter.execute](#adapter-execute)
  * [adapter.apply](#adapter-apply)
  * [adapter.begin](#adapter-begin)
  * [adapter.commit](#adapter-commit)
  * [adapter.rollback](#adapter-rollback)
  * [adapter.close](#adapter-close)<p>
  * [resultset.fetch](#resultset-fetch)
  * [resultset.erase](#resultset-erase)
  * [resultset.apply](#resultset-apply)

>## <a id="odbc-ns"></a> odbc

>### <a id="odbc-scope"></a> odbc.new(\[s | t] \[, function (adapter)])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> odbc를 통해 데이터베이스와 연결합니다. 
  * **입력** 
    * s - 문자열 형태의 연결 정보
    * t - 설정 형태의 연결 정보
    * function (adapter) - 연결이 완료되면 호출되는 함수
  * **반환**
    * f가 정의된 경우 - f 의 반환 값
    * f가 정의되지 않은 경우 - adapter 
  * **설명**<br>
    
    *s*로 설정되는 연결정보는 *ODBC Connection String* 또는 설정 파일의 *%odbc*에 정의된 이름을 지정합니다.

      |데이터베이스|참고 사이트 |
      |:---:|:---|
      | mysql| [Connector/ODBC](https://dev.mysql.com/doc/connector-odbc/en/connector-odbc-configuration-connection-without-dsn.html)|
      | mssql| [DSN 및 연결 문자열 키워드 및 특성](https://docs.microsoft.com/ko-kr/sql/connect/odbc/dsn-connection-string-attribute?view=sql-server-2017)|

    *t*는 *ODBC Connect String*을 *%odbc*에 정의되는 정보 형태로 설정한 값 입니다.
    > ODBC Connect String에 정의된 이름을 그대로 사용하시면 됩니다.

    다음에 나열되는 설정은 추가설정입니다.

      | 이름 | 자료형 | 설명 |
      |:---:|:---:|:---|
      | .executeTimeout | i | SQL 실행 타임 아웃 값 (msec) |
      | .connectionTimeout | i | 데이터베이스 연결 타임 아웃 값 (msec) |
      | .loginTimeout | i | 로그인 타임 아웃 값 (msec) |
      | .queryDriver | s | 데이터베이스 유형 (mysql, mssql) |

    ```lua
       local adp = odbc.new({
         ["DRIVER"] = "MySQL ODBC 8.0 ANSI Driver", -- /etc/odbcinst.ini에 정의
         ["SERVER"] = "localhost",
         ["DATABASE"] = "DF_DEVEL",
         ["USER"] = "test",
         ["PASSWORD"] = "test&&",
         [".executeTimeout"] = 1000,
         [".queryDriver"] = "mysql"
       })
    ```
    데이터베이스의 유형 (*.queryDriver*)는 쿼리에 컬럼의 이름을 표시하는 방법과 자동증가된 값을 얻는 방법등의 차이를 구분하기 위해 사용되는 값 입니다.

      *mysql*은 자동증가된 값을 얻기 위해 *"SELECT LAST_INSERT_ID()"* 를 사용하지만,<br>
      *mssql*은 "SELECT SCOPE_IDENTITY()"를 사용합니다.

      > 추가적인 지원이 필요하신 경우 support@datumflux.co.kr 로 요청 부탁드립니다.

    *function (adapter)* 가 정의되어 있다면, 데이터베이스에 연결후 해당 함수를 호출합니다.

  * 참고
    * [odbc.cache](#odbc-cache)
    * [odbc.purge](#odbc-purge)

  * **예제**
    ```lua
        function tprint (tbl, indent)
            if not indent then indent = 0 end
            for k, v in pairs(tbl) do
                formatting = string.rep("  ", indent) .. k .. ": "
                if type(v) == "table" or type(v) == "userdata" then
                    print(formatting)
                    tprint(v, indent+1)
                else
                    print(formatting .. v) 
                end 
            end 
        end

        odbc.new("DF_DEVEL", function (adp)
            local rs = adp.execute("SELECT * FROM DEMO");
            local rows = rs.fetch({"user"});
            print("SELECT", rs, rows);
            tprint(rows);

            -- rows["TEST"] = nil;
            rows["TEST"] = {        -- user varchar(40)
                ["name"] = "HELLO", -- name varchar(80)
                ["age"] = 10,       -- age int
                ["memo"] = {        -- memo blob
                    ["blob"] = "HAHA"
                }
            };

            rs.apply(rows, "DEMO", function (ai_k, log)
                print("COMMIT", ai_k, log);
                tprint(ai_k);
                tprint(log);
            end);
        end);
    ```

>### <a id="odbc-cache"></a> odbc.cache(s \[, function ()])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  *s*로 캐싱된 데이터가 없는 경우 *function*을 호출합니다.
  * **입력** 
    * s - 고유 이름
    * function () - 데이터가 없는 경우 호출됩니다.
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 캐싱된 값
  * **설명**<br>    
    
    반복되는 동일한 요청으로 인해 데이터베이스의 사용률에 높아지게 되면 다른 처리에 지연이 발생됩니다. 이러한 상황이 발생되지 않도록 동일한 요청에 대한 데이터를 캐싱하여 데이터베이스의 부하를 줄일수 있습니다.
    
    *odbc.cache*는 *function()* 의 반환값을 값을 저장하게 되는데 저장 가능한 데이터는

    * resultset.fetch()의 결과
    * lua 자료형 (문자열, 숫자, 테이블 등)

    을 저장하며, 캐싱된 데이터를 제거하기 위해서는 *odbc.purge*를 사용해 제거 할수 있습니다.
    
  * 참고
    * [odbc.new](#odbc-new)
    * [odbc.purge](#odbc-purge)

  * **예제**
    ```lua
       ...
        odbc.cache("IDLE", function ()

            return odbc.new(function (adp)
              local rs = adp.execute("SELECT 1 as sum");
              local rows = rs.fetch();
              for k, v in pairs(rows) do
                  print("SELECT", k, v);
              end
              return rows;
           end
        end);
    ```

>### <a id="odbc-purge"></a> odbc.purge([s, t])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  데이터를 제거합니다.
  * **입력** 
    * s - 고유 이름
    * t - 캐싱된 고유 이름 목록
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>    
    
    *odbc.cache*를 통해 저장된 데이터를 제거하는 함수로, *s* 또는 *t*를 정의하지 않고 호출하는 경우 저장된 모든 캐싱 데이터를 제거합니다.
        
  * 참고
    * [odbc.new](#odbc-new)
    * [odbc.cache](#odbc-cache)

  * **예제**
    ```lua
       ...
        odbc.purge({ "IDLE" })
    ```

>### <a id="adapter-ns"></a> adapter

>### <a id="adapter-execute"></a> adapter.execute(\[s | t] \[, v... ])

  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 데이터베이스로 명령을 전달합니다.
  * **입력** 
    * s - SQL 명령
    * t - SQL 명령과 사용되는 자료형을 지정
    * v... - SQL에 전달하는 값
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> resultset
  * **설명**<br>    
    
    *s*는 SQL 명령으로 "SELECT...", "UPDATE...", "INSERT...", "CALL ..." 등의 명령을 포함합니다. 
    
    이 중에, SELECT, UPDATE, INSERT등에서 자료형을 명확히 선언해야 하는 경우가 있을수 있습니다. 이러한 경우 사용되는 방법이 *t*를 사용하는 방법입니다.
    -- 스크립트 언어의 특성상 대표되는 자료형만 지원하고 있어 이를 명시하기 위한 방법입니다.
    
    두 명령의 차이를 보면

    * *"SELECT * FROM USER WHERE user_id = ? AND password = ?"*
    * { *"SELECT * FROM USER WHERE user_id = ? AND password = ?"*, "C", "C" }&nbsp; 

    로 구분할수 있습니다. <br>

    ```lua
       local rs = adp.execute({
         "SELECT * FROM USER WHERE user_id = ? AND password = ?", "C", "C" 
         }, "USER_ID", "PASSWORD")
    ```

    *t*는 정의할때 처음 위치에는 SQL 명령을 다음 위치 부터는 SQL 명령에 포함된 ? 에 지정할 SQL 자료형을 설정하면 됩니다.

    | format | SQL TYPE | 값 형태 |
    |:---:|:---:|:---|
    | W | WVARCHAR | 제한없음 |
    | w | WCHAR | 제한없음 |
    | C | VARCHAR | 제한없음 |
    | c | CHAR | 제한없음 |
    | B | BINARY | 제한없음 |
    | b | TINYINT | 제한없음 |
    | s | SHORT | 제한없음 |
    | i | INT | 제한없음 |
    | l | LONG | 제한없음 |
    | T | UNSIGNED TINYINT | 제한없음 |
    | S | UNSIGNED SHORT | 제한없음 |
    | I | UNSIGNED INT | 제한없음 |
    | L | UNSIGNED LONG | 제한없음 |
    | f | FLOAT | 제한없음 |
    | d | DOUBLE | 제한없음 |
    | D | DATETIME | 문자열의 경우 "YYYY-MM-DD HH:MM:SS" |

  * 참고
    * [odbc.new](#odbc-new)
    * [odbc.cache](#odbc-cache)

  * **예제**
    ```lua
       ...
        odbc.cache("IDLE", function ()

            return odbc.new(function (adp)
              local rs = adp.execute("SELECT 1 as sum");
              local rows = rs.fetch();
              for k, v in pairs(rows) do
                  print("SELECT", k, v);
              end
              return rows;
           end
        end);
    ```

>### <a id="adapter-apply"></a> adapter.apply(row \[, s] \[, function (t1, t2)])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  변경된 항목을 적용합니다.
  * **입력** 
    * row - resultset.fetch()를 통해 반환된 객체와 연결된 객체
    * s - *(생략 가능)* 적용할 테이블 명
    * function (t1, t2) - *(생략 가능)* 변경 정보를 전달할 함수
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> function (t1, t2)의 반환값
  * **설명**<br>    
    
    SELECT 이후 *resultset.fetch*를 사용해 얻은 결과를 수정한 경우, 해당 내용을 데이터베이스에 적용해야 하는경우 사용될 수 있습니다.
    > row는 *resultset.fetch()*의 결과 뿐만 아니라 row[*KEY*] 로 반환된 객체도 가능합니다. (값은 불가능)

    변경된 내용은 값의 변경 유무에 따라서 INSERT,UPDATE,DELETE로 처리가 됩니다.<br>
    그에 대한 정보는 *function (t1, t2)* 로 전달이 되며 *t1*은 자동증가되는 컬럼이 추가되는 경우 추가된 값을,
    *t2*는 다음 테이블의 변경된 로그 정보를 가지게 됩니다.

    |컬럼명|자료형|설명|
    |:---:|:---:|:---|
    |table|s|테이블명|
    |action|i|2: 업데이트, 4: 추가, 8: 삭제  |
    |where|t| { "COLUMN": 값, ... }&nbsp; |
    |value|t| { "o": 이전값, "n": 새로운값, "t": SQL_TYPE }&nbsp;|

    전달 되는 정보를 바탕으로 로깅을 할수 있습니다.

    * **주**

      1. 데이터베이스 종류에 따라 테이블 명을 확인할수 없어 명시가 필요한 경우가 있습니다.
         > MS-SQL을 사용하는 경우에는 테이블 명을 명시해야 합니다.
      1. SELECT 쿼리에서 JOIN 쿼리는 테이블 명을 명시해야 합니다.
      1. 적용되는 컬럼이 *s* (테이블명)에 포함되지 않은 경우 SQL오류가 발생될수 있습니다.

  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.execute](#adapter-execute)
    * [resultset.apply](#resultset-apply)

  * **예제**
    ```lua
       ...
        local rows = odbc.cache("USER")
        if rows ~= nil then
          ...
          odbc.new("DF_DEVEL", function (adp)
            adp.apply(rows, "USERS")
          end)
        end
    ```

>### <a id="adapter-begin"></a> adapter.begin( \[function (adapter)] )
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  트랜젝션을 시작합니다.
  * **입력** 
    * function (adapter) - *(생략 가능)* 트랜젝션 처리 후 호출되는 콜백 함수
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> function의 반환
  * **설명**<br>    
    
    데이터베이스의 트랜젝션을 시작합니다. 

    만약, *function (adapter)* 를 사용하는 경우 *adapter.commit()* 또는 *adapter.rollback()* 의 처리를 *function (adpater)* 의 반환값으로 대체 가능합니다.
        
    ```lua
       ...
        adp.begin(function (adp)
           ...
           return true; -- true: commit, false: rollback
        end)
    ```

    * **주**    
      1. 트랜젝션이 설정되면 동일한 데이터베이스에 접근에 제한이 됩니다.
      1. 트랜젝션 설정 후 처리 시간이 지연되면 데이터베이스의 사용률이 올라가 문제가 발생될수 있습니다.
      1. 트랜젝션이 설정되면 이후 UPDATE, INSERT등 SQL명령은 *adapter.commit()* 이후 데이터베이스에 적용 됩니다.
         > *adapter.rollback()* 은 트랜젝션 이후의 SQL명령을 취소합니다.

  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.commit](#adapter-commit)
    * [adapter.rollback](#adapter-rollback)

  * **예제**
    ```lua
       ...
       adp.begin()
    ```

>### <a id="adapter-commit"></a> adapter.commit()
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  트랜젝션 중에 처리된 변경 내용을 적용합니다.
  * **입력** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>    
    
    *adapter.begin()* 이후에 적용된 SQL명령을 데이터베이스에 적용 합니다.
    
  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.begin](#adapter-begin)
    * [adapter.rollback](#adapter-rollback)

  * **예제**
    ```lua
       ...
       adp.commit()
    ```

>### <a id="adapter-rollback"></a> adapter.rollback()
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  트랜젝션 중에 처리된 변경 내용을 취소합니다.
  * **입력** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>    
    
    *adapter.begin()* 이후에 적용된 SQL명령을 취소 합니다.
    
  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.begin](#adapter-begin)
    * [adapter.commit](#adapter-commit)

  * **예제**
    ```lua
       ...
       adp.rollback()
    ```

>### <a id="adapter-close"></a> adapter.close()
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 데이터베이스 연결을 닫습니다.
  * **입력** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>    
    
    *odbc.new()* 로 연결된 정보를 해제 합니다. 
    > 해당 함수를 호출하지 않아도 처리된 함수를 벗어나면 자동으로 해제됩니다.
    
    해제 될때, 완료되지 않은 트랜젝션은 *adapter.commit()* 이 됩니다.
    
  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.commit](#adapter-commit)

  * **예제**
    ```lua
       ...
       adp.close()
    ```

>### <a id="resultset-ns"></a> resultset

>### <a id="resultset-fetch"></a> resultset.fetch( \[t | s] \[, function (row)])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 실행된 명령에서 결과를 얻습니다.
  * **입력** 
    * t - 결과의 정렬을 원하는 컬럼 목록
    * s - 정렬을 원하는 컬럼명
    * function (row) - *(생략 가능)* 결과에 대한 콜백 함수
  * **반환** 
    * f가 정의된 경우 - f 의 반환 값
    * f가 정의되지 않은 경우 - row 
  * **설명**<br>    

    *adapter.execute* 에서 실행된 결과를 얻기 위한 함수로, 주로 "SELECT"이후에 사용되는 명령입니다. 해당 명령을 통해 얻어진 결과를 테이블 형태로 직접 접근하도록 변환합니다.

    > *s* 또는 *t*에 지정될수 있는 컬럼은 SELECT에서 얻어진 컬럼 명이어야 합니다.    
    > *s* 또는 *t*의 컬럼명에 *$*를 지정하게 되면 ROW 일련번호를 기준으로 결과를 얻어 옵니다. (0, 1, 2...)
    
    만약, *s* 또는 *t*가 지정되지 않는 경우 마지막 정보만 반환됩니다.

    row의 컬럼명은 SELECT를 통해 얻어진 컬럼을 사용합니다.
    > 만약, *adp.execute("SELECT 1 as B")* 로 요청을 하면 컬럼명은 "B"가 됩니다.
    
    ```lua
       ...
       local rows = rs.fetch({ "userid", "$" });

       print("USERNAME", rows["USER_ID"][0].username)
    ```

    반환된 row 정보는 테이블에 접근하듯이 사용할수 있으며 값을 추가 하거나, 수정 및 삭제를 할수 있습니다. 
    > 해당 처리 후 *resultset.apply* 또는 *adapter.apply*를 통해 데이터베이스에 적용 할수 있습니다.

  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.execute](#adapter-execute)
    * [adapter.apply](#adapter-apply)
    * [resultset.apply](#resultset-apply)

  * **예제**
    ```lua
       ...
       local rs = adp.execute("SELECT 1 as sum");
       local rows = rs.fetch();
       for k, v in pairs(rows) do
            print("SELECT", k, v);
       end       
    ```

>### <a id="resultset-erase"></a> resultset.erase(row, [s | t])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 얻어진 결과에서 *t* 를 삭제합니다.
  * **입력** 
    * row - resultset.fetch()의 반환 객체에 연결된 객체
    * s - 삭제를 원하는 객체의 값
    * t - 삭제를 원하는 객체가 위치한 값의 진입 순서
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 제거된 갯수
  * **설명**<br>    

    *s* 는 row에서 직접 접근이 가능한 값을 지정하며,<br>
    *t* 는 row에서 여러 단계에 걸쳐서 접근해야 하는 위치의 값을 지정하는데 사용됩니다.

    > row는 *resultset.fetch()* 의 결과 뿐만 아니라 row[*KEY*] 로 반환된 객체도 가능합니다. (값은 불가능)

       예를 들어,
       
       | 0 | 1 | 2 | 3 | ... |
       |:---:|:---:|:---:|:---:|:---:|
       | A0 | B0 | C0 | D0 | ... |
       | A0 | B0 | C1 | D1 | ... |

       로 저장된 값에서 D1을 삭제하기 위해서는 
       ```lua
          rs.erase(rows, { "A0", "B0", "C1", "D1" })
       ```

       "B0"의 모든 하위 값을 삭제하고자 한다면
       ```lua
          rs.erase(rows, { "A0", "B0" })
       ```
       
       으로 삭제 할수 있습니다.

       물론, 삭제하는데 있어 
       ```lua
          rows.A0.B0.C1.D1 = nil
          -- 동일한 rows["A0"]["B0"]["C1"]["D1"] = nil
       ```

       이나, 
       ```lua
          rows.A0.B0 = nil
          -- 동일한 rows["A0"]["B0"] = nil
       ```
       도 동일한 결과를 같습니다.

       해당 함수를 존재 이유는 하위 객체 접근에 대한 편의성을 위해 제공됩니다.

  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.execute](#adapter-execute)
    * [adapter.apply](#adapter-apply)
    * [resultset.apply](#resultset-apply)

  * **예제**
    ```lua
       ...
       local rs = adp.execute("SELECT 1 as sum");
       local rows = rs.fetch();
       for k, v in pairs(rows) do
            print("SELECT", k, v);
       end       
    ```

>### <a id="resultset-near"></a> resultset.near(row, [s | t])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 얻어진 결과에서 *t* 위치의 데이터를 얻습니다.
  * **입력** 
    * row - resultset.fetch()의 반환 객체에 연결된 객체
    * s - 얻고자 하는 객체의 값
    * t - 객체가 위치한 값의 진입 순서
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 객체
  * **설명**<br>    

    *s* 는 row에서 직접 접근이 가능한 값을 지정하며,<br>
    *t* 는 row에서 여러 단계에 걸쳐서 접근해야 하는 위치의 값을 지정하는데 사용됩니다.

    > row는 *resultset.fetch()* 의 결과 뿐만 아니라 row[*KEY*] 로 반환된 객체도 가능합니다. (값은 불가능)

       예를 들어,
       
       | 0 | 1 | 2 | 3 | ... |
       |:---:|:---:|:---:|:---:|:---:|
       | A0 | B0 | C0 | D0 | ... |
       | A0 | B0 | C1 | D1 | ... |

       로 저장된 값에서 D1을 얻고자 한다면 
       ```lua
          local r = rs.near(rows, { "A0", "B0", "C1", "D1" })
       ```

       "B0"의 모든 하위 값을 얻고자 한다면
       ```lua
          local r = rs.near(rows, { "A0", "B0" })
       ```
       
       또 다른 접근 방법인
       ```lua
         print(rows["A0"]) -- 없다면 추가로 처리
       ```
       와 같은 접근에서는 *"A0"* 객체가 없으면 생성이 되지만, **rs.near()** 는 생성되지 않습니다.

       **rows** 객체에서 여러 객체를 확인하는 방법은
       ```lua
          if rs.near(rows, "A0") and rs.near(rows, "A1") then
             ...
          end
       ```
       의 방식과
       ```lua
          if rows("A0") and rows("A1") then
             ...
          end
       ```

       또는 
       ```lua
          local r = rows({"A0", "A1"}, function (r)
             local total = 0
             local success = 0
             for k, v in pairs(r) do
                total = total + 1
                if v ~= nil then
                   success = success + 1
                end
                return total == success
             end
          end)
          if r then
             ...
          end
       ```
       으로 사용 가능합니다.

       ```lua
          if rs.near(rows, {"A0", "B0"}) and rs.near(rows, {"A1", "B0"}) then
             ...
          end
       ```
       의 경우에는 **rows()** 로 접근이 불가능 합니다. **rows()** 는 하위 객체로 접근하지 않습니다.

       해당 함수를 존재 이유는 하위 객체 접근에 대한 편의성을 위해 제공됩니다.

  * 참고
    * [resultset.fetch](#resultset-fetch)

  * **예제**
    ```lua
    ```

>### <a id="resultset-apply"></a> resultset.apply(row \[, table] \[, function (t1, t2)])

  * **기능**  <span style="white-space: pre;">&#9;&#9;</span>  변경된 항목을 적용합니다.
  * **입력** 
    * row - resultset.fetch()를 통해 반환된 객체와 연결된 객체
    * s - *(생략 가능)* 적용할 테이블 명
    * function (t1, t2) - *(생략 가능)* 변경 정보를 전달할 함수
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> function (t1, t2)의 반환값
  * **설명**<br>    
    
    SELECT 이후 *resultset.fetch*를 사용해 얻은 결과를 수정한 경우, 해당 내용을 데이터베이스에 적용해야 하는경우 사용될 수 있습니다.
    > row는 *resultset.fetch()* 의 결과 뿐만 아니라 row[*KEY*] 로 반환된 객체도 가능합니다. (값은 불가능)

    변경된 내용은 값의 변경 유무에 따라서 INSERT,UPDATE,DELETE로 처리가 됩니다.<br>
    그에 대한 정보는 *function (t1, t2)* 로 전달이 되며 *t1*은 자동증가되는 컬럼이 추가되는 경우 추가된 값을,
    *t2*는 다음 테이블의 변경된 로그 정보를 가지게 됩니다.

    |컬럼명|자료형|설명|
    |---:|:---:|---|
    |table|s|테이블명|
    |action|i|2: 업데이트, 4: 추가, 8: 삭제  |
    |where|t| { "COLUMN": 값, ... }&nbsp; |
    |value|t| { "o": 이전값, "n": 새로운값, "t": SQL_TYPE }&nbsp;|

    전달 되는 정보를 바탕으로 로깅을 할수 있습니다.

    * **주**

      1. 데이터베이스 종류에 따라 테이블 명을 확인할수 없어 명시가 필요한 경우가 있습니다.
         > MS-SQL을 사용하는 경우에는 테이블 명을 명시해야 합니다.
      1. SELECT 쿼리에서 JOIN 쿼리는 테이블 명을 명시해야 합니다.
      1. 적용되는 컬럼이 *s* (테이블명)에 포함되지 않은 경우 SQL오류가 발생될수 있습니다.

  * 참고
    * [odbc.new](#odbc-new)
    * [adapter.execute](#adapter-execute)
    * [adapter.apply](#adapter-apply)

  * **예제**
    ```lua
       ...
        rs.apply(rows["userid"], "USERS")
    ```

