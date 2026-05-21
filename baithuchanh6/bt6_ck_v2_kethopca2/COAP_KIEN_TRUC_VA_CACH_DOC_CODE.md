# CoAP, kiến trúc và cách đọc file `bt6_ck_v2_kethopca2.ino`

Tài liệu này giải thích đủ nền tảng để đọc hiểu phần CoAP và kiến trúc điều khiển trong file Arduino `.ino` của hệ thống solar tracker + che mưa.

## 1. CoAP là gì?

CoAP là viết tắt của **Constrained Application Protocol**.

Nói đơn giản, CoAP giống HTTP nhưng nhẹ hơn, thường dùng cho IoT, ESP32, cảm biến, thiết bị nhúng.

HTTP thường chạy trên TCP:

```text
Client --HTTP/TCP--> Server
```

CoAP thường chạy trên UDP:

```text
Client --CoAP/UDP--> Server
```

Trong code này, ESP32 đóng vai trò **CoAP server**. Điện thoại, máy tính hoặc app điều khiển đóng vai trò **CoAP client**.

```text
App điều khiển
    |
    | gửi CoAP request
    v
ESP32
    |
    | xử lý route: state, mode, servo1, servo2, umbrella
    v
Trả CoAP response
```

## 2. Vì sao dùng CoAP thay vì HTTP?

CoAP phù hợp với ESP32 vì:

- Nhẹ hơn HTTP.
- Dùng UDP nên ít overhead hơn TCP.
- Hợp với thiết bị IoT gửi lệnh nhỏ.
- Cấu trúc vẫn giống REST: có endpoint/route như `/state`, `/mode`.

Trong code đang dùng thư viện:

```cpp
#include <WiFiUdp.h>
#include <coap-simple.h>
```

`WiFiUdp` tạo nền UDP.

`coap-simple` tạo server CoAP bên trên UDP.

## 3. Kiến trúc tổng thể trong code

Hệ thống có các khối chính:

```text
+-------------------+
| WiFi              |
| connectWiFi()     |
| wifiTask()        |
+---------+---------+
          |
          v
+-------------------+
| CoAP server       |
| startCoapServer() |
| coap.loop()       |
+---------+---------+
          |
          v
+-------------------+
| CoAP callbacks    |
| callbackState()   |
| callbackMode()    |
| callbackServo1()  |
| callbackServo2()  |
| callbackUmbrella()|
+---------+---------+
          |
          v
+-------------------+
| Logic hệ thống    |
| currentMode       |
| currentState      |
| servo             |
| umbrella          |
+-------------------+
```

Ngoài CoAP, hệ thống còn có:

- LDR để tracking mặt trời.
- Servo ngang/dọc.
- Cảm biến mưa.
- Stepper motor kéo dù.
- State machine điều phối trạng thái.

## 4. Các biến quan trọng

### WiFi

```cpp
const char* WIFI_SSID = "Khu H";
const char* WIFI_PASS = "khuh1234";
```

Đây là tên WiFi và mật khẩu ESP32 sẽ kết nối.

### Secret key

```cpp
String SECRET_KEY = "SUNTRAC123";
```

Mọi payload gửi qua CoAP phải có dạng:

```text
SUNTRAC123:data
```

Ví dụ:

```text
SUNTRAC123:AUTO
SUNTRAC123:MANUAL
SUNTRAC123:90
SUNTRAC123:OPEN
```

Nếu sai secret, ESP32 trả:

```text
ERR_SECRET
```

### Mode

```cpp
enum ControlMode {
  MODE_AUTO,
  MODE_MANUAL
};
```

`MODE_AUTO`: hệ thống tự tracking và tự xử lý mưa.

`MODE_MANUAL`: người dùng điều khiển servo/dù qua CoAP.

### State

```cpp
enum AppState {
  STATE_WIFI_CONNECTING,
  STATE_WIFI_LOST,
  STATE_AUTO_TRACKING,
  STATE_MANUAL_CONTROL,
  STATE_RAIN_PROTECTION
};
```

State là trạng thái hoạt động hiện tại của hệ thống.

## 5. WiFi hoạt động như thế nào?

### `connectWiFi()`

Hàm này chạy trong `setup()`:

