# Demo Cases - Solar Tracker Có Che Mưa Tự Động

Tài liệu này liệt kê các case nên demo cho sketch `bt6_ck_v2_kethopca2.ino`.

Mục đích:

- Dùng làm checklist khi chạy demo.
- Dễ chia vai: người thuyết trình, người thao tác phần cứng, người gửi lệnh CoAP.
- Dễ kiểm tra hệ thống trước khi báo cáo.

## Thông tin chung

### WiFi

ESP32 kết nối WiFi theo thông tin trong code:

```cpp
const char* WIFI_SSID = "HOANG TAN";
const char* WIFI_PASS = "0795617961";
```

Khi ESP32 kết nối thành công, xem IP trên Serial Monitor:

```text
[WIFI] IP: 192.168.x.x
```

Trong tài liệu này, thay:

```text
<IP_ESP32>
```

bằng IP thật của ESP32.

### CoAP Secret Key

Tất cả lệnh CoAP hợp lệ phải có dạng:

```text
SUNTRAC123:<DATA>
```

Nếu sai key hoặc thiếu dấu `:`, ESP32 trả về:

```text
ERR_SECRET
```

### Endpoint CoAP

| Endpoint | Chức năng |
|---|---|
| `/state` | Đọc trạng thái hệ thống |
| `/mode` | Đổi chế độ `AUTO` hoặc `MANUAL` |
| `/servo1` | Điều khiển servo dọc |
| `/servo2` | Điều khiển servo ngang |
| `/umbrella` | Điều khiển mái che |

### Trạng thái chính

| State | Ý nghĩa |
|---|---|
| `WIFI_CONNECTING` | Đang kết nối WiFi lần đầu |
| `WIFI_LOST` | Đã mất WiFi sau khi từng kết nối thành công |
| `AUTO_TRACKING` | Đang tự động bám sáng |
| `MANUAL_CONTROL` | Đang điều khiển thủ công |
| `RAIN_PROTECTION` | Đang bảo vệ khi phát hiện mưa |

## Case 01 - Khởi động hệ thống

### Mục tiêu

Kiểm tra hệ thống khởi động đúng: servo về góc ban đầu, mái che homing, ESP32 bắt đầu kết nối WiFi.

### Điều kiện trước

- ESP32 đã nạp code.
- Serial Monitor mở ở `115200`.
- Motor, servo, cảm biến và công tắc hành trình đã nối đúng.
- Mái che có thể di chuyển về công tắc hành trình.

### Thao tác

1. Cấp nguồn cho ESP32.
2. Quan sát servo.
3. Quan sát motor mái che.
4. Quan sát Serial Monitor.

### Kết quả mong đợi

- Servo ngang về `90` độ.
- Servo dọc về `45` độ.
- Motor bước quay để tìm vị trí gốc.
- Khi công tắc hành trình được nhấn, motor dừng.
- ESP32 bắt đầu kết nối WiFi.

### Log mong đợi

```text
[UMBRELLA] Homing
[UMBRELLA] Home OK

[WIFI] Connecting...
[APP] Solar Tracker Started
```

### Đạt khi

- Có log `Home OK`.
- Motor không chạy mãi.
- Không có log `Home ERROR`.

## Case 02 - Kết nối WiFi và khởi động CoAP

### Mục tiêu

Kiểm tra ESP32 kết nối WiFi và CoAP server được start.

### Điều kiện trước

- WiFi đúng SSID/password.
- ESP32 trong vùng phủ sóng WiFi.

### Thao tác

1. Sau khi khởi động, chờ vài giây.
2. Xem IP trên Serial Monitor.
3. Gửi thử request `/state`.

### Lệnh CoAP

```text
Endpoint: coap://<IP_ESP32>/state
Payload:  SUNTRAC123:GET
```

### Kết quả mong đợi

- Serial Monitor in IP.
- CoAP server start.
- Request `/state` trả về JSON.

### Log mong đợi

