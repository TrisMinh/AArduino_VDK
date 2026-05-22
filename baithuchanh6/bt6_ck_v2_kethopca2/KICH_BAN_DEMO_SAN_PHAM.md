# Kịch Bản Demo Sản Phẩm Solar Tracker Có Che Mưa Tự Động

## 1. Mục tiêu demo

Demo này dùng để trình bày mô hình **hệ thống pin năng lượng mặt trời tự xoay theo ánh sáng**, có thêm chức năng **tự động che mưa** và **điều khiển từ xa qua WiFi bằng giao thức CoAP**.

Sau khi demo xong, người xem cần thấy rõ các điểm chính:

- ESP32 kết nối WiFi và chạy CoAP server.
- Hệ thống đọc 4 cảm biến ánh sáng LDR để xác định hướng sáng.
- Hai servo tự điều chỉnh tấm pin theo hướng có ánh sáng mạnh hơn.
- Cảm biến mưa phát hiện nước và kích hoạt mái che.
- Động cơ bước mở/đóng mái che.
- Công tắc hành trình giúp mái che biết vị trí gốc khi đóng.
- Người dùng có thể đổi chế độ AUTO/MANUAL và điều khiển từ xa qua CoAP.
- Lệnh điều khiển có kiểm tra khóa bí mật `SECRET_KEY`.

## 2. Tóm tắt sản phẩm

Tên sản phẩm đề xuất:

**Mô hình Solar Tracker thông minh có mái che mưa tự động**

Mô tả ngắn:

> Đây là mô hình hệ thống pin năng lượng mặt trời thông minh. Hệ thống dùng cảm biến ánh sáng để tự xoay tấm pin về hướng có ánh sáng mạnh nhất, giúp tăng khả năng hấp thụ năng lượng. Khi phát hiện mưa, hệ thống tự động mở mái che để bảo vệ tấm pin. Ngoài ra, ESP32 còn tạo server CoAP để người dùng giám sát trạng thái và điều khiển thiết bị từ xa qua WiFi.

## 3. Phần cứng sử dụng

| Thành phần | Chức năng |
|---|---|
| ESP32 | Bộ điều khiển trung tâm, đọc cảm biến, điều khiển servo, motor, WiFi và CoAP |
| 4 cảm biến LDR | Đo ánh sáng ở 4 vị trí: trên trái, trên phải, dưới trái, dưới phải |
| 2 servo | Điều chỉnh hướng tấm pin theo trục ngang và trục dọc |
| Cảm biến mưa | Phát hiện có mưa hoặc nước trên bề mặt cảm biến |
| Động cơ bước | Mở và đóng mái che |
| Driver động cơ bước | Điều khiển các cuộn dây của động cơ bước |
| Công tắc hành trình | Xác định vị trí gốc khi mái che đóng hoàn toàn |
| WiFi | Cho phép điều khiển và giám sát từ xa |

## 4. Chân kết nối trong code

### 4.1. Cảm biến ánh sáng LDR

| Cảm biến | Chân ESP32 | Ý nghĩa |
|---|---:|---|
| `LDR_TOP_LEFT` | `33` | Ánh sáng phía trên bên trái |
| `LDR_BOTTOM_LEFT` | `32` | Ánh sáng phía dưới bên trái |
| `LDR_BOTTOM_RIGHT` | `35` | Ánh sáng phía dưới bên phải |
| `LDR_TOP_RIGHT` | `34` | Ánh sáng phía trên bên phải |

### 4.2. Servo

| Servo | Chân ESP32 | Biến điều khiển | Góc ban đầu |
|---|---:|---|---:|
| Servo ngang | `18` | `servohori` | `90` độ |
| Servo dọc | `19` | `servovert` | `45` độ |

Giới hạn servo trong code:

| Servo | Góc thấp nhất | Góc cao nhất |
|---|---:|---:|
| Servo ngang | `10` độ | `170` độ |
| Servo dọc | `10` độ | `80` độ |

### 4.3. Cảm biến mưa

| Thành phần | Chân ESP32 | Ngưỡng |
|---|---:|---:|
| Cảm biến mưa | `36` | `RAIN_THRESHOLD = 2000` |

Trong code:

```cpp
bool isRainDetected()
{
  return readRainSensor() < RAIN_THRESHOLD;
}
```

Nghĩa là:

- Giá trị cảm biến mưa nhỏ hơn `2000`: có mưa.
- Giá trị cảm biến mưa lớn hơn hoặc bằng `2000`: không mưa.

### 4.4. Động cơ bước và công tắc hành trình

| Thành phần | Chân ESP32 |
|---|---:|
| `IN1` | `25` |
| `IN2` | `26` |
| `IN3` | `27` |
| `IN4` | `14` |
| Công tắc hành trình `LIMIT_SW` | `12` |

Thông số mái che:

| Hằng số | Giá trị | Ý nghĩa |
|---|---:|---|
| `STEPS_90_DEG` | `1000` | Số bước để mở mái che khoảng 90 độ |
| `UMBRELLA_OPEN_DIRECTION` | `-1` | Chiều mở mái che |
| `UMBRELLA_CLOSE_DIRECTION` | `1` | Chiều đóng mái che |
| `MAX_HOME_STEPS` | `10000` | Giới hạn số bước khi tìm vị trí gốc |

## 5. Các chế độ và trạng thái hệ thống

## 5.1. Chế độ điều khiển

Hệ thống có 2 chế độ:

| Chế độ | Ý nghĩa |
|---|---|
| `AUTO` | Tự động bám sáng và tự động che mưa |
| `MANUAL` | Người dùng điều khiển servo và mái che qua CoAP |

Trong code:

```cpp
enum ControlMode {
  MODE_AUTO,
  MODE_MANUAL
};
```

Mặc định khi khởi động:

```cpp
ControlMode currentMode = MODE_AUTO;
```

## 5.2. Trạng thái hệ thống

| Trạng thái | Ý nghĩa |
|---|---|
| `WIFI_CONNECTING` | ESP32 đang kết nối WiFi lần đầu |
| `WIFI_LOST` | Đã từng kết nối WiFi nhưng hiện bị mất kết nối |
| `AUTO_TRACKING` | Đang ở chế độ tự động bám sáng |
| `MANUAL_CONTROL` | Đang ở chế độ điều khiển thủ công |
| `RAIN_PROTECTION` | Đang phát hiện mưa và bảo vệ tấm pin |

Trong code:

```cpp
enum AppState {
  STATE_WIFI_CONNECTING,
  STATE_WIFI_LOST,
  STATE_AUTO_TRACKING,
  STATE_MANUAL_CONTROL,
  STATE_RAIN_PROTECTION
};
```

## 6. Luồng hoạt động tổng quát

Khi cấp nguồn, hệ thống chạy theo trình tự:

1. Khởi động Serial Monitor tốc độ `115200`.
2. Cấu hình ADC 12 bit cho các cảm biến analog.
3. Gắn servo ngang vào chân `18`, servo dọc vào chân `19`.
4. Đưa servo về vị trí ban đầu:
   - Ngang: `90` độ.
   - Dọc: `45` độ.
