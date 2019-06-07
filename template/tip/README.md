## *STAGE:플랫폼:* 사용팁

> #### 1. 변수의 값이 변경될때 감지할 수 있는 방법

  **stage.proxy()** 함수를 사용해 변수의 변경을 추적할 수 있습니다.
  
  * 숫자형은 설정된 값의 범위를 넘어가면 호출됩니다.
  * 문자열 또는 boolean은 일치하면 호출 됩니다.
  
  ```lua
  local DATA = {}
  local data = stage.proxy(DATA, {
      ["count"] = {
          20,
          30,
          function (t, edge_values, key, new_value)
              print("CHANGE", key, edge_values, new_value, t[key])
              for k, v in ipairs(edge_values) do
                  print("---", k, v)
              end
          end
      }
  })

  -- data.count = 10
  data.count = 31
  ```

  의 실행 결과는
  ```console
  CHANGE  count   table: 0x55fb3da33a30   31      nil
  ---     1       20.0
  ---     2       30.0
  ```

> #### 2. 대량의 동시 처리 요청을 관리하는 방법

> #### 3. 데이터 처리를 분산하는 방법