```cpp
void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}
```

Nó đưa ESP32 vào chế độ station, nghĩa là ESP32 kết nối vào router WiFi như điện thoại/laptop.

### `wifiTask()`

Hàm này chạy lặp lại trong từng state.

Nhiệm vụ:

- Kiểm tra WiFi mỗi 5 giây.
- Nếu đã kết nối thì log IP.
- Nếu mất kết nối thì gọi lại `WiFi.begin()`.

Logic chính:

```text
Nếu chưa đủ 5 giây từ lần kiểm tra trước
  -> bỏ qua

Nếu WiFi đang kết nối
  -> nếu chưa log thì in IP
  -> return

Nếu WiFi mất
  -> reset cờ log
  -> reconnect
```

## 6. CoAP server khởi động như thế nào?

Trong code có:

```cpp
WiFiUDP udp;
Coap coap(udp);
```

Ý nghĩa:

- `udp` là lớp truyền UDP.
- `coap` là server CoAP chạy trên UDP đó.

Server được khởi động trong:

```cpp
void startCoapServer()
```

Bên trong có đăng ký route:

```cpp
coap.server(callbackState, "state");
coap.server(callbackMode, "mode");
coap.server(callbackServo1, "servo1");
coap.server(callbackServo2, "servo2");
coap.server(callbackUmbrella, "umbrella");
coap.start();
```

Nghĩa là ESP32 có 5 endpoint CoAP:

```text
/state
/mode
/servo1
/servo2
/umbrella
```

Mỗi endpoint được nối với một callback:

| Route | Callback | Công dụng |
|---|---|---|
| `state` | `callbackState()` | Lấy trạng thái hệ thống |
| `mode` | `callbackMode()` | Đổi AUTO/MANUAL |
| `servo1` | `callbackServo1()` | Điều khiển servo dọc |
| `servo2` | `callbackServo2()` | Điều khiển servo ngang |
| `umbrella` | `callbackUmbrella()` | Mở/đóng dù |

## 7. Vì sao phải gọi `coap.loop()`?

CoAP server không tự chạy nền. Trong vòng lặp chính, code phải gọi:

```cpp
coap.loop();
```

Hàm này kiểm tra có request CoAP mới không.

Nếu có request đến route `mode`, nó gọi:

```cpp
callbackMode(...)
```

Nếu có request đến route `state`, nó gọi:

```cpp
callbackState(...)
```

Nếu không gọi `coap.loop()`, ESP32 sẽ không xử lý lệnh CoAP.

Trong code, `coap.loop()` chỉ chạy khi WiFi đã kết nối và hệ thống đang ở các state hoạt động:

```text
STATE_AUTO_TRACKING
STATE_MANUAL_CONTROL
STATE_RAIN_PROTECTION
```

Nó không chạy trong:

```text
STATE_WIFI_CONNECTING
STATE_WIFI_LOST
```

Vì lúc đó WiFi chưa sẵn sàng.

## 8. Payload CoAP trong code này

Payload là nội dung client gửi đến ESP32.

Code quy định payload phải theo dạng:

```text
SECRET_KEY:data
```

Ví dụ:

```text
SUNTRAC123:AUTO
SUNTRAC123:MANUAL
SUNTRAC123:45
SUNTRAC123:OPEN
SUNTRAC123:CLOSE
```

Phần trước dấu `:` là secret.

Phần sau dấu `:` là data.

## 9. Hàm đọc payload

### `getPayload()`

```cpp
String getPayload(CoapPacket &packet)
```

CoAP packet chứa payload dạng mảng byte:

```cpp
packet.payload[i]
```

Hàm này chuyển từng byte thành ký tự rồi ghép thành `String`.

Luồng:

```text
packet.payload bytes
  -> char
  -> String payload
  -> trim khoảng trắng
  -> return payload
```

Ví dụ client gửi:

```text
SUNTRAC123:AUTO
```

`getPayload()` trả về:

```text
SUNTRAC123:AUTO
```

## 10. Hàm kiểm tra secret

### `verifySecret()`

```cpp
bool verifySecret(CoapPacket &packet, String &data)
```

Hàm này làm 4 việc:

