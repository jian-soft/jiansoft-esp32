# 与小智服务器的激活模块

## 激活流程包括
- 查询激活状态
- 未激活向用户展示激活码
- 向服务器发送激活请求


## 服务器接口

- url  https://api.tenclass.net/xiaozhi/ota/
- 参数
```
{"version":2,"language":"zh-CN","flash_size":16777216,"minimum_free_heap_size":"8425044","mac_address":"94:a9:90:2c:84:b8","uuid":"e2314aed-0a16-43f0-8128-766fbd3207f2","chip_model_name":"esp32s3","chip_info":{"model":9,"cores":2,"revision":2,"features":18},"application":{"name":"xiaozhi","version":"2.0.4","compile_time":"Nov 15 2025T16:51:32Z","idf_version":"v5.5.1-dirty","elf_sha256":"ec236bc87baef23231029a0a90caf2b9b5eb462bc616ad8c5c5313187082ffdf"},"partition_table": [{"label":"nvs","type":1,"subtype":2,"address":36864,"size":16384},{"label":"otadata","type":1,"subtype":0,"address":53248,"size":8192},{"label":"phy_init","type":1,"subtype":1,"address":61440,"size":4096},{"label":"ota_0","type":0,"subtype":16,"address":131072,"size":4128768},{"label":"ota_1","type":0,"subtype":17,"address":4259840,"size":4128768},{"label":"assets","type":1,"subtype":130,"address":8388608,"size":8388608}],"ota":{"label":"ota_0"},"display":{"monochrome":true,"width":128,"height":32},"board":{"type":"bread-compact-ml307","name":"bread-compact-ml307","revision":"ML307R-DL-MBRH0S00","carrier":"CHINA MOBILE","csq":"24","imei":"869663071312008","iccid":"898604670224D0049048","cereg":{"stat":1,"tac":"2490","ci":"015B290F","AcT":7}}}


{"version":2,"language":"zh-CN","flash_size":16777216,"minimum_free_heap_size":"8425048","mac_address":"94:a9:90:2c:84:b8","uuid":"e2314aed-0a16-43f0-8128-766fbd3207f2","chip_model_name":"esp32s3","chip_info":{"model":9,"cores":2,"revision":2,"features":18},"application":{"name":"xiaozhi","version":"2.0.4","compile_time":"Nov 15 2025T16:51:32Z","idf_version":"v5.5.1-dirty","elf_sha256":"ec236bc87baef23231029a0a90caf2b9b5eb462bc616ad8c5c5313187082ffdf"},"partition_table": [{"label":"nvs","type":1,"subtype":2,"address":36864,"size":16384},{"label":"otadata","type":1,"subtype":0,"address":53248,"size":8192},{"label":"phy_init","type":1,"subtype":1,"address":61440,"size":4096},{"label":"ota_0","type":0,"subtype":16,"address":131072,"size":4128768},{"label":"ota_1","type":0,"subtype":17,"address":4259840,"size":4128768},{"label":"assets","type":1,"subtype":130,"address":8388608,"size":8388608}],"ota":{"label":"ota_0"},"display":{"monochrome":true,"width":128,"height":32},"board":{"type":"bread-compact-ml307","name":"bread-compact-ml307","revision":"ML307R-DL-MBRH0S00","carrier":"CHINA MOBILE","csq":"23","imei":"869663071312008","iccid":"898604670224D0049048","cereg":{"stat":1,"tac":"2490","ci":"015B290F","AcT":7}}}


{"version":2,"language":"zh-CN","flash_size":16777216,"minimum_free_heap_size":"8402204","mac_address":"94:a9:90:2c:84:b8","uuid":"e2314aed-0a16-43f0-8128-766fbd3207f2","chip_model_name":"esp32s3","chip_info":{"model":9,"cores":2,"revision":2,"features":18},"application":{"name":"xiaozhi","version":"2.0.4","compile_time":"Nov 15 2025T16:51:32Z","idf_version":"v5.5.1-dirty","elf_sha256":"ec236bc87baef23231029a0a90caf2b9b5eb462bc616ad8c5c5313187082ffdf"},"partition_table": [{"label":"nvs","type":1,"subtype":2,"address":36864,"size":16384},{"label":"otadata","type":1,"subtype":0,"address":53248,"size":8192},{"label":"phy_init","type":1,"subtype":1,"address":61440,"size":4096},{"label":"ota_0","type":0,"subtype":16,"address":131072,"size":4128768},{"label":"ota_1","type":0,"subtype":17,"address":4259840,"size":4128768},{"label":"assets","type":1,"subtype":130,"address":8388608,"size":8388608}],"ota":{"label":"ota_0"},"display":{"monochrome":true,"width":128,"height":32},"board":{"type":"bread-compact-ml307","name":"bread-compact-ml307","revision":"ML307R-DL-MBRH0S00","carrier":"CHINA MOBILE","csq":"20","imei":"869663071312008","iccid":"898604670224D0049048","cereg":{"stat":1,"tac":"2490","ci":"0A80AD0D","AcT":7}}}

//再次验证
{"version":2,"language":"zh-CN","flash_size":16777216,"minimum_free_heap_size":"8425048","mac_address":"94:a9:90:2c:84:b8","uuid":"e2314aed-0a16-43f0-8128-766fbd3207f2","chip_model_name":"esp32s3","chip_info":{"model":9,"cores":2,"revision":2,"features":18},"application":{"name":"xiaozhi","version":"2.0.4","compile_time":"Nov 15 2025T16:51:32Z","idf_version":"v5.5.1-dirty","elf_sha256":"ec236bc87baef23231029a0a90caf2b9b5eb462bc616ad8c5c5313187082ffdf"},"partition_table": [{"label":"nvs","type":1,"subtype":2,"address":36864,"size":16384},{"label":"otadata","type":1,"subtype":0,"address":53248,"size":8192},{"label":"phy_init","type":1,"subtype":1,"address":61440,"size":4096},{"label":"ota_0","type":0,"subtype":16,"address":131072,"size":4128768},{"label":"ota_1","type":0,"subtype":17,"address":4259840,"size":4128768},{"label":"assets","type":1,"subtype":130,"address":8388608,"size":8388608}],"ota":{"label":"ota_0"},"display":{"monochrome":true,"width":128,"height":32},"board":{"type":"bread-compact-ml307","name":"bread-compact-ml307","revision":"ML307R-DL-MBRH0S00","carrier":"CHINA MOBILE","csq":"22","imei":"869663071312008","iccid":"898604670224D0049048","cereg":{"stat":1,"tac":"2490","ci":"015B290F","AcT":7}}}
//再次验证的返回
{"mqtt":{"endpoint":"mqtt.xiaozhi.me","client_id":"GID_test@@@94_a9_90_2c_84_b8@@@e2314aed-0a16-43f0-8128-766fbd3207f2","username":"eyJpcCI6IjIyMy4xMDQuODIuNzYifQ==","password":"MDWkGWYWOzFEY+DBZ1TMIF4VX1tUOFJxFRta91wza0c=","publish_topic":"device-server","subscribe_topic":"null"},"websocket":{"url":"wss://api.tenclass.net/xiaozhi/v1/","token":"test-token"},"server_time":{"timestamp":1766848942364,"timezone_offset":480},"firmware":{"version":"2.0.4","url":""},"activation":{"code":"209835","message":"xiaozhi.me\n209835","challenge":"7c69ff8d-307c-4b90-ba29-7c24f4212fe9"}}
```
- 服务器返回值
```
{"mqtt":{"endpoint":"mqtt.xiaozhi.me","client_id":"GID_test@@@94_a9_90_2c_84_b8@@@e2314aed-0a16-43f0-8128-766fbd3207f2","username":"eyJpcCI6IjIyMy4xMDQuODIuNzYifQ==","password":"MDWkGWYWOzFEY+DBZ1TMIF4VX1tUOFJxFRta91wza0c=","publish_topic":"device-server","subscribe_topic":"null"},"websocket":{"url":"wss://api.tenclass.net/xiaozhi/v1/","token":"test-token"},"server_time":{"timestamp":1766844896134,"timezone_offset":480},"firmware":{"version":"2.0.4","url":""}}

{"mqtt":{"endpoint":"mqtt.xiaozhi.me","client_id":"GID_test@@@94_a9_90_2c_84_b8@@@e2314aed-0a16-43f0-8128-766fbd3207f2","username":"eyJpcCI6IjIyMy4xMDQuODIuNzYifQ==","password":"MDWkGWYWOzFEY+DBZ1TMIF4VX1tUOFJxFRta91wza0c=","publish_topic":"device-server","subscribe_topic":"null"},"websocket":{"url":"wss://api.tenclass.net/xiaozhi/v1/","token":"test-token"},"server_time":{"timestamp":1766848651234,"timezone_offset":480},"firmware":{"version":"2.0.4","url":""},"activation":{"code":"209835","message":"xiaozhi.me\n209835","challenge":"e8507672-94b6-4ba3-9396-302745a2e242"}}

{"mqtt":{"endpoint":"mqtt.xiaozhi.me","client_id":"GID_test@@@94_a9_90_2c_84_b8@@@e2314aed-0a16-43f0-8128-766fbd3207f2","username":"eyJpcCI6IjIyMy4xMDQuODIuNzYifQ==","password":"MDWkGWYWOzFEY+DBZ1TMIF4VX1tUOFJxFRta91wza0c=","publish_topic":"device-server","subscribe_topic":"null"},"websocket":{"url":"wss://api.tenclass.net/xiaozhi/v1/","token":"test-token"},"server_time":{"timestamp":1766848777231,"timezone_offset":480},"firmware":{"version":"2.0.4","url":""}}


Activate url:https://api.tenclass.net/xiaozhi/ota/activate

Activate rsp:{"message":"Device activated","device_id":1421825}

```

