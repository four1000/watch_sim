# 第 4 課：EventBus，模組之間為什麼不直接呼叫

[← 上一課](03-input.md) ｜ [課程目錄](../syllabus.md) ｜ [下一課 →](05-activity-state.md)

## 🎯 本課目標
1. 理解 Publish/Subscribe 怎麼讓模組彼此不認識。
2. 解釋「延遲派送」（deferred dispatch）的理由。
3. 看懂 topic mask、`Subscription`（RAII）、generation 這些細節。

**必讀檔案**：[event.h](../../../../ap/include/ap/event/event.h)、[event_bus.h](../../../../ap/include/ap/event/event_bus.h)、[event_bus.cpp](../../../../ap/src/event/event_bus.cpp)、[event_bus_test.cpp](../../../../tests/event_bus_test.cpp)、[design-decisions.md §2](../../../../docs/design-decisions.md)

---

## 4-1 問題：沒有 bus 會怎樣？

活動進入 Paused 時，要推出暫停選單、震動兩下、停止心率警示……如果用直接呼叫：

```cpp
// ❌ ActivitySession 必須認識所有人
m_nav.push(pauseMenu); m_vibrator.vibrate(Double); m_zones.reset(); ...
```

Session 會和 UI、震動、心率模組全部綁在一起。有了 bus，Session 只說一句話：

```cpp
m_bus.publish(Event::makeActivity(now, Running, Paused, None));   // 「我進入 Paused 了」
```

誰要回應，自己去訂閱就好：`ActivityFlowCoordinator` 推畫面，`OverlayManager` 震動，`HrZoneMonitor` 重置警示。

## 4-2 Event：固定大小的 tagged union