1. Đọc payload bằng `getPayload()`.
2. Tìm dấu `:`.
3. Tách secret và data.
4. So sánh secret với `SECRET_KEY`.

Ví dụ payload:

```text
SUNTRAC123:MANUAL
```

Kết quả:

```text
secret = "SUNTRAC123"
data   = "MANUAL"
```

Nếu secret đúng:

```cpp
return true;
```

Nếu secret sai hoặc không có dấu `:`:

```cpp
return false;
```

Trong callback, nếu sai secret thì trả:

```cpp
sendCoapText(..., "ERR_SECRET");
```

## 11. Hàm gửi response

### `sendCoapText()`

```cpp
void sendCoapText(const char* route,
                  IPAddress ip,
                  int port,
                  int messageid,
                  const char* response)
```

Hàm này gửi phản hồi CoAP về client:

```cpp
coap.sendResponse(ip, port, messageid, response);
```

Các tham số `ip`, `port`, `messageid` lấy từ request ban đầu.

Nói đơn giản:

```text
Client gửi request
ESP32 nhận ip + port + messageid
ESP32 xử lý
ESP32 gửi response về đúng client đó
```

## 12. Route `/state`

Callback:

```cpp
void callbackState(CoapPacket &packet, IPAddress ip, int port)
```

Mục đích: client hỏi trạng thái hiện tại của hệ thống.

Payload gửi lên:

```text
SUNTRAC123:anything
```

Phần data sau dấu `:` không quan trọng với route này, miễn secret đúng.

Luồng xử lý:

```text
Nhận request
Đọc payload
Log request
Kiểm tra secret
Nếu sai -> ERR_SECRET
Nếu đúng -> buildStateJson()
Gửi JSON về client
```

JSON trả về có dạng:

```json
{
  "lt": 1200,
  "rt": 1300,
  "ld": 1100,
  "rd": 1250,
  "v": 45,
  "h": 90,
  "rain": 0,
  "umbrella": 0
}
```

Ý nghĩa:

| Field | Ý nghĩa |
|---|---|
| `lt` | LDR top left |
| `rt` | LDR top right |
| `ld` | LDR bottom left |
| `rd` | LDR bottom right |
| `v` | Góc servo dọc |
| `h` | Góc servo ngang |
| `rain` | `1` có mưa, `0` không mưa |
| `umbrella` | `1` dù mở, `0` dù đóng |

## 13. Route `/mode`

Callback:

```cpp
void callbackMode(CoapPacket &packet, IPAddress ip, int port)
```

Mục đích: đổi chế độ AUTO hoặc MANUAL.

Payload hợp lệ:

```text
SUNTRAC123:AUTO
SUNTRAC123:MANUAL
```

Luồng xử lý:

```text
Nhận request
Đọc payload
Log request
Kiểm tra secret
Nếu sai -> ERR_SECRET
Chuyển data thành chữ hoa
Nếu data == AUTO
  currentMode = MODE_AUTO
  trả OK_AUTO
Nếu data == MANUAL
  currentMode = MODE_MANUAL
  trả OK_MANUAL
```

Khi `currentMode` đổi, state machine sẽ đổi state ở vòng `loop()` tiếp theo.

Ví dụ:

```text
Client gửi: SUNTRAC123:MANUAL
ESP32 trả: OK_MANUAL
currentMode = MODE_MANUAL
```

## 14. Route `/servo1`

Callback:

```cpp
void callbackServo1(CoapPacket &packet, IPAddress ip, int port)
```

Mục đích: điều khiển servo dọc.

Trong code:

```cpp
servovert = constrain(data.toInt(), servovertLimitLow, servovertLimitHigh);
vertical.write(servovert);
```

Payload hợp lệ:

```text
SUNTRAC123:45
SUNTRAC123:60
SUNTRAC123:80
```

Điều kiện quan trọng:

```cpp
if(currentMode != MODE_MANUAL)
```

Nếu chưa ở manual mode, ESP32 không cho điều khiển servo bằng tay.

Response:

| Trường hợp | Response |
|---|---|
| Sai secret | `ERR_SECRET` |
| Đang AUTO | `AUTO_MODE_ACTIVE` |
| Thành công | `OK` |

## 15. Route `/servo2`

Callback:

