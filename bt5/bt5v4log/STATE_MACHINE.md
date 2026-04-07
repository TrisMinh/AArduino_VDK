# State Machine `bt5v4log`

Tài liệu này mô tả state machine của hệ thống trong file `bt5v4log.ino`.

Mục tiêu của tài liệu:
- Giải thích hệ thống đang có những state nào
- Khi nào state đổi
- Mỗi state làm gì với quạt, còi, LCD và WebSocket
- Trình tự xử lý trong `loop()` để dễ debug

File code tham chiếu chính: [bt5v4log.ino](c:\Users\minht\OneDrive\Tài liệu\Arduino\bt5\bt5v4log\bt5v4log.ino)

## 1. Tổng quan

Hệ thống này thực chất có 2 state machine chạy song song:

1. `SystemState`
- Quản lý vòng đời kết nối của thiết bị
- Bao gồm WiFi, WebSocket, và trạng thái lỗi kết nối

2. `AppState`
- Quản lý trạng thái nghiệp vụ dựa trên dữ liệu cảm biến DHT
- Bao gồm `NORMAL`, `WARNING`, `ALARM`, `ERROR_STATE`

Hai state machine này độc lập nhưng liên quan:
- `SystemState` quyết định thiết bị đang online tới server hay không
- `AppState` quyết định quạt, còi, LCD và nội dung dữ liệu gửi đi

## 2. Biến Và Ngưỡng Quan Trọng

Các ngưỡng nghiệp vụ:

- `TEMP_LIMIT = 28`
- `HUMI_LIMIT = 85`
- `TEMP_ALARM = 32`
- `HUMI_ALARM = 95`

Quy ước phần cứng:

- `RELAY_PIN = 18`
- `BUZZER_PIN = 19`
- `BUZZER_ON = LOW`
- `BUZZER_OFF = HIGH`

Điều này cho thấy còi đang được điều khiển theo kiểu active-low.

Biến state:

- `SystemState sysState`
- `AppState appState`
- `AppState lastState`
- `bool wsConnected`
- `bool errorSensor`
- `int errorSensorCount`

## 3. SystemState

`SystemState` nằm ở enum:

```cpp
enum SystemState { SYS_INIT, SYS_WIFI, SYS_WS, SYS_RUNNING, SYS_ERROR };
```

Ý nghĩa từng state:

- `SYS_INIT`
  - Trạng thái khởi tạo ban đầu sau khi boot
  - Chỉ tồn tại rất ngắn

- `SYS_WIFI`
  - Đang chờ có kết nối WiFi
  - Mỗi 5 giây sẽ gọi `connectWiFi()` nếu chưa kết nối

- `SYS_WS`
  - Đã có WiFi, đang chờ WebSocket kết nối tới server
  - Mỗi 5 giây sẽ thử `webSocket.begin(...)` nếu chưa kết nối

- `SYS_RUNNING`
  - WiFi và WebSocket đã sẵn sàng
  - Hệ thống hoạt động bình thường

- `SYS_ERROR`
  - Đã từng vào `SYS_RUNNING` nhưng sau đó mất WiFi
  - Mỗi 5 giây thử kết nối lại WiFi

### 3.1 Sơ Đồ Chuyển Trạng Thái `SystemState`

```text
SYS_INIT
   |
   v
SYS_WIFI --(WiFi connected)--> SYS_WS --(WebSocket connected)--> SYS_RUNNING
   ^                                                           |
   |                                                           |
   +-------------------- retry WiFi every 5s ------------------+

SYS_RUNNING --(WiFi lost)--> SYS_ERROR --(retry WiFi every 5s)-->
```

Lưu ý quan trọng:

- Trong code hiện tại, `SYS_ERROR` không tự quay về `SYS_WS` hay `SYS_RUNNING` trực tiếp
- Nó chỉ retry WiFi
- Sau khi WiFi có lại, `updateSystemState()` hiện chưa có nhánh rõ ràng để kéo `SYS_ERROR` quay về `SYS_WS`

Điểm này có nghĩa là `SYS_ERROR` hiện là trạng thái lỗi kết nối một chiều. Nếu muốn phục hồi hoàn toàn, cần bổ sung logic chuyển state từ `SYS_ERROR` về `SYS_WS` hoặc `SYS_WIFI`.

### 3.2 Điều Kiện Chuyển `SystemState`

Trong `updateSystemState()`:

- `SYS_INIT -> SYS_WIFI`
  - xảy ra ngay trong lần chạy đầu tiên

- `SYS_WIFI -> SYS_WS`
  - khi `WiFi.status() == WL_CONNECTED`

- `SYS_WS -> SYS_RUNNING`
  - khi `wsConnected == true`

- `SYS_RUNNING -> SYS_ERROR`
  - khi `WiFi.status() != WL_CONNECTED`

