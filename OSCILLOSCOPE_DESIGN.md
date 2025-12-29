# CV Oscilloscope Design (Based on Asteroids Architecture)

## Overview
Repurpose the Asteroids page structure to create a real-time CV oscilloscope for monitoring track outputs.

## File Structure (Following Asteroids Pattern)

```
src/apps/sequencer/
├── oscilloscope/
│   ├── Oscilloscope.h       # Core oscilloscope logic (like Asteroids.h)
│   └── Oscilloscope.cpp     # Implementation (like Asteroids.cpp)
└── ui/pages/
    ├── OscilloscopePage.h   # Page wrapper (like AsteroidsPage.h)
    └── OscilloscopePage.cpp # UI integration (like AsteroidsPage.cpp)
```

## Core Components

### 1. **OscilloscopePage** (Page Wrapper)
```cpp
class OscilloscopePage : public BasePage {
public:
    OscilloscopePage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;
    virtual void draw(Canvas &canvas) override;
    virtual bool isModal() const override { return true; }

    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    oscilloscope::Display _display;
    uint32_t _lastTicks;
    int _selectedChannel = 0;  // Which CV output to monitor (0-7)
};
```

**Key Methods:**
- `enter()`: Does NOT suspend engine (need it running to see CV!)
- `draw()`: Read `_engine.cvOutput().channel(_selectedChannel)` each frame
- `keyPress()`: F1-F4 select channel, Encoder scrolls channel, Shift+Step15 exits
- `encoder()`: Adjust timebase/voltage scale

### 2. **oscilloscope::Display** (Core Logic)
```cpp
namespace oscilloscope {

class Display {
public:
    static constexpr int BufferSize = 256;  // Full screen width
    static constexpr int ScreenHeight = 64;

    void reset();
    void update(float dt, float voltage);
    void draw(Canvas &canvas, int selectedChannel);

    void setTimebase(float secondsPerDiv);  // 1ms, 10ms, 100ms, 1s per division
    void setVoltageScale(float voltsPerDiv); // ±1V, ±5V, ±10V ranges

    float getMin() const { return _min; }
    float getMax() const { return _max; }
    float getPeakToPeak() const { return _max - _min; }

private:
    std::array<float, BufferSize> _buffer;  // Circular buffer of voltage samples
    int _writeIndex = 0;
    float _min = 0.f;
    float _max = 0.f;
    float _timebase = 0.01f;  // seconds per division (10ms default)
    float _voltageScale = 5.f; // volts per division (±5V default)
    float _sampleAccumulator = 0.f;
};

} // namespace oscilloscope
```

## Implementation Details

### **1. Sample Capture (in Display::update)**
```cpp
void Display::update(float dt, float voltage) {
    _sampleAccumulator += dt;

    // Sample at appropriate rate based on timebase
    float sampleInterval = (_timebase * 8.f) / BufferSize; // 8 divisions across screen

    if (_sampleAccumulator >= sampleInterval) {
        _sampleAccumulator -= sampleInterval;

        // Write to circular buffer
        _buffer[_writeIndex] = voltage;
        _writeIndex = (_writeIndex + 1) % BufferSize;

        // Track min/max for measurements
        _min = std::min(_min, voltage);
        _max = std::max(_max, voltage);
    }
}
```

### **2. Drawing Waveform (in Display::draw)**
```cpp
void Display::draw(Canvas &canvas, int selectedChannel) {
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::None);
    canvas.fill();

    canvas.setBlendMode(BlendMode::Add);
    canvas.setColor(Color::Medium);

    // Draw graticule (8 horizontal divisions, 10 vertical)
    drawGraticule(canvas);

    // Draw waveform
    canvas.setColor(Color::Bright);

    const int scopeY = ScreenHeight / 2;
    const float voltageToY = ScreenHeight / (2.f * _voltageScale);

    int lastX = 0;
    int lastY = scopeY;

    for (int x = 0; x < BufferSize; ++x) {
        int idx = (_writeIndex + x) % BufferSize;
        float voltage = _buffer[idx];

        // Convert voltage to Y coordinate
        int y = scopeY - static_cast<int>(voltage * voltageToY);
        y = clamp(y, 0, ScreenHeight - 1);

        // Draw line segment
        canvas.line(lastX, lastY, x, y);
        lastX = x;
        lastY = y;
    }

    // Draw HUD
    drawHUD(canvas, selectedChannel);
}
```

### **3. Graticule Drawing**
```cpp
void Display::drawGraticule(Canvas &canvas) {
    // Horizontal center line (0V)
    canvas.setColor(Color::Low);
    canvas.hline(0, ScreenHeight / 2, 256);

    // Vertical divisions (32 pixels apart = 8 divisions)
    for (int x = 32; x < 256; x += 32) {
        for (int y = 0; y < ScreenHeight; y += 2) {
            canvas.point(x, y);
        }
    }

    // Horizontal divisions
    for (int y = 16; y < ScreenHeight; y += 16) {
        for (int x = 0; x < 256; x += 2) {
            canvas.point(x, y);
        }
    }
}
```

