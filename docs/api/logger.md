* [log4cxx](#log4cxx-ns)

>## <a id="logger-ns"></a> logger.*level*(s)
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 로그를 출력합니다.
  * **입력**
    * s - 로그 메시지
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>

    로그 메시지 *s*에 다음과 같은 문장이 포함되는 경우 변경되어 출력합니다.

    | 문장 | 변경되는 값 |
    |:---:|:---|
    | %l  |**%F [%M:%L]** |
    | %F  |  파일 이름 |
    | %M  |  함수 이름 |
    | %L   | 위치 (라인위치) |
    | %{*ENV*}&nbsp; |   환경변수 |
    | %{=log4cxx:level}&nbsp; | 출력 정보 변경  |

    *%{=log4cxx:level}* 을 사용하여, 출력시 로그 레벨을 변경할수 있습니다.
    기본적으로 제공되는 로그 레벨은 다음과 같습니다. (log4cxx.out의 기본 로그 레벨은 info와 동일합니다)

    | 로그 레벨 | 설명 |
    |:---:|:---|
    | info | 정보 (out)|
    | warn | 경고 |
    | error | 오류 |
    | trace | 추적 |

    로그 레벨명을 기준으로 logger.**level**(s) 함수가 정의되어 있으며, 해당 로그 레벨에 적합한 함수를 사용하는 방법과 로그 출력 전에 *%{=logger:level}* 를 지정하여 변경하는 방법이 있습니다. 필요에 따라서 사용할 수 있습니다.

    >#### logger.out(s)
    >#### logger.info(s)
    >#### logger.warn(s)
    >#### logger.error(s)
    >#### logger.trace(s)

  * **예제**
    ```lua
    -- 해당 로그를 bot의 그룹에 warn 레벨로 출력하라고 설정  
    logger.out("%{PWD} %l - " .. msg, "%{=bot:warn}");
    ```
