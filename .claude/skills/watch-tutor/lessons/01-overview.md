# 第 1 課：全局觀，分層與「誰擁有誰」

[← 課程目錄](../syllabus.md) ｜ [下一課 →](02-step-loop.md)

## 🎯 本課目標
1. AP 層是什麼？為什麼這個專案能在 Windows 上模擬一支手錶？
2. `ap/` 為什麼能「完全不知道」底下的硬體？
3. 程式裡的物件是誰建立的、由誰擁有？

**必讀檔案**：[README.md](../../../../README.md)、[services.h](../../../../ap/include/ap/services/services.h)、[watch_app.h](../../../../ap/include/ap/app/watch_app.h)、[main.cpp](../../../../app/main.cpp)、[architecture.md §1](../../../../docs/architecture.md)

---

## 1-1 一支手錶的軟體分三層

```
┌──────────────────────────────────┐
│ AP 層（Application）              │ ← 本專案主角，也是你要面試的職缺
│ 按鍵代表什麼、何時開始跑步、       │
│ 心率超標要不要震動、現在顯示哪頁   │
├──────────────────────────────────┤
│ Service / Driver 層               │ GPS、光學心率、Flash、背光、震動馬達
├──────────────────────────────────┤
│ RTOS / 硬體                       │
└──────────────────────────────────┘
```

這個專案把下面兩層換成**模擬**：用 Windows 執行緒產生假的 GPS 與心率，用 OpenCV 視窗當螢幕。真正要展示的，是最上面那層怎麼設計。

## 1-2 資料夾就是分層

| 資料夾 | 角色 | 可以用什麼 |
|---|---|---|
| [ap/](../../../../ap/) | 可移植的 AP 層 | 不能用 heap、exception、RTTI，也不能依賴 OS |
| [platform/sim/](../../../../platform/sim/) | 假的 driver：OpenCV 畫布、感測器執行緒、CSV 紀錄器 | 一般桌面 C++ |
| [app/main.cpp](../../../../app/main.cpp) | **composition root**：把兩邊接起來 | 一般桌面 C++ |
| [tests/](../../../../tests/) | 94 個單元測試，[fakes.h](../../../../tests/fakes.h) 為每個 service 提供假實作 | GoogleTest |

## 1-3 關鍵規則：相依箭頭只能指向 `ap/`（Dependency Inversion）

