# Canary Map Editor - Architecture

**Version:** v3.9.14+
**Established:** 2025-12-07
**Status:** Production - Event-Driven Rendering Model

---

## Core Principles

The Canary Map Editor is an **event-driven document editor**, not a continuous-rendering application.

### 1. Event-Driven Rendering

**Rendering occurs ONLY when:**
- User input changes state (mouse, keyboard, wheel)
- Explicit state changes (zoom, floor, mode switch)
- Optional AnimationTimer active (100ms interval, stoppable)

**No continuous render loop exists.** Idle = 0 CPU, 0 OnPaint calls.

### 2. Event Compression is Mandatory

**Mechanism:**
- `is_rendering` flag blocks new paint events during OnPaint()
- `render_pending` flag coalesces multiple Refresh() calls into single deferred refresh
- Input coalescing (e.g., `pending_zoom_delta`) accumulates events, applies in one frame

**Purpose:** Prevent input flooding (Linux generates 100+ wheel events per scroll gesture).

**Result:** OnPaint() frequency << Input event frequency (by design).

### 3. Visual Presentation vs Application Redraws

**Two-Layer Model:**

```
Layer 1: Application Redraws (OnPaint)
  - Frequency: ~4-5 Hz idle, 10-60 Hz active
  - Measured by: "Redraws" telemetry counter
  - Controlled by: Event compression

Layer 2: Visual Presentation (GPU)
  - Frequency: ~60 Hz (VSync enforced by compositor)
  - Controlled by: SwapBuffers() blocking + OS compositor
  - What users see
```

**Critical:** Redraws ≠ Visual FPS. Low redraw count is EFFICIENT, not SLOW.

### 4. VSync Delegation

**The application does NOT manage frame timing.**

- SwapBuffers() blocks until vertical refresh (compositor enforced)
- VSync handled by wxWidgets + OpenGL driver + compositor
- Display rate independent of OnPaint() rate

No explicit VSync request in code—relies on platform defaults (60 Hz typical).

---

## Key Subsystems

### Event Compression State Machine

**Location:** `source/map_display.cpp:171-184` (Refresh), `214-346` (OnPaint)

```cpp
void MapCanvas::Refresh() {
    if (is_rendering) {
        render_pending = true;  // Coalesce
        return;
    }
    // Throttle Update(), always call Refresh()
    if (refresh_watch.Time() > HARD_REFRESH_RATE) {
        wxGLCanvas::Update();  // Immediate paint
    }
    wxGLCanvas::Refresh();  // Queued paint
}
```

**Flags:**
- `is_rendering`: OnPaint() executing (blocks new paints)
- `render_pending`: Refresh() requested during render (defer via CallAfter)

### Input Coalescing Pattern

**Example:** Wheel zoom (`source/map_display.cpp:1860`)

```cpp
pending_zoom_delta += wheel_rotation;  // Accumulate
Refresh();  // Request paint (may be coalesced)

// In OnPaint():
zoom += pending_zoom_delta;  // Apply accumulated delta
pending_zoom_delta = 0.0;    // Reset
```

**Benefit:** 100+ wheel events → 1 zoom application → smooth, no lag.

### AnimationTimer

**Location:** `source/map_display.cpp:3013-3024`

**Interval:** 100ms (10 Hz max)

**Active when:**
- `show_preview` enabled OR
- `position_indicator_time > 0`

**Condition:** Only refreshes if `zoom <= 2.0`

**Behavior:** Calls Refresh() every 100ms (subject to event compression).

**Purpose:** Provide baseline rendering for animations/previews without continuous loop.

---

## Telemetry

### "Redraws" Counter

**Measures:** OnPaint() invocations per second (application-level redraw events)

**Does NOT measure:** Visual frame rate, GPU presentation rate, display refresh

**Location:** `source/map_display.cpp:314-335` (Linux only)

**Typical values:**
- Idle: 4-5 Hz
- Active input: 10-20 Hz
- Animation: ~10 Hz (AnimationTimer limited)

**Visual FPS:** ~60 Hz (VSync enforced), independent of Redraws counter.

### Texture Binds Counter

**Measures:** Actual `glBindTexture()` calls per frame (GPU state changes)

**Purpose:** Diagnose texture caching efficiency, GPU state overhead

**Typical value:** 4000-5000 binds/frame

---

## Performance Assumptions

### Hardcoded Parameters

| Parameter | Value | Scope | Purpose |
|-----------|-------|-------|---------|
| **HARD_REFRESH_RATE** | 16ms | Throttles `Update()` only | Prevent GPU stalls from immediate paint spam |
| **AnimationTimer** | 100ms | Baseline refresh rate | Animations/previews without continuous loop |
| **VSync** | ~60 Hz | Implicit (OS/driver) | Display timing, enforced by SwapBuffers() |

