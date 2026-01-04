# Porting Telex Operations to Native Teletype

This document outlines how to port the advanced functionality of the Telex extenders (TXo/TXi) to the native Teletype hardware. It provides the original Telex C++ logic and the corresponding C implementation strategy for Teletype's `main.c` and `ops/hardware.c`.

## 1. Trigger Extensions (TR)

Telex adds Metronomes, Dividers, and Toggling to standard Triggers.

### Feature: Pulse Divider (`TR.DIV`)
**Telex Logic (`TriggerOutput.cpp`):**
```cpp
void TriggerOutput::Pulse() {
  if (_divide) {
    if (++_counter >= _division)
      _counter = 0;
    else
      return; 
  }
  // ... perform pulse ...
}
```

**Teletype Port (`module/main.c`):**
Add `div_counter` and `div_target` to `scene_state.variables` or a new struct.

```c
// In tele_tr_pulse (teletype/module/main.c)
void tele_tr_pulse(uint8_t i, int16_t time) {
    // Check divider
    if (scene_state.variables.tr_div[i] > 1) {
        scene_state.tr_div_counter[i]++;
        if (scene_state.tr_div_counter[i] < scene_state.variables.tr_div[i]) {
            return; // Skip pulse
        }
        scene_state.tr_div_counter[i] = 0; // Reset
    }
    
    // Original logic (using configured pulse time if not provided)
    if (time <= 0) time = scene_state.variables.tr_time[i]; 
    
    if (i >= TR_COUNT) return;
    timer_remove(&trPulseTimer[i]);
    timer_add(&trPulseTimer[i], time, &trPulseTimer_callback, (void*)(int32_t)i);
}
```

### Feature: Pulse Length Configuration (`TR.TIME`)
Telex allows setting the default pulse length for each trigger.

**Telex Logic (`TriggerOutput.cpp`):**
```cpp
void TriggerOutput::SetTime(int value, short format){
  _widthMode = false;
  _pulseTime = TxHelper::ConvertMs(value, format);
}
```

**Teletype Port:**
Teletype already has `TR.TIME`, but we can ensure it supports the extended time formats (seconds, minutes).

```c
// ops/hardware.c
static void op_TR_TIME_set(...) {
    int16_t a = cs_pop(cs);
    int16_t b = cs_pop(cs);
    // ... validation ...
    
    // Check for extended format (assuming we add TR.TIME.S / M)
    // or just assume MS for base op
    ss->variables.tr_time[a] = b;
}
```

### Feature: Pulse Width (`TR.WIDTH`)
Sets pulse length as a percentage of the metronome interval.

**Telex Logic (`TriggerOutput.cpp`):**
```cpp
void TriggerOutput::SetWidth(int value){
  _widthMode = true;
  _width = constrain(value, 0, 100);
  _pulseTime = _metroInterval * _width / 100.;
}
```

**Teletype Port:**
Requires tracking `metro_interval` per output.

```c
// In tele_tr_pulse or metronome update logic
if (scene_state.variables.tr_width_mode[i]) {
    int32_t width_pct = scene_state.variables.tr_width[i];
    int32_t interval = tr_metro[i].interval_ticks;
    int32_t time = (interval * width_pct) / 100;
    scene_state.variables.tr_time[i] = time;
}
```

### Feature: Metronomes (`TR.M`)
Telex implements per-output metronomes that run independently, with support for multipliers and counts.

**Telex Logic (`TriggerOutput.cpp`):**
```cpp
void FASTRUN TriggerOutput::Update(unsigned long currentTime){
  // ...
  // evaluate pinging the metro event
  if (_metro && currentTime >= _nextEvent){

    if (_multiply){

      if (_multiplyCount == 0){
        if (_metroCount == 0 || (_metroCount > 0 && --_actualCount > 0)){
          // set the next reference beat (avoids divisionn drift)
          _nextNormal = _nextNormal + _metroInterval;
          // copy over any new values
          _multiplyInterval = _tempMultiplyInterval;
          _multiplication = _tempMultiplication;
          // reset multiplication counter
          _multiplyCount = 0;
        } else {
          // we have beat for the expected count - disable the metro
          _metro = false;
        }
      }

      if (++_multiplyCount < _multiplication) {
        // set the next event to the multiply interval
        _nextEvent = _nextEvent + _multiplyInterval;
      } else {
        // set the next event to the metro interval (normal) and reset count
        _nextEvent = _nextNormal;
        _multiplyCount = 0;
      }

    } else {

      // we are just doing basic metronomes
      if (_metroCount == 0 || (_metroCount > 0 && --_actualCount > 0)){
        _nextEvent = _nextNormal + _metroInterval;
        _nextNormal = _nextEvent;
      } else
        _metro = false;

    }

    Pulse();
  }
}
```

