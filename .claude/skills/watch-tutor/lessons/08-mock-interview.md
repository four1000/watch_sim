# 第 8 課：面試模擬

[← 上一課](07-embedded.md) ｜ [課程目錄](../syllabus.md)

## 🎯 本課目標
能流暢介紹專案，並應付追問與情境題。

---

## 8-1 上課流程（導師依此進行）

1. **1 分鐘專案介紹**：你先講，導師批改後再對照下方參考稿。
2. **3 分鐘架構介紹**：要涵蓋分層和 DIP、資料流、2 到 3 個 pattern 的取捨、嵌入式限制。
3. **逐題問答**：優先從 [progress.md](../progress.md) 的「待加強」出題，再從第 1 到 7 課的「面試追問」和 [design-decisions.md](../../../../docs/design-decisions.md) 隨機出題。一次一題，評分 1 到 5，指出缺漏，並給出更好的說法。
4. **情境題** 2 到 3 題（見 8-4）。
5. **總結**：表現好的地方、待加強的地方、建議重讀哪一課。

## 8-2 參考稿：1 分鐘介紹

> 「這是一個 GPS 跑錶的 AP 層模擬器，重點放在 driver 和畫面之間的應用層。AP 層自己定義 service 介面，由 platform 注入，所以能在 Windows 上跑，也能完全做單元測試。
> 資料流是這樣：driver 執行緒把資料投進加鎖的 inbox，單一 AP 執行緒把按鍵轉成手勢，再透過 EventBus 延遲派送。輸入走責任鏈，活動用 State pattern，畫面流程由 Mediator 決定，渲染只讀快照。
> 整個 AP 層沒有 RTTI、沒有 exception、init 之後零 heap，也處理了 tick 溢位。這些規則都由編譯參數和測試強制執行。」

## 8-3 參考骨架：3 分鐘架構介紹

| 時間 | 內容 | 對應課程 |
|---|---|---|
| 0:00–0:30 | 目的：展示 AP 層設計；後端是模擬的 | [第 1 課](01-overview.md) |
| 0:30–1:00 | 分層 + DIP + composition root；移植只改 platform | [第 1 課](01-overview.md) |
| 1:00–1:45 | 資料流：inbox → 手勢 → bus → 責任鏈 → Session → Coordinator → 快照 → render | [第 2 課](02-step-loop.md)、[第 6 課 6-6](06-ui.md) |
| 1:45–2:30 | 挑兩個 pattern 講取捨：State vs switch；延遲派送 vs 直接呼叫 | [第 4 課](04-event-bus.md)、[第 5 課](05-activity-state.md) |
| 2:30–3:00 | 嵌入式限制 + 已知簡化 | [第 7 課](07-embedded.md) |

## 8-4 情境題庫

<details>
<summary>Q1：要加 AutoPause（速度低於門檻自動暫停），你會怎麼做？</summary>

- 新增 `AutoPausedState`。Running 收到 GPS 樣本時，判斷速度低於門檻並持續一段時間（debounce）就轉過去；速度恢復時轉回 Running。
- 新增 `ActivityState::AutoPaused`，Coordinator 決定是否顯示提示，Overlay 決定震動方式。
- 和使用者手動暫停分開：手動 Paused 時不會自動恢復。
- 加測試：用 fake GPS 餵速度序列，驗證狀態轉換和 metrics 暫停。
</details>

<details>
<summary>Q2：要支援新感測器（例如血氧 SpO2），要改哪些地方？</summary>

1. `services.h` 新增 `ISpO2Service`；`ISensorSink` 新增 `postSpO2`（或泛化 inbox 訊息）。
2. `InboxMsg` 和 `Event` 新增對應種類，確認 Event 仍然 ≤ 24 bytes（static_assert 會守住）。
3. 新模組（例如 SpO2Monitor）訂閱事件、產生警示事件。
4. UiModel 加欄位，畫面讀取。
5. platform 實作 driver，`tests/fakes.h` 加上 fake。

重點：既有模組幾乎都不用改，只要新增。
</details>

<details>
<summary>Q3：產品要求雙核心，UI 在另一顆核心，你怎麼改？</summary>

邊界早就存在：AP 執行緒產生 `UiModel` 快照（trivially copyable），用 double buffer 或 queue 交給 UI 核心。UI 核心只負責 render，不碰 AP 狀態。Inbox 本來就是跨執行緒的入口。改動集中在 platform 和 main，`ap/` 幾乎不動。
</details>

<details>
<summary>Q4：使用者回報「錶開機一個多月後，背光不會自動關」，你會懷疑什麼？</summary>

tick 溢位（約 49.7 天）。檢查是否有地方用 `>=` 直接比較時間，而沒有用 `timeReached()`。這個專案用 `static_assert` 和 `--wrap` 模式預防這類問題，可以在模擬器上用 `--wrap` 重現。
</details>

<details>
<summary>Q5：跑步中 GPS 訊號斷了，系統會怎樣？</summary>

`ActivitySession::snapshot()` 判斷最後一次 fix 超過 3 秒（`kGpsStaleMs`）就把 `gpsFix` 設成 false，狀態列的 GPS 點會變紅。`ActivityMetrics::onGps()` 會忽略沒有 fix 的樣本，精度太差或速度不合理（跳點）的樣本也會丟掉（[activity_metrics.cpp:61](../../../../ap/src/activity/activity_metrics.cpp#L61)），所以距離不再累加，但計時繼續。活動不中斷，也不需要任何特例程式碼。
</details>

## 8-5 綜合問答題庫（從各課彙整）

| 主題 | 題目 | 複習 |
|---|---|---|
| DIP | 介面為什麼由 AP 擁有？解構子為什麼是 protected？ | [第 1 課](01-overview.md) |
| Active Object | 為什麼 AP 層內部不需要鎖？ | [第 2 課](02-step-loop.md) |
| 責任鏈 | 鏈的順序代表什麼？怎麼加組合鍵？ | [第 3 課](03-input.md) |
| Command | 為什麼不用 std::function？ | [第 3 課](03-input.md) |
| EventBus | 為什麼延遲派送？缺點是什麼？generation 的用途？ | [第 4 課](04-event-bus.md) |
| State | 為什麼不用 switch？狀態物件為什麼不 new？save 失敗怎麼辦？ | [第 5 課](05-activity-state.md) |
| 心率 | hysteresis 和 debounce 差在哪裡？ | [第 5 課](05-activity-state.md) |
| Mediator / NVI | Coordinator 和 bus 的差別？render 為什麼非虛擬？ | [第 6 課](06-ui.md) |
| Decorator / Adapter | 主題為什麼放在 AP 層而不是 OpenCV adapter？ | [第 6 課](06-ui.md) |
| 嵌入式 | 怎麼證明零 heap？tick 溢位怎麼處理？ | [第 7 課](07-embedded.md) |
| 誠實度 | 模擬器和真手錶差在哪裡？ | [第 7 課 7-6](07-embedded.md) |