5. Cấu hình chân LDR, cảm biến mưa, motor bước, công tắc hành trình.
6. Tắt motor bước để tránh nóng cuộn dây.
7. Chạy homing mái che bằng `homeMotor()`.
8. Kết nối WiFi bằng `connectWiFi()`.
9. Cập nhật state machine.
10. Vòng lặp chính liên tục chạy:
    - `updateStateMachine()`
    - `runStateAction()`

## 7. Chuẩn bị trước khi demo

## 7.1. Chuẩn bị phần cứng

Trước khi bắt đầu trình bày, cần kiểm tra:

- ESP32 đã được nạp đúng sketch `bt6_ck_v2_kethopca2.ino`.
- Servo ngang và servo dọc hoạt động, không bị kẹt cơ khí.
- 4 LDR được đặt đúng vị trí quanh tấm pin.
- Cảm biến mưa kết nối đúng chân `36`.
- Motor bước quay đúng chiều mở/đóng mái che.
- Công tắc hành trình được nhấn khi mái che về vị trí đóng.
- Nguồn cấp đủ dòng cho servo và motor.
- ESP32 kết nối được WiFi có SSID trong code:

```cpp
const char* WIFI_SSID = "HOANG TAN";
const char* WIFI_PASS = "0795617961";
```

Lưu ý khi trình bày công khai: nếu không muốn lộ mật khẩu WiFi, nên sửa code hoặc che phần này khi chiếu màn hình.

## 7.2. Chuẩn bị phần mềm

Cần chuẩn bị:

- Arduino IDE hoặc Serial Monitor.
- Tool gửi CoAP, ví dụ:
  - Copper CoAP client.
  - CoAP CLI.
  - App điện thoại hỗ trợ CoAP.
  - Node-RED hoặc phần mềm nhóm tự viết nếu có.
- Đèn pin hoặc đèn điện thoại để demo bám sáng.
- Một ít nước hoặc khăn ướt để demo cảm biến mưa.
- Khăn khô để lau cảm biến sau khi demo mưa.

Serial Monitor:

```text
Baud rate: 115200
```

## 7.3. Thông tin CoAP cần nhớ

Khóa bí mật:

```text
SUNTRAC123
```

Dạng payload:

```text
SUNTRAC123:<DU_LIEU>
```

Ví dụ:

```text
SUNTRAC123:AUTO
SUNTRAC123:MANUAL
SUNTRAC123:OPEN
SUNTRAC123:CLOSE
SUNTRAC123:90
```

Các endpoint:

| Endpoint | Chức năng |
|---|---|
| `/state` | Đọc trạng thái hệ thống |
| `/mode` | Đổi chế độ AUTO/MANUAL |
| `/servo1` | Điều khiển servo dọc |
| `/servo2` | Điều khiển servo ngang |
| `/umbrella` | Điều khiển mái che |

## 8. Kịch bản demo tổng thể

Trình tự demo đề xuất:

1. Giới thiệu sản phẩm và phần cứng.
2. Cấp nguồn và quan sát hệ thống khởi động.
3. Demo trạng thái kết nối WiFi và CoAP.
4. Demo chế độ AUTO bám sáng.
5. Demo cảm biến mưa và mái che tự động.
6. Demo đọc trạng thái qua CoAP.
7. Demo chuyển sang MANUAL.
8. Demo điều khiển servo từ xa.
9. Demo điều khiển mái che từ xa.
10. Demo bảo vệ lệnh sai hoặc sai chế độ.
11. Chuyển lại AUTO và kết luận.

## 9. Lời thoại mở đầu

Có thể nói:

> Xin chào thầy/cô và các bạn. Nhóm em xin trình bày sản phẩm mô hình Solar Tracker thông minh có mái che mưa tự động. Sản phẩm sử dụng ESP32 làm bộ điều khiển trung tâm, 4 cảm biến ánh sáng để xác định hướng sáng, 2 servo để xoay tấm pin theo 2 trục, cảm biến mưa để phát hiện nước, động cơ bước để điều khiển mái che và WiFi CoAP để giám sát, điều khiển từ xa.

Tiếp theo:

> Mục tiêu của hệ thống là giúp tấm pin luôn hướng về nơi có ánh sáng mạnh hơn, đồng thời tự bảo vệ khi trời mưa. Hệ thống có 2 chế độ: tự động và thủ công. Ở chế độ tự động, thiết bị tự bám sáng và tự che mưa. Ở chế độ thủ công, người dùng có thể gửi lệnh qua mạng để điều khiển góc servo hoặc mở đóng mái che.

## 10. Demo 1: Khởi động hệ thống

## 10.1. Thao tác

1. Mở Serial Monitor ở baud rate `115200`.
2. Cấp nguồn hoặc nhấn reset ESP32.
3. Quan sát servo và mái che.
4. Quan sát log trên Serial Monitor.

## 10.2. Hiện tượng mong đợi

Khi khởi động:

- Servo ngang về `90` độ.
- Servo dọc về `45` độ.
- Motor bước chạy về vị trí gốc.
- Khi công tắc hành trình được nhấn, motor dừng.
- ESP32 bắt đầu kết nối WiFi.

Log mong đợi:

```text
[UMBRELLA] Homing
[UMBRELLA] Home OK

[WIFI] Connecting...
[APP] Solar Tracker Started
```

Sau khi WiFi kết nối:

```text
[WIFI] Reconnected
[WIFI] IP: 192.168.x.x
```

Khi state thay đổi:

```text
[STATE] WIFI_CONNECTING -> AUTO_TRACKING
```

## 10.3. Lời thoại

> Khi vừa cấp nguồn, ESP32 sẽ đưa 2 servo về vị trí ban đầu. Sau đó hệ thống chạy homing cho mái che. Motor bước quay đến khi công tắc hành trình được nhấn, lúc đó chương trình biết mái che đang ở vị trí đóng hoàn toàn và đặt `positionSteps = 0`.

> Sau bước homing, ESP32 bắt đầu kết nối WiFi. Khi có WiFi, CoAP server sẽ được khởi động để nhận lệnh điều khiển từ xa.

## 10.4. Điểm kỹ thuật nên nhấn mạnh

Trong hàm `setup()`:

```cpp
horizontal.write(servohori);
vertical.write(servovert);
homeMotor();
connectWiFi();
updateStateMachine();
```

Ý nghĩa:

- Servo được đưa về góc mặc định.
- Mái che được đưa về vị trí gốc.
- WiFi được khởi động.
- State machine được cập nhật ngay sau khi setup.

## 11. Demo 2: Theo dõi log hệ thống

## 11.1. Thao tác

Sau khi hệ thống chạy, giữ Serial Monitor mở.

Cứ khoảng 1 giây, hệ thống in thông tin:

```text
========== SYSTEM ==========
Mode: AUTO
State: AUTO_TRACKING
IP: 192.168.x.x
Rain: 0
Sensors: LT=... RT=... LD=... RD=... RainRaw=... RainThreshold=2000 RainDetected=0
Umbrella: 0
============================
```

## 11.2. Lời thoại

