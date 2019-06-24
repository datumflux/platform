## JavaScript(node.js)와 STAGE:플랫폼의 통신

> ## stage.js: STAGE:플랫폼과 node.js의 연동

  node.js에 구현된 함수를 STAGE:플랫폼에서 사용할 수 있도록 하기 위해 구현되었습니다. 단순히 클라이언트 형태로 접속하는 것이 아니라 3rd-Party 형태로 연결을 합니다.

  ```javascript
  new STAGE("nodejs", "STAGE_IP", STAGE_PORT, "ETH").then(function (stage) {
     stage.waitfor("result", function (v) {
        ...
     });

     stage.signal("stage:hello=result", arg);
  }).catch(function (e) { console.log("ERROR", e); });
  ```

  와 같이, 기존 STAGE:플랫폼과 유사하게 개발이 가능합니다.

  참고로, node.js에서 지원되는 함수는 통신에 필요한 기본적인 기능을 포함하는

  1. stage.waitfor()
  2. stage.signal()

  함수를 지원하여, 이 외의 함수는 직접 구현할 필요가 있습니다.

### 참고
> #### 1. BSON
   - https://www.npmjs.com/package/bson
   - https://github.com/mongodb/js-bson