**IMPORTANT:**
- `HARD_REFRESH_RATE = 16ms` does NOT enforce 60 FPS rendering.
- It only limits `Update()` (immediate paint) frequency.
- `Refresh()` (queued paint) is ALWAYS called, subject to event compression.

---

## Development Constraints

### DO: Respect Event-Driven Model

✅ **Call Refresh() to trigger rendering** - Expect it may be coalesced/deferred
✅ **Use pending_* pattern for input accumulation** - Apply in OnPaint()
✅ **Make timers optional** - Stop when idle, use coarse intervals (100ms+)
✅ **Label telemetry accurately** - "Redraws" not "FPS" for app-level metrics
✅ **Assume VSync exists** - Don't manage frame timing explicitly

### DON'T: Fight Event Compression

❌ **No polling timers at high frequency** - Creates continuous loop, defeats compression
❌ **No frame rate enforcement** - VSync handles display timing
❌ **No forced Update() spam** - Use Refresh(), let compression work
❌ **No "60 FPS target" language** - Unless explicitly referring to visual presentation
❌ **No assumptions about immediate Refresh()** - Event compression may defer

---

## Platform-Specific Adaptations

### Linux (GTK) Specifics

**Event Compression:** Mandatory on Linux due to input event flooding (100+ wheel events/gesture).

**Direct Update() Calls:**
`source/map_display.cpp:602, 617` - Force immediate paint during rubber-band selection for smooth visual feedback. Platform-specific departure from pure event-driven (acceptable for UX parity).

**Telemetry:** Redraws counter only compiled on Linux (`#ifdef __LINUX__`).

---

## Architectural Evolution

### v3.9.13 - Critical Performance Breakthrough

**Changes:**
- Event compression introduced (`is_rendering`/`render_pending`)
- Input coalescing for zoom (`pending_zoom_delta`)
- Z-axis occlusion culling (75% overdraw reduction)

**Result:** Eliminated 8-second input lag, reduced CPU/GPU load.

### v3.9.14 - Telemetry Semantic Correction

**Change:** Renamed "FPS" → "Redraws" in StatusBar telemetry

**Rationale:** Clarify that counter measures OnPaint() frequency (4-5 Hz), not visual frame rate (60 Hz).

**Result:** Honest telemetry aligned with event-driven architecture.

---

## Future Development Guidelines

### Adding Features

**When adding rendering logic:**
- Call Refresh() to request paint
- Expect coalescing (multiple calls → single paint)
- Apply state changes in OnPaint(), not input handlers

**When adding timers:**
- Make stoppable (no mandatory background loops)
- Use coarse intervals (100ms+ preferred)
- Check state before calling Refresh() (zoom, preview mode, etc.)

**When adding telemetry:**
- Distinguish app-level metrics (Redraws) from GPU metrics (texture binds, draw calls)
- Provide tooltips/documentation for technical metrics
- Never label app-level counters "FPS" without clarification

### Prohibited Patterns

**Anti-patterns that break event-driven model:**
- Continuous render loops (`while(true) { Refresh(); Sleep(16); }`)
- High-frequency polling timers (16ms, 8ms)
- Forced Update() calls everywhere (bypasses event queue)
- Frame rate caps at application level (VSync owns timing)

---

## Verification

### How to Validate Event-Driven Behavior

**Idle test:** No user input → 0% CPU, 0 OnPaint/sec
**Input test:** Rapid wheel scroll → Low Redraws count (compression working)
**Visual test:** Smooth 60 Hz presentation despite low Redraws count
**Animation test:** AnimationTimer stops when preview disabled

### Expected Telemetry

**Idle:**
- Redraws: 0-1 Hz
- CPU: 0%

**Active editing:**
- Redraws: 4-20 Hz (depends on input frequency)
- Texture Binds: 4000-5000/frame
- Visual: 60 Hz (user perception)

**Animation active:**
- Redraws: ~10 Hz (AnimationTimer baseline)
- Visual: 60 Hz (VSync enforced)

---

## References

**Detailed Analysis:**
- `ARCHITECTURAL_SYNTHESIS_v3.9.14.md` - Comprehensive subsystem analysis
- `FPS_TELEMETRY_ANALYSIS.md` - Telemetry semantic deep dive
- `LINUX_PORT_AUDIT.md` - Platform-specific changes log

**Key Source Locations:**
- `source/map_display.cpp:171-346` - Event compression + OnPaint
- `source/map_display.cpp:3013-3024` - AnimationTimer
- `source/settings.cpp:287` - HARD_REFRESH_RATE default
- `source/definitions.h:27` - Version number

---

## Conclusion

**The Canary Map Editor is event-driven by design.**

Low OnPaint frequency (4-5 Hz) is proof of efficient event compression, not poor performance. Visual presentation (60 Hz) is decoupled from application redraws via VSync and double-buffering.

Future development must respect this architecture: render on state change, coalesce events, delegate timing to VSync.

This is not an optimization—it is the execution model.