> Đây là log trạng thái của hệ thống. Mỗi giây chương trình in ra chế độ hiện tại, trạng thái hiện tại, địa chỉ IP, giá trị 4 cảm biến ánh sáng, giá trị cảm biến mưa và trạng thái mái che. Phần này giúp mình kiểm tra hệ thống đang hoạt động đúng hay không trong lúc demo.

## 11.3. Ý nghĩa các dòng log

| Dòng log | Ý nghĩa |
|---|---|
| `Mode` | Chế độ hiện tại: AUTO hoặc MANUAL |
| `State` | Trạng thái của state machine |
| `IP` | IP của ESP32 trong mạng WiFi |
| `Rain` | Có mưa hay không |
| `LT RT LD RD` | Giá trị 4 cảm biến ánh sáng |
| `RainRaw` | Giá trị analog cảm biến mưa |
| `RainThreshold` | Ngưỡng phát hiện mưa |
| `RainDetected` | Kết quả sau khi so sánh với ngưỡng |
| `Umbrella` | Mái che đang mở hay đóng |

## 12. Demo 3: Chế độ AUTO bám sáng

## 12.1. Mục tiêu

Chứng minh hệ thống tự điều chỉnh hướng tấm pin dựa vào độ lệch ánh sáng giữa các cảm biến LDR.

## 12.2. Thao tác

1. Đảm bảo hệ thống đang ở chế độ `AUTO`.
2. Dùng đèn pin chiếu vào vùng bên trái của cụm LDR.
3. Quan sát servo ngang xoay.
4. Chiếu sang vùng bên phải.
5. Quan sát servo ngang xoay theo chiều ngược lại.
6. Chiếu vào vùng phía trên.
7. Quan sát servo dọc thay đổi.
8. Chiếu vào vùng phía dưới.
9. Quan sát servo dọc thay đổi theo hướng ngược lại.

## 12.3. Hiện tượng mong đợi

- Khi ánh sáng lệch trái/phải, servo ngang thay đổi góc.
- Khi ánh sáng lệch trên/dưới, servo dọc thay đổi góc.
- Servo không nhảy mạnh mà thay đổi từng bước nhỏ.
- Góc servo luôn nằm trong giới hạn:
  - Ngang: `10` đến `170` độ.
  - Dọc: `10` đến `80` độ.

## 12.4. Lời thoại

> Ở chế độ tự động, hệ thống đọc 4 cảm biến ánh sáng. Sau đó chương trình tính trung bình ánh sáng phía trên, phía dưới, bên trái và bên phải. Nếu ánh sáng giữa hai phía bị lệch quá ngưỡng `tolerance = 250`, servo sẽ xoay từng bước nhỏ để đưa tấm pin về hướng cân bằng ánh sáng.

> Khi em chiếu đèn sang một phía, giá trị LDR phía đó thay đổi. ESP32 nhận ra độ lệch và điều chỉnh servo. Việc xoay từng bước `1 độ` sau mỗi `80 ms` giúp hệ thống chuyển động mượt hơn và tránh rung do nhiễu cảm biến.

## 12.5. Giải thích thuật toán trong code

Hàm chính:

```cpp
void autoTrackingTask()
```

Code đọc 4 LDR:

```cpp
int lt = readLDR(LDR_TOP_LEFT);
int rt = readLDR(LDR_TOP_RIGHT);
int ld = readLDR(LDR_BOTTOM_LEFT);
int rd = readLDR(LDR_BOTTOM_RIGHT);
```

Tính trung bình:

```cpp
int avt = (lt + rt) / 2;
int avd = (ld + rd) / 2;
int avl = (lt + ld) / 2;
int avr = (rt + rd) / 2;
```

Tính độ lệch:

```cpp
int dvert = avt - avd;
int dhoriz = avl - avr;
```

Nếu lệch dọc đủ lớn:

```cpp
if(abs(dvert) > tolerance)
{
  servovert += avt > avd ? stepSize : -stepSize;
  servovert = constrain(servovert, servovertLimitLow, servovertLimitHigh);
  vertical.write(servovert);
}
```

Nếu lệch ngang đủ lớn:

```cpp
if(abs(dhoriz) > tolerance)
{
  servohori += avl > avr ? -stepSize : stepSize;
  servohori = constrain(servohori, servohoriLimitLow, servohoriLimitHigh);
  horizontal.write(servohori);
}
```

## 12.6. Điểm kỹ thuật nên nhấn mạnh

- Hàm `readLDR()` đọc cảm biến 10 lần rồi lấy trung bình.
- Điều này giúp giảm nhiễu ADC.
- Servo chỉ xoay khi độ lệch vượt ngưỡng `250`.
- Có giới hạn góc để tránh servo quay quá biên cơ khí.
- Khoảng thời gian tracking là `80 ms`, giúp chuyển động không quá nhanh.

## 13. Demo 4: Phát hiện mưa và tự động mở mái che

## 13.1. Mục tiêu

Chứng minh hệ thống tự bảo vệ tấm pin khi phát hiện mưa.

## 13.2. Thao tác

1. Đảm bảo hệ thống đang ở chế độ `AUTO`.
2. Quan sát log `RainRaw` khi cảm biến khô.
3. Nhỏ vài giọt nước lên cảm biến mưa hoặc dùng khăn ướt chạm vào cảm biến.
4. Quan sát log `RainDetected`.
5. Quan sát mái che mở ra.
6. Lau khô cảm biến.
7. Quan sát mái che đóng lại về vị trí gốc.

## 13.3. Hiện tượng mong đợi khi có mưa

Khi cảm biến mưa bị ướt:

- Giá trị `RainRaw` giảm.
- Nếu `RainRaw < 2000`, hệ thống xác định có mưa.
- `RainDetected` chuyển thành `1`.
- State chuyển sang `RAIN_PROTECTION`.
- Motor bước mở mái che.

Log có thể xuất hiện:

```text
[STATE] AUTO_TRACKING -> RAIN_PROTECTION
[UMBRELLA] OPEN
```

## 13.4. Hiện tượng mong đợi khi hết mưa

Khi lau khô cảm biến:

- Giá trị `RainRaw` tăng lại.
- `RainDetected` chuyển thành `0`.
- Mái che bắt đầu đóng.
- Motor quay đến khi nhấn công tắc hành trình.
- Khi về gốc, motor dừng.

Log có thể xuất hiện:

```text
[UMBRELLA] CLOSE
[UMBRELLA] Home OK
[STATE] RAIN_PROTECTION -> AUTO_TRACKING
```

## 13.5. Lời thoại

> Bây giờ em sẽ demo chức năng bảo vệ khi trời mưa. Cảm biến mưa được đọc qua chân analog `36`. Trong chương trình, nếu giá trị cảm biến nhỏ hơn `2000`, hệ thống xem như đang có mưa.

> Khi em nhỏ nước lên cảm biến, trạng thái chuyển sang `RAIN_PROTECTION`. Lúc này chương trình gọi hàm mở mái che. Động cơ bước quay một số bước tương ứng để mở mái che bảo vệ tấm pin.

