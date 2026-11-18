# Architecture

## 1. Layers and dependency direction

```mermaid
flowchart TB
    subgraph platform["platform/sim + app/main.cpp  (Windows: threads, heap, OpenCV)"]
        main[main.cpp<br/>composition root]
        canvas[OpenCvCanvas]
        sensors[SensorSimulator<br/>SimGps / SimHeartRate]
        rec[CsvActivityRecorder]
        out[SimVibrator / SimBacklight / SimWallClock / StdMutex]
    end
    subgraph ap["ap/  (portable: no RTTI, no exceptions, no heap after init)"]
        app[WatchApp]
        svc[[services.h / osal.h / canvas.h<br/>interfaces owned by AP]]
    end
    main --> app
    canvas -. implements .-> svc
    sensors -. implements .-> svc
    rec -. implements .-> svc
    out -. implements .-> svc
    app --> svc
```

The arrows only point *into* `ap/`. The AP layer declares the interfaces it needs, and the platform implements them (Dependency Inversion). Porting to the watch means rewriting `platform/` and `main.cpp`, not `ap/`.

## 2. Main classes

```mermaid
classDiagram
    class WatchApp {
        +init(now) bool
        +postKey(KeyEvent) bool
        +postHr(HrSample) bool
        +postGps(GpsSample) bool
        +step(now)
        +render(ICanvas&)
    }
    class EventBus {
        +subscribe(IEventListener&, TopicMask) Subscription
        +publish(Event) bool
        +dispatch(budget) size_t
    }
    class InputRouter {
        +append(IInputHandler&) bool
        +route(GestureEvent) IInputHandler*
    }
    class ActivitySession {
        -State* m_state
        -IdleState / ReadyState / RunningState / PausedState
    }
    class IActivityControl {
        <<interface>>
        prepare / cancel / startPause / lap / save / discard
    }
    class Screen {
        <<abstract>>
        +enter() +exit() +handle() +render()  (non-virtual)
        #onEnter() #onGesture() #onRender()  (hooks)
    }
    class Navigator
    class OverlayManager
    class KeyLockHandler
    class BacklightHandler
    class ActivityFlowCoordinator
    class HrZoneMonitor
    class GestureRecognizer

    WatchApp *-- EventBus
    WatchApp *-- GestureRecognizer
    WatchApp *-- InputRouter
    WatchApp *-- ActivitySession
    WatchApp *-- HrZoneMonitor
    WatchApp *-- Navigator
    WatchApp *-- OverlayManager
    WatchApp *-- ActivityFlowCoordinator
    ActivitySession ..|> IActivityControl
    Navigator ..|> IInputHandler
    OverlayManager ..|> IInputHandler
    KeyLockHandler ..|> IInputHandler
    BacklightHandler ..|> IInputHandler
    InputRouter o-- IInputHandler : chain
    Navigator o-- Screen : stack (borrowed)
    Screen <|-- WatchFaceScreen
    Screen <|-- MenuScreen
    Screen <|-- ActivityDataScreen
    Screen <|-- SettingsScreen
    MenuScreen o-- ICommand
    ActivityDataScreen --> IActivityControl
```

Ownership is plain composition: every AP object is a member of `WatchApp` (`*--`). Screens and handlers are *borrowed* by the navigator and the router (`o--`), so popping a screen never destroys anything.

## 3. Sequence: pressing START while running

```mermaid
sequenceDiagram
    participant K as Key driver (sim)
    participant W as WatchApp
    participant G as GestureRecognizer
    participant B as EventBus
    participant R as InputRouter
    participant D as ActivityDataScreen
    participant S as ActivitySession
    participant F as ActivityFlowCoordinator
    participant N as Navigator
    participant O as OverlayManager

    K->>W: postKey(START Down/Up)  [mutex, inbox]
    W->>G: onKeyEvent (in step)
    G-->>W: GestureEvent START short
    W->>B: publish(Gesture)
    B->>W: onEvent(Gesture)  [dispatch]
    W->>R: route()
    R->>R: KeyLock: no · Overlay: not modal
    R->>N: handle()
    N->>D: handle()
    D->>S: startPause(t)
    S->>S: RunningState::startPause → metrics.pause, transitionTo(Paused)
    S->>B: publish(ActivityStateChanged Running→Paused)
    Note over B: same dispatch() loop, next event
    B->>F: onEvent → Navigator.push(PauseMenu)
    B->>O: onEvent → vibrate(Double)
    W->>W: render(): build UiModel → Navigator.render → Overlay.render
```

Two properties make this predictable:

* **Deferred dispatch.** `publish()` only enqueues. The session finishes its transition before anyone hears about it, and no listener is ever re-entered.
* **Single AP thread.** Only `postKey/postHr/postGps` are called from other threads, and they only touch the mutex-protected inbox.

## 4. Activity state machine

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Ready: prepare / GPS.start()
    Idle --> Idle: prepare fails (StartFailed)
    Ready --> Idle: cancel
    Ready --> Running: startPause / recorder.begin(), metrics.start
    Ready --> Ready: recorder.begin fails (StartFailed)
    Running --> Running: lap / recorder.recordLap
    Running --> Paused: startPause / metrics.pause
    Paused --> Running: startPause / metrics.resume
    Paused --> Idle: save / close last lap, recorder.finish (Saved)
    Paused --> Paused: save fails (SaveFailed)
    Paused --> Idle: discard
    note right of Idle: entry: GPS.stop()  (single place, whatever path led here)
```

## 5. Threads and data flow

| Context | Calls | Touches |
|---|---|---|
| Sensor thread (sim) / ISR (watch) | `postHr`, `postGps` | inbox (mutex) only |
| UI thread in the simulator | `postKey`, `step`, `render` | everything in `ap/` |
| AP inside `step()` | drain inbox → recognizer/bus → timers → 1 s tick → `dispatch()` | single-threaded, run-to-completion |

The inbox is drained in bounded batches: at most 16 messages are copied out under the lock and processed without it, so a slow AP step never blocks a producer. When it is full, messages are counted as dropped and never block the producer, which is the right default for an ISR.

## 6. Input chain

| # | Handler | Consumes | Why here |
|---|---|---|---|
| 1 | `KeyLockHandler` | double LIGHT; everything while locked | must win over every screen |
| 2 | `OverlayManager` | any key while a *modal* alert is up | the alert must be acknowledged before the screen reacts |
| 3 | `Navigator` → top `Screen` | whatever the screen handles; otherwise BACK short pops | screen-specific meaning |
| 4 | `BacklightHandler` | LIGHT short | global fallback |

Adding a feature that intercepts keys (for example "touch lock" or a "workout step prompt") means adding one handler in the right place. No screen changes.