```cpp
void callbackServo2(CoapPacket &packet, IPAddress ip, int port)
```

Mục đích: điều khiển servo ngang.

Trong code:

```cpp
servohori = constrain(data.toInt(), servohoriLimitLow, servohoriLimitHigh);
horizontal.write(servohori);
```

Payload hợp lệ:

```text
SUNTRAC123:90
SUNTRAC123:120
SUNTRAC123:170
```

Điều kiện giống `/servo1`: chỉ hoạt động khi:

```cpp
currentMode == MODE_MANUAL
```

## 16. Route `/umbrella`

Callback:

```cpp
void callbackUmbrella(CoapPacket &packet, IPAddress ip, int port)
```

Mục đích: mở hoặc đóng dù bằng tay.

Payload hợp lệ:

```text
SUNTRAC123:OPEN
SUNTRAC123:CLOSE
```

Điều kiện:

```cpp
currentMode == MODE_MANUAL
```

Luồng:

```text
Nếu data == OPEN
  openUmbrella()
  trả OPEN_OK

Nếu data == CLOSE
  closeUmbrella()
  trả CLOSE_OK
```

Nếu đang AUTO thì trả:

```text
AUTO_MODE_ACTIVE
```

## 17. State machine hoạt động như thế nào?

State machine nằm trong:

```cpp
void updateStateMachine()
```

Mỗi vòng `loop()` đều chạy:

```cpp
updateStateMachine();
runStateAction();
```

`updateStateMachine()` quyết định hệ thống đang ở state nào.

`runStateAction()` quyết định state đó phải chạy việc gì.

### Luồng quyết định state

```text
Nếu WiFi chưa kết nối:
  Nếu chưa từng kết nối thành công:
    STATE_WIFI_CONNECTING
  Nếu đã từng kết nối rồi bị mất:
    STATE_WIFI_LOST

Nếu WiFi đã kết nối:
  Nếu MODE_AUTO:
    Nếu có mưa:
      STATE_RAIN_PROTECTION
    Nếu không mưa:
      STATE_AUTO_TRACKING

  Nếu MODE_MANUAL:
    STATE_MANUAL_CONTROL
```

Biểu diễn dạng bảng:

| Điều kiện | State |
|---|---|
| Chưa có WiFi, chưa từng connect | `STATE_WIFI_CONNECTING` |
| Mất WiFi sau khi đã connect | `STATE_WIFI_LOST` |
| Có WiFi, AUTO, không mưa | `STATE_AUTO_TRACKING` |
| Có WiFi, AUTO, có mưa | `STATE_RAIN_PROTECTION` |
| Có WiFi, MANUAL | `STATE_MANUAL_CONTROL` |

## 18. Action của từng state

Hàm:

```cpp
void runStateAction()
```

### `STATE_WIFI_CONNECTING`

```cpp
wifiTask();
logSystem();
```

Chỉ cố kết nối WiFi và log.

### `STATE_WIFI_LOST`

```cpp
wifiTask();
logSystem();
```

Tương tự connecting, nhưng ý nghĩa khác: đã từng chạy rồi nhưng bị mất WiFi.

### `STATE_AUTO_TRACKING`

```cpp
wifiTask();
startCoapServer();
coap.loop();
logSystem();
autoTrackingTask();
rainTask();
```

Nghĩa là:

- Giữ WiFi.
- Bật CoAP server nếu chưa bật.
- Lắng nghe lệnh CoAP.
- Tracking mặt trời.
- Kiểm tra mưa và điều khiển dù.

### `STATE_MANUAL_CONTROL`

```cpp
wifiTask();
startCoapServer();
coap.loop();
logSystem();
umbrellaMotionTask();
```

Nghĩa là:

- Giữ WiFi.
- Nhận lệnh CoAP.
- Không tự tracking.
- Chỉ tiếp tục xử lý chuyển động dù nếu dù đang mở/đóng.

Servo trong manual được điều khiển bởi callback `/servo1` và `/servo2`.

### `STATE_RAIN_PROTECTION`

```cpp
wifiTask();
startCoapServer();
coap.loop();
logSystem();
autoTrackingTask();
rainTask();
```

Hiện tại state này vẫn tracking mặt trời và đồng thời xử lý dù khi có mưa.