> Khi em lau khô cảm biến, hệ thống nhận biết hết mưa và gọi hàm đóng mái che. Motor quay về vị trí gốc cho đến khi công tắc hành trình được nhấn. Nhờ công tắc hành trình, hệ thống không bị lệch vị trí sau nhiều lần đóng mở.

## 13.6. Giải thích code phát hiện mưa

Hàm đọc cảm biến mưa:

```cpp
int readRainSensor()
{
  long total = 0;

  for(int i = 0; i < 10; i++)
  {
    total += analogRead(RAIN_SENSOR_PIN);
  }

  return total / 10;
}
```

Hàm xác định mưa:

```cpp
bool isRainDetected()
{
  return readRainSensor() < RAIN_THRESHOLD;
}
```

Hàm xử lý mưa:

```cpp
void rainTask()
{
  rainDetected = isRainDetected();

  if(rainDetected)
  {
    if(!umbrellaOpened || umbrellaIsClosing())
      openUmbrella();
  }
  else
  {
    if(umbrellaOpened || umbrellaIsMoving())
      closeUmbrella();
  }

  umbrellaMotionTask();
}
```

## 13.7. Điểm kỹ thuật nên nhấn mạnh

- Cảm biến mưa cũng được đọc 10 lần rồi lấy trung bình để giảm nhiễu.
- Hệ thống tự mở mái che khi có mưa.
- Hệ thống tự đóng mái che khi hết mưa.
- Mái che đóng bằng homing qua công tắc hành trình, không chỉ dựa vào số bước.
- Điều khiển motor trong `umbrellaMotionTask()` là dạng không chặn lâu, giúp chương trình vẫn xử lý WiFi và CoAP trong lúc motor đang chạy.

## 14. Demo 5: Đọc trạng thái qua CoAP

## 14.1. Mục tiêu

Chứng minh ESP32 có thể gửi dữ liệu cảm biến và trạng thái hệ thống cho thiết bị khác qua mạng WiFi.

## 14.2. Điều kiện

ESP32 đã kết nối WiFi và Serial Monitor đã in IP, ví dụ:

```text
[WIFI] IP: 192.168.1.25
```

Trong các ví dụ bên dưới, thay:

```text
<IP_ESP32>
```

bằng IP thật của ESP32.

## 14.3. Lệnh gửi

Endpoint:

```text
coap://<IP_ESP32>/state
```

Payload:

```text
SUNTRAC123:GET
```

Thực tế với `/state`, phần sau dấu `:` có thể là `GET` hoặc nội dung bất kỳ, miễn là khóa bí mật đúng.

## 14.4. Phản hồi mong đợi

ESP32 trả về JSON:

```json
{
  "lt": 1234,
  "rt": 1300,
  "ld": 1200,
  "rd": 1250,
  "v": 45,
  "h": 90,
  "rain": 0,
  "umbrella": 0,
  "mode": "AUTO",
  "state": "AUTO_TRACKING"
}
```

Ý nghĩa:

| Trường | Ý nghĩa |
|---|---|
| `lt` | Giá trị LDR trên trái |
| `rt` | Giá trị LDR trên phải |
| `ld` | Giá trị LDR dưới trái |
| `rd` | Giá trị LDR dưới phải |
| `v` | Góc servo dọc |
| `h` | Góc servo ngang |
| `rain` | `1` là có mưa, `0` là không mưa |
| `umbrella` | `1` là mái che đang mở, `0` là đóng |
| `mode` | Chế độ hiện tại |
| `state` | Trạng thái hiện tại |

## 14.5. Log Serial mong đợi

```text
[COAP RX] state -> SUNTRAC123:GET
[COAP TX] state -> {"lt":...,"rt":...,"ld":...,"rd":...,"v":...,"h":...,"rain":0,"umbrella":0,"mode":"AUTO","state":"AUTO_TRACKING"}
```

## 14.6. Lời thoại

> Đây là chức năng giám sát từ xa. Khi em gửi request đến endpoint `/state`, ESP32 kiểm tra khóa bí mật. Nếu đúng, ESP32 trả về dữ liệu dạng JSON gồm giá trị cảm biến, góc servo, trạng thái mưa, trạng thái mái che, chế độ và trạng thái hoạt động hiện tại.

## 15. Demo 6: Chuyển chế độ AUTO/MANUAL qua CoAP

## 15.1. Chuyển sang MANUAL

Endpoint:

```text
coap://<IP_ESP32>/mode
```

Payload:

```text
SUNTRAC123:MANUAL
```

Phản hồi:

```text
OK_MANUAL
```

Log Serial:

```text
[COAP RX] mode -> SUNTRAC123:MANUAL
[COAP TX] mode -> OK_MANUAL
[STATE] AUTO_TRACKING -> MANUAL_CONTROL
```

## 15.2. Chuyển sang AUTO

Endpoint:

```text
coap://<IP_ESP32>/mode
```

Payload:

```text
SUNTRAC123:AUTO
```

Phản hồi:

```text
OK_AUTO
```

Log Serial:

```text
[COAP RX] mode -> SUNTRAC123:AUTO
[COAP TX] mode -> OK_AUTO
[STATE] MANUAL_CONTROL -> AUTO_TRACKING
```

## 15.3. Lời thoại

> Hệ thống có 2 chế độ. Ở chế độ AUTO, ESP32 tự động bám sáng và xử lý mưa. Ở chế độ MANUAL, thuật toán bám sáng tạm dừng để người dùng có thể điều khiển góc servo và mái che từ xa.

> Em sẽ gửi lệnh `SUNTRAC123:MANUAL` đến endpoint `/mode`. Sau khi nhận lệnh, ESP32 chuyển sang trạng thái `MANUAL_CONTROL`. Khi muốn hệ thống tự động lại, em gửi `SUNTRAC123:AUTO`.

## 15.4. Giải thích code

```cpp
if(data == "AUTO")
{
  currentMode = MODE_AUTO;
  sendCoapText("mode", ip, port, packet.messageid, "OK_AUTO");
}
else if(data == "MANUAL")
{
  currentMode = MODE_MANUAL;
  sendCoapText("mode", ip, port, packet.messageid, "OK_MANUAL");
}
else
{
  sendCoapText("mode", ip, port, packet.messageid, "ERR_MODE");
}
```

## 16. Demo 7: Điều khiển servo thủ công qua CoAP

## 16.1. Điều kiện

Trước khi điều khiển servo, phải chuyển hệ thống sang `MANUAL`:

```text
SUNTRAC123:MANUAL
```

Nếu còn ở `AUTO`, ESP32 sẽ từ chối lệnh servo.

## 16.2. Điều khiển servo dọc

Endpoint:

```text
coap://<IP_ESP32>/servo1
```

Payload ví dụ:

```text
SUNTRAC123:60
```

Phản hồi:

```text
OK
```

Hiện tượng:

- Servo dọc quay đến khoảng `60` độ.
- Biến `servovert` được cập nhật.

Giới hạn:

```text
10 độ <= servo dọc <= 80 độ
```

Nếu gửi giá trị ngoài giới hạn, code dùng `constrain()` để ép về giới hạn an toàn.

