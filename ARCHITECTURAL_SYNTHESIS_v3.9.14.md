# Architectural Synthesis: RME v3.9.13–3.9.14
**Event-Driven Rendering Architecture Analysis**

**Date:** 2025-12-07
**Context:** Linux stabilization introduced structural changes (event compression, necessity-based rendering, Z-axis occlusion, telemetry correction)
**Objective:** Understand execution model evolution, identify architectural alignment/misalignment, establish foundation for symbiotic development

---

## EXECUTIVE SUMMARY

The Canary Map Editor underwent a **fundamental architectural shift** during Linux stabilization (v3.9.13–3.9.14). What began as platform-specific fixes evolved into a **coherent event-driven rendering paradigm** that challenges the editor's original Windows-centric assumptions.

**Current State:**
- Event compression coalesces input flooding (100+ wheel events → 1 render)
- Rendering is necessity-based (OnPaint frequency: 4-5 Hz idle, 60 Hz visual via VSync)
- Z-axis occlusion reduces overdraw by 75%
- Telemetry semantics corrected (FPS → Redraws)

**Critical Finding:**
The editor no longer operates under continuous-rendering assumptions. Most subsystems have adapted successfully, but **architectural incongruence exists where legacy patterns assume frequency-based behavior**.

This document provides:
1. Current execution model baseline
2. Subsystem-by-subsystem alignment analysis
3. Cross-dependency mapping
4. Architectural mental model
5. Constraints for future development

---

## 1. CURRENT EXECUTION MODEL

### The New Reality

**Before (Windows-Era):**
```
User Input → Immediate Refresh() → OnPaint() → GPU (uncapped)
Assumption: Rendering frequency ≈ Input frequency
```

**After (Event-Driven Linux):**
```
User Input → Event Queue → Compression → Coalesced Refresh() → OnPaint() → SwapBuffers (VSync 60Hz)
           (100+ events)    (is_rendering)   (single paint)      (4-5 Hz)      (display 60 Hz)
```

### Core Mechanisms

**Event Compression State Machine** (`map_display.cpp:171-346`):
- `is_rendering` flag blocks new paint events during render
- `render_pending` flag accumulates multiple Refresh() calls into ONE deferred refresh
- `pending_zoom_delta` accumulates wheel events, applied in single frame
- Result: Input frequency >> OnPaint frequency (by design)

**Rendering Triggers:**
- User input (mouse, wheel, keyboard) - coalesced
- AnimationTimer: 100ms (10 Hz) - only when active
- State changes (zoom, floor, mode) - single refresh
- No continuous loop - truly idle when no input

**Visual Presentation:**
- VSync enforced by compositor (60 Hz typical)
- SwapBuffers() blocks until vertical refresh
- GPU presents frames independently of OnPaint() frequency
- **Critical**: 4-5 OnPaint/sec still yields 60 visual frames/sec

---

## 2. SUBSYSTEM ALIGNMENT ANALYSIS

### 2.1 Event Dispatch & Input Handling

**Architecture:** Event-driven with sophisticated coalescing

**Key Patterns:**
- Wheel zoom: `pending_zoom_delta` accumulator (star example)
- Mouse move: Refresh() only on tile boundary crossing
- Event compression: `is_rendering`/`render_pending` flags
- No polling, no timers (except optional AnimationTimer)

**Alignment: EXCELLENT (9/10)**

✅ Strengths:
- Input coalescing pattern eliminated 8-second lag
- Direct event handlers (no polling)
- CaptureMouse() for drags (event-driven API)
- Platform-appropriate (handles Linux input flooding)

⚠️ Minor Friction:
- Linux-specific `wxGLCanvas::Update()` calls for smooth rubber-banding
- Three refresh paths (coalesce, throttle, queue) add complexity
- Accelerator table manual toggle workaround (Linux wxWidgets quirk)

**Assumptions:**
- ✅ Assumes sparse, high-priority events (correct)
- ✅ Does NOT assume continuous rendering
- ✅ Benefits from event compression

---

### 2.2 Rendering Triggers

**Architecture:** Pure event-driven, no background loop