[event.h:41](../../../../ap/include/ap/event/event.h#L41)：

```cpp
struct Event {
    EventType type;          // 哪一種事件（tag）
    TickMs timestampMs;
    union {                  // 只有一個欄位有效
        GestureEvent gesture; HrSample hr; GpsSample gps;
        ActivityStateChange activity; LapRecord lap; ...
    };
};
static_assert(std::is_trivially_copyable_v<Event>);
static_assert(sizeof(Event) <= 24, "keep events small: the queue is statically allocated");
```

- **trivially copyable 而且 ≤ 24 bytes**：可以直接按值放進靜態 RingBuffer，複製便宜，也不用 heap。
- 由 `static_assert` 在**編譯期**強制，有人加了太大的欄位就編不過。
- 事件種類列在 [event.h:15](../../../../ap/include/ap/event/event.h#L15)：Gesture、HrSample、GpsSample、SecondTick、ActivityStateChanged、LapCompleted、HrZoneChanged、HrAlert、SettingsChanged、KeyLockChanged。

## 4-3 Topic mask：用位元表示「我要聽哪些」

[event.h:31-33](../../../../ap/include/ap/event/event.h#L31-L33)：

```cpp
constexpr TopicMask topic(EventType t) { return TopicMask{1} << t; }
template <typename... Ts>
constexpr TopicMask topics(Ts... ts) { return (topic(ts) | ...); }   // fold expression
```

訂閱的寫法（[hr_zone_monitor.cpp](../../../../ap/src/health/hr_zone_monitor.cpp)）：

```cpp
m_sub = m_bus.subscribe(*this, topics(EventType::HrSample,
                                      EventType::ActivityStateChanged,
                                      EventType::SettingsChanged));
```

派送時只要做一次 `(slot.mask & bit) != 0` 的位元 AND，成本是 O(訂閱者數)。32 位元的 mask 最多支援 32 種事件，由 [event.h:28](../../../../ap/include/ap/event/event.h#L28) 的 `static_assert` 守住。

## 4-4 延遲派送：publish 只排隊，dispatch 才送

```cpp
bool EventBus::publish(const Event& e) {          // event_bus.cpp:55
    if (m_queue.push(e)) return true;
    ++m_dropped;  return false;                    // 滿了：丟棄＋計數
}
```

[event_bus.cpp:61](../../../../ap/src/event/event_bus.cpp#L61) 的 `dispatch()` 才真的呼叫 listener：

```cpp
std::size_t EventBus::dispatch(std::size_t budget) {
    if (m_dispatching) return 0;           // ① 防止重入
    m_dispatching = true;
    while (delivered < budget) {           // ② 預算上限（預設 64）
        e = m_queue.pop();
        for (Slot& s : m_slots)
            if (s.listener && s.armed && (s.mask & bit)) s.listener->onEvent(*e);
        if (m_trace) m_trace->onDispatch(*e, receivers);   // ③ 可觀測性
        for (Slot& s : m_slots) if (s.listener) s.armed = true;  // ④ 新訂閱者下一個事件才生效
    }
    m_dispatching = false;
}
```

**為什麼要延遲？** 如果 publish 當下就呼叫 listener：

```
Session.startPause()
  └─ publish → Coordinator.onEvent → nav.push → 某畫面又呼叫 Session...
                                                   ↑ Session 還改到一半！
```

延遲之後，Session **先完整做完自己的狀態轉換**，事件才在 `dispatch()` 裡一個一個 run-to-completion。listener 在 `onEvent` 裡 publish 的新事件，也只是排到隊尾，不會重入。

- `budget`：避免事件 A 觸發 B、B 又觸發 A，變成無限迴圈。
- `armed`：派送進行中才新訂閱的人，從下一個事件開始收，避免同一個事件派送到一半時名單改變。

## 4-5 Subscription：RAII 退訂 + generation

[event_bus.h:30](../../../../ap/include/ap/event/event_bus.h#L30)：

- `subscribe()` 回傳一個 `Subscription` token（`[[nodiscard]]`），**move-only**。
- token 解構時自動 `reset()` → `unsubscribe(slot, generation)`。物件死掉就自動退訂，不會留下懸空指標。
- **generation**：slot 被釋放之後，可能分配給新的訂閱者，同時 `++generation`。如果舊 token 再呼叫 reset，generation 對不上，就什麼都不做，不會誤刪新訂閱者（典型的 ABA 問題）。

> 同樣的「slot + generation handle」技巧，第 7 課的 SoftTimerPool 也有用到。

## 4-6 可觀測性和「0 receivers」

- `IEventTrace`：dev panel 和 `--trace` 會顯示每個事件送給了幾個 listener。
- `HrZoneChanged` 目前是 **0 receivers**：UI 每幀直接從 UiModel 讀 zone。這個事件是**預留的擴充點**，之後要加「區間改變時震動」或「各區間累計時間」，只要新增一個訂閱者，心率監控模組不必改。

---

## ⌚ 對應真實手錶
手錶的功能模組（運動、通知、健康、UI）通常由不同人或團隊開發。bus 讓新功能「訂閱就好」，不必改發佈者。固定容量和丟棄計數，讓系統在事件風暴時仍然可預測。

## 💬 面試金句
> 「Session 只說『我進入 Paused 了』。至於誰要推畫面、誰要震動，它不知道，也不需要知道。」

---

## 📝 小測驗
1. 為什麼要「延遲」派送，而不是 publish 當下就呼叫 listener？
2. `Subscription` 的 generation 解決什麼問題？
3. Event 為什麼要限制在 24 bytes 以內，而且必須 trivially copyable？

<details>
<summary>參考答案</summary>

1. 避免重入和半更新的狀態。發佈者完成整個轉換之後，事件才依序 run-to-completion 送出，行為可以預測。
2. slot 被釋放後可能分配給新訂閱者。舊 token 如果再呼叫 reset，generation 對不上就不會誤刪新的訂閱（ABA 問題）。
3. queue 是靜態的 RingBuffer，按值複製。事件小而固定，才能不用 heap、複製便宜，記憶體用量也可預估。
</details>

## 🎤 面試追問
<details>
<summary>EventBus 的缺點是什麼？你怎麼緩解？</summary>

很難一眼看出「誰會回應這個事件」，控制流變得間接。緩解方式：`IEventTrace` 記錄每次派送和收件人數、`describe()` 把事件轉成可讀字串，再用 dev panel 即時觀察。
</details>

<details>
<summary>callback 為什麼不用 `std::function`？</summary>

`std::function` 在 MSVC 上佔 64 bytes，捕獲的東西太大時會配置 heap。這裡用只有一個方法的介面 `IEventListener`，加上 RAII 的 `Subscription`，零配置。
</details>

<details>
<summary>事件 queue 滿了怎麼辦？要不要分優先權？</summary>

目前的做法是丟棄並累加 `m_dropped`，可以觀測。改進方向：依事件類型分成多個 queue（例如 UI 手勢優先於 GPS 樣本），或者把可合併的事件（連續的 GPS 樣本）合成一筆。但要先用計數器證明真的會滿，再決定要不要加複雜度。
</details>