- `SYS_ERROR`
  - hiện tại không có nhánh chuyển tiếp rõ ràng sang state khác trong `updateSystemState()`

### 3.3 Hành Vi Theo `SystemState`

- `SYS_WIFI`
  - cứ mỗi 5 giây gọi `connectWiFi()`

- `SYS_WS`
  - cứ mỗi 5 giây mở WebSocket tới `host:port`

- `SYS_RUNNING`
  - không có tác vụ riêng ở `handleSystemState()`
  - loop chính tiếp tục xử lý sensor, LCD, realtime, queue

- `SYS_ERROR`
  - cứ mỗi 5 giây thử kết nối WiFi lại

## 4. AppState

`AppState` nằm ở enum:

```cpp
enum AppState { NORMAL, WARNING, ALARM, ERROR_STATE };
```

Đây là state machine nghiệp vụ chính của bài.

Ý nghĩa từng state:

- `NORMAL`
  - Nhiệt độ và độ ẩm nằm trong giới hạn an toàn
  - Sensor đang đọc bình thường

- `WARNING`
  - Nhiệt độ hoặc độ ẩm vượt ngưỡng cảnh báo
  - Chưa tới mức báo động

- `ALARM`
  - Nhiệt độ hoặc độ ẩm vượt ngưỡng báo động
  - Quạt bật, còi kêu luân phiên 0.5 giây

- `ERROR_STATE`
  - DHT lỗi liên tiếp đủ nhiều lần
  - Xem như lỗi cảm biến
  - Quạt bật, còi kêu liên tục

### 4.1 Điều Kiện Chuyển `AppState`

`AppState` được cập nhật trong `updateAppState()` theo thứ tự ưu tiên sau:

1. Nếu `errorSensor == true` thì `appState = ERROR_STATE`
2. Nếu không lỗi sensor và `temp > TEMP_ALARM` hoặc `humi > HUMI_ALARM` thì `appState = ALARM`
3. Nếu chưa tới alarm nhưng `temp > TEMP_LIMIT` hoặc `humi > HUMI_LIMIT` thì `appState = WARNING`
4. Còn lại là `NORMAL`

### 4.2 Sơ Đồ Chuyển Trạng Thái `AppState`

```text
                       sensor read failed >= 5 lần
                +--------------------------------------+
                |                                      v
NORMAL <--> WARNING <--> ALARM                    ERROR_STATE
  ^                                          |
  |                                          |
  +---------- sensor đọc lại thành công -----+
```

Giải thích:

- `NORMAL`, `WARNING`, `ALARM` phụ thuộc vào giá trị `temp` và `humi`
- `ERROR_STATE` phụ thuộc vào lỗi đọc sensor
- Khi sensor đọc lại thành công:
  - `errorSensor = false`
  - `errorSensorCount = 0`
  - `appState` được tính lại từ nhiệt độ và độ ẩm

### 4.3 Cách Sensor Sinh Ra `ERROR_STATE`

Trong `readSensor()`:

1. Đọc DHT lần 1
2. Nếu lỗi `NaN`, chờ `50 ms` rồi đọc lại lần 2
3. Nếu vẫn lỗi:
   - `errorSensorCount++`
   - nếu `errorSensorCount >= 5` thì `errorSensor = true`
   - log lỗi sensor
   - thoát hàm mà không cập nhật `temp/humi`
4. Nếu đọc thành công:
   - cập nhật `temp`, `humi`
   - `errorSensor = false`
   - `errorSensorCount = 0`

Điều này có nghĩa là:

- Hệ thống chỉ vào `ERROR_STATE` khi đọc lỗi liên tiếp ít nhất 5 chu kỳ sensor
- Chu kỳ đọc sensor hiện tại là 2 giây một lần
- Do đó cần khoảng 10 giây lỗi liên tục để chắc chắn vào `ERROR_STATE`

## 5. Tác Động Phần Cứng Theo `AppState`

Hành vi phần cứng nằm ở `handleAppState()`.

### 5.1 `NORMAL`

- Quạt: tắt
- Còi: tắt
- `buzzerOn = false`

```text
RELAY = LOW
BUZZER = OFF
```

### 5.2 `WARNING`

- Quạt: bật
- Còi: tắt
- `buzzerOn = false`

```text
RELAY = HIGH
BUZZER = OFF
```

### 5.3 `ALARM`

- Quạt: bật
- Còi: kêu luân phiên mỗi 500 ms

Cơ chế:

- Nếu `millis() - buzzer_time > 500`
  - cập nhật `buzzer_time`
  - đảo `buzzerOn`
  - ghi mức tương ứng ra `BUZZER_PIN`

Ngoài ra, khi vừa đổi state sang `ALARM`, trong `loop()` có đoạn:

```cpp
if(appState == ALARM) {
  buzzerOn = true;
  buzzer_time = millis();
  digitalWrite(BUZZER_PIN, BUZZER_ON);
}
```

Mục đích:

- Cho còi kêu ngay khi vừa vào `ALARM`
- Sau đó `handleAppState()` tiếp tục chớp còi theo chu kỳ 500 ms

### 5.4 `ERROR_STATE`

- Quạt: bật
- Còi: bật liên tục

```text
RELAY = HIGH
BUZZER = ON
```

Đây là trạng thái lỗi sensor, không phải lỗi kết nối mạng.

## 6. LCD Hiển Thị Gì Theo State

LCD được cập nhật mỗi 1 giây trong `displayLCD()`.

Dòng 1:

- Luôn hiển thị nhiệt độ dạng `T:xx.xC`

Dòng 2:

- `NORMAL`: hiển thị `H:xx.x% NOR`
- `WARNING`: hiển thị `H:xx.x% WAR`
- `ALARM`: hiển thị `H:xx.x% ALM`
- `ERROR_STATE`: hiển thị `SENSOR ERROR`

Lưu ý:

- Khi sensor lỗi, `temp/humi` không được cập nhật mới
- Nhưng LCD ở `ERROR_STATE` sẽ bỏ qua dòng humidity và hiện `SENSOR ERROR`

## 7. Dữ Liệu Gửi Server

Hệ thống gửi 2 loại dữ liệu chính.

### 7.1 Event Theo Chuyển Trạng Thái

Khi `appState != lastState`, hệ thống push event vào queue:

- `ALARM` gửi `ALARM_ON`
- `NORMAL` gửi `NORMAL`
- `WARNING` gửi `WARNING`
- `ERROR_STATE` gửi `ERROR`

Ý nghĩa:

- Đây là message theo kiểu "trạng thái vừa đổi"
- Dùng để biết sự kiện xảy ra vào thời điểm nào

Quy trình:

1. `pushEvent(...)` đưa event vào queue
2. `processQueue()` gửi message đầu hàng đợi nếu không chờ ACK
3. `webSocketEvent()` nhận ACK thì xóa queue
4. Nếu quá 3 giây chưa ACK, `checkTimeout()` resend

### 7.2 Realtime Định Kỳ

Cứ mỗi 3 giây, nếu `wsConnected == true`, hệ thống gửi gói realtime:

```json
{
  "temp": 29.1,
  "humi": 82.0,
  "fan": 1,
  "alarm": 0,
  "error": 1,
  "app_state": "ERROR",
  "system_state": "RUNNING"
}
```

Ý nghĩa từng trường:

- `temp`: nhiệt độ gần nhất đọc được
- `humi`: độ ẩm gần nhất đọc được
- `fan`: trạng thái chân relay hiện tại
- `alarm`: có đang ở `ALARM` không
- `error`: có đang ở `ERROR_STATE` không
- `app_state`: state nghiệp vụ hiện tại
- `system_state`: state kết nối hiện tại

## 8. Trình Tự Xử Lý Trong `loop()`

Đây là phần quan trọng nhất để hiểu hệ thống chạy ra sao.

Trình tự hiện tại:

1. `webSocket.loop()`
   - xử lý sự kiện WebSocket

2. `updateSystemState()`
   - tính lại `sysState`

3. `handleSystemState()`
   - nếu cần thì reconnect WiFi hoặc mở WebSocket

4. Mỗi 2 giây:
   - `readSensor()`
   - `updateAppState()`

5. `handleAppState()`
   - áp hành vi phần cứng theo `appState`

6. Nếu `appState != lastState`
   - log chuyển state
   - nếu là `ALARM` thì bật còi ngay lập tức
   - push event tương ứng
   - cập nhật `lastState`

7. Mỗi 1 giây:
   - `displayLCD()`

8. `sendRealtime()`
   - mỗi 3 giây gửi dữ liệu realtime nếu có WS

9. `processQueue()`
   - gửi event trong queue nếu chưa chờ ACK

10. `checkTimeout()`
   - quá 3 giây chưa ACK thì resend

11. `heartbeat()`
   - mỗi 10 giây gửi ping

### 8.1 Hệ Quả Của Thứ Tự Này

- `handleAppState()` chạy ở mọi vòng lặp, nên quạt/còi phản ứng liên tục theo `appState`
- `appState` chỉ được tính lại mỗi 2 giây, nên độ trễ nhận biết thay đổi sensor tối đa khoảng 2 giây
- Event chỉ được gửi khi state đổi, không gửi lặp lại liên tục
- Realtime vẫn gửi định kỳ, kể cả khi state không đổi

## 9. Một Số Tình Huống Thực Tế