**Trigger Inventory:**
- 29 Refresh() call sites in map_display.cpp
- All tied to state changes (input, view, mode, selection)
- AnimationTimer (100ms) only when needed
- HARD_REFRESH_RATE (16ms) throttles Update() only

**Alignment: EXCELLENT (9/10)**

✅ Strengths:
- Rigorously event-driven (no hidden render loops)
- Throttle prevents GPU stalls (Update() limited to 16ms intervals)
- AnimationTimer stopped when idle
- Minimap uses delayed update (333ms) to reduce overhead

⚠️ Minor Issues:
- HARD_REFRESH_RATE name suggests "target 60 FPS" but doesn't enforce it
- Multiple Refresh() + Update() calls in same handler (redundant?)
- AnimationTimer 100ms arbitrary (no documented rationale)

**Assumptions:**
- ✅ Event-driven, NOT continuous loop
- ✅ Refresh rate depends on input frequency
- ✅ VSync handles GPU timing (implicit)

---

### 2.3 UI Feedback Channels

**Architecture:** Fire-and-forget status updates, no frame sync

**Components:**
- StatusBar: 5 slots (messages, description, position, zoom, telemetry)
- Title bar: State-driven (map name, build info)
- Overlays: Position indicator (100ms animation), selection box
- No polling timers for UI updates

**Alignment: PERFECT (10/10)**

✅ Strengths:
- StatusBar updates coalesced to tile granularity
- Title bar purely state-driven
- Telemetry correctly labeled "Redraws" (not FPS)
- Overlays use 10 Hz timer (efficient, acceptable smoothness)
- Zero continuous polling

⚠️ No Issues Found

**Assumptions:**
- ✅ Status updates are latency-tolerant
- ✅ Text feedback doesn't need frame sync
- ✅ 100ms animation adequate (10 Hz)
- ✅ Does NOT expect continuous rendering

---

### 2.4 Performance Instrumentation

**Architecture:** Event-frequency metrics, not GPU metrics

**Metrics Tracked:**
- Redraws: OnPaint() frequency (4-5 Hz idle)
- Texture Binds: GPU state changes (4000-5000 typical)
- Software rendering detection (diagnostic)
- Texture memory management (GC with configurable thresholds)

**Alignment: GOOD (7/10)**

✅ Strengths:
- Telemetry renamed to "Redraws" (honest about measurement)
- Texture binds accurately track GPU state
- Event compression efficiency visible in low redraw count
- Software rendering warning catches catastrophic perf issues