Ví dụ:

```text
SUNTRAC123:100
```

Servo dọc sẽ bị giới hạn về `80` độ.

## 16.3. Điều khiển servo ngang

Endpoint:

```text
coap://<IP_ESP32>/servo2
```

Payload ví dụ:

```text
SUNTRAC123:120
```

Phản hồi:

```text
OK
```

Hiện tượng:

- Servo ngang quay đến khoảng `120` độ.
- Biến `servohori` được cập nhật.

Giới hạn:

```text
10 độ <= servo ngang <= 170 độ
```

## 16.4. Lời thoại

> Sau khi chuyển sang chế độ thủ công, em có thể gửi góc servo từ xa. Endpoint `/servo1` điều khiển servo dọc, còn `/servo2` điều khiển servo ngang. Code có dùng `constrain()` để giới hạn góc quay, tránh servo bị ép quá giới hạn cơ khí.

## 16.5. Log Serial mong đợi

Servo dọc:

```text
[COAP RX] servo1 -> SUNTRAC123:60
[COAP TX] servo1 -> OK
```

Servo ngang:

```text
[COAP RX] servo2 -> SUNTRAC123:120
[COAP TX] servo2 -> OK
```

## 16.6. Giải thích code servo dọc

```cpp
if(currentMode != MODE_MANUAL)
{
  sendCoapText("servo1", ip, port, packet.messageid, "AUTO_MODE_ACTIVE");
  return;
}

servovert = constrain(data.toInt(), servovertLimitLow, servovertLimitHigh);
vertical.write(servovert);

sendCoapText("servo1", ip, port, packet.messageid, "OK");
```

## 16.7. Giải thích code servo ngang

```cpp
if(currentMode != MODE_MANUAL)
{
  sendCoapText("servo2", ip, port, packet.messageid, "AUTO_MODE_ACTIVE");
  return;
}

servohori = constrain(data.toInt(), servohoriLimitLow, servohoriLimitHigh);
horizontal.write(servohori);

sendCoapText("servo2", ip, port, packet.messageid, "OK");
```

## 17. Demo 8: Điều khiển mái che thủ công qua CoAP

## 17.1. Điều kiện

Hệ thống phải ở chế độ `MANUAL`.

Nếu đang ở `AUTO`, endpoint `/umbrella` sẽ trả về:

```text
AUTO_MODE_ACTIVE
```

## 17.2. Mở mái che

Endpoint:

```text
coap://<IP_ESP32>/umbrella
```

Payload:

```text
SUNTRAC123:OPEN
```

Phản hồi:

```text
OPEN_OK
```

Hiện tượng:

- Motor bước quay theo chiều mở.
- Mái che mở ra.
- Khi đủ số bước, motor dừng.
- Biến `umbrellaOpened` chuyển thành `true`.

Log:

```text
[COAP RX] umbrella -> SUNTRAC123:OPEN
[UMBRELLA] OPEN
[COAP TX] umbrella -> OPEN_OK
```

## 17.3. Đóng mái che

Endpoint:

```text
coap://<IP_ESP32>/umbrella
```

Payload:

```text
SUNTRAC123:CLOSE
```

Phản hồi:

```text
CLOSE_OK
```

Hiện tượng:

- Motor bước quay theo chiều đóng.
- Mái che đóng về vị trí gốc.
- Khi công tắc hành trình được nhấn đủ thời gian chống dội, motor dừng.
- Biến `umbrellaOpened` chuyển thành `false`.

Log:

```text
[COAP RX] umbrella -> SUNTRAC123:CLOSE
[UMBRELLA] CLOSE
[COAP TX] umbrella -> CLOSE_OK
[UMBRELLA] Home OK
```

## 17.4. Lời thoại

> Ngoài điều khiển servo, hệ thống còn cho phép điều khiển mái che từ xa. Khi gửi `OPEN`, motor bước mở mái che theo số bước đã cấu hình. Khi gửi `CLOSE`, motor quay về vị trí gốc cho đến khi công tắc hành trình được nhấn. Cách này giúp mái che đóng chính xác hơn so với chỉ đếm số bước.

## 17.5. Giải thích code

```cpp
if(data == "OPEN")
{
  openUmbrella();
  sendCoapText("umbrella", ip, port, packet.messageid, "OPEN_OK");
}
else if(data == "CLOSE")
{
  closeUmbrella();
  sendCoapText("umbrella", ip, port, packet.messageid, "CLOSE_OK");
}
else
{
  sendCoapText("umbrella", ip, port, packet.messageid, "ERR_CMD");
}
```

## 18. Demo 9: Kiểm tra bảo mật bằng SECRET_KEY

## 18.1. Mục tiêu

Chứng minh ESP32 không nhận lệnh nếu payload không có khóa đúng.

## 18.2. Thao tác

Gửi request đến `/mode` với payload sai:

```text
SAI_KEY:MANUAL
```

Hoặc gửi payload thiếu dấu `:`:

```text
MANUAL
```

## 18.3. Phản hồi mong đợi

```text
ERR_SECRET
```

## 18.4. Lời thoại

> Mỗi lệnh gửi đến ESP32 phải có dạng `SECRET_KEY:DU_LIEU`. Trong code hiện tại, khóa là `SUNTRAC123`. Nếu khóa sai hoặc payload không đúng định dạng, ESP32 trả về `ERR_SECRET` và không thực hiện lệnh.

## 18.5. Giải thích code

```cpp
bool verifySecret(CoapPacket &packet, String &data)
{
  String payload = getPayload(packet);
  int index = payload.indexOf(':');

  if(index < 0)
  {
    data = "";
    return false;
  }

  String secret = payload.substring(0, index);
  data = payload.substring(index + 1);
  data.trim();

  return secret == SECRET_KEY;
}
```

Ý nghĩa:

- Tách payload tại dấu `:`.
- Phần trước dấu `:` là khóa bí mật.
- Phần sau dấu `:` là dữ liệu lệnh.
- Nếu khóa đúng thì xử lý tiếp.
- Nếu khóa sai thì trả về lỗi.

## 19. Demo 10: Kiểm tra bảo vệ sai chế độ

## 19.1. Mục tiêu

Chứng minh khi đang ở chế độ `AUTO`, người dùng không thể điều khiển servo hoặc mái che thủ công.

## 19.2. Thao tác

Chuyển về AUTO:

```text
Endpoint: coap://<IP_ESP32>/mode
Payload:  SUNTRAC123:AUTO
```

Sau đó thử điều khiển servo:

```text
Endpoint: coap://<IP_ESP32>/servo1
Payload:  SUNTRAC123:60
```

Hoặc thử điều khiển mái che:

```text
Endpoint: coap://<IP_ESP32>/umbrella
Payload:  SUNTRAC123:OPEN
```

## 19.3. Phản hồi mong đợi

```text
AUTO_MODE_ACTIVE
```

## 19.4. Lời thoại

> Khi hệ thống đang ở AUTO, thuật toán tự động đang có quyền điều khiển servo và mái che. Vì vậy các lệnh thủ công sẽ bị từ chối để tránh xung đột điều khiển. Muốn điều khiển bằng tay, người dùng phải chuyển sang MANUAL trước.

