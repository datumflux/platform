## Quick-Start

**STAGE:플랫폼** 을 통해 서버를 개발하는 과정을 설명하고자 합니다. <br>

해당 내용은 대부분의 서버 개발에 공통적인 부분으로 프로젝트의 성격상 달라지는 부분은 각 항목 별로 다시 설명 드릴 예정입니다.

**STAGE:플랫폼** 은 리눅스를 기반으로 Ubuntu 18.04 LTS 또는 CentOS 7 이상의 환경에서 실행이 되도록 최적화 되어 있습니다. 개발을 위해 리눅스를 설치하는 부담을 최소화 하고, 실 서비스에서도 효과적으로 사용할 수 있는 도커(Docker)를 사용해 보도록 하겠습니다.

*도커(Docker)로 서비스 구성을 지원하는 클라우드*

  * [AWS](https://aws.amazon.com/ko/docker/)
  * [Google Cloud](https://cloud.google.com/cloud-build/docs/quickstart-docker?hl=ko)
    * https://cloud.google.com/kubernetes-engine/docs/quickstart

도커(Docker)는 확장성 및 운영 효율성이 높아 지속적으로 확대 중입니다.

> ### 기능 확장
  1. 비동기 처리를 위한 [libuv 사용](https://github.com/datumflux/stage/tree/master/template/luv)
  2. [StackTracePlus 사용](https://github.com/ignacio/StackTracePlus)

> ### 설치
  
1. Docker 설치

    설치에 대한 자세한 내용은 [Docker CE](https://docs.docker.com/install/) 설치 페이지를 통해 확인 할 수 있습니다.

    > #### Ubuntu 18.04 LTS

      ##### Docker 설치 전 환경을 구성 합니다.
      ```console
      $ sudo apt update
      $ sudo apt install apt-transport-https ca-certificates curl software-properties-common
      ```
      
      ##### Docker 를 설치 합니다.
      ```console
      $ curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
      $ sudo add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu bionic stable"
      $ sudo apt update
      $ sudo apt install docker-ce 
      ```

      ##### Docker 실행 여부를 확인 합니다.
      ```console
      $ sudo systemctl status docker

      ● docker.service - Docker Application Container Engine
      Loaded: loaded (/lib/systemd/system/docker.service; enabled; vendor preset: e
      Active: active (running) since Wed 2019-05-29 12:11:38 KST; 7h ago
          Docs: https://docs.docker.com
      Main PID: 1091 (dockerd)
          Tasks: 15
      CGroup: /system.slice/docker.service
              └─1091 /usr/bin/dockerd -H fd:// --containerd=/run/containerd/contain        
      ```

      ##### Docker 실행시 sudo 없이 실행하기 위해서
      ```console
      $ sudo usermod -aG docker $USER
      ```
      이후, 재 로그인을 하면 적용 됩니다.


    > #### Windows

      > Windows 10이상의 경우에 Windows Subsystem Linux를 사용하지 않는 주된 이유는 WSL이 Linux의 기능 구현 중 *IPC* 에 관련된 부분과 *UNIX 내부 통신* 을 정상적으로 지원하지 않음에 있습니다. *-- 정상적으로 이용이 가능해지면 다시 한번 공유해 드리도록 하겠습니다.*

      여기에서는 Docker for Windows를 사용하여 개발하는 방법을 설명 하도록 하겠습니다.

     [Windows 10에서 Linux 컨테이너](https://docs.microsoft.com/ko-kr/virtualization/windowscontainers/quick-start/quick-start-windows-10-linux) 설치 방법을 통해 Docker를 설치 합니다.

     > #### Docker for Windows 설치 조건
     1. Hyper-V 지원 필요
     2. [docker hub](https://hub.docker.com/editions/community/docker-ce-desktop-windows)에서 계정 생성 필요합니다. (*다운로드를 위해서 로그인이 필수*) - 간단히 계정 생성이 가능합니다.

2. STAGE:플랫폼 설치

   1. docker hub에 있는 [STAGE:Platform](https://hub.docker.com/r/datumflux/stage)

      Windows에서는 CMD 또는 PowerShell을 통해, Linux에서는 쉘 환경에서
        ```console
        $ docker pull datumflux/stage

        Using default tag: latest
        latest: Pulling from datumflux/stage
        6abc03819f3e: Pull complete
        05731e63f211: Pull complete
        0bd67c50d6be: Pull complete
        a19ed5693bc8: Pull complete
        deebcdcd5176: Pull complete
        95ff260222d6: Pull complete
        8780a30c5c28: Pull complete
        4b810196e58b: Pull complete
        97b0b593964d: Pull complete
        fd6730806de4: Pull complete
        Digest: sha256:5df6536b616d9af5ea3ac37fddfdd94db1ebd2ec963ad2a5e6b048e52424f3e3
        Status: Downloaded newer image for datumflux/stage:latest
        ```
      을 통해 설치를 합니다. 
      
      STAGE:플랫폼은 개발 버젼과 릴리즈 버젼이 동시에 배포되고 있습니다. *latest* 버젼은 개발 버젼으로 분리됩니다.

      만약, 릴리즈 버젼을 받고자 하는 경우에는
        ```console
        $ docker pull datumflux/stage:v1.0
        ```
      을 통해 설치가 가능합니다.

      설치 여부는
        ```console
        $ docker images
        REPOSITORY          TAG                 IMAGE ID            CREATED             SIZE
        datumflux/stage     latest              2ca8e4122aa4        About an hour ago   129MB
        ```
      를 통해 확인 할 수 있습니다.

      실행 방법에 대한 내용은 다음에 다시 설명 하도록 하겠습니다.

   2. 개발에 필요한 디랙토리 구성

      개발에 필요한 디랙토리는

        ```
        stage
        │   
        └─── package
        │   
        └─── rollback
        │   
        └─── preload
        │   
        └─── index
        │   
        └─── logs
        │   
        └─── lib
        │   
        └─── conf
            │ odbcinst.ini
            │ odbc.ini
            │ log4cxx.xml
            │ stage.json
        │   
        │ start.lua
        ```
      구성이 필요합니다.

      * 스크립트 파일은 package > lib > rollback 순서로 사용 됩니다.
      * 데이터베이스에 접근하기 위해 사용하는 odbc 드라이버는 lib에 저장하며, 경로에 대한 설정 정보는 odbcinst.ini에 사용 합니다.

        ```
        [MySQL ODBC 8.0 Unicode Driver]
        Driver=/opt/stage/lib/libmyodbc8w.so
        Setup=/opt/stage/lib/libmyodbc8S.so
        Description=Unicode Driver for connecting to MySQL database server
        Threading=0
        FileUsage=1

        [MySQL ODBC 8.0 ANSI Driver]
        Driver=/opt/stage/lib/libmyodbc8a.so
        Setup=/opt/stage/lib/libmyodbc8S.so
        Description=ANSI Driver for connecting to MySQL database server
        Threading=0
        FileUsage=1        
        ```

        형태로 지정을 합니다. 
        > mysql의 [Connector/ODBC](https://dev.mysql.com/downloads/connector/odbc/)에서 *Ubuntu Linux 18.04(x86. 64-bit)* 를 다운로드 받아 설치 하였습니다.

        설정된 파일은

        ```lua
        local adp = odbc.new({
            ["DRIVER"] = "MySQL ODBC 8.0 ANSI Driver", 
            ["SERVER"] = "localhost",
            ["DATABASE"] = "DF_DEVEL",
            ["USER"] = "test",
            ["PASSWORD"] = "test&&",
            [".executeTimeout"] = 1000,
            [".queryDriver"] = "mysql"
        })
        ```

        형태로 접근 할 수 있습니다.

   3. 테스트 실행

        정상 실행 여부에 대한 확인을 위해

        ```console
        $ docker run -it --rm datumflux/stage

        2019-05-29 10:29:09,543 [INFO ] LOG4CXX: CONFIGURATION - 'conf/log4cxx.xml'
        .....
        2019-05-29 10:29:10,549 [WARN ] USAGE 16: 3 - CPU time 0.007341, user 0.000000, RSS 8780 kb        
        ```

        의 결과를 확인 하실 수 있습니다. <br>
        확인 완료 후 *Ctrl+C*를 통해 Docker를 중지합니다.

        > Docker 실행시 *-it* 옵션을 포함하지 않으면 *Ctrl+C*를 통해 서비스를 정지 할수 없습니다.
         
        이제 개발을 위해 구성한 폴더를 포함하여 다시 연결해 보도록 하겠습니다.

        > ##### 1. Windows

        개발에 사용할 폴더 구성을 *"D:\Stage"* 에 하였다는 가정 하여 다음과 같이 실행 합니다.

        ```ps
        > docker run -it --rm -v d:/stage:/opt/stage datumflux/stage
        ```

        > ##### 2. Linux

        개발에 사용할 폴더 구성을 홈 디랙토리의 *"~/stage"* 에 하였다는 가정 하여 다음과 같이 실행 합니다.

        ```console
        $ docker run -it --rm -v $HOME/stage:/opt/stage datumflux/stage
        ```

        실행 결과를 확인 하실 수 있습니다. *start.lua*가 작성되지 않았다면 다음과 같은 메시지를 출력합니다.

        ```console
        2019-05-29 10:31:56,524 [INFO ] CLUSTER: READY - 14
        2019-05-29 10:31:56,524 [WARN ] start - '<unknown>'
        2019-05-29 10:31:56,525 [INFO ] READY 14: CLUSTER [42405 port]
        ```

        이제 남은 일은 *start.lua*을 포함에 콘텐츠 스크립트를 작성하고 다른 서버 또는 클라이언트와 통신하면 됩니다.

        > docker 의 실행 시 *-v* 옵션은 절대 경로를 사용해야 정상적으로 연결이 됩니다.

        > 시작 스크립트(start.lua)의 변경시에만 docker를 중지(Ctrl+C)하고 다시 시작 하시면 됩니다.

2. **참고:** 추가 서버 환경 설정
   1. 서비스에 사용할 네트워크 연결 포트의 결정

      외부(클라이언트 또는 다른 서버와 통신)에서 연결할 수 있는 포트를 미리 결정하고, 설정 하는 이유는 STAGE 플랫폼이 지정된 포트를 여러 프로세서가 공유해 연결을 분산하기 위한 목적으로 사용합니다.
      > 프로세서간의 공유가 필요하지 않다면 해당 설정을 할 필요가 없습니다.
       
      *Docker HUB*에 설정된 외부 사용 포트는 8801로 정의되어 있습니다. 만약, 기본 포트의 변경 또는 추가를 위해서는 설정의 변경이 필요합니다. 

      설정은
        ```json
            "#services": {
                "tcp":["0.0.0.0:8801"]
            },
        ```

       부분으로, 설정은 **tcp 통신이 가능한 8801 포트를 사용함** 을 뜻합니다.<br>
       **0.0.0.0** 은 IPv4로 접속되는 IP 연결을 받겠다는 의미입니다.

       설정된 정보는 스크립트의 다음 처리와 1:1 대응 됩니다.

        ```lua
            broker.ready("tcp://0.0.0.0:8801?bson=2k,4k", ...)
        ```

       만약, IPv6로 들어오는 연결을 받고자 하는 경우에는 **[::]** 으로 지정 하면 됩니다. 대부분은 IPv6를 지정하면 IPv4도 지원합니다.

        ```json
            "#services": {
                "tcp":["[::]:8801"]
            },
        ```

        ```lua
            broker.ready("tcp://[::]:8801?bson=2k,4k", ...)
        ```

   3. 클러스터링 설정

      STAGE 플랫폼은 다른 STAGE 플랫폼으로 요청을 전달해 실행 할 수 있는 기능을 가지고 있습니다.

      설정은
        ```json
            "#cluster": [ 9801 ],
        ```

      로 지정이 되어 있으며, 다른 STAGE 플랫폼과 연결을 원하면

      설정을
        ```json
            "#cluster": [ 9801, "IP:9801" ],
        ```

      로 변경해 재 시작을 하면 설정된 *"IP"* 에 존재하는 STAGE 플랫폼과 연결을 합니다.

      여기에서 연결을 진행한 STAGE 플랫폼을 Slave라고 하고, 연결을 받은 STAGE 플랫폼을 Master라고 이해 하시면 됩니다. 
      
      Master에서 발생된 요청은 Slave를 통해 처리를 할 수 있으나, Slave에서 발생된 요청은 Master로 전달되지 않습니다.

      즉, 단방향으로 요청이 되며 양방향 설정을 위해서는 Master와 Slave가 서로의 IP로 연결을 구성하면 가능합니다.

      > 클러스터링 구성은 UDP를 통해 전달이 되며 네트워크를 사용합니다. 그러나, 내부 통신은 UNIX 통신을 통해 네트워크의 부하를 발생시키지 않습니다.

       1. 클러스터링 이더넷 장치 지정

           클러스터링이 구성되면 다른 하드웨어에 위치한 STAGE:플랫폼에 연결이 되며 처리된 결과를 다시 받기 위해서는 외부 통신이 가능한 IPv4 주소를 얻어야 합니다.

           만약, 클러스터링이 구성되는 하드웨어의 이더넷 장치가 두개 이상이라고 한다면 어떠한 장치가 외부 통신이 가능한지 설정할 필요가 있습니다.

            ```console
            $ ifconfig
            docker0: flags=4099<UP,BROADCAST,MULTICAST>  mtu 1500
                    inet 172.17.0.1  netmask 255.255.0.0  broadcast 172.17.255.255
                    ether 02:42:98:71:da:f1  txqueuelen 0  (Ethernet)
                    ...

            eth0: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
                    inet 10.211.55.10  netmask 255.255.255.0  broadcast 10.211.55.255
                    inet6 fdb2:2c26:f4e4:0:f99e:7ba8:b20f:6122  prefixlen 64  scopeid 0x0<global>
                    inet6 fe80::206a:651e:6fe7:3581  prefixlen 64  scopeid 0x20<link>
                    ether 00:1c:42:8b:69:83  txqueuelen 1000 
                    ...

            lo: flags=73<UP,LOOPBACK,RUNNING>  mtu 65536
                    inet 127.0.0.1  netmask 255.0.0.0
                    inet6 ::1  prefixlen 128  scopeid 0x10<host>
                    loop  txqueuelen 1000  (Local Loopback)
                    ...
           ```

           명령을 통해 확인한 이더넷 장치는 **eth0** 이므로,

           설정에
             ```json
                 "#cluster": [ ":eth0", 9801, "IP:9801" ],
             ```

           로 지정하면 STAGE:플랫폼은 *eth0*의 *inet 10.211.55.10*를 사용합니다.

       2. FAIL-OVER STAGE:플랫폼 지정

          STAGE:플랫폼은 메시지를 처리할 수 있는 프로세서를 찾는 과정을 라우팅이라고 합니다. 이 과정 중에 메시지를 처리할 수 있는 프로세서가 없다면 메시지는 폐기 됩니다.

          이러한 경우에, 메시지를 처리할 수 있는 *FAIL-OVER STAGE:플랫폼*을 설정하는 옵션으로 해당 메시지를 전달 할 수 없는 경우에만 사용 됩니다.

          설정에
            ```json
                "#cluster": [ ":eth0", "=FAIL-OVER_IP:9801", 9801, "IP:9801" ],
            ```

          지정을 하면 메시지를 폐기하는 대신 *FAIL-OVER_IP*로 전달합니다.
          > *FAIL-OVER_IP*는 연결되는 형태로 구성이 가능합니다.

3. 스크립트 작성

    ##### 1. Windows

    ```ps
    > docker run -it --rm -v d:/stage:/opt/stage datumflux/stage
    ```

    ##### 2. Linux
    
    ```console
    $ docker run -it --rm -v $HOME/stage:/opt/stage datumflux/stage
    ```

    를 통해 실행을 하면, *start.lua* 가 없는 상태로 실행이 됩니다.
    
    > [Visual Studio Code](https://code.visualstudio.com/)를 사용해 개발하시면 좀더 쉽게 개발 할 수 있습니다.

    1. 시작 스크립트(**start.lua**) 설정

       start.lua를 다음과 같이 작성 해 봅니다.

       ```lua
       print("Hello STAGE:Platform")
       ```

       저장 후, 기존에 실행 중인 상태라면 **Ctrl+C** 를 입력해 실행을 중지 합니다.

        ##### 1. Windows

        ```ps
        > docker run -it --rm -v d:/stage:/opt/stage datumflux/stage
        ```

        ##### 2. Linux
        
        ```console
        > docker run -it --rm -v $HOME/stage:/opt/stage datumflux/stage
        ```

        를 통해 간단하게 작성한 *"Hello STAGE:Platform"* 이 출력됨을 확인 하셨을것 입니다.
        > 백엔드(서버) 플랫폼이라 화면 상에 다양한 정보가 출력이 됩니다. 출력되는 정보는 *각 환경의 stage/logs* 에서도 확인 할 수 있습니다.
        
        스크립트에 대한 보다 많은 정보는 다양한 예제를 통해 확인 하실 수 있습니다.

    2. 스크립트의 사용 순서

       STAGE:플랫폼은 스크립트를 다양한 경로에서 가져와 사용 합니다. 기본 설정된 경로는

       - package
       - rollback

       순서로 스크립트를 확인 합니다. STAGE:플랫폼은 항상 처음에 설정된 경로에서 스크립트를 가져와 실행을 하려고 시도를 합니다. 만약 문제가 발생되면 다음 폴더에서 동일한 파일명의 이름을 찾게 됩니다.
       
       1분 주기로 파일의 변경여부를 모니터링 하며, 파일이 변경되면 해당 파일을 다시 로딩하여 사용하게 됩니다.

       해당 처리는 스크립트의 오류로 인한 서비스의 문제를 해소 하기 위한 방법으로 package/aaaa.lua 의 파일에 오류가 있다면 rollback/aaaa.lua를 사용하고 다음 모니터링 시간에 다시 순서대로 확인을 하여 복구를 시도합니다.   

       > Docker를 사용하지 않은 실행에서는 해당 처리의 순서를 자유롭게 변경 하실 수 있습니다.

4. 참고 사항

    1. 런타임 디버깅
       
       STAGE:플랫폼은 현재 런타임 디버깅을 지원하지 않습니다. 
       
       내부 처리가 분산되고 병렬로 처리되는 특성으로 인해 기존 방식의 런타임 디버깅 방식으로는 문제를 해결할 수 없기 때문입니다.
       그 예로,
       
         - 함수를 원격으로 전송하여 실행 후, 결과만 전송 받는 방식
         - 샌드박스로 분리되어 구동되는 동시 실행 방식
         - MapReduce 방식의 처리 분산을 통해 결과를 수집하는 방식
         - 동시 접근 제한 객체 풀
         
       등의 특성들이 기존의 디버깅 방식으로 처리하기 어려움이 있어 새로운 방식의 런타임 디버깅을 고려중에 있습니다.

    2. BSON

       STAGE:플랫폼은 외부 통신을 위해 [BSON](http://bsonspec.org/) 형태로 데이터를 주고 받습니다.

    3. odbc 사용

       STAGE:플랫폼은 ODBC연결을 위한 queryDriver 설정을 MS-SQL과 MySQL를 선택적으로 사용 할 수 있도록 하였습니다.

       > 이외의 다른 데이터베이스를 사용하고자 하시면 해당 설정을 추가하여 재 배포 하도록 하겠습니다.

       굳이 해당 설정을 분류한 이유는 MySQL과 MS-SQL의 *AUTO INCREMENT* 컬럼에 대한 처리 방식의 차이 때문입니다.

       실 사용시 해당 속성을 사용하지 않으신다면 기본 설정인 MySQL을 기준으로 사용해도 무방하나 만약 사용을 하신다면 해당 설정을 확인해야 합니다.

       MySQL의 경우에는 데이터를 INSERT한 이후에
       ```sql
       SELECT LAST_INSERT_ID
       ```
       의 형태로 처음 추가된 데이터의 값을 얻을 수 있지만,

       MS-SQL의 경우에는
       ```sql
       SELECT SCOPE_IDENTITY()
       ```
       의 형태로 마지막에 추가된 데이터의 값을 반환 합니다.