### **4. HUD Display**
```cpp
void Display::drawHUD(Canvas &canvas, int selectedChannel) {
    canvas.setFont(Font::Tiny);
    canvas.setColor(Color::Bright);

    // Channel indicator
    FixedStringBuilder<16> ch("CH%d", selectedChannel + 1);
    canvas.drawText(2, 2, ch);

    // Voltage scale
    FixedStringBuilder<16> vScale("%.1fV/div", _voltageScale);
    canvas.drawText(2, 10, vScale);

    // Timebase
    FixedStringBuilder<16> tBase("%.0fms", _timebase * 1000.f);
    canvas.drawText(2, 18, tBase);

    // Measurements (top right)
    FixedStringBuilder<16> vpp("Vpp: %.2fV", getPeakToPeak());
    canvas.drawText(256 - canvas.textWidth(vpp) - 2, 2, vpp);

    FixedStringBuilder<16> minMax("Min/Max: %.2f/%.2fV", _min, _max);
    canvas.drawText(256 - canvas.textWidth(minMax) - 2, 10, minMax);
}
```

## Key Differences from Asteroids

| Aspect | Asteroids | Oscilloscope |
|--------|-----------|--------------|
| **Engine** | Suspended | Running (need CV output!) |
| **Update Loop** | Game logic (physics, collisions) | Sample capture only |
| **Drawing** | Vector shapes (asteroids, ship) | Line graph (waveform) |
| **Inputs** | CV inputs control game | Encoder/keys control view |
| **Outputs** | Gates mirror game events | No gate output (read-only) |
| **Memory** | Pools for entities | Fixed circular buffer |

## CV Source Options

Pick one of these as the scope source (and document it in the UI):

1. **Selected track output (pre-rotation)**
   - Use the selected track engine directly: `engine.selectedTrackEngine().cvOutput(0)`.
   - Pros: shows the track's true output without CV rotation.
   - Cons: not necessarily what appears on a physical CV jack.

2. **Physical CV output channel**
   - Use `engine.cvOutput().channel(index)` and let the user select channel 1-8.
   - Pros: matches the actual jack (including CV rotation).
   - Cons: may not correspond to the selected track if rotation is active.

3. **Selected track mapped to physical output**
   - Reproduce the rotation mapping used in `Engine::updateTrackOutputs()` to
     resolve the selected track to an output channel, then scope that channel.
   - Pros: "what you hear on the jack" for the selected track.
   - Cons: slightly more logic.

## Sampling Notes

- Do **not** suspend the engine (unlike Asteroids), otherwise CV output freezes.
- Sampling should be driven by the `dt` accumulator, not by LCD refresh alone.
- If you want a "lagging" view, keep the last full buffer and render it after
  it completes (stable scope sweep).

## User Controls

### **Key Mappings:**
- **F0**: Channel 1-2
- **F1**: Channel 3-4
- **F2**: Channel 5-6
- **F3**: Channel 7-8
- **Encoder**: Adjust timebase (rotate), voltage scale (shift+rotate)
- **Shift+Step15**: Exit to previous page

### **Display Modes (Future):**
1. **Single**: One channel full screen
2. **Dual**: Two channels overlaid
3. **XY**: Channel X vs Channel Y (Lissajous)
4. **FFT**: Frequency spectrum (if CPU allows)

## Performance Considerations

- **Sample Rate**: ~50 FPS draw = 50 samples/sec max (adequate for CV monitoring)
- **Buffer Size**: 256 samples = 5.12 seconds at 50 Hz
- **CPU Impact**: Minimal - just reading floats and drawing lines
- **Memory**: 256 floats × 4 bytes = 1KB (negligible)

## Integration with Existing UI

Register in PageManager (like Asteroids):
```cpp
case ContextAction::EnterOscilloscope:
    _manager.push<OscilloscopePage>();
    break;
```

Add menu entry in appropriate page (e.g., SystemPage or hidden in StartupPage like Asteroids).

## Future Enhancements

1. **Trigger Modes**: Edge, level, auto
2. **Cursor Measurements**: Voltage/time cursors
3. **Persistence**: Hold previous traces
4. **Recording**: Save waveform to file
5. **Multi-channel**: Show all 8 channels stacked

---

## Conclusion

The Asteroids architecture provides an **excellent template** for creating a CV oscilloscope:
- Modal page pattern handles UI isolation
- Vector graphics engine is perfect for waveforms
- Delta-time update loop supports flexible sampling
- Minimal code reuse needed - mostly new logic in `oscilloscope::Display`

**Estimated Implementation**:
- Core Display class: ~200 lines
- Page wrapper: ~100 lines
- Total: ~300 lines of new code + registration

This is a **highly feasible** project that would add significant debugging/monitoring value to the firmware!