```text
[WIFI] Reconnected
[WIFI] IP: 192.168.x.x
[COAP] Started
[COAP RX] state -> SUNTRAC123:GET
[COAP TX] state -> {"lt":...}
```

### Đạt khi

- Gửi được CoAP.
- Có JSON trạng thái trả về.

## Case 03 - Đọc trạng thái hệ thống qua `/state`

### Mục tiêu

Kiểm tra endpoint `/state` trả về đầy đủ thông tin cảm biến, servo, mưa, mái che, mode và state.

### Điều kiện trước

- ESP32 đã có IP.
- CoAP client cùng mạng với ESP32.

### Thao tác

Gửi request:

```text
Endpoint: coap://<IP_ESP32>/state
Payload:  SUNTRAC123:GET
```

### Kết quả mong đợi

ESP32 trả về JSON tương tự:

```json
{
  "lt": 1234,
  "rt": 1250,
  "ld": 1200,
  "rd": 1220,
  "v": 45,
  "h": 90,
  "rain": 0,
  "umbrella": 0,
  "mode": "AUTO",
  "state": "AUTO_TRACKING"
}
```

### Các trường cần kiểm tra

| Trường | Kỳ vọng |
|---|---|
| `lt`, `rt`, `ld`, `rd` | Có giá trị analog |
| `v` | Góc servo dọc |
| `h` | Góc servo ngang |
| `rain` | `0` khi khô, `1` khi có nước |
| `umbrella` | `0` đóng, `1` mở |
| `mode` | `AUTO` hoặc `MANUAL` |
| `state` | State hiện tại |

### Đạt khi

- JSON đúng định dạng.
- Có đủ tất cả trường trên.

## Case 04 - AUTO tracking theo ánh sáng ngang

### Mục tiêu

Kiểm tra servo ngang tự xoay khi ánh sáng lệch trái/phải.

### Điều kiện trước

- Mode đang là `AUTO`.
- State đang là `AUTO_TRACKING`.
- Không có mưa.

Nếu chưa ở AUTO, gửi:

```text
Endpoint: coap://<IP_ESP32>/mode
Payload:  SUNTRAC123:AUTO
```

### Thao tác

1. Dùng đèn pin chiếu lệch sang bên trái cụm LDR.
2. Quan sát servo ngang.
3. Chuyển đèn pin sang bên phải cụm LDR.
4. Quan sát servo ngang đổi hướng.

### Kết quả mong đợi

- Servo ngang thay đổi từng bước nhỏ.
- Góc servo không vượt giới hạn `10` đến `170` độ.
- Serial log giá trị LDR trái/phải thay đổi.

### Giải thích khi demo

Hệ thống tính:

```cpp
int avl = (lt + ld) / 2;
int avr = (rt + rd) / 2;
int dhoriz = avl - avr;
```

Nếu `abs(dhoriz) > tolerance`, servo ngang sẽ xoay.

### Đạt khi

- Chiếu trái/phải làm servo ngang phản ứng rõ.
- Servo không rung quá mạnh.

## Case 05 - AUTO tracking theo ánh sáng dọc

### Mục tiêu

Kiểm tra servo dọc tự xoay khi ánh sáng lệch trên/dưới.

### Điều kiện trước

- Mode đang là `AUTO`.
- State đang là `AUTO_TRACKING`.
- Không có mưa.

### Thao tác

1. Dùng đèn pin chiếu lệch phía trên cụm LDR.
2. Quan sát servo dọc.
3. Chuyển đèn pin xuống phía dưới cụm LDR.
4. Quan sát servo dọc đổi hướng.

### Kết quả mong đợi

- Servo dọc thay đổi từng bước nhỏ.
- Góc servo không vượt giới hạn `10` đến `80` độ.

### Giải thích khi demo

Hệ thống tính:

```cpp
int avt = (lt + rt) / 2;
int avd = (ld + rd) / 2;
int dvert = avt - avd;
```