## 20. Demo 11: Mất WiFi và tự kết nối lại

## 20.1. Mục tiêu

Chứng minh hệ thống có xử lý trường hợp mất WiFi.

## 20.2. Thao tác

Nếu có thể thực hiện an toàn trong buổi demo:

1. Tắt hotspot hoặc router WiFi trong thời gian ngắn.
2. Quan sát Serial Monitor.
3. Bật WiFi lại.
4. Quan sát ESP32 tự reconnect.

## 20.3. Hiện tượng mong đợi

Khi mất WiFi:

```text
[STATE] AUTO_TRACKING -> WIFI_LOST
[WIFI] Reconnecting...
```

Khi WiFi có lại:

```text
[WIFI] Reconnected
[WIFI] IP: 192.168.x.x
[STATE] WIFI_LOST -> AUTO_TRACKING
```

## 20.4. Lời thoại

> Hệ thống kiểm tra WiFi định kỳ mỗi 5 giây. Nếu mất kết nối, ESP32 gọi lại `WiFi.begin()` để reconnect. Nếu đã từng kết nối thành công mà sau đó mất mạng, state sẽ chuyển sang `WIFI_LOST`.

## 20.5. Giải thích code

```cpp
void wifiTask()
{
  if(lastWiFiCheck != 0 && millis() - lastWiFiCheck < 5000)
    return;

  lastWiFiCheck = millis();

  if(WiFi.status() == WL_CONNECTED)
  {
    if(!wifiConnectedLogged)
    {
      wifiConnectedLogged = true;
      Serial.println("[WIFI] Reconnected");
      Serial.print("[WIFI] IP: ");
      Serial.println(WiFi.localIP());
    }

    return;
  }

  wifiConnectedLogged = false;

  Serial.println("[WIFI] Reconnecting...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}
```

## 21. Bảng lệnh CoAP dùng khi demo

Thay `<IP_ESP32>` bằng IP thật của ESP32.

| Mục đích | Endpoint | Payload | Phản hồi mong đợi |
|---|---|---|---|
| Đọc trạng thái | `coap://<IP_ESP32>/state` | `SUNTRAC123:GET` | JSON trạng thái |
| Chuyển AUTO | `coap://<IP_ESP32>/mode` | `SUNTRAC123:AUTO` | `OK_AUTO` |
| Chuyển MANUAL | `coap://<IP_ESP32>/mode` | `SUNTRAC123:MANUAL` | `OK_MANUAL` |
| Servo dọc 60 độ | `coap://<IP_ESP32>/servo1` | `SUNTRAC123:60` | `OK` |
| Servo dọc 30 độ | `coap://<IP_ESP32>/servo1` | `SUNTRAC123:30` | `OK` |
| Servo ngang 120 độ | `coap://<IP_ESP32>/servo2` | `SUNTRAC123:120` | `OK` |
| Servo ngang 70 độ | `coap://<IP_ESP32>/servo2` | `SUNTRAC123:70` | `OK` |
| Mở mái che | `coap://<IP_ESP32>/umbrella` | `SUNTRAC123:OPEN` | `OPEN_OK` |
| Đóng mái che | `coap://<IP_ESP32>/umbrella` | `SUNTRAC123:CLOSE` | `CLOSE_OK` |
| Test sai key | `coap://<IP_ESP32>/mode` | `WRONG:AUTO` | `ERR_SECRET` |
| Test sai mode | `coap://<IP_ESP32>/mode` | `SUNTRAC123:ABC` | `ERR_MODE` |
| Test sai lệnh mái che | `coap://<IP_ESP32>/umbrella` | `SUNTRAC123:STOP` | `ERR_CMD` |

## 22. Bảng lỗi và cách giải thích khi demo

| Phản hồi | Nguyên nhân | Cách giải thích |
|---|---|---|
| `ERR_SECRET` | Sai khóa hoặc thiếu dấu `:` | ESP32 từ chối lệnh vì không đúng khóa bí mật |
| `ERR_MODE` | Gửi mode không phải `AUTO` hoặc `MANUAL` | Lệnh đổi chế độ không hợp lệ |
| `AUTO_MODE_ACTIVE` | Đang AUTO nhưng gửi lệnh servo/mái che thủ công | Phải chuyển sang MANUAL trước khi điều khiển tay |
| `ERR_CMD` | Lệnh mái che không phải `OPEN` hoặc `CLOSE` | Endpoint `/umbrella` chỉ nhận 2 lệnh mở và đóng |

## 23. Checklist trước khi thuyết trình

Trước khi bắt đầu:

- [ ] ESP32 đã nạp code mới nhất.
- [ ] Serial Monitor mở ở `115200`.
- [ ] WiFi đúng SSID và password.
- [ ] ESP32 lấy được IP.
- [ ] CoAP client kết nối cùng mạng WiFi với ESP32.
- [ ] Gửi thử `/state` nhận được JSON.
- [ ] Servo ngang hoạt động.
- [ ] Servo dọc hoạt động.
- [ ] Motor bước mở mái che đúng chiều.
- [ ] Motor bước đóng mái che đúng chiều.
- [ ] Công tắc hành trình hoạt động.
- [ ] Cảm biến mưa trả giá trị hợp lý.
- [ ] Có đèn pin để demo LDR.
- [ ] Có nước hoặc khăn ướt để demo mưa.
- [ ] Có khăn khô để lau cảm biến sau demo.

## 24. Checklist trong lúc demo

Thứ tự thao tác ngắn gọn:

- [ ] Cấp nguồn/reset ESP32.
- [ ] Chờ homing mái che xong.
- [ ] Chờ WiFi kết nối.
- [ ] Ghi lại IP ESP32.
- [ ] Giới thiệu log hệ thống.
- [ ] Demo bám sáng bằng đèn pin.
- [ ] Demo mưa bằng nước/khăn ướt.
- [ ] Lau khô cảm biến để mái che đóng lại.
- [ ] Gửi `/state` để đọc JSON.
- [ ] Gửi `/mode` sang MANUAL.
- [ ] Gửi `/servo1` điều khiển servo dọc.
- [ ] Gửi `/servo2` điều khiển servo ngang.
- [ ] Gửi `/umbrella OPEN`.
- [ ] Gửi `/umbrella CLOSE`.
- [ ] Test sai key hoặc sai chế độ.
- [ ] Gửi `/mode` về AUTO.
- [ ] Kết luận.

## 25. Checklist sau khi demo

Sau khi hoàn thành:

- [ ] Đóng mái che về vị trí gốc.
- [ ] Chuyển hệ thống về `AUTO`.
- [ ] Lau khô cảm biến mưa.
- [ ] Tắt nguồn motor/servo nếu không sử dụng nữa.
- [ ] Ngắt nguồn ESP32 nếu kết thúc buổi trình bày.

## 26. Kịch bản nói hoàn chỉnh

Phần này là lời thoại có thể đọc gần như nguyên văn.

### 26.1. Mở đầu