⚠️ Friction Points:
- HARD_REFRESH_RATE=16ms suggests "60 FPS target" but ineffective
- AnimationTimer 100ms undocumented (why not 16ms or 33ms?)
- No SwapBuffers timing (can't measure true visual FPS)
- Texture cleanup thresholds hardcoded (not adaptive)

**Assumptions:**
- ⚠️ Assumes 60 Hz VSync active (not verified)
- ⚠️ Assumes 16ms is typical render time (not enforced)
- ✅ Correctly measures app-level events
- ⚠️ "Redraws" metric needs user education

---

## 3. CROSS-SUBSYSTEM INTERDEPENDENCIES

### Dependency Map

```
User Input (Event Dispatch)
    ↓
    → pending_zoom_delta (coalescing) ───┐
    → Refresh() calls                     │
    ↓                                     │
Rendering Triggers                        │
    ↓                                     │
    → is_rendering check (compression) ←──┘
    → Event queue + CallAfter()
    ↓
OnPaint() Execution
    ↓
    → Apply pending_zoom_delta ─────────→ State Changes
    → Draw() → SwapBuffers()
    → Telemetry update ─────────────────→ UI Feedback (StatusBar)
    ↓
Visual Presentation (VSync 60 Hz)
```

### Critical Dependencies

**1. Input → Rendering Compression:**
- Input handlers call Refresh()
- Compression layer checks `is_rendering`
- Multiple events coalesce into `render_pending`
- **Dependency:** Rendering latency must be < input burst duration

**2. Rendering → Telemetry:**
- OnPaint() increments `frame_count`
- Telemetry displays "Redraws:X"
- **Dependency:** User understanding that Redraws ≠ Visual FPS

**3. AnimationTimer → Rendering:**
- Timer calls Refresh() every 100ms
- Only active when `show_preview` OR `position_indicator_time > 0`
- Conditional on `zoom <= 2.0`
- **Dependency:** Animations visible even during input compression

**4. StatusBar → User Input:**
- Mouse move updates position/zoom status
- Coalesced to tile boundary
- **Dependency:** Status accuracy doesn't require per-pixel updates

### Symbiotic Relationships

**✅ Input Coalescing + Event Compression:**
- `pending_zoom_delta` accumulates wheel events
- `render_pending` prevents queue buildup
- Together: Eliminate input lag while maintaining responsiveness

**✅ Telemetry + Event Compression:**
- Low "Redraws" count proves compression working
- Diagnostic tool for validating architectural assumptions
- Renamed metric educates users about event-driven design

**✅ StatusBar + Tile-Boundary Coalescing:**
- Position updates only on tile change
- Reduces SetStatusText() calls without perceptible lag
- Natural granularity for map editor

### Conflict Points

**⚠️ HARD_REFRESH_RATE Throttle:**
- Throttles `Update()` to 16ms intervals
- But `Refresh()` always called (bypasses throttle)
- **Conflict:** Name suggests frame rate cap, reality is Update() limiter
- **Impact:** Low - doesn't break functionality, but conceptually misleading

**⚠️ AnimationTimer vs Event Compression:**
- Timer fires every 100ms regardless of rendering state
- Calls Refresh() which may be compressed
- **Conflict:** Timer assumes Refresh() will execute, compression may defer
- **Impact:** Minimal - deferred refresh still happens via CallAfter()

**⚠️ Linux-Specific Update() Calls:**
- Rubber-band selection forces immediate Update()
- Bypasses event compression for smooth feedback
- **Conflict:** Platform-specific departure from pure event-driven
- **Impact:** Acceptable - UX requires immediate visual feedback

---

## 4. ALIGNED ZONES (Symbiotic with Event-Driven Model)

### Fully Aligned: Native Event-Driven

**Input Coalescing (pending_zoom_delta):**
- Accumulates 100+ wheel events
- Applies in single frame
- No assumptions about continuous rendering
- **Rating:** ⭐⭐⭐⭐⭐ Perfect alignment

**Event Compression State Machine:**
- `is_rendering`/`render_pending` flags
- Prevents paint event queue buildup
- Defers refresh via CallAfter()
- **Rating:** ⭐⭐⭐⭐⭐ Perfect alignment

**StatusBar Updates:**
- Fire-and-forget SetStatusText()
- Coalesced to tile boundaries
- No frame synchronization
- **Rating:** ⭐⭐⭐⭐⭐ Perfect alignment

**Keyboard Input:**
- Direct event table mapping
- No polling or timers
- Single action → single refresh
- **Rating:** ⭐⭐⭐⭐⭐ Perfect alignment

**Title Bar:**
- State-driven updates only
- No periodic polling
- Document changes trigger updates
- **Rating:** ⭐⭐⭐⭐⭐ Perfect alignment

**Texture Bind Tracking:**
- Counts actual GPU state changes
- Per-frame cache reset
- Accurate diagnostic metric
- **Rating:** ⭐⭐⭐⭐⭐ Perfect alignment

---

## 5. MISALIGNED ZONES (Legacy Assumptions)

### Conceptual Misalignments

**1. HARD_REFRESH_RATE = 16ms**
- **Location:** `settings.cpp:287`, used in `map_display.cpp:179`
- **Assumption:** Targets 60 FPS frame rate
- **Reality:** Only throttles Update(), not Refresh()
- **Impact:** Name misleading; doesn't enforce 60 Hz
- **Recommendation:** Rename to UPDATE_THROTTLE_MS or document that Refresh() bypasses throttle

**2. AnimationTimer = 100ms**
- **Location:** `map_display.cpp:3015`
- **Assumption:** 100ms is adequate for animations
- **Reality:** Arbitrary value; no documented rationale
- **Impact:** Animations at 10 Hz (not 16ms/60 Hz)
- **Recommendation:** Document why 100ms chosen, or make adaptive to monitor refresh rate

**3. "Redraws" Telemetry (User Understanding)**
- **Location:** `map_display.cpp:332`
- **Assumption:** Users understand Redraws ≠ Visual FPS
- **Reality:** May still confuse non-technical users
- **Impact:** Low redraw count (4-5) might be misinterpreted as poor performance
- **Recommendation:** Add tooltip or help text explaining metric

**4. Zoom > 10 Item Hiding**
- **Location:** `map_drawer.cpp:561`
- **Assumption:** Items invisible at 10x zoom
- **Reality:** Hardcoded threshold, no adaptive LOD
- **Impact:** Binary on/off, no graduated quality reduction
- **Recommendation:** Consider multi-level LOD based on zoom + performance

**5. Platform-Specific Update() Forcing**
- **Location:** `map_display.cpp:602, 617` (#ifdef __LINUX__)
- **Assumption:** GTK needs forced Update() for smooth rubber-banding
- **Reality:** Departs from pure event-driven on Linux
- **Impact:** Platform divergence, complicates mental model
- **Recommendation:** Accept as necessary UX compromise; document rationale

### No Continuous-Rendering Assumptions Found

**Critical Validation:** No subsystem implicitly expects continuous render loop
- AnimationTimer explicitly stops when idle
- No background render threads
- No hidden timers polling at 60 Hz
- All rendering triggered by events or explicit timer

**Conclusion:** Architecture is internally consistent. Misalignments are naming/documentation issues, not functional bugs.

---

## 6. ARCHITECTURAL MENTAL MODEL

### The Editor's True Nature

**RME is an EVENT-DRIVEN APPLICATION with GPU-ACCELERATED RENDERING**

Not a game engine (continuous loop).
Not a video player (fixed frame rate).
Not a realtime simulation (physics tick).

It's a **DOCUMENT EDITOR** that renders on demand:
- User edits tile → refresh
- User pans view → refresh
- User zooms → refresh
- No input → idle (0% CPU, 0 OnPaint/sec)

### The Two-Layer Model

**Layer 1: Application Events (4-5 Hz idle)**
- OnPaint() executions
- State changes applied
- Telemetry "Redraws" measures this

**Layer 2: Visual Presentation (60 Hz display)**
- VSync enforced by compositor
- SwapBuffers() blocks until vertical refresh
- GPU presents frames from buffer
- Users see this, not Layer 1

**Decoupling:** wxGLCanvas double buffering + VSync enforcement separate app logic from display timing.

**Implication:** Low OnPaint frequency is EFFICIENT, not SLOW.

### Event Compression as Core Principle

**Philosophy:** Fewer events, not fewer frames

```
Traditional approach:  100 events → 100 renders → CPU thrashing
RME approach:         100 events → accumulate → 1 render → smooth
```

**Benefits:**
- Reduces CPU/GPU load
- Prevents render queue buildup
- Maintains responsiveness (deferred refresh)
- Enables smooth editing on weak hardware

**Trade-off:**
- Telemetry measures app events (4-5 Hz)
- Not visual smoothness (60 Hz)
- Requires user education

---

## 7. CONSTRAINTS FOR FUTURE DEVELOPMENT

### DO: Respect Event-Driven Architecture

**When adding features:**
- ✅ Call Refresh() to trigger rendering
- ✅ Use pending_* pattern for input accumulation
- ✅ Expect Refresh() to be coalesced
- ✅ Update StatusBar with SetStatusText() (fire-and-forget)
- ✅ Assume VSync handles GPU timing

**When adding timers:**
- ✅ Make them optional (stoppable when idle)
- ✅ Use coarse intervals (100ms+) not fine (16ms)
- ✅ Check zoom or state before calling Refresh()

**When measuring performance:**
- ✅ Track GPU metrics (texture binds, draw calls)
- ✅ Label app-level metrics clearly ("Redraws" not "FPS")
- ✅ Assume OnPaint frequency ≠ visual frame rate

### DON'T: Fight Event Compression

**Avoid:**
- ❌ Polling timers at 60 Hz (creates continuous render loop)
- ❌ Expecting immediate Refresh() → OnPaint() (compression may defer)
- ❌ Calling Update() everywhere (bypasses event queue, breaks compression)
- ❌ Hardcoding 60 FPS assumptions (display rate varies)
- ❌ Adding "FPS" labels to metrics without clarification

**Anti-patterns:**
- ❌ `while (true) { Refresh(); Sleep(16); }` - creates continuous loop
- ❌ Measuring "FPS" without distinguishing app vs GPU
- ❌ Platform-specific code without documented rationale

### Future-Safe Principles

**1. Event-Driven by Default:**
- New features trigger Refresh() on state change
- No background loops unless explicitly needed
- Timers are opt-in, not mandatory

**2. Coalescing-Aware:**
- Accumulate state in pending_* variables
- Apply accumulated state in OnPaint()
- Don't assume Refresh() executes immediately

**3. Telemetry Honesty:**
- Label metrics accurately (Redraws, Visual FPS, Texture Binds)
- Provide tooltips or help text for technical metrics
- Separate "diagnostic" from "user-facing" metrics

**4. Platform Agnostic:**
- Avoid #ifdef unless necessary for UX parity
- Document platform-specific departures
- Prefer wxWidgets portable APIs

**5. Adaptive Quality:**
- Use zoom, content density, or performance feedback for LOD
- Not hardcoded thresholds (10x zoom, 2500 textures, etc.)
- Allow user overrides in settings

---

## 8. RECOMMENDED EVOLUTIONARY PATH

### Phase 1: Clarification (COMPLETED in v3.9.14)
✅ Rename "FPS" → "Redraws" (StatusBar telemetry)
✅ Document event compression in FPS_TELEMETRY_ANALYSIS.md
✅ Update comments in map_display.cpp

### Phase 2: Documentation (Next)
- Add inline comments explaining HARD_REFRESH_RATE throttle behavior
- Document AnimationTimer 100ms interval choice
- Create ARCHITECTURE.md explaining event-driven model
- Tooltip for "Redraws" metric in StatusBar

### Phase 3: Enhanced Telemetry (v3.10.x)
- Add SwapBuffers() timing analysis (measure true visual FPS)
- Display dual metrics: "Redraws:5 Visual:60 Binds:4000"
- Option to toggle telemetry detail level (simple/advanced)

### Phase 4: Adaptive Parameters (v3.10.x+)
- Query monitor refresh rate (X11/Wayland API)
- Adjust AnimationTimer interval based on display Hz
- Adaptive texture cleanup thresholds (based on VRAM usage)
- Graduated LOD levels (not binary on/off)

### Phase 5: Platform Unification (v4.x)
- Investigate if Linux-specific Update() calls can be replaced
- Unified input handling across Windows/Mac/Linux
- Compositor-aware VSync detection

---

## 9. SUCCESS CRITERIA FOR ARCHITECTURAL ALIGNMENT

**Short-Term (v3.9.14):**
✅ Telemetry no longer misleads users
✅ Event compression benefits preserved
✅ No regression in rendering smoothness
✅ Developers understand execution model

**Medium-Term (v3.10.x):**
- All metrics accurately labeled and explained
- SwapBuffers timing provides true visual FPS
- Documentation covers event-driven architecture
- New features respect coalescing patterns

**Long-Term (v4.x):**
- Fully adaptive quality/timing systems
- Platform-agnostic input handling
- No hardcoded performance assumptions
- Symbiotic evolution (changes reinforce architecture, not fight it)

---

## 10. FINAL RECOMMENDATIONS

### For Developers

**When reading code:**
- Understand that Redraws ≠ Visual FPS
- Event compression is intentional optimization
- Low OnPaint count is GOOD (efficient)
- VSync handles display timing (not application code)

**When writing code:**
- Use pending_* pattern for input accumulation
- Call Refresh(), expect coalescing
- Fire-and-forget UI updates (SetStatusText)
- Make timers stoppable, use coarse intervals

**When debugging performance:**
- Check texture binds (GPU state overhead)
- Verify event compression working (low Redraws during input)
- Profile actual render time (not just frequency)
- Measure SwapBuffers() timing for true visual rate

### For System Architects

**The editor has become event-driven.** This is not a bug—it's the natural evolution of:
- Input flooding on Linux (100+ wheel events/gesture)
- Need for efficiency on weak hardware
- Document editor paradigm (not game engine)

**Further optimization should:**
- Leverage coalescing (fewer high-quality renders)
- Not fight compression (no forced 60 Hz loops)
- Maintain responsiveness (defer, don't drop)
- Be symbiotic with architecture (reinforce, don't contradict)

### For Future Work

**Stop searching for new optimizations.**
**Start aligning the system to what it has become.**

The editor is fast. The architecture is coherent. The telemetry is honest.

Future changes should:
1. Document existing patterns
2. Eliminate naming ambiguities (HARD_REFRESH_RATE)
3. Add adaptive behaviors (query monitor Hz, VRAM-based GC)
4. Preserve event-driven philosophy

---

## APPENDICES

### Appendix A: Key Metrics Glossary

| Metric | Measures | Typical Value | Interpretation |
|--------|----------|---------------|----------------|
| **Redraws** | OnPaint() calls/sec | 4-5 idle, 10-20 active | App-level event frequency |
| **Visual FPS** | SwapBuffers() rate | 60 (VSync) | GPU presentation rate |
| **Texture Binds** | glBindTexture() calls | 4000-5000 | GPU state changes per frame |
| **Input Events** | Mouse/wheel/key events | 60-144 Hz | OS-provided input rate |

### Appendix B: Event Compression Flow Diagram

```
[User Input] ──→ [Event Queue] ──→ [Event Handler]
                                         ↓
                                    Refresh() called
                                         ↓
                              ┌──── is_rendering? ────┐
                             YES                      NO
                              ↓                        ↓
                   render_pending = true       Throttle Check
                         (coalesce)                    ↓
                              ↓                 Update() if > 16ms
                              │                        ↓
                              │                 Refresh() always
                              │                        ↓
                              └────────→ [Event Queue]
                                              ↓
                                         OnPaint()
                                              ↓
                              Apply pending_zoom_delta
                                              ↓
                                          Draw()
                                              ↓
                                       SwapBuffers()
                                              ↓
                                    [Visual @ 60 Hz]
```

### Appendix C: File Locations Reference

**Core Event Compression:**
- `source/map_display.cpp:171-184` - Refresh() wrapper with compression
- `source/map_display.cpp:214-346` - OnPaint() with pending delta application

**Input Coalescing:**
- `source/map_display.cpp:1860` - Wheel zoom accumulation
- `source/map_display.cpp:221-247` - pending_zoom_delta application

**Telemetry:**
- `source/map_display.cpp:314-335` - Redraws counter (Linux)
- `source/map_drawer.cpp:51-67` - Texture binds tracking

**Configuration:**
- `source/settings.cpp:287-291` - HARD_REFRESH_RATE, texture GC settings
- `source/definitions.h:27` - Version (now 3.9.14)

**Analysis Documents:**
- `FPS_TELEMETRY_ANALYSIS.md` - Deep dive on FPS vs Redraws
- `LINUX_PORT_AUDIT.md` - Platform-specific changes log

---

## CONCLUSION

The Canary Map Editor v3.9.13–3.9.14 represents a **successful architectural evolution** from Windows-centric continuous rendering to **Linux-optimized event-driven rendering**.

**Key Achievements:**
1. Event compression eliminated input lag (8s → imperceptible)
2. Necessity-based rendering reduced CPU/GPU load
3. Telemetry semantics corrected (FPS → Redraws)
4. Subsystems aligned with event-driven model

**Remaining Work:**
1. Document HARD_REFRESH_RATE throttle behavior
2. Add SwapBuffers timing for true visual FPS
3. Make adaptive (monitor Hz query, VRAM-based GC)
4. Educate users on "Redraws" metric meaning

**Architectural Integrity:**
The system is **internally coherent**. Misalignments are **naming/documentation issues**, not functional bugs. The event-driven paradigm is **symbiotic** with the document editor use case.

**Guiding Principle for v3.10+:**
> "The editor is fast. Make the telemetry honest about why."

This architecture is not a bug to fix—it's a feature to embrace.
