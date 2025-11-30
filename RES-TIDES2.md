# Mutable Instruments Tides v2 Modulator Functionality Analysis

This document provides a comprehensive overview of the Tides v2's modulator functionality, based on an analysis of the module's firmware.

### Core Architecture

The signal generation is primarily handled by the `PolySlopeGenerator` class (defined in `tides2/poly_slope_generator.h`). This class is responsible for generating four related signals and sending them to the outputs. The main processing loop in `tides2/tides.cc` continuously calls `poly_slope_generator.Process()` to generate new samples.

The generation process can be broken down into these main stages:

1.  **Ramp Generation:** A foundational phase ramp is created by the `RampGenerator` class. The nature of this ramp is determined by the selected **`RampMode`** and **`Range`**.
2.  **Shaping:** The basic ramp is then processed by a series of shapers that apply curvature (`SLOPE`), asymmetry (`SHAPE`), and wave-folding/smoothing (`SMOOTHNESS`).
3.  **Output Mapping:** The final shaped waveform is then distributed to the four outputs according to the selected **`OutputMode`** and the `SHIFT/LEVEL` parameter, which controls the relationship between the outputs.

### Modes of Operation (`RampMode`)

The fundamental behavior of the generator is controlled by the `RampMode`, which is defined in `tides2/ramp_generator.h`. The user cycles through these modes using the mode button.

*   **`RAMP_MODE_AD` (Attack-Decay Envelope):**
    *   This is a one-shot envelope mode. A rising edge on the `TRIG` input starts the attack phase, and a falling edge (or the completion of the attack phase if the gate is short) starts the decay phase.
    *   In the code, the `RampGenerator`'s `Step` function checks the state of the gate input and transitions the internal phase accordingly.
*   **`RAMP_MODE_AR` (Attack-Release Envelope):**
    *   This mode creates an envelope that rises as long as the gate at the `TRIG` input is high and falls when the gate is low.
    *   The implementation is similar to `RAMP_MODE_AD`, but the phase directly follows the gate signal's state.
*   **`RAMP_MODE_LOOPING` (Cyclic LFO/VCO):**
    *   The generator runs continuously, creating a looping LFO. The `TRIG` input can be used to reset the phase of the LFO.
    *   The `Step` function in `RampGenerator` will continuously increment the phase unless a trigger is detected.

There is also a hidden `RAMP_MODE_HIDDEN_SEQUENCE` which is not elaborated here.

### Output Modes (`OutputMode`)

The `OutputMode`, also defined in `tides2/ramp_generator.h`, determines what signal is sent to each of the four outputs. The `SHIFT/LEVEL` parameter modifies the relationship between the outputs in each mode.

*   **`OUTPUT_MODE_GATES` (Different timings):**
    *   This is the default mode. The four outputs produce signals with related but shifted timings.
    *   The `SHIFT/LEVEL` parameter controls the amount of phase shift between the outputs. At 12 o'clock, the outputs are perfectly in phase. Turning the knob clockwise or counter-clockwise shifts the phases of outputs 2, 3, and 4 relative to output 1.
    *   In `poly_slope_generator.h`, the `RenderInternal` function calculates these phase shifts based on the `shift` parameter.
*   **`OUTPUT_MODE_AMPLITUDE` (Different amplitudes):**
    *   All outputs have the same phase and frequency, but their amplitudes are different.
    *   The `SHIFT/LEVEL` knob acts as a multi-channel VCA and panner. When the knob is at 12 o'clock, all outputs are at maximum amplitude. Turning it CCW fades outputs 4, 3, 2, and 1 in sequence. Turning it CW fades outputs 1, 2, 3, and 4.
    *   The code achieves this by applying a gain factor to each output based on the `shift` value.
*   **`OUTPUT_MODE_FREQUENCY` (Different frequencies):**
    *   The outputs are at different frequencies, which are harmonically or melodically related to the main frequency set by the `FREQUENCY` knob.
    *   The `SHIFT/LEVEL` parameter selects the frequency ratio for each output from a set of tables defined in `tides2/poly_slope_generator.cc` (`audio_ratio_table_` and `control_ratio_table_`). This allows the module to generate chords or polyrhythms.
*   **`OUTPUT_MODE_PHASE` (Different shapes):**
    *   The outputs have different shapes, created by phase-shifting and combining different points of the main waveform.
    *   The `SHIFT/LEVEL` parameter controls the amount of phase distortion, creating a wide variety of waveshapes. This is implemented in the `RampWaveshaper` class.

### Core Parameters and their Implementation

The feel and sound of the generated waveforms are controlled by several parameters, which are read from the physical controls and CV inputs by the `CvReader` class (in `tides2/cv_reader.cc`).

*   **`FREQUENCY`:**
    *   Controls the main frequency of the LFO or the duration of the envelope.
    *   The `CvReader` reads the potentiometer and CV input and converts them to a value that is used by the `RampGenerator` to determine the phase increment per sample.
*   **`SHAPE`:**
    *   Controls the asymmetry of the waveform. At 12 o'clock, the waveform is symmetrical (e.g., a triangle wave). Turning it CCW makes the rising slope faster and the falling slope slower. Turning it CW has the opposite effect.
    *   This is implemented in `poly_slope_generator.h` by applying a shaping function to the raw phase ramp, effectively bending it.
*   **`SLOPE` (or `BALANCE` in AD mode):**
    *   Controls the curvature of the waveform, making it more linear or exponential.
    *   This is implemented by another shaping function that is applied to the ramp, changing its contour from linear to logarithmic or exponential.
*   **`SMOOTHNESS`:**
    *   This parameter has a dual function. From 0 to 50% it acts as a low-pass filter, smoothing the waveform. From 50% to 100% it acts as a wave-folder, adding harmonics.
    *   The filtering is done by a simple one-pole low-pass filter. The wave-folding is implemented by a mathematical function that folds the waveform back on itself.