**Teletype Port:**
Teletype has a main system timer. We can add a check in `handler_EventTimer` or a new dedicated timer.

```c
// In teletype/module/main.c

// New struct for Metronome state
typedef struct {
    bool active;
    uint32_t interval_ticks;
    uint32_t next_tick;
    uint16_t count; // For TR.M.COUNT
} metro_t;

metro_t tr_metro[4];

// In tele_tick (called every ~1ms)
void tele_tick(...) {
    uint32_t now = get_ticks();
    
    for (int i=0; i<4; i++) {
        if (tr_metro[i].active && now >= tr_metro[i].next_tick) {
            tr_metro[i].next_tick += tr_metro[i].interval_ticks;
            tele_tr_pulse(i, scene_state.variables.tr_time[i]);
            
            // Handle Count
            if (tr_metro[i].count > 0) {
                tr_metro[i].count--;
                if (tr_metro[i].count == 0) tr_metro[i].active = false;
            }
        }
    }
}
```

### Feature: Toggle Mode (`TR.TOG`)
**Telex Logic:**
```cpp
void TriggerOutput::ToggleState(){
  _toggle = MAXTIME;
  SetState(!_state);
}
```

**Teletype Port:**
Already partially exists, but can be formalized in `ops/hardware.c`.
```c
// ops/hardware.c
static void op_TR_TOG_get(const void *data, scene_state_t *ss, ...) {
    int16_t a = cs_pop(cs) - 1;
    if (a < 4) {
        bool new_state = !ss->variables.tr[a];
        ss->variables.tr[a] = new_state;
        // Include timing aspect for proper toggle behavior
        ss->variables.tr_toggle[a] = MAXTIME; // Assuming MAXTIME is defined
        tele_tr(a, new_state);
    }
}
```

## 2. CV Extensions (CV)

Telex adds Slew modes, Logarithmic scaling, and Quantization.

### Feature: Slew Time Base (`CV.SLEW.S`, `CV.SLEW.M`, `CV.SLEW.BPM`)
Telex allows setting slew in Seconds (.S), Minutes (.M), and Beats Per Minute (BPM) in addition to Milliseconds.

**Telex Logic (`TxHelper.cpp`):**
```cpp
unsigned long TxHelper::ConvertMs(unsigned long ms, short format){
  switch(format){
    case 1: ms *= 1000; break; // Seconds
    case 2: ms *= 60000; break; // Minutes
    case 3: ms = 60000 / ms; break; // BPM (Beats Per Minute)
  }
  return ms;
}
```

**Teletype Port:**
Add new Ops that multiply the input before calling `tele_cv_slew`.

```c
// ops/hardware.c

// CV.SLEW.S n s
static void op_CV_SLEW_S_set(...) {
    int16_t n = cs_pop(cs) - 1;
    int16_t s = cs_pop(cs);
    // Teletype native slew is somewhat arbitrary units roughly related to ms
    // We need to scale `s` * 1000 to get ms, then apply standard slew math
    tele_cv_slew(n, s * 1000); 
}
```

### Feature: Logarithmic Scaling (`CV.LOG`)
**Telex Logic (`CVOutput.cpp`):**
```cpp
void FASTRUN CVOutput::UpdateDAC(int value){
  // do log translation
  if (_doLog){
    if (value < 0){
      value *= -1;
      _wasNg = true;
    } else {
      _wasNg = false;
    }
    value = ExpTable[constrain(value << _logRange, 0 , 32768)];
    if (_wasNg) value *= -1;
    value = value >> _logRange;
  }

  // invert for DAC circuit
  if (_oscilMode)
    value = (int)(value * (_oscillator->Oscillate() / 32768.));

  // added the conditional write only if the CV value changes
  if (value != _cvHelper){
    _cvHelper = value;
    _dac.writeChannel(_output, (_dacCenter - _cvHelper));
  }
}
```

