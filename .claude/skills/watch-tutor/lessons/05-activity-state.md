# 第 5 課：活動狀態機（State pattern）與心率監控

[← 上一課](04-event-bus.md) ｜ [課程目錄](../syllabus.md) ｜ [下一課 →](06-ui.md)

## 🎯 本課目標
1. 能畫出活動狀態機，說出每個轉換做了什麼。
2. 理解這個專案的 State pattern 怎麼做到「無 heap」和「entry/exit 一定執行」。
3. 理解心率區間的 hysteresis、debounce、re-alert。

**必讀檔案**：[activity_session.h](../../../../ap/include/ap/activity/activity_session.h)、[activity_session.cpp](../../../../ap/src/activity/activity_session.cpp)、[hr_zone_monitor.h](../../../../ap/include/ap/health/hr_zone_monitor.h)、[hr_zone_monitor.cpp](../../../../ap/src/health/hr_zone_monitor.cpp)、[architecture.md §4](../../../../docs/architecture.md)、[design-decisions.md §1](../../../../docs/design-decisions.md)

---

## 5-1 狀態機全貌

```
              ┌─────────┐
              │  Idle   │ ◀─────────────────────────┐
              └─────────┘ ◀──────────────┐          │
                │     ▲                  │          │
       prepare  │     │  cancel          │          │
                ▼     │                  │          │
              ┌─────────┐                │          │
              │  Ready  │                │          │
              └─────────┘                │          │
                │                        │          │
    startPause  │                        │          │
                ▼                        │          │
              ┌─────────┐   lap          │          │
              │ Running │ ◀────┐         │          │
              └─────────┘ ─────┘         │          │
                │     ▲                  │          │
    startPause  │     │  startPause      │ save     │ discard
                ▼     │                  │          │
              ┌─────────┐                │          │
              │ Paused  │ ───────────────┘          │
              └─────────┘ ──────────────────────────┘
```

| 狀態 | 事件 | 動作 | 成功 → | 失敗 |
|---|---|---|---|---|
| Idle | prepare | `GPS.start()` | Ready | 留在 Idle（StartFailed） |
| Ready | cancel | — | Idle（Cancelled） | — |
| Ready | startPause | `recorder.begin()`、`metrics.start()` | Running | 留在 Ready（StartFailed） |
| Running | lap | `recordLap`、發布 Lap 事件 | 留在 Running | — |
| Running | startPause | `metrics.pause()` | Paused | — |
| Paused | startPause | `metrics.resume()` | Running | — |
| Paused | save | `recorder.finish()` | Idle（Saved） | 留在 Paused（SaveFailed） |
| Paused | discard | `recorder.discard()` | Idle（Discarded） | — |

圖上沒畫的事件（例如 Running 收到 save）一律由 base class 回 `false`，狀態不變。

## 5-2 State pattern：狀態物件是成員，沒有 heap