Nếu muốn mưa thì dừng tracking, có thể bỏ `autoTrackingTask()` khỏi state này.

## 19. Auto tracking hoạt động ra sao?

Hàm:

```cpp
void autoTrackingTask()
```

Nó đọc 4 LDR:

```text
lt = top left
rt = top right
ld = bottom left
rd = bottom right
```

Tính trung bình:

```cpp
avt = (lt + rt) / 2;  // phía trên
avd = (ld + rd) / 2;  // phía dưới
avl = (lt + ld) / 2;  // bên trái
avr = (rt + rd) / 2;  // bên phải
```

Sai lệch:

```cpp
dvert = avt - avd;
dhoriz = avl - avr;
```

Nếu lệch nhiều hơn `tolerance`, servo sẽ quay từng bước `stepSize`.

```cpp
if(abs(dvert) > tolerance)
```

Servo dọc:

```cpp
servovert += avt > avd ? stepSize : -stepSize;
```

Servo ngang:

```cpp
servohori += avl > avr ? -stepSize : stepSize;
```

Sau đó giới hạn góc bằng `constrain()`.

## 20. Rain task hoạt động ra sao?

Hàm:

```cpp
void rainTask()
```

Đọc cảm biến mưa:

```cpp
rainDetected = digitalRead(RAIN_SENSOR_PIN) == LOW;
```

Ở đây `LOW` nghĩa là có mưa.

Nếu có mưa:

```cpp
if(!umbrellaOpened || umbrellaIsClosing())
  openUmbrella();
```

Nếu không mưa:

```cpp
if(umbrellaOpened || umbrellaIsMoving())
  closeUmbrella();
```

Cuối hàm luôn gọi:

```cpp
umbrellaMotionTask();
```

Vì mở/đóng dù không làm một phát xong, mà chạy từng chút qua nhiều vòng loop.

## 21. Vì sao dù cần motion state riêng?

Dù dùng stepper motor. Nếu quay motor bằng một vòng lặp dài, ESP32 sẽ bị kẹt, CoAP và WiFi không phản hồi tốt.

Vì vậy code dùng state nhỏ riêng cho dù:

```cpp
enum UmbrellaMotionState {
  UMBRELLA_IDLE,
  UMBRELLA_OPENING,
  UMBRELLA_HOMING,
  UMBRELLA_SETTLE
};
```

Ý nghĩa:

| Motion state | Ý nghĩa |
|---|---|
| `UMBRELLA_IDLE` | Dù đứng yên |
| `UMBRELLA_OPENING` | Đang mở |
| `UMBRELLA_HOMING` | Đang chạy về công tắc hành trình để đóng |
| `UMBRELLA_SETTLE` | Đã chạm home, chờ ổn định |

`umbrellaMotionTask()` mỗi lần chỉ chạy vài bước:

```cpp
UMBRELLA_STEPS_PER_TASK = 4
```

Nhờ vậy `loop()` vẫn chạy nhanh, CoAP vẫn có thể xử lý request.

## 22. Stepper motor và `stepSequence`

```cpp
const int stepSequence[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};
```

Đây là bảng điều khiển 4 cuộn dây của stepper motor.

Mỗi dòng là trạng thái của:

```text
IN1, IN2, IN3, IN4
```

Ví dụ:

```text
{1, 0, 0, 0}
```

nghĩa là bật `IN1`, tắt `IN2`, `IN3`, `IN4`.

Hàm ghi ra motor:

```cpp
void writeStep(int stepIndex)
```

Khi `currentStep` tăng dần, motor quay một chiều.

Khi `currentStep` giảm dần, motor quay chiều ngược lại.

## 23. Các lệnh CoAP mẫu

Tùy công cụ client, cú pháp có thể khác nhau, nhưng payload nên giống dưới đây.

### Lấy state

Route:

```text
state
```

Payload:

```text
SUNTRAC123:GET
```

Response ví dụ:

```json
{"lt":1234,"rt":1200,"ld":1180,"rd":1195,"v":45,"h":90,"rain":0,"umbrella":0}
```

### Chuyển sang manual

Route:

```text
mode
```

Payload:

```text
SUNTRAC123:MANUAL
```

Response:

