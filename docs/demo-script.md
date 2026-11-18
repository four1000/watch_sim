# 5-minute live demo

Start: `build\RelWithDebInfo\watch_sim.exe --trace`. Put the console next to the window.

| # | Do | Point out |
|---|---|---|
| 1 | Show the watch face, then move the HR slider | HR samples come from a **separate thread** and go through the inbox into `WatchApp`. The watch face only reads the `UiModel`. |
| 2 | START → menu → START on *Run* | `RunCommand` → `session.prepare()` → `Idle → Ready`. The **coordinator** pushed the data screen; the menu did not. GPS shows `GPS ...` until a fix. |
| 3 | Untick *GPS signal* for a moment | Status dot turns red after 3 s (stale fix). When the signal returns, distance catches up through the odometer delta, which is counted once, never twice. Implausible jumps (> 15 m/s) are dropped by the glitch filter. |
| 4 | Speed 11 km/h, START | `Ready → Running`. Timer, distance and pace are live. UP/DOWN switch data pages (screen-local state). |
| 5 | BACK | Lap: toast + short vibration. Show `Lap #1 … ->1` in the trace (one receiver: the overlay). |
| 6 | HR slider to 175 | Zone changes with hysteresis. After 3 s out of the target zone, a **modal** alert appears. |
| 7 | Press BACK on the alert | Alert dismissed, and **no lap taken**: the overlay sits before the screen in the input chain. |
| 8 | Double LIGHT, then press START | Key lock. START is swallowed by the first handler in the chain. Double LIGHT again unlocks. |
| 9 | START (pause), tick *Save fails*, choose Save | `Paused → Paused (save failed)`: the state machine refuses to lose data. Untick and save again → `SAVED`, back to the watch face, CSV written. |
| 10 | Tick *GPS start fails*, start a run | `START FAILED`, and the session stays Idle. Error handling with return values and events, no exceptions. |

Close with the tests: `ctest` → 83 tests, including **zero heap allocations after init** and **tick wrap**. If asked: `--wrap` starts 30 s before the 32-bit tick overflow.

## Questions to expect

| Question | Short answer | Where |
|---|---|---|
| Why the State pattern here but a switch in the gesture recognizer? | State objects pay off when states have different behaviour for many requests plus entry/exit actions. The recognizer has 5 tiny states and one input. | design-decisions §1 |
| How do you avoid a callback calling back into you? | Deferred dispatch, run-to-completion, a re-entrant `dispatch()` is ignored | `event_bus.cpp` |
| What if a listener is destroyed while subscribed? | RAII `Subscription` member, declared last; the bus is declared first in `WatchApp`, so it is destroyed last | `event_bus.h` |
| What happens when the sensor thread is faster than the AP? | Bounded inbox, drop counter visible in the dev panel; batches of 16 per step | `watch_app.cpp` |
| How would this run on an RTOS? | `WatchApp::step` becomes the AP task body blocked on a queue; `IMutex` → RTOS mutex; `postHr` from the driver task | design-decisions §10 |
| Why is `IGpsService` owned by AP and not by the GPS team? | Dependency Inversion: AP states its needs, the adapter maps the vendor API to them | `services.h` |
| How do you test UI without a screen? | `FakeCanvas` records draw calls; screens render from a hand-made `UiModel` | `ui_test.cpp` |