### 9.1 Nhiệt độ tăng từ bình thường lên 29 độ

- `temp > TEMP_LIMIT`
- `appState = WARNING`
- Quạt bật
- Còi không kêu
- LCD hiện `WAR`
- Event `WARNING` được queue để gửi server

### 9.2 Nhiệt độ tăng lên 33 độ

- `temp > TEMP_ALARM`
- `appState = ALARM`
- Quạt bật
- Còi chớp 0.5 giây
- LCD hiện `ALM`
- Event `ALARM_ON` được queue
- Realtime gửi `app_state = "ALARM"`

### 9.3 Cảm biến lỗi liên tục 5 lần

- `errorSensorCount >= 5`
- `errorSensor = true`
- `appState = ERROR_STATE`
- Quạt bật
- Còi kêu liên tục
- LCD hiện `SENSOR ERROR`
- Event `ERROR` được queue
- Realtime gửi `error = 1`, `app_state = "ERROR"`

### 9.4 WiFi đang chạy rồi bị mất

- `SYS_RUNNING -> SYS_ERROR`
- Hệ thống log đổi `system_state`
- Mỗi 5 giây thử kết nối WiFi lại
- Realtime/event không gửi được nếu WebSocket không còn hoạt động

## 10. Quan Hệ Giữa `SystemState` Và `AppState`

Đây là điểm dễ nhầm nhất.

`SystemState` không quyết định quạt/còi.

`AppState` mới quyết định quạt/còi.

Ví dụ:

- Mất WiFi:
  - `SystemState = SYS_ERROR`
  - nhưng `AppState` vẫn có thể là `NORMAL`, `WARNING`, `ALARM` hoặc `ERROR_STATE`
  - quạt/còi vẫn chạy theo `AppState`, không phụ thuộc mạng

- Sensor lỗi:
  - `AppState = ERROR_STATE`
  - nhưng `SystemState` vẫn có thể là `RUNNING`
  - nghĩa là hệ thống vẫn online nhưng cảm biến đang lỗi

Đây là lý do gói realtime nên gửi cả:

- `app_state`
- `system_state`

để server phân biệt:

- lỗi môi trường
- lỗi cảm biến
- lỗi kết nối

## 11. Các Điểm Cần Lưu Ý Khi Debug

### 11.1 Không nghe thấy còi

Kiểm tra theo thứ tự:

1. Có thật sự vào `ALARM` hoặc `ERROR_STATE` không
2. Có đang chỉ ở `WARNING` không
3. `BUZZER_ON` có đúng với phần cứng active-low không
4. GPIO 19 có đúng chân đang nối buzzer không

### 11.2 Quạt không chạy

Kiểm tra:

1. Có đang ở `WARNING`, `ALARM`, hoặc `ERROR_STATE` không
2. Relay module là active-high hay active-low
3. `digitalRead(RELAY_PIN)` có phản ánh đúng phần cứng không

### 11.3 Server không nhận được event

Kiểm tra:

1. Có state change thật không
2. `queueSize` có tăng không
3. Có `WS TX` không
4. Server có trả `ACK` không
5. Có log `RESEND` không

## 12. Điểm Yếu Hoặc Hạn Chế Của State Machine Hiện Tại

Một số điểm nên biết:

- `SYS_ERROR` hiện chưa có đường phục hồi hoàn chỉnh về `SYS_WS` hoặc `SYS_RUNNING`
- `AppState` dùng `>` thay vì `>=`, nên đúng bằng ngưỡng vẫn chưa đổi state
- Khi sensor lỗi, `temp/humi` trong realtime vẫn là giá trị đọc thành công gần nhất
- `fan` trong realtime đang lấy từ mức chân relay, không chắc phản ánh đúng trạng thái thực nếu relay active-low

## 13. Tóm Tắt Ngắn

Nếu nhìn ngắn gọn, state machine hoạt động như sau:

- `SystemState` lo chuyện mạng
- `AppState` lo chuyện cảnh báo và phần cứng
- Cứ 2 giây đọc sensor một lần
- Từ kết quả sensor, hệ thống quyết định `NORMAL/WARNING/ALARM/ERROR_STATE`
- Từ `AppState`, hệ thống điều khiển quạt và còi
- Mỗi khi đổi `AppState`, hệ thống gửi event
- Mỗi 3 giây, hệ thống gửi realtime có cả trạng thái ứng dụng và trạng thái hệ thống

## 14. Gợi Ý Mở Rộng Tài Liệu

Nếu cần, tài liệu này có thể mở rộng thêm các phần sau:

- sơ đồ sequence giữa `loop()`, sensor, queue, WebSocket
- bảng truth table cho quạt/còi/LCD theo từng state
- tài liệu API cho payload gửi server
- phiên bản Mermaid để render biểu đồ tự động
