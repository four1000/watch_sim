# 第 3 課：輸入，按鍵 → 手勢 → 責任鏈 → 命令

[← 上一課](02-step-loop.md) ｜ [課程目錄](../syllabus.md) ｜ [下一課 →](04-event-bus.md)

## 🎯 本課目標
1. 理解原始按鍵邊緣（按下／放開）如何變成短按、長按、雙擊。
2. 解釋「同一顆鍵在不同情境意思不同」，怎麼在不寫巨大 if/else 的情況下做到（**Chain of Responsibility**）。
3. 理解選單項目如何用 **Command** 和它要做的事解耦。

**必讀檔案**：[gesture_recognizer.h](../../../../ap/include/ap/input/gesture_recognizer.h)、[gesture_recognizer.cpp](../../../../ap/src/input/gesture_recognizer.cpp)、[input_router.h](../../../../ap/include/ap/input/input_router.h)、[input_router.cpp](../../../../ap/src/input/input_router.cpp)、[system_handlers.h](../../../../ap/include/ap/ui/system_handlers.h)、[navigator.cpp](../../../../ap/src/ui/navigator.cpp)、[commands.h](../../../../ap/include/ap/ui/commands.h)

---

## 3-1 GestureRecognizer：每顆鍵一個小狀態機

硬體只會告訴你「某顆鍵 Down」或「某顆鍵 Up」，外加時間戳。手勢辨識器把它們翻譯成使用者的意圖：

```
           Down              Up（未開雙擊）→ 發 Short
  Idle ─────────▶ Pressed ──────────────────────────▶ Idle
                    │ 按住 ≥ 800ms → 發 Long
                    ▼
                 LongFired ──Up──▶ Idle

  （開了雙擊的鍵）
  Pressed ──Up──▶ WaitSecond ──Down──▶ SecondDown ──Up──▶ 發 Double
                     │ 300ms 內沒按第二下 → 發 Short
```