Nếu `abs(dvert) > tolerance`, servo dọc sẽ xoay.

### Đạt khi

- Chiếu trên/dưới làm servo dọc phản ứng rõ.

## Case 06 - Ngưỡng chống rung tracking

### Mục tiêu

Chứng minh servo không xoay khi độ lệch ánh sáng nhỏ hơn ngưỡng `tolerance = 250`.

### Điều kiện trước

- Mode đang là `AUTO`.
- Không có mưa.

### Thao tác

1. Để ánh sáng tương đối đều trên 4 LDR.
2. Quan sát servo trong vài giây.
3. Xem log cảm biến.

### Kết quả mong đợi

- Servo gần như đứng yên.
- Giá trị LDR có thể dao động nhẹ nhưng servo không liên tục thay đổi.

### Đạt khi

- Không có hiện tượng servo giật liên tục khi ánh sáng cân bằng.

## Case 07 - Phát hiện mưa

### Mục tiêu

Kiểm tra cảm biến mưa phát hiện nước đúng theo ngưỡng `RAIN_THRESHOLD = 2000`.

### Điều kiện trước

- Cảm biến mưa đang khô.
- Mode đang là `AUTO`.

### Thao tác

1. Xem log `RainRaw` khi cảm biến khô.
2. Nhỏ nước hoặc chạm khăn ướt vào cảm biến mưa.
3. Quan sát `RainRaw` và `RainDetected`.

### Kết quả mong đợi

Khi khô:

```text
RainDetected=0
```

Khi ướt và giá trị nhỏ hơn `2000`:

```text
RainDetected=1
```

### Đạt khi

- Cảm biến khô ra `rain = 0`.
- Cảm biến ướt ra `rain = 1`.

## Case 08 - Tự mở mái che khi có mưa

### Mục tiêu

Kiểm tra hệ thống tự mở mái che khi phát hiện mưa trong chế độ AUTO.

### Điều kiện trước

- Mode đang là `AUTO`.
- Mái che đang đóng.
- Cảm biến mưa đang khô.

### Thao tác

1. Nhỏ nước lên cảm biến mưa.
2. Quan sát state.
3. Quan sát motor mái che.

### Kết quả mong đợi

- State chuyển sang `RAIN_PROTECTION`.
- Motor bước quay mở mái che.
- Sau khi mở đủ bước, motor tắt.
- `umbrellaOpened = true`.

### Log mong đợi

```text
[STATE] AUTO_TRACKING -> RAIN_PROTECTION
[UMBRELLA] OPEN
```

### Kiểm tra bằng CoAP

```text
Endpoint: coap://<IP_ESP32>/state
Payload:  SUNTRAC123:GET
```

Kỳ vọng:

```json
{
  "rain": 1,
  "umbrella": 1,
  "mode": "AUTO",
  "state": "RAIN_PROTECTION"
}
```

### Đạt khi

- Có nước thì mái che mở.
- `/state` báo `rain = 1`, `umbrella = 1`.

## Case 09 - Tự đóng mái che khi hết mưa

### Mục tiêu

Kiểm tra hệ thống tự đóng mái che khi cảm biến mưa khô lại.

### Điều kiện trước

- Mode đang là `AUTO`.
- Mái che đang mở do mưa.
- State đang là `RAIN_PROTECTION`.

### Thao tác

1. Lau khô cảm biến mưa.
2. Quan sát motor mái che đóng.
3. Quan sát công tắc hành trình.
4. Kiểm tra trạng thái qua `/state`.

### Kết quả mong đợi

- Motor bước quay theo chiều đóng.
- Khi nhấn công tắc hành trình, motor dừng.
- Mái che về trạng thái đóng.
- State quay lại `AUTO_TRACKING`.

### Log mong đợi

```text
[UMBRELLA] CLOSE
[UMBRELLA] Home OK
[STATE] RAIN_PROTECTION -> AUTO_TRACKING
```