**Teletype Port (`module/main.c`):**
Requires importing the `ExpTable` array.

```c
// In cvTimer_callback (teletype/module/main.c)
// Apply before calibration
if (scene_state.variables.cv_log[hardware_index]) {
    int32_t val = aout[software_index].now;
    // Map 0-16383 to ExpTable range and lookup
    val = ExpTable[val]; // Simplified lookup
    aout[software_index].now = val;
}
```

## 3. Input Extensions (IN / PARAM)

Telex inputs feature built-in quantization.

### Feature: Quantization (`IN.QT`, `PARAM.QT`)
**Telex Logic (`Quantizer.cpp`):**
```cpp
QuantizeResponse Quantizer::Quantize(int in) {

  QuantizeResponse response;

  // deal with negative values
  in = in < 0 ? abs(in) : in;

  // short circuit if we are within our current bounary
  if (in >= _below && in < _above){
    return _last;
  }

  // if not - we need to find where we are in the list
  // hints are a type of skip-list that jumps us to the proper octave
  _octave = (int)(in / 1638.3) - 1;
  _octave = _octave > 0 ? _octave : 0;

  _index = hints[_scale][_octave];

  int distance = 32768;
  int distanceTemp = 0;

  // find where we can't get any closer then back out
  while (_index < notecount[_scale]) {

    distanceTemp = abs(in - scales[_scale][_index]);

    if (distanceTemp > distance){
      _index--;
      break;
    } else {
      // else increment
      distance = distanceTemp;
      _index++;
    }

  }

  // move it down to the next to last if we made it to the end
  if (_index >= notecount[_scale])
    _index = notecount[_scale] - 1;

  // use index to quantize
  _current = scales[_scale][_index];

  // set the response
  response.Note = _index;
  response.Value = _current;

  _last = response;

  // now set the helper boundries
  _above = _index < notecount[_scale] - 1 ? scales[_scale][_index + 1] : 32767;
  _below = _index > 0 ? scales[_scale][_index-1] : scales[_scale][_index];

  // similar to a mutable quantize trick to expand the region slightly
  // had been doing this using floats - thx for the fixed math tip oliver
  _above = ((13 * _current) + (19 * _above)) >> 5;
  _below = ((13 * _current) + (19 * _below)) >> 5;

  // constrain the above and below values
  _above = _above > 32767 ? 32767 : _above;
  _below = _below < 0 ? 0 : _below;

  return response;

}
```

**Teletype Port:**
Teletype already has `scales.h`. We just need to apply it to the input reading.

```c
// ops/hardware.c
static void op_IN_QT_get(...) {
    // 1. Read raw input
    int16_t raw = tele_get_in(index);
    
    // 2. Quantize using existing TT scale functions
    // (Assuming scale 0 is active, or add IN.SCALE variable)
    int16_t scale_idx = scene_state.variables.in_scale[index];
    int16_t quantized = quantize(raw, scale_idx); 
    
    cs_push(cs, quantized);
}
```

## 4. Envelope Operations (ENV)

This is a new module for Teletype, ported from `CVOutput.cpp`.

### Data Structure (`teletype/module/main.c`)
Add to `aout_t` or `scene_state`.

```c
typedef enum { ENV_IDLE, ENV_ATTACK, ENV_DECAY } env_state_t;

typedef struct {
    // Existing fields...
    
    // New ENV fields
    env_state_t env_state;
    uint16_t env_attack_ms;
    uint16_t env_decay_ms;
    uint16_t env_target_val; // The "peak" voltage
    bool env_loop;
} aout_ext_t;
```

### Ops (`ops/hardware.c`)

