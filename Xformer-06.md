# Xformer-06b Release Notes

## 0. General

Added Wallclock reference, so freerunning tracks should not drift even 1 tick apart in 71 minutes.

## 1. Changes to Pre-Existing Tracks


### Note (Circus) Track

*   **New Play Modes:**
    *   **Ikra:** Features a separate **Note Loop** (`Note First/Last Step`) running independently of the rhythm sequence with its own divisor (`Divisor Y`).
    *   **Re:Rene:** Cartesian sequencer (X/Y) with independent divisors and **XY Seek Mode** to keep the playhead within defined step bounds.
    
**Accumulator Enhancements**

*   **Trigger Modes:** **STEP**, **GATE**, and **RTRIG** (Retrigger) modes.
*   **Conditional Logic:** Accumulator evaluations now respect per-step conditions.


### Discrete Map Track

*   **Pluck Effect:** Decay envelope applied to CV output on gate fire, featuring adjustable depth, direction, and random **Jitter**. (creating a detune effect)
*   **Linear Slew:** Constant-rate slew limiting for smooth voltage transitions.
*   **Monitor Mode:** Added to both layers for easier stage/pitch auditing.

### Indexed Track

*   **Step Count:** Increased to **48 steps**.
*   **Gate and Duration**: set to same scale of 0 to 1023
*   **Monitor Mode:** Added to the Note layer.
*   **Initialization:** Defaults to **Free** play mode.



---

## 2. New Functionality

### CV Router Engine
*   **4x4 Matrix Routing:** Independently maps 4 inputs to 4 outputs at a 1000Hz control rate.
*   **Scanning & Routing:**
    *   **Scan:** Crossfades between the 4 selected input sources (CV In, Bus, or Off).
    *   **Route:** Distributes the scanned signal across 4 selected destinations (CV Out, Bus, or None) using weighted distribution.
*   **Integration:** Controllable via `CVR Scan` and `CVR Route` targets in the Routing system.

### Routing System

*   **4 Bus Routes with Shapers:** Full shaper support (Crease, Envelope, Follower, etc.) for internal Bus targets.

---

## 3. T9type Track

Integration of the Monome Teletype scripting language into the Performer environment. No Grid, No Usb Keyboard support. Edit the scripts with performers buttons. 

Well it is techinically a track, but as soon as you assign it to a slot it becomes a relationship between 2 systems - Westlicht and Teletype can immedeately become fused or stay separate, or anywhere inbetween. 


### Some New & Enhanced Ops
*   **WBPM / WBPM.S:** System BPM control.
*   **WMS / WTU / BAR:** Converting Performers time and bar phase to teletype-readable numbers.
*   **WP / WP.SET / WR / W.ACT / WNG:** managing other tracks patterns and steps, Performers Transport state.
*   **RT:** Read Route source values directly into scripts.
*   **M.A / M.ACT.A / M.RESET.A:** Global metronome control.
*   **E.* , LFO.* , G.* ,** - Moduelators.