### Đạt khi

- Lau khô cảm biến thì mái che đóng.
- Motor dừng sau khi chạm công tắc hành trình.

## Case 10 - Chuyển từ AUTO sang MANUAL

### Mục tiêu

Kiểm tra endpoint `/mode` chuyển hệ thống sang điều khiển thủ công.

### Điều kiện trước

- ESP32 đang chạy bình thường.

### Thao tác

Gửi:

```text
Endpoint: coap://<IP_ESP32>/mode
Payload:  SUNTRAC123:MANUAL
```

### Kết quả mong đợi

Phản hồi:

```text
OK_MANUAL
```

State:

```text
MANUAL_CONTROL
```

### Log mong đợi

```text
[COAP RX] mode -> SUNTRAC123:MANUAL
[COAP TX] mode -> OK_MANUAL
[STATE] AUTO_TRACKING -> MANUAL_CONTROL
```

### Đạt khi

- `/state` trả về `"mode":"MANUAL"`.
- `/state` trả về `"state":"MANUAL_CONTROL"`.

## Case 11 - Chuyển từ MANUAL sang AUTO

### Mục tiêu

Kiểm tra endpoint `/mode` đưa hệ thống trở về chế độ tự động.

### Điều kiện trước

- Mode đang là `MANUAL`.

### Thao tác

Gửi:

```text
Endpoint: coap://<IP_ESP32>/mode
Payload:  SUNTRAC123:AUTO
```

### Kết quả mong đợi

Phản hồi:

```text
OK_AUTO
```

Nếu không có mưa:

```text
AUTO_TRACKING
```

Nếu đang có mưa:

```text
RAIN_PROTECTION
```

### Đạt khi

- `/state` trả về `"mode":"AUTO"`.
- Hệ thống tự bám sáng hoặc bảo vệ mưa tùy điều kiện thực tế.

## Case 12 - Điều khiển servo dọc trong MANUAL

### Mục tiêu

Kiểm tra endpoint `/servo1` điều khiển servo dọc.

### Điều kiện trước

- Mode đang là `MANUAL`.

Nếu chưa, gửi:

```text
Endpoint: coap://<IP_ESP32>/mode
Payload:  SUNTRAC123:MANUAL
```

### Thao tác

Gửi lần lượt:

```text
Endpoint: coap://<IP_ESP32>/servo1
Payload:  SUNTRAC123:30
```

```text
Endpoint: coap://<IP_ESP32>/servo1
Payload:  SUNTRAC123:60
```

### Kết quả mong đợi

- Servo dọc quay đến góc tương ứng.
- ESP32 phản hồi `OK`.
- `/state` cập nhật trường `v`.

### Giới hạn

Servo dọc bị giới hạn:

```text
10 <= v <= 80
```

### Đạt khi

- Gửi `30`, servo dọc đổi góc.
- Gửi `60`, servo dọc đổi góc.
- `/state` hiển thị `"v":30` hoặc `"v":60` gần đúng theo lệnh vừa gửi.

## Case 13 - Điều khiển servo ngang trong MANUAL

### Mục tiêu

Kiểm tra endpoint `/servo2` điều khiển servo ngang.

### Điều kiện trước

- Mode đang là `MANUAL`.

### Thao tác

Gửi lần lượt:

```text
Endpoint: coap://<IP_ESP32>/servo2
Payload:  SUNTRAC123:70
```

```text
Endpoint: coap://<IP_ESP32>/servo2
Payload:  SUNTRAC123:120
```

### Kết quả mong đợi

- Servo ngang quay đến góc tương ứng.
- ESP32 phản hồi `OK`.
- `/state` cập nhật trường `h`.

### Giới hạn

Servo ngang bị giới hạn:

```text
10 <= h <= 170
```

### Đạt khi

- Gửi `70`, servo ngang đổi góc.
- Gửi `120`, servo ngang đổi góc.
- `/state` hiển thị `"h":70` hoặc `"h":120` gần đúng theo lệnh vừa gửi.

