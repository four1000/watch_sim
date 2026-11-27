# 第 2 課：心跳，`step()` 一圈做了什麼

[← 上一課](01-overview.md) ｜ [課程目錄](../syllabus.md) ｜ [下一課 →](03-input.md)

## 🎯 本課目標
1. 能從頭到尾說出一次 `step(now)` 的處理順序。
2. 理解跨執行緒的資料怎麼安全進入 AP 層（Active Object）。
3. 知道畫面什麼時候才會重畫。

**必讀檔案**：[watch_app.cpp](../../../../ap/src/app/watch_app.cpp)、[ring_buffer.h](../../../../ap/include/ap/core/ring_buffer.h)、[osal.h](../../../../ap/include/ap/services/osal.h)

---

## 2-1 唯一的多執行緒入口：inbox（Active Object）

driver 執行緒（模擬的 HR、GPS、按鍵）**只能**呼叫 `postKey`、`postHr`、`postGps`。這三個函式都走到同一個 `post()`，見 [watch_app.cpp:63](../../../../ap/src/app/watch_app.cpp#L63)：

```cpp
bool WatchApp::post(const InboxMsg& m) {
    osal::LockGuard lock(m_svc.inboxMutex);   // 鎖透過 OSAL 介面，不綁 OS
    if (m_inbox.push(m)) return true;
    if (m_inboxDropped != UINT16_MAX) ++m_inboxDropped;  // 滿了：丟棄＋計數
    return false;
}
```

- `m_inbox` 是固定容量 32 的 `RingBuffer`（[watch_app.h:38](../../../../ap/include/ap/app/watch_app.h#L38)），不用 heap。
- **滿了不阻塞**：寧可丟資料並計數，也不能讓 driver 卡住。
- `InboxMsg` 是 Key、Hr、Gps 三選一的 tagged union。

> **Active Object 的意思是**：別的執行緒不直接呼叫物件的方法，而是「投信」，由物件自己的執行緒逐一處理。所以 inbox 之後的所有程式碼都在**單一 AP 執行緒**上跑，完全不需要鎖。

## 2-2 收信：鎖內只複製，鎖外才處理

[watch_app.cpp:96](../../../../ap/src/app/watch_app.cpp#L96)：

```cpp
void WatchApp::drainInbox() {
    InboxMsg batch[kMaxInboxPerStep];        // 堆疊上的區域陣列，最多 16 筆
    {
        osal::LockGuard lock(m_svc.inboxMutex);
        while (n < kMaxInboxPerStep) { ... batch[n++] = *m; }   // 鎖內只複製
    }
    for (...) switch (m.kind) {                                   // 鎖外處理
        case Key: m_gestures.onKeyEvent(m.key); break;            // → 第 3 課
        case Hr:  m_bus.publish(Event::makeHr(m.hr)); break;      // → 第 4 課
        case Gps: m_bus.publish(Event::makeGps(m.gps)); break;
    }
}
```

- **持鎖時間最短**：只做複製，driver 的 `post` 幾乎不會被卡住。
- **每輪上限 16 筆**：就算感測器灌爆，一次 step 的工作量也有上限，不會餓死計時器和畫面。
- 分流方式：按鍵先交給手勢辨識，HR 和 GPS 直接變成事件。

## 2-3 一次 `step()` 的完整順序

[watch_app.cpp:117-136](../../../../ap/src/app/watch_app.cpp#L117-L136)：

```
step(now)
 ├─ 1. drainInbox()                 收信、分流
 ├─ 2. m_gestures.poll(now)         檢查長按 / 雙擊逾時
 │     while (next()) publish(Gesture)
 ├─ 3. m_timers.poll(now)           軟體計時器到期（背光 8 秒、toast 2 秒…）
 ├─ 4. 每滿 1 秒 publish(SecondTick)  落後太多就重新對齊
 └─ 5. m_bus.dispatch()             把這輪累積的事件全部送出
        └─ 有送出事件 → m_dirty = true
```

注意：第 1 到第 4 步只是**排隊**（publish），真正送給各模組要等第 5 步。這叫「延遲派送」，第 4 課會解釋為什麼。

手勢事件由 `WatchApp` 自己訂閱，收到後交給責任鏈，見 [watch_app.cpp:138](../../../../ap/src/app/watch_app.cpp#L138)：

```cpp
void WatchApp::onEvent(const Event& e) {
    if (e.type == EventType::Gesture) m_router.route(e.gesture);   // → 第 3 課
    m_dirty = true;
}
```

## 2-4 畫面：只在髒了才畫

主迴圈（[main.cpp](../../../../app/main.cpp) 的 `tick()`）是這樣：

```cpp
app->step(now);
if (app->needsRender()) app->render(canvas);
```

`render()` 見 [watch_app.cpp:155](../../../../ap/src/app/watch_app.cpp#L155)：

```cpp
void WatchApp::render(ICanvas& c) {
    buildModel();                                      // 拍一張 UiModel 快照
    ThemedCanvas themed(c, backlightOn ? Light : Dark); // 主題 Decorator
    m_nav.render(themed, m_model);                     // 畫頂層畫面
    m_overlay.render(themed, m_model);                 // 疊上 toast 或警示
    m_dirty = false;
}
```

快照和 Decorator 的細節留到第 6 課。

---

## ⌚ 對應真實手錶
- AP 是一個 RTOS task，inbox 就是 message queue。ISR 或 driver task 只負責 post，不碰 AP 的狀態。
- `step` 可以由 queue 喚醒，也可以由 tick 驅動。沒事的時候 task 睡覺，dirty flag 讓螢幕不必每幀重畫，這兩點都省電。

## 💬 面試金句
> 「多執行緒只發生在 inbox 這一個點。inbox 之後全部在單一 AP 執行緒上跑，所以 AP 層內部完全不需要鎖。」

---

## 📝 小測驗
1. 為什麼 `drainInbox` 要先把訊息複製到區域陣列再處理，而不是在鎖內直接處理？
2. inbox 滿了會發生什麼事？為什麼這樣設計？
3. `m_dirty` 的用途是什麼？

<details>
<summary>參考答案</summary>

1. 處理過程可能很久（publish、手勢運算），在鎖內做會讓 driver 執行緒的 post 被卡住。鎖內只做 O(n) 複製，持鎖時間最短。
2. `post` 回傳 false 並累加 `m_inboxDropped`，不阻塞，也不配置記憶體。嵌入式寧可丟資料而且可觀測（有計數），也不能讓 driver 卡死或讓 heap 成長。
3. 只有狀態變了才重畫，省 CPU 和功耗。事件派送、背光切換都會把 dirty 設起來。
</details>

## 🎤 面試追問
<details>
<summary>為什麼每次 step 最多處理 16 筆？設太大或太小會怎樣？</summary>

設上限是為了限制單次 step 的最壞執行時間，讓計時器和畫面能準時更新。設太小，訊息會在 inbox 累積，最後被丟棄；設太大，一次 step 可能太久，造成延遲尖峰。容量 32 配每輪 16 筆，代表瞬間爆量時兩輪就能清空。
</details>

<details>
<summary>SecondTick 落後太多時怎麼處理？</summary>

`m_nextSecond += 1000` 之後，如果還是已經到期（代表落後超過 1 秒），就直接重設成 `now + 1000`。不會連續補發一堆 tick，見 [watch_app.cpp:129-133](../../../../ap/src/app/watch_app.cpp#L129-L133)。
</details>

<details>
<summary>改成雙核心、UI 在另一顆核心上畫圖，你會怎麼拆？</summary>

AP task 每次狀態變化後產生一份 UiModel 快照（純資料、trivially copyable），交給 UI task 去畫，例如用 double buffer 或 queue。兩邊不共享可變狀態，也不需要共用鎖。
</details>