- 狀態定義在 [gesture_recognizer.h:36](../../../../ap/include/ap/input/gesture_recognizer.h#L36)，參數在 [gesture_recognizer.h:13-14](../../../../ap/include/ap/input/gesture_recognizer.h#L13-L14)：長按 800ms、雙擊視窗 300ms。
- `onKeyEvent()`（[L34](../../../../ap/src/input/gesture_recognizer.cpp#L34)）處理邊緣，`poll(now)`（[L73](../../../../ap/src/input/gesture_recognizer.cpp#L73)）處理「時間到了」，也就是長按和雙擊逾時。
- 這裡用 **`enum` + `switch`**，不用 State pattern：每顆鍵的狀態很小，也沒有各狀態專屬的資料，`switch` 更短更清楚（第 5 課會對比）。
- 時間全是**從外面帶進來**的，所以 800ms 這類規則能被測試精準驗證。

### 為什麼雙擊是 opt-in，而且只開在 LIGHT？
一顆鍵只要開了雙擊，放開時就**不能馬上**判定為短按，必須等 300ms 確認沒有第二下。START 和 BACK 用來計圈、暫停，需要即時反應，所以不開。只有 LIGHT 需要「雙擊鎖鍵」，見 [watch_app.cpp:51](../../../../ap/src/app/watch_app.cpp#L51)：

```cpp
m_gestures.setDoubleEnabled(KeyId::Light, true);
```

## 3-2 InputRouter：責任鏈

手勢產生之後，要問「**現在誰該處理它？**」。答案取決於情境：鍵盤鎖了嗎？有警示視窗嗎？現在在哪個畫面？

[input_router.cpp:16](../../../../ap/src/input/input_router.cpp#L16)：

```cpp
IInputHandler* InputRouter::route(const GestureEvent& e) {
    for (i = 0; i < m_count; ++i)
        if (m_chain[i]->handle(e)) return m_chain[i];   // 第一個吃掉的就停
    ++m_unhandled;                                       // 沒人要：計數
    return nullptr;
}
```

鏈的順序在 `init()` 時組好，之後不再改變，見 [watch_app.cpp:46-49](../../../../ap/src/app/watch_app.cpp#L46-L49)：

```
手勢 ─▶ ① KeyLock ─▶ ② Overlay ─▶ ③ Navigator ─▶ ④ Backlight
```

| 順位 | Handler | 什麼時候吃掉事件 | 程式碼 |
|---|---|---|---|
| ① | **KeyLock** | 雙擊 LIGHT 時切換鎖定；**鎖定中吃掉所有鍵** | [system_handlers.h:12](../../../../ap/include/ap/ui/system_handlers.h#L12) |
| ② | **Overlay** | 有 modal 警示（例如 SLOW DOWN）時攔截，短按關閉 | [overlay.cpp:21](../../../../ap/src/ui/overlay.cpp#L21) |
| ③ | **Navigator** | 交給頂層 Screen；Screen 不要而且是 BACK 短按，就 pop | [navigator.cpp:48](../../../../ap/src/ui/navigator.cpp#L48) |
| ④ | **Backlight** | 全域 fallback：LIGHT 短按開關背光，8 秒自動關 | [system_handlers.h:32](../../../../ap/include/ap/ui/system_handlers.h#L32) |

**順序本身就是優先權**：鎖定 > 警示 > 畫面 > 全域預設。加新功能時插入一個 handler 就好，不必改一個巨大的 `handleKey()`。

看 KeyLock 有多精簡：

```cpp
bool handle(const GestureEvent& e) override {
    if (e.key == KeyId::Light && e.gesture == Gesture::Double) {
        m_locked = !m_locked;
        m_bus.publish(Event::makeKeyLock(e.timestampMs, m_locked));  // 通知別人（顯示 LOCKED）
        return true;
    }
    return m_locked;    // 鎖定中：吃掉一切；沒鎖：放行
}
```

## 3-3 Command：選單不知道選項會做什麼

選單 `MenuScreen` 的每個項目綁一個 `ICommand`（[input_router.h:41](../../../../ap/include/ap/input/input_router.h#L41)），選了就呼叫 `execute(now)`。見 [commands.h](../../../../ap/include/ap/ui/commands.h)：

```cpp
class ActivityCommand final : public ICommand {
    using Request = bool (IActivityControl::*)(TickMs);   // 成員函式指標
    void execute(TickMs now) override { (m_control.*m_request)(now); }
};
class PushScreenCommand final : public ICommand {
    void execute(TickMs now) override { m_nav.push(m_target, now); }
};
```

綁定在 [watch_app.cpp](../../../../ap/src/app/watch_app.cpp) 的建構子和 `init()`：

```cpp
m_cmdRun(m_session, &IActivityControl::prepare)     // 「Run」→ 準備開始
m_cmdSave(m_session, &IActivityControl::save)       // 「Save」→ 存檔
m_activityMenu.addItem("Run", m_cmdRun);
m_pauseMenu.addItem("Save", m_cmdSave);
```

一個 `ActivityCommand` 類別，就能代表 Run、Resume、Save、Discard 四種命令。**用成員函式指標，而不是 `std::function` 或 lambda**：大小固定，不配置 heap。

---

## ⌚ 對應真實手錶
Garmin 跑錶通常是 5 鍵。同一顆 BACK，在跑步中是計圈，在選單裡是返回，警示出現時是關閉警示，鎖定時什麼都不做。責任鏈讓這些規則各自待在自己的 handler 裡。

## 💬 面試金句
> 「BACK 現在代表什麼，由它在鏈中的位置決定，而不是靠一個每加一個功能就要改一次的巨型 handleKey()。」

---

## 📝 小測驗
1. 為什麼 KeyLock 放在鏈的最前面，Backlight 放在最後？
2. 為什麼不在所有鍵上都開雙擊偵測？
3. `ActivityCommand` 為什麼用成員函式指標，而不用 `std::function` 或 lambda？

<details>
<summary>參考答案</summary>

1. 鎖定要最優先攔截所有輸入（防止誤觸）。背光是沒人處理時的全域 fallback，放最後才不會搶走畫面要用的 LIGHT 鍵事件。
2. 開了雙擊，就要等 300ms 視窗結束才能送出短按，造成延遲。START 和 BACK 需要即時（計圈、暫停），只有 LIGHT 需要雙擊鎖鍵。
3. AP 層禁用 `std::function`，因為它可能配置 heap，體積也大（MSVC 上 64 bytes）。成員函式指標大小固定、不配置記憶體，一個類別就能代表多種命令。
</details>

## 🎤 面試追問
<details>
<summary>手勢辨識用 switch，活動狀態卻用 State pattern，為什麼不一致？</summary>

依情況選工具。每顆鍵的狀態很小，也沒有各狀態專屬的資料，`switch` 最清楚。活動狀態每個狀態接受的操作都不同，還有 entry/exit 動作，用 State pattern 才能把規則集中（見 [design-decisions.md §1](../../../../docs/design-decisions.md)）。
</details>

<details>
<summary>雙擊的第二下按住不放變成長按，會發出什麼？</summary>

先補發一個 Short（代表第一下），再發 Long，見 [gesture_recognizer.cpp:73](../../../../ap/src/input/gesture_recognizer.cpp#L73) `poll()` 裡的 `SecondDown` 分支。
</details>

<details>
<summary>如果要加「同時按住 UP+DOWN 截圖」的組合鍵，你會加在哪裡？</summary>

組合鍵的偵測放在手勢層，新增一種 Gesture，或做一個組合偵測器。處理則新增一個 `IInputHandler`（例如 ScreenshotHandler），插在 KeyLock 之後、Overlay 之前。現有的 handler 都不用改。
</details>