## Case 14 - Giới hạn góc servo dọc

### Mục tiêu

Kiểm tra code dùng `constrain()` để giới hạn servo dọc.

### Điều kiện trước

- Mode đang là `MANUAL`.

### Thao tác

Gửi giá trị vượt giới hạn:

```text
Endpoint: coap://<IP_ESP32>/servo1
Payload:  SUNTRAC123:100
```

### Kết quả mong đợi

- ESP32 phản hồi `OK`.
- Servo dọc không vượt `80` độ.
- `/state` trả về `"v":80`.

### Đạt khi

- Lệnh không làm servo quay quá giới hạn cơ khí.

## Case 15 - Giới hạn góc servo ngang

### Mục tiêu

Kiểm tra code dùng `constrain()` để giới hạn servo ngang.

### Điều kiện trước

- Mode đang là `MANUAL`.

### Thao tác

Gửi giá trị vượt giới hạn:

```text
Endpoint: coap://<IP_ESP32>/servo2
Payload:  SUNTRAC123:200
```

### Kết quả mong đợi

- ESP32 phản hồi `OK`.
- Servo ngang không vượt `170` độ.
- `/state` trả về `"h":170`.

### Đạt khi

- Lệnh không làm servo quay quá giới hạn cơ khí.

## Case 16 - Mở mái che trong MANUAL

### Mục tiêu

Kiểm tra endpoint `/umbrella` mở mái che thủ công.

### Điều kiện trước

- Mode đang là `MANUAL`.
- Mái che đang đóng.

### Thao tác

Gửi:

```text
Endpoint: coap://<IP_ESP32>/umbrella
Payload:  SUNTRAC123:OPEN
```

### Kết quả mong đợi

- ESP32 phản hồi `OPEN_OK`.
- Log `[UMBRELLA] OPEN`.
- Motor bước mở mái che.
- Khi mở xong, motor tắt.
- `/state` trả về `"umbrella":1`.

### Đạt khi

- Mái che mở theo lệnh từ xa.

## Case 17 - Đóng mái che trong MANUAL

### Mục tiêu

Kiểm tra endpoint `/umbrella` đóng mái che thủ công.

### Điều kiện trước

- Mode đang là `MANUAL`.
- Mái che đang mở.

### Thao tác

Gửi:

```text
Endpoint: coap://<IP_ESP32>/umbrella
Payload:  SUNTRAC123:CLOSE
```

### Kết quả mong đợi

- ESP32 phản hồi `CLOSE_OK`.
- Log `[UMBRELLA] CLOSE`.
- Motor bước đóng mái che.
- Khi công tắc hành trình được nhấn, log `[UMBRELLA] Home OK`.
- `/state` trả về `"umbrella":0`.

### Đạt khi

- Mái che đóng theo lệnh từ xa.
- Motor dừng tại công tắc hành trình.

## Case 18 - Sai secret key

### Mục tiêu

Kiểm tra ESP32 từ chối lệnh không đúng khóa bí mật.

### Điều kiện trước

- ESP32 đang chạy bình thường.

### Thao tác

Gửi:

```text
Endpoint: coap://<IP_ESP32>/mode
Payload:  WRONGKEY:MANUAL
```

### Kết quả mong đợi

Phản hồi:

```text
ERR_SECRET
```

Mode không thay đổi.

### Đạt khi

- ESP32 không thực hiện lệnh.
- Có phản hồi `ERR_SECRET`.

## Case 19 - Payload thiếu dấu hai chấm

### Mục tiêu

Kiểm tra ESP32 từ chối payload sai định dạng.

### Điều kiện trước

- ESP32 đang chạy bình thường.

### Thao tác

Gửi:

```text
Endpoint: coap://<IP_ESP32>/mode
Payload:  MANUAL
```

### Kết quả mong đợi

```text
ERR_SECRET
```

### Đạt khi