> Xin chào thầy/cô và các bạn. Nhóm em xin trình bày sản phẩm mô hình Solar Tracker thông minh có mái che mưa tự động.

> Sản phẩm sử dụng ESP32 làm bộ điều khiển trung tâm. Hệ thống gồm 4 cảm biến ánh sáng LDR, 2 servo để xoay tấm pin, cảm biến mưa, động cơ bước để đóng mở mái che, công tắc hành trình để xác định vị trí gốc và module WiFi tích hợp trên ESP32 để điều khiển qua giao thức CoAP.

> Mục tiêu của sản phẩm là giúp tấm pin tự hướng về nơi có ánh sáng mạnh hơn, đồng thời tự bảo vệ khi gặp mưa. Ngoài ra người dùng có thể giám sát và điều khiển hệ thống từ xa qua mạng WiFi.

### 26.2. Giới thiệu phần cứng

> Trên mô hình, đây là cụm 4 cảm biến LDR được đặt ở 4 hướng: trên trái, trên phải, dưới trái và dưới phải. Dựa vào sự chênh lệch giá trị giữa các cảm biến, ESP32 xác định hướng ánh sáng đang lệch về phía nào.

> Đây là 2 servo. Servo thứ nhất điều khiển trục dọc, servo thứ hai điều khiển trục ngang. Nhờ đó tấm pin có thể thay đổi hướng theo ánh sáng.

> Đây là cảm biến mưa. Khi có nước trên cảm biến, giá trị analog giảm xuống. Nếu giá trị nhỏ hơn ngưỡng `2000`, chương trình xác định là có mưa.

> Đây là động cơ bước dùng để mở và đóng mái che. Khi đóng, motor quay đến khi công tắc hành trình được nhấn, nhờ vậy hệ thống biết mái che đã về đúng vị trí gốc.

### 26.3. Demo khởi động

> Bây giờ em sẽ cấp nguồn cho hệ thống.

> Khi khởi động, 2 servo được đưa về góc ban đầu. Servo ngang là `90` độ, servo dọc là `45` độ. Sau đó động cơ bước bắt đầu homing mái che. Motor quay cho đến khi công tắc hành trình được nhấn, lúc đó chương trình đặt vị trí mái che là gốc.

> Trên Serial Monitor có dòng `[UMBRELLA] Homing`, sau đó là `[UMBRELLA] Home OK`. Điều này cho thấy phần mái che đã xác định vị trí ban đầu thành công.

> Tiếp theo ESP32 kết nối WiFi. Khi kết nối thành công, Serial Monitor in ra địa chỉ IP. Địa chỉ IP này sẽ được dùng để gửi lệnh CoAP.

### 26.4. Demo AUTO bám sáng

> Hiện tại hệ thống đang ở chế độ mặc định là AUTO. Ở chế độ này, hệ thống tự động đọc 4 cảm biến ánh sáng và điều khiển servo.

> Em sẽ dùng đèn pin chiếu vào một phía của cảm biến. Khi ánh sáng lệch sang trái hoặc sang phải, servo ngang sẽ thay đổi góc. Khi ánh sáng lệch phía trên hoặc phía dưới, servo dọc sẽ thay đổi góc.

> Trong code, chương trình tính trung bình ánh sáng phía trên, phía dưới, bên trái và bên phải. Nếu độ lệch lớn hơn `tolerance = 250`, servo sẽ xoay từng bước `1` độ. Việc này giúp tấm pin xoay mượt và tránh bị rung khi cảm biến có nhiễu nhỏ.

### 26.5. Demo che mưa tự động

> Tiếp theo em sẽ demo chức năng phát hiện mưa. Khi cảm biến mưa khô, giá trị analog thường lớn hơn ngưỡng `2000`, hệ thống hiểu là không mưa.

> Bây giờ em nhỏ nước lên cảm biến. Khi giá trị cảm biến nhỏ hơn `2000`, biến `rainDetected` chuyển thành `true`, trạng thái hệ thống chuyển sang `RAIN_PROTECTION`, và motor bước mở mái che.

> Khi em lau khô cảm biến, hệ thống hiểu là hết mưa. Motor bước sẽ đóng mái che về vị trí gốc. Khi công tắc hành trình được nhấn, motor dừng và Serial Monitor hiển thị `[UMBRELLA] Home OK`.

> Điểm quan trọng là phần điều khiển mái che được viết theo dạng task, không chặn chương trình lâu. Vì vậy trong khi motor đang chạy, ESP32 vẫn có thể xử lý WiFi, CoAP và cập nhật trạng thái.

### 26.6. Demo đọc trạng thái CoAP

> Bây giờ em sẽ demo phần giám sát từ xa qua CoAP. Em gửi request đến endpoint `/state` với payload `SUNTRAC123:GET`.

> ESP32 kiểm tra khóa bí mật. Nếu đúng, ESP32 trả về chuỗi JSON gồm giá trị 4 cảm biến LDR, góc servo, trạng thái mưa, trạng thái mái che, chế độ hiện tại và state hiện tại.

> Ví dụ trường `mode` cho biết đang là AUTO hay MANUAL. Trường `state` cho biết hệ thống đang bám sáng, điều khiển thủ công, mất WiFi hay đang bảo vệ mưa.

### 26.7. Demo chuyển MANUAL

> Tiếp theo em chuyển hệ thống sang chế độ thủ công bằng cách gửi lệnh `SUNTRAC123:MANUAL` đến endpoint `/mode`.

> ESP32 phản hồi `OK_MANUAL`. Lúc này state chuyển sang `MANUAL_CONTROL`. Ở chế độ này, thuật toán bám sáng không tự điều khiển servo nữa, người dùng có thể điều khiển bằng lệnh từ xa.

### 26.8. Demo điều khiển servo

> Em gửi lệnh đến `/servo1` với payload `SUNTRAC123:60`. Servo dọc sẽ quay đến 60 độ và ESP32 phản hồi `OK`.

> Tiếp theo em gửi lệnh đến `/servo2` với payload `SUNTRAC123:120`. Servo ngang sẽ quay đến 120 độ.

> Trong code có giới hạn góc bằng `constrain()`. Servo dọc chỉ nằm trong khoảng `10` đến `80` độ, servo ngang nằm trong khoảng `10` đến `170` độ. Điều này giúp bảo vệ cơ khí và tránh servo quay quá giới hạn.

### 26.9. Demo điều khiển mái che

> Ở chế độ MANUAL, em cũng có thể điều khiển mái che. Khi gửi `SUNTRAC123:OPEN` đến endpoint `/umbrella`, ESP32 phản hồi `OPEN_OK` và motor bước mở mái che.

> Khi gửi `SUNTRAC123:CLOSE`, ESP32 phản hồi `CLOSE_OK` và motor bước đóng mái che về công tắc hành trình.

### 26.10. Demo bảo vệ lệnh sai

> Hệ thống có kiểm tra khóa bí mật. Nếu em gửi payload sai, ví dụ `WRONG:AUTO`, ESP32 trả về `ERR_SECRET` và không thực hiện lệnh.

