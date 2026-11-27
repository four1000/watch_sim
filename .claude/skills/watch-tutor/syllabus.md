# 課程目錄：watch_sim AP 層導讀（8 課）

每一課都是一份完整講義，可以隨時點開複習。講義內容就是上課內容，導師上課時也照這些檔案講。
每份講義的格式：🎯 目標 → 講解 → ⌚ 對應真實手錶 → 💬 面試金句 → 📝 小測驗（答案收合）→ 🎤 面試追問（答案收合）。

| 課 | 講義 | 你會搞懂的問題 | 涵蓋的 pattern / 技術 |
|---|---|---|---|
| 1 | [全局觀：分層與誰擁有誰](lessons/01-overview.md) | AP 層是什麼？為什麼能在 Windows 上模擬？ | Dependency Inversion、建構子注入、composition root |
| 2 | [心跳：step() 一圈](lessons/02-step-loop.md) | 資料怎麼從按鍵和感測器流到畫面？ | Active Object、固定容量 inbox、dirty flag |
| 3 | [輸入：按鍵 → 手勢 → 責任鏈](lessons/03-input.md) | 同一顆 BACK 為什麼在不同畫面意思不同？ | 手勢狀態機、Chain of Responsibility、Command |
| 4 | [EventBus](lessons/04-event-bus.md) | 模組之間為什麼不直接呼叫？ | Pub/Sub、延遲派送、topic mask、RAII Subscription |
| 5 | [活動狀態機與心率監控](lessons/05-activity-state.md) | Idle → Ready → Running ⇄ Paused → Saved 怎麼運作？ | State pattern、ISP、hysteresis/debounce |
| 6 | [UI：畫面切換與繪製](lessons/06-ui.md) | 畫面怎麼切換、怎麼畫？ | Mediator、NVI、View-model snapshot、Decorator、Adapter |
| 7 | [嵌入式限制](lessons/07-embedded.md) | 零 heap、不用 exception、tick 溢位怎麼處理？ | 編譯參數、零配置測試、wrap-safe 時間、soft timer、時間注入 |
| 8 | [面試模擬](lessons/08-mock-interview.md) | 能不能流暢介紹專案並應付追問？ | 介紹稿、情境題、綜合題庫 |

學習進度見 [progress.md](progress.md)。

## 建議複習方式
- **面試前一天**：每課只看「💬 面試金句」和「🎤 面試追問」。
- **面試前一小時**：看 [第 6 課 6-6 完整追蹤](lessons/06-ui.md) 和 [第 8 課 8-2 介紹稿](lessons/08-mock-interview.md)。