```c
// ENV.ATT n ms
static void op_ENV_ATT_set(...) {
    int16_t n = cs_pop(cs) - 1;
    int16_t ms = cs_pop(cs);
    aout[n].env_attack_ms = ms;
    // Calculate slew rate: (Target / (ms / TickRate))
}

// ENV.TRIG n
static void op_ENV_TRIG_set(...) {
    int16_t n = cs_pop(cs) - 1;
    aout[n].env_state = ENV_ATTACK;
    aout[n].target = aout[n].env_target_val;
    
    // Set Slew to Attack Rate
    tele_cv_slew(n, calc_slew_from_ms(aout[n].env_attack_ms)); 
}
```

### Timer Logic (`cvTimer_callback` in `module/main.c`)

```c
void cvTimer_callback(void* o) {
    for (int i = 0; i < 4; i++) {
        // Standard CV Slew Logic...
        // ...

        // ENV State Machine
        if (aout[i].step == 0) { // Slew finished
            if (aout[i].env_state == ENV_ATTACK) {
                // Attack done, switch to Decay
                aout[i].env_state = ENV_DECAY;
                aout[i].target = 0; // Or Sustain/Offset
                tele_cv_slew(i, calc_slew_from_ms(aout[i].env_decay_ms));
                // Trigger new slew
                // ...
            } else if (aout[i].env_state == ENV_DECAY) {
                if (aout[i].env_loop) {
                    // Loop back to Attack
                    aout[i].env_state = ENV_ATTACK;
                    aout[i].target = aout[i].env_target_val;
                    tele_cv_slew(i, calc_slew_from_ms(aout[i].env_attack_ms));
                } else {
                    aout[i].env_state = ENV_IDLE;
                }
            }
        }
    }
}

// Additional Envelope Operations

### Feature: Envelope Triggering
**Telex Logic (`CVOutput.cpp`):**
```cpp
void CVOutput::TriggerEnvelope(){

  if (_envelopeMode) {

    if (_decaying) {

      // retrigger the envelope by going to zero/offset first
      _slew = CalculateRawSlew(RETRIGGERMS, _lOffset, _current);
      _target = _lOffset;
      _retrigger = true;

    } else {

      _current = _lOffset;
      _target = _envTarget;
      _slew = _attackSlew;

    }
    if (!_envLoop && _loopTimes != 1){
      _loopCount = 0;
      _envLoop = true;
    }
    _envelopeActive = true;
  }

}
```

### Feature: Envelope Attack/Decay Configuration
**Telex Logic (`CVOutput.cpp`):**
```cpp
void CVOutput::SetAttack(int att, short format){
  _attack = TxHelper::ConvertMs(max(att, 1), format);
  _attackSlew = CalculateRawSlew(_attack, _envTarget, _lOffset);
  if (_envelopeActive && !_decaying){
    SlewSteps tempSlew = CalculateRawSlew(_attack, _envTarget, _current);
    tempSlew.Steps = (_envTarget - _current) / tempSlew.Delta;
    _slew = tempSlew;
  }
}

void CVOutput::SetDecay(int dec, short format){
  _decay = TxHelper::ConvertMs(max(dec, 1), format);
  _decaySlew = CalculateRawSlew(_decay, _lOffset, _envTarget);
  if (!_envelopeActive && _decaying){
    SlewSteps tempSlew = CalculateRawSlew(_decay, _lOffset, _current);
    tempSlew.Steps = (_lOffset - _current) / tempSlew.Delta;
    _slew = tempSlew;
  }
}
```

### Feature: Envelope Looping
**Telex Logic (`CVOutput.cpp`):**
```cpp
void CVOutput::SetLoop(int loopEnv){
  _loopTimes = max(loopEnv, 0);
  _infLoop = _loopTimes == 0;
}
```

### Feature: Envelope EOR/EOC Triggers
**Telex Logic (`CVOutput.cpp`):**
```cpp
void CVOutput::SetEOR(int trNumber){
  if (_triggerOutputCount > 0 && trNumber >= 0 && trNumber < _triggerOutputCount){
    _triggerForEOR = trNumber;
    _triggerEOR = true;
  } else {
    _triggerEOR = false;
  }
}

void CVOutput::SetEOC(int trNumber){
  if (_triggerOutputCount > 0 && trNumber >= 0 && trNumber < _triggerOutputCount){
    _triggerForEOC = trNumber;
    _triggerEOC = true;
  } else {
    _triggerEOC = false;
  }
}
```