- ESP32 không đổi mode.
- Có phản hồi `ERR_SECRET`.

## Case 20 - Sai lệnh mode

### Mục tiêu

Kiểm tra endpoint `/mode` từ chối giá trị không phải `AUTO` hoặc `MANUAL`.

### Điều kiện trước

- ESP32 đang chạy bình thường.

### Thao tác

Gửi:

```text
Endpoint: coap://<IP_ESP32>/mode
Payload:  SUNTRAC123:ABC
```

### Kết quả mong đợi

```text
ERR_MODE
```

### Đạt khi

- Mode không bị đổi sai.

## Case 21 - Điều khiển servo khi đang AUTO

### Mục tiêu

Kiểm tra ESP32 chặn lệnh servo thủ công khi hệ thống đang ở `AUTO`.

### Điều kiện trước

- Mode đang là `AUTO`.

### Thao tác

Gửi:

```text
Endpoint: coap://<IP_ESP32>/servo1
Payload:  SUNTRAC123:60
```

Gửi tiếp:

```text
Endpoint: coap://<IP_ESP32>/servo2
Payload:  SUNTRAC123:120
```

### Kết quả mong đợi

Cả hai lệnh đều trả về:

```text
AUTO_MODE_ACTIVE
```

### Đạt khi

- Servo không bị điều khiển thủ công trong AUTO.

## Case 22 - Điều khiển mái che khi đang AUTO

### Mục tiêu

Kiểm tra ESP32 chặn lệnh mở/đóng mái che thủ công khi hệ thống đang ở `AUTO`.

### Điều kiện trước

- Mode đang là `AUTO`.

### Thao tác

Gửi:

```text
Endpoint: coap://<IP_ESP32>/umbrella
Payload:  SUNTRAC123:OPEN
```

### Kết quả mong đợi

```text
AUTO_MODE_ACTIVE
```

### Đạt khi

- Mái che không nhận lệnh thủ công khi đang AUTO.

## Case 23 - Sai lệnh mái che

### Mục tiêu

Kiểm tra endpoint `/umbrella` từ chối lệnh không phải `OPEN` hoặc `CLOSE`.

### Điều kiện trước

- Mode đang là `MANUAL`.

### Thao tác

Gửi:

```text
Endpoint: coap://<IP_ESP32>/umbrella
Payload:  SUNTRAC123:STOP
```

### Kết quả mong đợi

```text
ERR_CMD
```

### Đạt khi

- Mái che không chạy.
- ESP32 trả về `ERR_CMD`.

## Case 24 - Mất WiFi sau khi đã kết nối

### Mục tiêu

Kiểm tra state `WIFI_LOST` và cơ chế reconnect.

### Điều kiện trước

- ESP32 đã kết nối WiFi thành công.
- Đã thấy IP trên Serial Monitor.

### Thao tác

1. Tắt hotspot/router trong thời gian ngắn.
2. Quan sát Serial Monitor.
3. Bật lại hotspot/router.
4. Chờ ESP32 reconnect.

### Kết quả mong đợi

Khi mất WiFi:

```text
[STATE] AUTO_TRACKING -> WIFI_LOST
[WIFI] Reconnecting...
```

Khi WiFi có lại:

```text
[WIFI] Reconnected
[WIFI] IP: 192.168.x.x
```

### Đạt khi

- ESP32 tự thử kết nối lại mỗi khoảng 5 giây.
- Khi mạng quay lại, ESP32 lấy được IP.

## Case 25 - Log hệ thống định kỳ

### Mục tiêu

Kiểm tra hệ thống in log trạng thái mỗi khoảng 1 giây.

### Điều kiện trước

- Serial Monitor đang mở.

### Thao tác

Chỉ quan sát Serial Monitor trong vài giây.

### Kết quả mong đợi

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

### Đạt khi

- Log có đủ mode, state, IP, sensor, rain và umbrella.

## Case 26 - CoAP server không start lại nhiều lần