> Ngoài ra, nếu hệ thống đang ở AUTO mà người dùng gửi lệnh điều khiển servo hoặc mái che thủ công, ESP32 trả về `AUTO_MODE_ACTIVE`. Điều này tránh việc thuật toán tự động và lệnh thủ công cùng điều khiển thiết bị một lúc.

### 26.11. Kết luận

> Qua phần demo, hệ thống đã thực hiện được các chức năng chính: tự động xoay tấm pin theo hướng sáng, tự phát hiện mưa và mở mái che, tự đóng mái che khi hết mưa, điều khiển và giám sát từ xa qua CoAP, đồng thời có kiểm tra khóa bí mật và kiểm soát chế độ hoạt động.

> Sản phẩm mô phỏng được một hệ thống solar tracker thông minh, vừa tối ưu hướng nhận ánh sáng, vừa có khả năng bảo vệ phần pin khi gặp điều kiện thời tiết xấu.

## 27. Câu hỏi thường gặp khi bị hỏi

### Câu 1: Vì sao dùng 4 cảm biến LDR?

Trả lời:

> Vì 4 cảm biến giúp xác định ánh sáng lệch theo cả 2 trục. So sánh trung bình trên/dưới để điều khiển servo dọc, so sánh trung bình trái/phải để điều khiển servo ngang.

### Câu 2: Vì sao cần ngưỡng `tolerance = 250`?

Trả lời:

> Nếu không có ngưỡng, chỉ cần nhiễu nhỏ từ cảm biến cũng làm servo liên tục rung. Ngưỡng `250` giúp servo chỉ xoay khi độ lệch ánh sáng đủ lớn.

### Câu 3: Vì sao đọc cảm biến 10 lần rồi lấy trung bình?

Trả lời:

> Vì tín hiệu analog có thể bị nhiễu. Lấy trung bình 10 lần giúp giá trị ổn định hơn trước khi ra quyết định.

### Câu 4: Vì sao cần công tắc hành trình?

Trả lời:

> Motor bước có thể bị trượt bước hoặc sai lệch sau nhiều lần chạy. Công tắc hành trình giúp hệ thống xác định lại vị trí gốc khi đóng mái che, nhờ đó vị trí đóng chính xác hơn.

### Câu 5: Nếu mất WiFi thì hệ thống có còn bám sáng không?

Trả lời:

> Trong code hiện tại, khi mất WiFi state chuyển sang `WIFI_LOST` và chương trình ưu tiên reconnect, in log hệ thống. Khi WiFi kết nối lại, hệ thống quay về trạng thái hoạt động tương ứng. Phần này có thể cải tiến thêm để vẫn bám sáng offline nếu yêu cầu thực tế cần.

### Câu 6: CoAP dùng để làm gì?

Trả lời:

> CoAP là giao thức truyền thông nhẹ, phù hợp với IoT. Trong mô hình này, CoAP dùng để gửi lệnh điều khiển và nhận trạng thái từ ESP32 qua WiFi.

### Câu 7: Vì sao cần `SECRET_KEY`?

Trả lời:

> `SECRET_KEY` là lớp kiểm tra đơn giản để tránh thiết bị nhận lệnh không mong muốn. Nếu payload không có đúng khóa, ESP32 trả về `ERR_SECRET`.

### Câu 8: Hệ thống có thể mở rộng như thế nào?

Trả lời:

> Có thể mở rộng bằng cách thêm giao diện web hoặc app điện thoại, lưu dữ liệu cảm biến lên server, thêm cảm biến dòng áp để đo công suất pin, thêm pin sạc và thuật toán tối ưu năng lượng, hoặc thêm cơ chế điều khiển mái che theo dự báo thời tiết.

## 28. Rủi ro khi demo và cách xử lý nhanh

| Vấn đề | Nguyên nhân có thể | Cách xử lý |
|---|---|---|
| ESP32 không có IP | Sai WiFi, WiFi yếu, chưa cùng mạng | Kiểm tra SSID/password, reset ESP32, đưa gần router |
| Không gửi được CoAP | Sai IP, client không cùng mạng, CoAP server chưa start | Xem Serial Monitor, kiểm tra IP, gửi lại `/state` |
| Servo không quay | Thiếu nguồn, sai chân, đang AUTO/MANUAL không đúng | Kiểm tra nguồn servo, kiểm tra mode, kiểm tra dây |
| Servo rung | Nguồn yếu hoặc nhiễu cảm biến | Dùng nguồn ngoài đủ dòng, kiểm tra mass chung |
| Mái che mở sai chiều | Đảo chiều motor hoặc sai hằng số direction | Đổi `UMBRELLA_OPEN_DIRECTION` và `UMBRELLA_CLOSE_DIRECTION` |
| Mái che không dừng khi đóng | Công tắc hành trình chưa hoạt động | Kiểm tra chân `12`, wiring, `INPUT_PULLUP`, cơ khí nhấn switch |
| Cảm biến mưa không nhận | Ngưỡng chưa phù hợp hoặc cảm biến chưa ướt đủ | Xem `RainRaw`, điều chỉnh `RAIN_THRESHOLD` |
| Lệnh trả `ERR_SECRET` | Payload sai định dạng | Gửi đúng dạng `SUNTRAC123:<DU_LIEU>` |
| Lệnh trả `AUTO_MODE_ACTIVE` | Đang ở AUTO | Gửi `/mode` với `SUNTRAC123:MANUAL` trước |

## 29. Phiên bản demo ngắn 3 phút

Nếu thời gian ngắn, trình bày theo thứ tự này:

1. Giới thiệu:
   > Đây là mô hình Solar Tracker dùng ESP32, 4 LDR, 2 servo, cảm biến mưa, motor bước và CoAP.

2. Khởi động:
   > Khi cấp nguồn, hệ thống homing mái che bằng công tắc hành trình, sau đó kết nối WiFi.

3. Bám sáng:
   > Em chiếu đèn sang các hướng, servo tự xoay theo hướng sáng.

4. Che mưa:
   > Em nhỏ nước lên cảm biến, mái che tự mở. Lau khô cảm biến, mái che tự đóng.

5. CoAP:
   > Em gửi `/state` để đọc JSON, gửi `/mode` để chuyển MANUAL, gửi `/servo1`, `/servo2`, `/umbrella` để điều khiển.

6. Kết luận:
   > Hệ thống đã có tự động bám sáng, bảo vệ mưa và điều khiển từ xa.

## 30. Phiên bản demo đầy đủ 7-10 phút

Nếu có nhiều thời gian, trình bày:

1. Giới thiệu bài toán và mục tiêu.
2. Giới thiệu phần cứng.
3. Giải thích state machine.
4. Demo khởi động và homing.
5. Demo log hệ thống.
6. Demo AUTO tracking.
7. Giải thích thuật toán LDR.
8. Demo cảm biến mưa và mái che.
9. Giải thích công tắc hành trình.
10. Demo CoAP `/state`.
11. Demo đổi mode.
12. Demo servo thủ công.
13. Demo mái che thủ công.
14. Demo sai key/sai chế độ.
15. Kết luận và hướng phát triển.

