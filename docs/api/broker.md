* [broker](#broker-ns)
  * [broker.ready](#broker-ready)
  * [broker.join](#broker-join)
  * [broker.close](#broker-close)
  * [broker.signal](#broker-signal)
  * [broker.f](#broker-f)
  * [broker.ntoa](#broker-ntoa)
  * [broker.aton](#broker-aton)
  * [broker.ifaddr](#broker-ifaddr)<p>
  * [socket.commit](#socket-commit)
  * [socket.close](#socket-close)

>## <a id="broker-ns"></a> broker

 stage는 서버간의 통신을 위한 영역이라면, broker는 클라이언트 또는 타 서버와의 연동을 위한 처리를 제공합니다.

 기본적인 통신 방식은 [BSON](http://bsonspec.org/)을 통해 데이터를 전달 받을수 있도록 구성되어 있습니다.
 > BSON는 JSON을 이진화하는 형태로 상호 변환이 가능한 직렬화 기법으로 데이터를 전달하는 효과적인 방법입니다.

>#### <a id="broker-ready"></a> broker.ready(s, function (socket))
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> *s*로 정의된 네트워크 연결을 대기 상태로 설정합니다.
  * **입력**
    * s - 설정 정보
    * function (socket) - 연결이 발생될때 호출될 콜백 함수
  * **반환** 
    * *i* <span style="white-space: pre;">&#9;&#9;</span> 오류 번호 
    * *socket* <span style="white-space: pre;">&#9;</span> 연결 정보 
  * **설명**<br>
    
    *s*는 uri 형태의 정보를 가지고 있습니다.

      | 네트워크 | 응답 주소 | 연결 정보 | 비고 |
      |:---:|:---:|:---:|:---|
      |   tcp:// | *IP*:*PORT*   | ?packet=*IN*:*OUT* | 캐싱 사용 가능 |
      |   udp:// | *IP*:*PORT*   | - | 캐싱 무시 |

      > udp의 경우 *IP*가 멀티케스트 대역을 지원 합니다.    
      > *IP*는 IPv4와 IPv6를 지원합니다.
      
      > *IN* 과 *OUT* 값은 숫자로 설정이 되며, *k*, *m* 등의 단위로 포함하여 사용할 수 있습니다.

    *packet*에 대해

       - 32비트 자료형(INTEGER)으로 데이터의 크기를 [리틀 엔디언](https://ko.wikipedia.org/wiki/%EC%97%94%EB%94%94%EC%96%B8) 으로 저장합니다.
       - BSON 버퍼인 경우 별도의 처리 없이 사용 가능합니다.
         > BSON으로 전송되는 경우 직접 접근이 가능합니다.

    > 지정된 *PORT*가 설정("#service")에 지정된 값인 경우 여러 프로세서가 공유 가능

    > *socket*에서 다음의 추가 정보를 읽을수 있습니다.

      | 이름 | 자료형 | 설명 |
      |:---:|:---:|:---|
      |id|i|소캣 번호|
      |crypto|b|암호화 유무|전달되는 데이터의 암호화 유무를 지정|
      |errno|i|오류 번호|
      |addrs|t|연결 주소 [로컬, 원격]|
      |-|v|사용자 정의|

    > *socket*에 다음 정보를 설정할수 있습니다.

      | 이름 | 자료형 | 설명 |
      |:---:|:---:|:---|
      |expire|i|데이터가 발생되지 않을때 소켓을 닫을 시간 (msec)|
      |commit|i|데이터 전송 간격 (msec)|
      |crypto|s|암호화 공개키(CBC/RIJNDAEL 알고리즘)|
      |crypto|nil|암호화 OFF|
      |crypto|t|암호화 설정 ["알고리즘/블록운영/패딩방식" = 공개키 ]|
      |close|f|close 발생시 호출할 콜백 함수|
      |-|v|사용자 정의|

      * 알고리즘
        * [AES](https://ko.wikipedia.org/wiki/%EA%B3%A0%EA%B8%89_%EC%95%94%ED%98%B8%ED%99%94_%ED%91%9C%EC%A4%80)
        * [DES](https://ko.wikipedia.org/wiki/%EB%8D%B0%EC%9D%B4%ED%84%B0_%EC%95%94%ED%98%B8%ED%99%94_%ED%91%9C%EC%A4%80)
        * [SEED](https://ko.wikipedia.org/wiki/SEED)
        * [RIJNDAEL](https://ko.wikipedia.org/wiki/%EA%B3%A0%EA%B8%89_%EC%95%94%ED%98%B8%ED%99%94_%ED%91%9C%EC%A4%80)

      * [블록 운영](https://ko.wikipedia.org/wiki/%EB%B8%94%EB%A1%9D_%EC%95%94%ED%98%B8_%EC%9A%B4%EC%9A%A9_%EB%B0%A9%EC%8B%9D)
        * [CBC](https://ko.wikipedia.org/wiki/%EB%B8%94%EB%A1%9D_%EC%95%94%ED%98%B8_%EC%9A%B4%EC%9A%A9_%EB%B0%A9%EC%8B%9D#%EC%95%94%ED%98%B8_%EB%B8%94%EB%A1%9D_%EC%B2%B4%EC%9D%B8_%EB%B0%A9%EC%8B%9D_(CBC))
        * [CFB](https://ko.wikipedia.org/wiki/%EB%B8%94%EB%A1%9D_%EC%95%94%ED%98%B8_%EC%9A%B4%EC%9A%A9_%EB%B0%A9%EC%8B%9D#%EC%95%94%ED%98%B8_%ED%94%BC%EB%93%9C%EB%B0%B1_(CFB))

      * [패딩 방식](https://cryptosys.net/pki/manpki/pki_paddingschemes.html)
        * PKCS5Padding
        * PKCS1Padding
        * NoPadding

  * **예제**
    ```lua
       ...
       broker.ready("tcp://0.0.0.0:8081?packet=10k,40k", function (socket)
          ...
       end);
    ```

>#### <a id="broker-join"></a> broker.join(s, function (socket, s))
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> *s*로 정의된 네트워크 연결을 진행 합니다.
  * **입력**
    * s - 설정 정보
    * function (socket, s) - 연결이 완료될때 호출될 콜백 함수
  * **반환** 
    * *i* <span style="white-space: pre;">&#9;&#9;</span> 오류 번호 
    * *socket* <span style="white-space: pre;">&#9;</span> 연결 정보 
  * **설명**<br>
    
    *s*는 uri 형태의 정보를 가지고 있습니다. -- 연결 지향성이라 tcp만 허용됩니다
    
      | 네트워크 | 연결 주소 | 연결 정보 | 비고 |
      |:---:|:---:|:---:|:---|
      |   tcp:// | *IP*:*PORT*   | ?packet=*IN*:*OUT* | 캐싱 사용 가능 |

      > *IP*는 IPv4와 IPv6를 지원합니다.
      
      > *IN* 과 *OUT* 값은 숫자로 설정이 되며, *k*, *m* 등의 단위로 포함하여 사용할 수 있습니다.

    > *packet*에 대해

       - 32비트 자료형(INTEGER)으로 데이터의 크기를 [리틀 엔디언](https://ko.wikipedia.org/wiki/%EC%97%94%EB%94%94%EC%96%B8) 으로 저장합니다.
       - BSON 버퍼인 경우 별도의 처리 없이 사용 가능합니다.
         > BSON으로 전송되는 경우 직접 접근이 가능합니다.

    > 지정된 *PORT*가 설정("#service")에 지정된 값인 경우 여러 프로세서가 공유 가능

    > *socket*에서 다음의 추가 정보를 읽을수 있습니다.

      | 이름 | 자료형 | 설명 |
      |:---:|:---:|:---|
      |id|i|소캣 번호|
      |crypto|b|암호화 유무|전달되는 데이터의 암호화 유무를 지정|
      |errno|i|오류 번호|
      |addrs|t|연결 주소 [로컬, 원격]|
      |-|v|사용자 정의|

    > *socket*에 다음 정보를 설정할수 있습니다.

      | 이름 | 자료형 | 설명 |
      |:---:|:---:|:---|
      |expire|i|데이터가 발생되지 않을때 소켓을 닫을 시간 (msec)|
      |commit|i|데이터 전송 간격 (msec)|
      |crypto|s|암호화 공개키(CBC/RIJNDAEL 알고리즘)|
      |crypto|nil|암호화 OFF|
      |crypto|t|암호화 설정 [ "알고리즘/블록운영/패딩방식" = 공개키 ] |
      |close|f|close 발생시 호출할 콜백 함수|
      |-|v|사용자 정의|

      * 알고리즘
        * [AES](https://ko.wikipedia.org/wiki/%EA%B3%A0%EA%B8%89_%EC%95%94%ED%98%B8%ED%99%94_%ED%91%9C%EC%A4%80)
        * [DES](https://ko.wikipedia.org/wiki/%EB%8D%B0%EC%9D%B4%ED%84%B0_%EC%95%94%ED%98%B8%ED%99%94_%ED%91%9C%EC%A4%80)
        * [SEED](https://ko.wikipedia.org/wiki/SEED)
        * [RIJNDAEL](https://ko.wikipedia.org/wiki/%EA%B3%A0%EA%B8%89_%EC%95%94%ED%98%B8%ED%99%94_%ED%91%9C%EC%A4%80)

      * [블록 운영](https://ko.wikipedia.org/wiki/%EB%B8%94%EB%A1%9D_%EC%95%94%ED%98%B8_%EC%9A%B4%EC%9A%A9_%EB%B0%A9%EC%8B%9D)
        * [CBC](https://ko.wikipedia.org/wiki/%EB%B8%94%EB%A1%9D_%EC%95%94%ED%98%B8_%EC%9A%B4%EC%9A%A9_%EB%B0%A9%EC%8B%9D#%EC%95%94%ED%98%B8_%EB%B8%94%EB%A1%9D_%EC%B2%B4%EC%9D%B8_%EB%B0%A9%EC%8B%9D_(CBC))
        * [CFB](https://ko.wikipedia.org/wiki/%EB%B8%94%EB%A1%9D_%EC%95%94%ED%98%B8_%EC%9A%B4%EC%9A%A9_%EB%B0%A9%EC%8B%9D#%EC%95%94%ED%98%B8_%ED%94%BC%EB%93%9C%EB%B0%B1_(CFB))

      * [패딩 방식](https://cryptosys.net/pki/manpki/pki_paddingschemes.html)
        * PKCS5Padding
        * PKCS1Padding
        * NoPadding

  * **예제**
    ```lua
       ...
       broker.join("tcp://192.168.1.10:8081?packet=10k,40k", function (socket)
          ...
          print("NTOA: ", k, broker.ntoa(socket.addrs.remote));
          socket.commit("GET / HTTP/1.0\r\n\r\n");
       end);
    ```

>#### <a id="broker-close"></a> broker.close([i | { i, ... }])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 네트워크 연결을 페쇄합니다.
  * **입력**
    * i - 소켓 번호
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>
    
    연결된 네트워크를 페쇄합니다.

  * 참고
    * [broker.ready](#broker-ready)
    * [broker.join](#broker-join)
    * [socket.close](#socket-close)

  * **예제**
    ```lua
       ...
       broker.close(socket.id);
    ```

>#### <a id="broker-f"></a> broker.f(i)
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 소켓번호를 연결 정보로 전환합니다.
  * **입력**
    * i - 소켓 번호
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 연결 정보
  * **설명**<br>
    
    **샌드 박스**로 실행되는 콜백 함수로는 연결 정보를 전달할 수 없습니다.<br> 이러한 경우에는 소켓번호로 전달 후 다시 연결 정보로 전환하여 사용하는 방법을 제공하고 있습니다. 

    ```lua
       ...
       stage.submit(socket.id, function (socket_id, data)
          local socket = broker.f(socket_id)
          ...
       end, socket.id, data)
    ```
    의 형태로 사용될수 있습니다.

  * 참고
    * [broker.ready](#broker-ready)
    * [broker.join](#broker-join)
    * [broker.close](#broker-close)

  * **예제**
    ```lua
       ...
       local socket = broker.f(socket_id);
    ```

>#### <a id="broker-signal"></a> broker.signal(\[i | s | { i, s... }], v)
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 메시지를 전달합니다.
  * **입력**
    * i - 소켓 번호에 메시지가 발생된 것 처럼 전달합니다.
    * s - 연결 주소 (broker.aton())에 메시지를 전달합니다.
    * v - 메시지
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>
    
    *i*의 경우는 broker.signal()을 통해 메시지가 발생된것 처럼 처리하게 됩니다. 
    
    만약, *i*에 메시지를 보내고자 한다면
      * broker.signal({ *i* }, ...)
      * broker.f(*i*).commit(...)

    를 사용할수 있습니다.

    *s*는 udp로 연결된 서버에 데이터를 보내고자 할때 다음과 같이 사용할수 있습니다.    
    ```lua
       ...
       local addr = broker.aton("udp://224.0.0.1:8082", {})[1];

       broker.signal(addr, { "한글 테스트" });
    ```

  * 참고
    * [broker.ready](#broker-ready)
    * [broker.join](#broker-join)
    * [broker.close](#broker-close)
    * [broker.aton](#broker-aton)

  * **예제**
    ```lua
       ...
       -- socket에서 logout 메시지가 발생된것으로 보냅니다.
       broker.signal(socket.id, {
           ["logout"] = ""
       })
    ```
>#### <a id="broker-ntoa"></a> broker.ntoa(s \[, t])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 네트워크 주소를 문자열로 변환합니다.
  * **입력**
    * s - 연결 주소
    * t - 테이블에 서버 연결 주소를 추가합니다
  * **반환** 
    * s <span style="white-space: pre;">&#9;&#9;</span> 문자열로 반환
    * t <span style="white-space: pre;">&#9;&#9;</span> 테이블로 반환
  * **설명**<br>

    *t*를 사용하는 경우에 다음의 항목이 추가됩니다.

       |이름|자료형|값|비고|
       |:---:|:---:|:---|:---|
       |family|i| 네트워크 형태 | 2 - IPv4, 10 - IPv6 |
       |addr|s|연결 주소| |
       |port|i|포트 번호| |
       |scope_id|i|IPv6의 scope id| |

    *t*가 정이되지 않은 경우에는
       - IPv4 <span style="white-space: pre;">&#9;&#9;</span> "IP:PORT" 
       - IPv6 <span style="white-space: pre;">&#9;&#9;</span> "[IP]:PORT"

    형태로 반환됩니다.   

  * 참고
    * [broker.aton](#broker-aton)

  * **예제**
    ```lua
       ...
       print("IP", broker.ntoa(addr));
    ```

>#### <a id="broker-aton"></a> broker.aton(\[s | i\] \[, t])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 문자열을 네트워크 주소로 변환합니다.
  * **입력**
    * s - 연결 주소
    * i - 네트워크 형태 (2 - IPv4, 10 - IPv6)
    * t - 설정하고자 하는 값
  * **반환** <span style="white-space: pre;">&#9;&#9;&#9;</span> 네트워크 주소
  * **설명**<br>

    *t*에 설정된 값으로 변경 합니다.
    
       |이름|자료형|값|비고|
       |:---:|:---:|:---|---|
       |addr|s|연결 주소| |
       |port|i|포트 번호| |
       |scope_id|i|IPv6의 scope id| |

  * 참고
    * [broker.ntoa](#broker-ntoa)

  * **예제**
    ```lua
       ...
       local addr = broker.aton("udp://224.0.0.1:8082", {})[1];
    ```

>#### <a id="broker-ifaddr"></a> broker.ifaddr(s)
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 네트워크 장치의 주소를 얻습니다.
  * **입력**
    * s - 얻고자 하는 장치명
  * **반환** <span style="white-space: pre;">&#9;&#9;&#9;</span> 장치 정보
  * **설명**<br>

    네트워크 장치(랜카드)에 할당된 주소를 얻습니다.

    만약, *s*가 지정되는 경우 해당 장치의 주소를 얻습니다. (*s* 문자열이 포함된 장비를 모두 얻습니다.)

  * 참고
    * [broker.ntoa](#broker-ntoa)

  * **예제**
    ```lua
       ...

       local devices = broker.ifaddr();
       for k, v in pairs(devices) do
            print("DEVICE: " .. k);
            for v_i, v_v in pairs(v) do
                local addr = broker.ntoa(v_v, {});
                print("", "CHECK -", addr.family, addr.addr);
                print("", v_i, broker.ntoa(broker.aton(v_v, { ["port"] = 8080 })));
            end
       end
    ```

>### <a id="socket-ns"></a> socket

>#### <a id="socket-commit"></a> socket.commit(v \[, s])
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 메시지 *v*를 *s*로 보냅니다.
  * **입력**
    * v - 보내고자 하는 메시지
    * s - *(생략 가능)* 메시지를 받을 네트워크 주소 (broker.aton())
  * **반환** <span style="white-space: pre;">&#9;&#9;&#9;</span> 없음
  * **설명**<br>

    *socket* 으로 메시지를 전달합니다.

  * 참고
    * [broker.ready](#broker-ready)
    * [broker.join](#broker-join)
    * [broker.signal](#broker-signal)
    * [broker.ntoa](#broker-ntoa)

  * **예제**
    ```lua
       ...
       socket.commit({
           ["message"] = {
               ["SYS"] = "공지사항: 임시 점검 예정입니다."
           }
       })
    ```

>#### <a id="socket-close"></a> socket.close()
  * **기능**  <span style="white-space: pre;">&#9;&#9;</span> 네트워크 연결을 페쇄합니다.
  * **입력** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **반환** <span style="white-space: pre;">&#9;&#9;</span> 없음
  * **설명**<br>
    
    연결된 네트워크를 페쇄합니다.

  * 참고
    * [broker.ready](#broker-ready)
    * [broker.join](#broker-join)
    * [broker.close](#broker-close)

  * **예제**
    ```lua
       ...
       socket.close();
    ```