### Mục tiêu

Kiểm tra `startCoapServer()` chỉ start CoAP một lần.

### Điều kiện trước

- ESP32 đã kết nối WiFi.

### Thao tác

1. Quan sát Serial Monitor trong nhiều giây.
2. Gửi nhiều request `/state`.

### Kết quả mong đợi

- Log `[COAP] Started` chỉ xuất hiện một lần sau khi hệ thống vào state có WiFi.
- Các request sau vẫn xử lý bình thường.

### Đạt khi

- Không có log `[COAP] Started` lặp liên tục.

## Case 27 - Mái che không mở lặp khi đã mở

### Mục tiêu

Kiểm tra khi mái che đã mở, lệnh mở tiếp không làm motor chạy thừa.

### Điều kiện trước

- Mode đang là `MANUAL`.
- Mái che đang mở.

### Thao tác

Gửi lại:

```text
Endpoint: coap://<IP_ESP32>/umbrella
Payload:  SUNTRAC123:OPEN
```

### Kết quả mong đợi

- ESP32 vẫn phản hồi `OPEN_OK`.
- Motor không chạy mở thêm nếu `umbrellaOpened` đang `true` và motion state idle.

### Đạt khi

- Mái che không bị cố mở quá hành trình.

## Case 28 - Mái che không đóng lặp khi đã đóng

### Mục tiêu

Kiểm tra khi mái che đã đóng, lệnh đóng tiếp không làm motor chạy thừa.

### Điều kiện trước

- Mode đang là `MANUAL`.
- Mái che đang đóng.

### Thao tác

Gửi lại:

```text
Endpoint: coap://<IP_ESP32>/umbrella
Payload:  SUNTRAC123:CLOSE
```

### Kết quả mong đợi

- ESP32 vẫn phản hồi `CLOSE_OK`.
- Motor không chạy nếu `umbrellaOpened` đang `false` và motion state idle.

### Đạt khi

- Mái che không cố đóng quá hành trình.

## Case 29 - AUTO tracking vẫn chạy trong RAIN_PROTECTION

### Mục tiêu

Kiểm tra trong state `RAIN_PROTECTION`, hệ thống vẫn gọi `autoTrackingTask()` và `rainTask()`.

### Điều kiện trước

- Mode đang là `AUTO`.
- Cảm biến mưa đang ướt.
- State đang là `RAIN_PROTECTION`.

### Thao tác

1. Giữ cảm biến mưa ở trạng thái ướt.
2. Dùng đèn pin chiếu lệch vào LDR.
3. Quan sát servo.

### Kết quả mong đợi

- Mái che mở.
- Servo vẫn có thể tracking ánh sáng theo code hiện tại.

### Lưu ý khi thuyết trình

Trong `runStateAction()`, state `RAIN_PROTECTION` vẫn gọi:

```cpp
autoTrackingTask();
rainTask();
```

### Đạt khi

- Hệ thống vẫn phản ứng với ánh sáng khi đang ở trạng thái bảo vệ mưa.

## Case 30 - Trở về trạng thái ổn định sau demo

### Mục tiêu

Đưa hệ thống về trạng thái an toàn sau khi demo xong.

### Điều kiện trước

- Có thể hệ thống đang ở `MANUAL`, mái che đang mở, hoặc cảm biến mưa còn ướt.

### Thao tác

1. Lau khô cảm biến mưa.
2. Chuyển sang `MANUAL` nếu cần đóng mái che bằng tay.
3. Gửi lệnh đóng mái che:

```text
Endpoint: coap://<IP_ESP32>/umbrella
Payload:  SUNTRAC123:CLOSE
```

4. Chuyển lại AUTO:

```text
Endpoint: coap://<IP_ESP32>/mode
Payload:  SUNTRAC123:AUTO
```

5. Đọc lại `/state`.

### Kết quả mong đợi