`ap/` 需要 GPS，但它**不 include 任何 GPS driver**。它自己定義一個介面，說明「我需要什麼」，見 [services.h:21](../../../../ap/include/ap/services/services.h#L21)：

```cpp
class IGpsService {
public:
    AP_INTERFACE(IGpsService)
    virtual bool start() = 0;
    virtual void stop() = 0;
};
```

同一個檔案裡還有 `IHeartRateService`、`IActivityRecorder`、`IVibrator`、`IBacklight`、`IWallClock`、`IActivityHistory`，全部都是 AP 層**需要的能力清單**。

```
platform/sim/ ──實作──▶ ap/services.h ◀──使用── ap/ 其他模組
```

[services.h:11](../../../../ap/include/ap/services/services.h#L11) 的 `AP_INTERFACE` 巨集做了兩件事：
- **禁止複製**（`= delete`）：介面代表一個服務，不應該被複製。
- **建構子和解構子設成 `protected`、非 virtual**：沒人能透過介面指標 `delete` 它，所以不需要 virtual destructor。負責銷毀的，永遠是知道具體型別的擁有者。

## 1-4 誰擁有誰：一切都是 `WatchApp` 的成員

[watch_app.h:82-120](../../../../ap/include/ap/app/watch_app.h#L82-L120)：

```cpp
EventBus          m_bus;       // 事件中心         （第 4 課）
RingBuffer<...>   m_inbox;     // 跨執行緒收件匣   （第 2 課）
GestureRecognizer m_gestures;  // 按鍵 → 手勢     （第 3 課）
InputRouter       m_router;    // 責任鏈           （第 3 課）
ActivitySession   m_session;   // 活動狀態機       （第 5 課）
Navigator         m_nav;       // 畫面堆疊         （第 6 課）
WatchFaceScreen   m_watchFace; // 各個畫面 ...
```

- **沒有任何 `new`。** 物件全是成員變數，`WatchApp` 一建好，所有記憶體就到位了。
- **擁有 vs 借用：** `Navigator` 和 `InputRouter` 只存指標，**借用**畫面和 handler，不擁有它們。所以 pop 一個畫面時，什麼都不會被銷毀。

外部 service 透過建構子注入，而且全是**參考**，見 [watch_app.h:27](../../../../ap/include/ap/app/watch_app.h#L27)：

```cpp
struct Services {
    IGpsService& gps;
    IHeartRateService& hr;
    IActivityRecorder& recorder;
    ...
};
```

接線只在一個地方，也就是 [main.cpp:100-106](../../../../app/main.cpp#L100-L106) 的 `build()`：建立模擬的 sensors 和 recorder → 塞進 `WatchApp::Services` → `sensors.connect(*app)`。

## 1-5 WatchApp 對外只有 6 個入口

[watch_app.h:45-53](../../../../ap/include/ap/app/watch_app.h#L45-L53)：

| 入口 | 誰呼叫 | 做什麼 |
|---|---|---|
| `init(now)` | 主程式一次 | 訂閱事件、組好責任鏈、推出錶面 |
| `postKey` / `postHr` / `postGps` | **其他執行緒** | 往收件匣投信 |
| `step(now)` | AP 執行緒反覆呼叫 | 心跳：處理所有事情 |
| `render(canvas)` | AP 執行緒 | 畫出畫面 |

把它想成一個黑盒子：**外面只能「投信」和「撥時鐘」。** 盒子裡面怎麼運作，就是第 2 到第 6 課的內容。

---

## ⌚ 對應真實手錶
- **要重寫：** `platform/`（真的 GPS 和 HR driver、LCD canvas、RTOS mutex）和 `main.cpp`（改成 RTOS task 的組裝）。
- **不用動：** `ap/` 完全不動，`tests/` 照樣在電腦上跑。

## 💬 面試金句
> 「介面歸使用者所有，不歸實作者所有。所以移植時只需要改 platform 層，AP 層可以直接注入 fake 做單元測試。」

---

## 📝 小測驗
1. 為什麼 `IGpsService` 放在 `ap/` 裡，而不是放在 `platform/sim/` 裡？
2. 換到真的手錶上時，哪些部分要重寫？哪些不用動？
3. `WatchApp` 裡的物件為什麼全部是成員變數，而不是用 `new` 建出來？

<details>
<summary>參考答案（先自己作答再展開）</summary>

1. AP 層定義自己需要的能力，platform 依賴 AP，而不是反過來（DIP）。這樣 `ap/` 不 include 任何平台標頭，可以移植，也能用 fake 測試。
2. 重寫 `platform/`（driver 實作、canvas、mutex）和 `app/main.cpp`（組裝方式、主迴圈或 RTOS task）。`ap/` 不動，`tests/` 照跑。
3. 記憶體在建構時一次確定，init 之後零 heap，避免碎片化和配置失敗。生命週期也清楚（跟著 WatchApp）；借用者不擁有物件，就不會重複釋放。
</details>

## 🎤 面試追問
<details>
<summary>介面的解構子為什麼是 protected 而且不是 virtual？</summary>

不允許透過介面指標 delete，所以省下 virtual destructor。物件由知道具體型別的擁有者（WatchApp 或 main.cpp）負責銷毀。
</details>

<details>
<summary>什麼是 composition root？為什麼只能有一個？</summary>

整個程式唯一負責建立具體實作並接線的地方（`main.cpp` 的 `build()`）。接線集中在一處，其他程式碼就只依賴介面；換平台只換這一處。
</details>

<details>
<summary>沒有硬體，你怎麼測 AP 層？</summary>

[tests/fakes.h](../../../../tests/fakes.h) 為每個 service 提供 fake，直接注入 `WatchApp::Services`。時間也由測試傳入，所以每個情境都能決定性地重現。
</details>
