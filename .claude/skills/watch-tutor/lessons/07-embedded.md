# 第 7 課：嵌入式限制，由 build 強制執行

[← 上一課](06-ui.md) ｜ [課程目錄](../syllabus.md) ｜ [下一課 →](08-mock-interview.md)

## 🎯 本課目標
1. 能說出每條嵌入式規則，以及它為什麼存在。
2. 理解專案怎麼**強制**這些規則，而不只是口頭約定。
3. 知道模擬器和真手錶的已知差異。

**必讀檔案**：[CMakeLists.txt](../../../../CMakeLists.txt)、[time.h](../../../../ap/include/ap/core/time.h)、[soft_timer.h](../../../../ap/include/ap/core/soft_timer.h)、[soft_timer.cpp](../../../../ap/src/core/soft_timer.cpp)、[ring_buffer.h](../../../../ap/include/ap/core/ring_buffer.h)、[watch_app_test.cpp](../../../../tests/watch_app_test.cpp)、[design-decisions.md §7–§10](../../../../docs/design-decisions.md)

---

## 7-1 編譯參數：不合規就編不過

[CMakeLists.txt:30-31](../../../../CMakeLists.txt#L30-L31)：

```cmake
target_compile_options(ap PRIVATE /W4 /WX /utf-8 /permissive- /GR- /EHs-c-)
target_compile_definitions(ap PRIVATE _HAS_EXCEPTIONS=0)
```

| 參數 | 效果 | 為什麼 |
|---|---|---|
| `/GR-` | 沒有 RTTI，`dynamic_cast` 和 `typeid` 編不過 | 省 ROM，型別判斷交給虛擬函式 |
| `/EHs-c-` + `_HAS_EXCEPTIONS=0` | 沒有 exception | 省 ROM，執行時間可預測，錯誤改用回傳值和事件 |
| `/W4 /WX` | 警告視為錯誤 | 品質門檻 |

注意：這些參數**只套用在 `ap` target**。測試和模擬器用一般設定（`/EHsc`），因為它們扮演的是「平台」。

## 7-2 init 之後零 heap，而且有測試證明

規則：所有容器固定容量，每個都有**溢位計數**。

| 容器 | 容量 | 滿了 |
|---|---|---|
| inbox | 32 | `inboxDropped` |
| EventBus queue / 訂閱者 | 32 / 16 | `busDropped` / 訂閱失敗 |
| 手勢 FIFO | 8 | `m_dropped` |
| SoftTimerPool | 8 | `start` 回傳無效 handle |
| 畫面堆疊 / 責任鏈 | 6 / 6 | `push` / `append` 回傳 false |

證明方式在 [watch_app_test.cpp:18](../../../../tests/watch_app_test.cpp#L18)：替換全域 `operator new` 加上計數。[L383](../../../../tests/watch_app_test.cpp#L383) 的 `ZeroHeapAllocationAfterInit` 在 init 之後跑 2 分鐘活動（包含警示、計圈、存檔），斷言配置次數是 0：

```cpp
EXPECT_EQ(after - before, 0) << "operator new called inside the AP main loop";
```

[L443](../../../../tests/watch_app_test.cpp#L443) 的 `AllocationCounterWorks` 驗證計數器本身有效，避免「測試永遠通過」。

同樣的理由，AP 層禁用 `std::function`、`std::string`、`std::vector`，它們大小不固定，或者會配置記憶體。字串一律用 `snprintf` 寫進固定大小的 buffer。

## 7-3 Tick 溢位：49.7 天的定時炸彈

`TickMs` 是 `uint32_t` 毫秒，約 **49.7 天**就會溢位歸零。手錶可能連續開機好幾週，所以這是真實的 bug 來源。

[time.h:9-17](../../../../ap/include/ap/core/time.h#L9-L17)：

```cpp
constexpr TickMs elapsedMs(TickMs now, TickMs since) { return now - since; }  // 模數減法，自然正確
constexpr bool timeReached(TickMs now, TickMs deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;   // 用有號差判斷先後
}
static_assert(timeReached(5u, 0xFFFFFFF0u), "wrap-safe compare");   // 編譯期就驗證
static_assert(elapsedMs(5u, 0xFFFFFFFBu) == 10u, "wrap-safe elapsed");
```

- ❌ `now >= deadline`：溢位後 now 變得很小，判斷就錯了。
- ✅ 有號差：只要兩個時間相距不超過 2³¹ ms（約 24.8 天）就正確。
- 模擬器的 `--wrap`（[main.cpp:36](../../../../app/main.cpp#L36)）從溢位前 30 秒開始跑，實際穿越 `0xFFFFFFFF`。里程表也從 `0xFFFF0000` 起跳，測試距離的溢位。

## 7-4 SoftTimerPool：不阻塞的計時器

[soft_timer.h](../../../../ap/include/ap/core/soft_timer.h)：固定 8 個 slot，由 `step()` 呼叫 `poll(now)` 驅動，沒有任何 OS timer。

- `start()` 回傳 `TimerHandle{slot, generation}`。`cancel()` 只在 generation 相符時生效，不會取消到「重用了同一個 slot 的別人的 timer」（和第 4 課的 Subscription 同樣技巧）。
- 到期時先釋放 slot 再回呼，所以回呼裡可以安全地重新 start。
- 在 poll 期間新建的 timer，下一輪才會 armed，避免同一輪立刻觸發。
- 實際用途：背光 8 秒自動關、toast 2 秒、警示 3 秒。

## 7-5 時間注入：AP 層不讀時鐘

`ap/` 裡**沒有任何**讀取系統時鐘的程式碼。時間的來源只有：
- 按鍵事件自帶的時間戳，
- `step(now)` 的參數。

好處：
- 所有時序規則（長按 800ms、debounce 3s、背光 8s）都能**決定性**地測試，測試裡直接傳 `now + 801`。
- 腳本模式（`--script`）每次重播結果都一樣，連 tick 溢位都能重現。
- 計圈的時間用的是手勢時間，不受處理延遲影響。

## 7-6 已知簡化（誠實說出來）

[design-decisions.md §10](../../../../docs/design-decisions.md)：

| 面向 | 模擬器 | 真實手錶 |
|---|---|---|
| 執行緒 | AP 跑在 UI 執行緒 | AP task 有自己的 queue，UI task 畫快照 |
| 儲存 | CSV 暫存在記憶體，存檔時寫出 | FIT 檔串流寫入 Flash，要能斷電恢復 |
| 配速 | 6 點移動視窗 | Kalman 濾波，加上加速度計融合 |
| 渲染 | 髒了就全畫面重畫 | 局部更新、frame buffer 限制、MIP 顯示 |

面試時主動說出這張表，會比被問到才承認好得多。

---

## ⌚ 對應真實手錶
手錶 RAM 少、要連續運行好幾週，也不能當機重開。heap 碎片化和配置失敗都不可接受，49 天的 tick 溢位是業界真實發生過的 bug。

## 💬 面試金句
> 「這些規則不是靠 code review 記住的，而是由編譯參數和一個計算 operator new 次數的測試強制執行。」

---

## 📝 小測驗
1. `timeReached(now, deadline)` 為什麼不能直接寫成 `now >= deadline`？
2. 這個專案怎麼「證明」init 之後沒有任何 heap 配置？
3. AP 層為什麼不自己讀時鐘？

<details>
<summary>參考答案</summary>

1. tick 溢位後 now 會變小，直接比較會誤判。改用 `(int32_t)(now - deadline) >= 0`，在兩個時間相距小於 2³¹ 時都正確。
2. `ZeroHeapAllocationAfterInit` 測試替換全域 `operator new` 加上計數，init 之後跑完整情境，斷言計數為 0。另外有 `AllocationCounterWorks` 測試驗證計數器本身有效。
3. 時間注入讓時序邏輯能決定性地測試、能重播、能模擬 tick 溢位，也不依賴 OS。
</details>

## 🎤 面試追問
<details>
<summary>每個容器都有上限，上限怎麼決定？滿了怎麼辦？</summary>

依最壞情況估算（例如每秒感測器樣本數 × 最長處理間隔），再留餘裕，然後用溢位計數器在測試和實機上驗證。滿了就丟棄並計數，絕不阻塞或配置。計數器非零就代表要調整容量或處理速度。
</details>

<details>
<summary>不能用 exception，建構失敗怎麼辦？</summary>

建構子只做不會失敗的事（存參考、初始化成員）。可能失敗的工作放到 `init()`，用回傳 `bool` 表示成敗。執行期的失敗用回傳值加事件（StartFailed、SaveFailed）表達。
</details>

<details>
<summary>你會怎麼把模擬器改成真正的雙 task 架構？</summary>

AP task 阻塞在自己的 queue 上（取代 inbox 加輪詢），被訊息或計時器喚醒後執行 `step()`。狀態變化後產生 UiModel 快照，交給 UI task，UI task 只負責畫。AP 層的程式碼幾乎不用改，因為 inbox 和快照的邊界早就是執行緒邊界。
</details>
