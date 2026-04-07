# Sơ đồ máy trạng thái (State Machine Diagrams)

Dưới đây là các sơ đồ máy trạng thái cho hệ thống của bạn. Bằng cách tách thành các cụm riêng biệt, bạn sẽ dễ dàng giải thích luật logic cho người khác hơn rất nhiều.

## 1. SystemState - Vòng đời kết nối hệ thống
Đây là sơ đồ lớn nhất, quản lý sinh mệnh kết nối của bo mạch. Nó chạy theo tính chất "tuần tự" (cái này có rồi mới làm tới cái kia) và "tự phục hồi" (rớt lúc nào thì quay lại chờ lúc đó).

```mermaid
stateDiagram-v2
    [*] --> SYS_INIT : Bật nguồn
    
    SYS_INIT --> SYS_WIFI : Khởi tạo thành công
    
    SYS_WIFI --> SYS_WS : Kết nối WiFi (WL_CONNECTED)
    SYS_WIFI --> SYS_WIFI : Đợi/Thử lại WiFi
    
    SYS_WS --> SYS_RUNNING : Mở WebSocket thành công
    SYS_WS --> SYS_WIFI_LOST : Mất kết nối WiFi (<> WL_CONNECTED)
    SYS_WS --> SYS_WS : Đợi/Thử lại WebSocket
    
    SYS_RUNNING --> SYS_WIFI_LOST : Mất WiFi
    SYS_RUNNING --> SYS_WS_LOST : Mất WebSocket (WIFI vẫn còn)
    
    SYS_WIFI_LOST --> SYS_WS : Có WiFi trở lại
    
    SYS_WS_LOST --> SYS_WIFI_LOST : Mất WiFi luôn
    SYS_WS_LOST --> SYS_RUNNING : WebSocket kết nối lại thành công
```

---

## 2. AppState - Trạng thái môi trường (Theo cảm biến)
Trạng thái này được tính toán hoàn toàn độc lập dựa trên điều kiện môi trường. Tuy nhiên, nó có một **cơ chế trễ (delay confirmation)** với khoảng thời gian `APP_STATE_CONFIRM_MS = 5000ms` để chống nhiễu cảm biến tạm thời. Nghĩa là giá trị mới phải duy trì đủ 5 giây mới được công nhận.

```mermaid
stateDiagram-v2
    classDef safe fill:#a8e6cf,color:black,stroke:#333,stroke-width:2px;
    classDef warn fill:#ffd3b6,color:black,stroke:#333,stroke-width:2px;
    classDef danger fill:#ffaaa5,color:black,stroke:#333,stroke-width:2px;
    classDef err fill:#d4a5a5,color:black,stroke:#333,stroke-width:2px;

    [*] --> Pending : Dữ liệu cảm biến mới

    state Pending {
        [*] --> Timer5s
        Timer5s --> UpdateAppState : > 5 giây
        Timer5s --> ResetTimer : Dữ liệu biến động/chưa đủ 5s
    }

    UpdateAppState --> NORMAL
    UpdateAppState --> WARNING
    UpdateAppState --> ALARM
    UpdateAppState --> ERROR_STATE

    NORMAL:::safe : NORMAL
    NORMAL : T <= 32 && H <= 85
    
    WARNING:::warn : WARNING
    WARNING : T(32.1->32) || H(85.1->95)
    
    ALARM:::danger : ALARM
    ALARM : T > 32 || H > 95
    
    ERROR_STATE:::err : ERROR_STATE
    ERROR_STATE : Lỗi đọc DHT22 >= 5 lần
```

---

## 3. ControlMode - Chế độ điều khiển

Đây là cái "Van" chuyển mạch cho phép Nút Bấm Vật Lý/Web tác động vào Fan và Buzzer.

```mermaid
stateDiagram-v2
    [*] --> MODE_AUTO : Mặc định
    
    MODE_AUTO --> MODE_MANUAL : Lệnh Web(mode:manual)
    MODE_AUTO --> MODE_MANUAL : Nhấn Nút Mode
    
    MODE_MANUAL --> MODE_AUTO : Lệnh Web(mode:auto)
    MODE_MANUAL --> MODE_AUTO : Nhấn Nút Mode
```

---

## 4. Quạt & Còi (Sự phân tách giữa Auto và Manual)

Biểu đồ này mô tả cách Fan và Buzzer quyết định sẽ nghe lời ai (nghe lời AppState hay nghe lời người dùng) thông qua cái "Van" ControlMode.

```mermaid
flowchart TD
    subgraph TrạngTháiMôiTrường [Trạng thái Môi trường]
        AS[AppState]
        AS -- Normal --> F_Off[autoFanState = OFF] & B_Idl[autoBuzzerState = IDLE]
        AS -- Warning --> F_On[autoFanState = ON] & B_Idl
        AS -- Alarm --> F_On & B_Bln[autoBuzzerState = BLINK]
        AS -- Error --> F_On & B_StO[autoBuzzerState = STEADY]
    end

    subgraph ChếĐộĐiềuKhiển [Chế độ điều khiển]
        Mode{ControlMode}
    end
    
    Mode -- AUTO --> M_Fan[fanState = autoFanState] & M_Buzz[buzzerState = autoBuzzerState]
    Mode -- MANUAL --> M_FanMan[fanState = manualFanState] & M_BuzzMan{Kiểm tra Cờ Mute?}
    
    M_BuzzMan -- Bị Mute --> M_BuzzOff[buzzerState = IDLE]
    M_BuzzMan -- Không Mute --> M_BuzzAuto[buzzerState = autoBuzzerState]
    
    subgraph NgườiDùng [Lệnh Người Dùng Nút/Web]
        UsrFan[Lệnh Bật/Tắt Quạt]
        UsrBuzz[Lệnh MUTE Còi]
    end
    
    UsrFan --> M_FanMan
    UsrBuzz --> M_BuzzMan
    
    M_Fan --> HW_Fan((Cập Nhật Chân RELAY Quạt))
    M_FanMan --> HW_Fan
    M_Buzz --> HW_Buzz((Cập Nhật Chân BUZZER Còi))
    M_BuzzOff --> HW_Buzz
    M_BuzzAuto --> HW_Buzz
```
