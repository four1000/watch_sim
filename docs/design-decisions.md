# Design decisions and trade-offs

Each entry lists the alternative that was rejected and the reason, the questions an interviewer is likely to ask.

## 1. State pattern for the activity, not a `switch`

| | `switch (state)` in each request | State objects (chosen) |
|---|---|---|
| Add a state (AutoPaused) | edit every request function | add one class |
| "What can I do while Paused?" | scattered across 6 functions | one class, `PausedState` |
| Entry/exit actions | easy to forget on one path | `transitionTo()` always runs exit → enter |
| Cost | none | 1 vtable pointer per state object, 4 objects, no heap |

The base `State` answers `false` (ignored) to every request, so a state only overrides what it accepts. `IdleState::onEnter()` is the single place that stops GPS, whatever path led to Idle (save, discard, cancel).

**When I would *not* use it:** a small state machine with no per-state data, such as the per-key gesture recognizer. There a `switch` over an `enum class` is shorter and just as clear, so the recognizer uses one.

## 2. Event bus with deferred dispatch

* **Why not direct calls?** The session would then have to know about the overlay, the navigator, the zone monitor and the recorder UI. With the bus, it only publishes `ActivityStateChanged`.
* **Why deferred?** Synchronous callbacks re-enter listeners (A notifies B, B calls A, A is half-updated). Here every event runs to completion before the next one starts.
* **Why a topic mask and a fixed queue?** No heap, O(subscribers) dispatch, and overflow is *counted* (`busDropped`) instead of silently growing.
* **Why not `std::function`?** On MSVC it is 64 bytes and may allocate. Listeners implement a one-method interface, and the `Subscription` token is RAII and move-only.
* **Trade-off:** it is harder to follow "who reacts to this". The dev panel trace (`event → receivers`) and `describe()` exist for exactly that reason.

Some events have **0 receivers** in the trace (`HrZoneChanged`). The UI reads the zone from the view model every frame. The event is the extension point for time-in-zone statistics or zone-change vibration, which can be added without touching the monitor.

## 3. Chain of Responsibility for input

The question "what does BACK mean now?" is answered by the *position* in the chain: key lock, then modal overlay, then the top screen, then global fallbacks. The alternative is one `handleKey()` with `if (locked) … else if (alert) … else switch (screen)`, which every new feature has to edit.

The chain is built once in `init()` and never mutated while `route()` iterates. Dynamic behaviour (overlay shown, screen pushed) lives *inside* the handlers.

## 4. Mediator between session and navigation

`ActivityFlowCoordinator` owns the rule "Paused → show pause menu, Idle → back to watch face". Screens call `IActivityControl`, and the session publishes state changes. Neither one knows about the other. Changing the UX flow (for example, a summary screen after save) touches exactly one class.

## 5. View model snapshot

`WatchApp::render()` builds a `UiModel` (plain data, trivially copyable), and screens draw only from it:

* rendering cannot change state,
* screens are unit-tested by filling a struct (`ui_test.cpp`),
* on a dual-core watch the snapshot could be handed to a UI task without sharing locks.

## 6. Narrow interfaces (ISP)

| Client | Sees | Does not see |
|---|---|---|
| `ActivityDataScreen`, menu commands | `IActivityControl` | recorder, GPS, metrics internals |
| `SettingsScreen` | `ISettingsEditor` | the event bus |
| `ActivitySession` | `IGpsService`, `IActivityRecorder` | the platform |

## 7. Embedded subset

| Rule | How it is enforced |
|---|---|
| No exceptions / RTTI | compiler flags on the `ap` target. Using them does not compile |
| No heap after init | fixed containers + `ZeroHeapAllocationAfterInit` test (counting `operator new`) |
| Bounded everything | every queue/table has a capacity and an overflow counter: inbox, bus, gesture FIFO, laps, timers, screen stack |
| Tick wrap | `timeReached()` / `elapsedMs()` only; `--wrap` runs the simulator across `0xFFFFFFFF` |
| Odometer wrap | modulo-2³² subtraction; the simulator starts its odometer at `0xFFFF0000` |

Failures are values: `prepare()` returns `false` and publishes `StartFailed`, and a failed save **stays Paused** so nothing is lost.

## 8. Time is injected

Nothing in `ap/` reads a clock. Gesture timestamps come with the key edge, and `step(now)` drives timers and the 1 s tick. Consequences:

* every timing rule (long press 800 ms, alert debounce 3 s, backlight 8 s) is tested deterministically,
* the scripted mode replays the same run with identical results, even across a tick wrap.

A lap is stamped with the **gesture** time, not the processing time. If the AP thread was busy for 50 ms, the lap is still correct.

## 9. Gesture latency trade-off

A double-press detector has to wait for the double window (300 ms) before it can report a short press. That is why double detection is **opt-in per key** and only enabled on LIGHT (key lock). START and BACK, which time laps, stay immediate.

## 10. Known simplifications

| Area | Simplification | On a real watch |
|---|---|---|
| Threads | simulator runs AP on the UI thread | AP task with its own queue; UI task renders the snapshot |
| Storage | CSV buffered in memory, written on save | FIT file streamed to flash, with power-loss recovery |
| Pace | 6-sample window | Kalman-filtered speed, accelerometer fusion |
| Rendering | full redraw on dirty | partial updates, frame-buffer constraints, MIP display |