```text
OK_MANUAL
```

### Chuyển sang auto

Route:

```text
mode
```

Payload:

```text
SUNTRAC123:AUTO
```

Response:

```text
OK_AUTO
```

### Điều khiển servo dọc

Route:

```text
servo1
```

Payload:

```text
SUNTRAC123:60
```

Response:

```text
OK
```

Điều kiện: đang `MODE_MANUAL`.

### Điều khiển servo ngang

Route:

```text
servo2
```

Payload:

```text
SUNTRAC123:120
```

Response:

```text
OK
```

Điều kiện: đang `MODE_MANUAL`.

### Mở dù

Route:

```text
umbrella
```

Payload:

```text
SUNTRAC123:OPEN
```

Response:

```text
OPEN_OK
```

Điều kiện: đang `MODE_MANUAL`.

### Đóng dù

Route:

```text
umbrella
```

Payload:

```text
SUNTRAC123:CLOSE
```

Response:

```text
CLOSE_OK
```

Điều kiện: đang `MODE_MANUAL`.

## 24. Cách đọc file `.ino` theo thứ tự hợp lý

Không nên đọc từ trên xuống một lần rồi cố hiểu hết. Nên đọc theo thứ tự này:

1. Đọc phần khai báo biến, enum.
2. Đọc `setup()` để biết phần cứng được khởi tạo thế nào.
3. Đọc `loop()` để biết chương trình chạy lặp ra sao.
4. Đọc `updateStateMachine()` để hiểu hệ thống chọn state.
5. Đọc `runStateAction()` để biết mỗi state làm gì.
6. Đọc `startCoapServer()` để biết các route CoAP.
7. Đọc từng callback CoAP.
8. Đọc `rainTask()` và `autoTrackingTask()`.
9. Đọc `umbrellaMotionTask()` sau cùng vì đây là phần dài nhất.

## 25. Luồng chạy đầy đủ từ lúc bật ESP32

```text
setup()
  Serial.begin()
  ADC setup
  Servo attach
  pinMode
  motorOff()
  homeMotor()
  connectWiFi()
  updateStateMachine()

loop()
  updateStateMachine()
  runStateAction()
```

Nếu WiFi kết nối thành công và đang AUTO, luồng chính là:

```text
loop()
  updateStateMachine()
    -> STATE_AUTO_TRACKING hoặc STATE_RAIN_PROTECTION

  runStateAction()
    -> wifiTask()
    -> startCoapServer()
    -> coap.loop()
    -> logSystem()
    -> autoTrackingTask()
    -> rainTask()
```

Nếu client gửi lệnh CoAP trong lúc `coap.loop()` chạy:

```text
coap.loop()
  -> phát hiện request
  -> gọi callback tương ứng
  -> verifySecret()
  -> xử lý lệnh
  -> sendCoapText()
```

## 26. Những điểm cần nhớ khi sửa code

- Muốn thêm route CoAP mới thì thêm `coap.server(callback, "route")`.
- Muốn route mới hoạt động thì phải viết callback tương ứng.
- Mọi callback nên gọi `verifySecret()` trước khi xử lý.
- `coap.loop()` phải được gọi thường xuyên khi WiFi đã kết nối.
- Lệnh manual nên kiểm tra `currentMode == MODE_MANUAL`.
- Không nên dùng delay dài trong logic chính vì sẽ làm CoAP phản hồi chậm.
- Với stepper/dù, nên tiếp tục dùng kiểu task nhỏ như `umbrellaMotionTask()`.

## 27. Tóm tắt cực ngắn

ESP32 là CoAP server.

Client gửi lệnh đến các route:

```text
state, mode, servo1, servo2, umbrella
```

Mỗi lệnh có payload:

```text
SUNTRAC123:data
```

ESP32 kiểm tra secret, xử lý data, rồi trả response.

State machine quyết định hệ thống đang:

```text
CONNECTING, LOST, AUTO_TRACKING, MANUAL_CONTROL, RAIN_PROTECTION
```

CoAP chỉ là lớp nhận lệnh/gửi trạng thái. Logic thật sự nằm ở:

```text
updateStateMachine()
runStateAction()
autoTrackingTask()
rainTask()
umbrellaMotionTask()
```