```json
{
  "rain": 0,
  "umbrella": 0,
  "mode": "AUTO",
  "state": "AUTO_TRACKING"
}
```

### Đạt khi

- Cảm biến mưa khô.
- Mái che đóng.
- Mode là `AUTO`.
- Hệ thống sẵn sàng cho lần demo tiếp theo.

## Bảng demo nhanh

| Case | Tên case | Có nên demo trước lớp |
|---:|---|---|
| 01 | Khởi động hệ thống | Có |
| 02 | WiFi và CoAP | Có |
| 03 | Đọc `/state` | Có |
| 04 | Tracking ngang | Có |
| 05 | Tracking dọc | Có |
| 06 | Ngưỡng chống rung | Nếu còn thời gian |
| 07 | Phát hiện mưa | Có |
| 08 | Tự mở mái che | Có |
| 09 | Tự đóng mái che | Có |
| 10 | Chuyển MANUAL | Có |
| 11 | Chuyển AUTO | Có |
| 12 | Servo dọc thủ công | Có |
| 13 | Servo ngang thủ công | Có |
| 14 | Giới hạn servo dọc | Nếu còn thời gian |
| 15 | Giới hạn servo ngang | Nếu còn thời gian |
| 16 | Mở mái che thủ công | Có |
| 17 | Đóng mái che thủ công | Có |
| 18 | Sai secret key | Có |
| 19 | Payload sai định dạng | Nếu còn thời gian |
| 20 | Sai mode | Nếu còn thời gian |
| 21 | Chặn servo khi AUTO | Có |
| 22 | Chặn mái che khi AUTO | Có |
| 23 | Sai lệnh mái che | Nếu còn thời gian |
| 24 | Mất WiFi/reconnect | Nếu an toàn để demo |
| 25 | Log định kỳ | Có |
| 26 | CoAP không start lặp | Không cần demo trực tiếp |
| 27 | Không mở lặp mái che | Nếu còn thời gian |
| 28 | Không đóng lặp mái che | Nếu còn thời gian |
| 29 | Tracking trong RAIN_PROTECTION | Nếu bị hỏi |
| 30 | Reset trạng thái sau demo | Nên làm sau demo |

## Kịch bản case tối thiểu trong 5 phút

Nếu chỉ có ít thời gian, demo các case sau:

1. Case 01 - Khởi động hệ thống.
2. Case 03 - Đọc `/state`.
3. Case 04 hoặc 05 - AUTO tracking.
4. Case 08 - Tự mở mái che khi có mưa.
5. Case 09 - Tự đóng mái che khi hết mưa.
6. Case 10 - Chuyển MANUAL.
7. Case 12 hoặc 13 - Điều khiển servo thủ công.
8. Case 16 và 17 - Mở/đóng mái che thủ công.
9. Case 18 - Sai secret key.
10. Case 30 - Đưa hệ thống về trạng thái ổn định.

## Kịch bản case đầy đủ trong 10 phút

Nếu có đủ thời gian, demo theo thứ tự:

1. Case 01 - Khởi động hệ thống.
2. Case 02 - WiFi và CoAP.
3. Case 25 - Log định kỳ.
4. Case 03 - Đọc `/state`.
5. Case 04 - Tracking ngang.
6. Case 05 - Tracking dọc.
7. Case 06 - Ngưỡng chống rung.
8. Case 07 - Phát hiện mưa.
9. Case 08 - Tự mở mái che.
10. Case 09 - Tự đóng mái che.
11. Case 10 - Chuyển MANUAL.
12. Case 12 - Servo dọc thủ công.
13. Case 13 - Servo ngang thủ công.
14. Case 16 - Mở mái che thủ công.
15. Case 17 - Đóng mái che thủ công.
16. Case 18 - Sai secret key.
17. Case 21 - Chặn servo khi AUTO.
18. Case 22 - Chặn mái che khi AUTO.
19. Case 11 - Chuyển lại AUTO.
20. Case 30 - Trở về trạng thái ổn định.