[activity_session.h:50](../../../../ap/include/ap/activity/activity_session.h#L50)：

```cpp
class State {
public:
    virtual ActivityState id() const = 0;
    virtual void onEnter(ActivitySession&, TickMs) {}
    virtual void onExit(ActivitySession&, TickMs) {}
    virtual bool prepare(ActivitySession&, TickMs)    { return false; }  // 預設：不接受
    virtual bool startPause(ActivitySession&, TickMs) { return false; }
    virtual bool save(ActivitySession&, TickMs)       { return false; }
    ...
};
```

- **base 每個請求預設回 `false`**：每個狀態只覆寫它接受的操作。想知道「Paused 時能做什麼」，只要看 `PausedState`（[L87](../../../../ap/include/ap/activity/activity_session.h#L87)）這一個類別。
- **四個狀態物件是 `ActivitySession` 的成員**（[L103-107](../../../../ap/include/ap/activity/activity_session.h#L103-L107)），`m_state` 只是指向其中一個。切換狀態就是換指標，沒有 `new` 或 `delete`。
- 對外的請求只是轉送：

```cpp
bool ActivitySession::startPause(TickMs now) { return m_state->startPause(*this, now); }
```

## 5-3 transitionTo：轉換的唯一路徑

[activity_session.cpp:43](../../../../ap/src/activity/activity_session.cpp#L43)：

```cpp
void ActivitySession::transitionTo(State& next, TickMs now, ActivityOutcome outcome) {
    const ActivityState from = m_state->id();
    m_state->onExit(*this, now);     // 1. 舊狀態收尾
    m_state = &next;                 // 2. 換指標
    m_state->onEnter(*this, now);    // 3. 新狀態初始化
    m_bus.publish(Event::makeActivity(now, from, next.id(), outcome));  // 4. 告訴全世界
}
```

所有轉換都走這裡，所以 exit 到 enter 的順序**一定**正確，也一定會發出事件。

最好的例子在 [L56](../../../../ap/src/activity/activity_session.cpp#L56)：

```cpp
void ActivitySession::IdleState::onEnter(ActivitySession& s, TickMs) {
    s.m_gps.stop();          // 唯一停 GPS 的地方
    s.m_gpsFix = false;
}
```

不管是 save、discard 還是 cancel，只要回到 Idle 就一定停 GPS，不會因為某條路徑漏寫而一直耗電。

## 5-4 失敗是「值」，不是 exception

[activity_session.cpp:61](../../../../ap/src/activity/activity_session.cpp#L61)：

```cpp
bool ActivitySession::IdleState::prepare(ActivitySession& s, TickMs now) {
    if (!s.m_gps.start()) {
        s.publishFailure(now, ActivityOutcome::StartFailed);  // from == to == Idle
        return false;
    }
    s.transitionTo(s.m_ready, now);
    return true;
}
```

- AP 層禁用 exception，錯誤用**回傳值**加**事件**表達。Overlay 收到 `StartFailed` 會顯示紅色警示，並震動。
- **save 失敗時留在 Paused**（[L107](../../../../ap/src/activity/activity_session.cpp#L107)）：資料不會丟，使用者可以重試或選擇丟棄。
- save 前會補記最後一圈（如果有未計的時間），再呼叫 `recorder.finish()`。

## 5-5 窄介面：IActivityControl（ISP）

[activity_session.h:12](../../../../ap/include/ap/activity/activity_session.h#L12)：畫面和命令拿到的是 `IActivityControl&`，只看得到 `prepare / cancel / startPause / lap / save / discard / state`。它們看不到 recorder、GPS，也看不到 metrics 的內部。

## 5-6 HrZoneMonitor：讓心率訊號「不吵」

區間定義：Z1 到 Z5 的下界分別是 maxHr 的 50%、60%、70%、80%、90%（[hr_zone_monitor.cpp:7](../../../../ap/src/health/hr_zone_monitor.cpp#L7)）。

光學心率很吵，所以有三道防線（常數在 [hr_zone_monitor.h:16-18](../../../../ap/include/ap/health/hr_zone_monitor.h#L16-L18)）：

| 機制 | 參數 | 解決的問題 | 程式碼 |
|---|---|---|---|
| **Hysteresis** | 2 bpm | 心率剛好在邊界上下跳時，zone 不會一直切換 | [updateZone L70](../../../../ap/src/health/hr_zone_monitor.cpp#L70) |
| **Debounce** | 3 秒 | 短暫偏離目標區間不發警示，要連續偏離 3 秒 | [checkAlert L83](../../../../ap/src/health/hr_zone_monitor.cpp#L83) |
| **Re-alert** | 30 秒 | 已經警示過，30 秒內不重複震動 | 同上 |

另外，警示只在 **Running 而且設定開啟**時作用（它訂閱 `ActivityStateChanged` 來得知）。警示帶有方向（AboveTarget / BelowTarget），Overlay 會顯示 **SLOW DOWN** 或 **SPEED UP** 加上雙箭頭。

---

## ⌚ 對應真實手錶
真實手錶還有 AutoPause、多項運動（鐵人三項換項）、休息計時等狀態。State pattern 讓「新增一個狀態」等於新增一個類別。心率的 hysteresis 和 debounce 是避免手錶一直亂震、使用者把功能關掉的關鍵。

## 💬 面試金句
> 「要回答『Paused 時能做什麼』，只要看 PausedState 一個類別，不必翻遍 6 個 switch。」

---

## 📝 小測驗
1. 如果要新增一個 AutoPaused 狀態，用 State pattern 要改哪些地方？如果是 switch 寫法，又要改哪些地方？
2. 為什麼停 GPS 放在 `IdleState::onEnter`，而不是在 save、discard、cancel 裡各自呼叫？
3. 如果沒有 hysteresis，會發生什麼事？

<details>
<summary>參考答案</summary>

1. State 寫法：新增一個 `AutoPausedState` 類別和成員物件，在 Running 和 AutoPaused 裡加上互相轉換的條件。Coordinator 可能也要決定顯示哪個畫面。switch 寫法則要改每一個請求函式裡的 switch，散落在 6 個以上的地方。
2. 單一入口保證任何回到 Idle 的路徑都會停 GPS，不會漏掉，否則會一直耗電。
3. 心率在區間邊界上下跳動時，zone 會一直切換，導致事件、畫面和震動不停閃爍。
</details>

## 🎤 面試追問
<details>
<summary>狀態物件為什麼不用 new 出來？成本是什麼？</summary>

狀態物件沒有各自的資料（資料都在 ActivitySession），所以可以常駐當成員。成本是 4 個物件各帶一個 vptr，沒有 heap、沒有碎片化，切換就是換指標。
</details>

<details>
<summary>save 失敗為什麼不回到 Idle？</summary>

回到 Idle 等於放棄這次活動，使用者跑了一小時的資料就沒了。留在 Paused，使用者還能重試或主動丟棄，決定權留給使用者。
</details>

<details>
<summary>計圈的時間戳用的是什麼？為什麼？</summary>

用的是**手勢的時間戳**（按鍵發生的時刻），不是 AP 處理的時刻。就算 AP 執行緒忙了 50ms，圈時還是準確的（[design-decisions.md §8](../../../../docs/design-decisions.md)）。
</details>
