# Plan for DiscreteMap ABOVE/BELOW Implementation

## Background
The DiscreteMap track currently uses a fixed voltage range of -5V to +5V. The ABOVE and BELOW parameters allow users to define custom voltage windows, enabling more flexible sequencing behavior.

## Two Implementation Approaches

### Option 1: Input Normalization Approach

#### Description
Normalize the input signal relative to the user-defined voltage window, then use the existing threshold detection logic.

#### Implementation Steps
1. Add `RangeHigh` (Above) and `RangeLow` (Below) parameters to DiscreteMapSequence
2. Modify `DiscreteMapTrackEngine::tick()` to normalize input: 
   ```cpp
   float active_window = _sequence->rangeHigh() - _sequence->rangeLow();
   if (abs(active_window) < 0.0001f) active_window = 0.0001f; // Avoid division by zero
   float normalized_input = (_currentInput - _sequence->rangeLow()) / active_window;
   // Convert back to voltage range for threshold comparison
   _currentInput = _sequence->rangeLow() + normalized_input * active_window;
   ```
3. Update threshold calculation functions to consider the new voltage window instead of fixed -5V to +5V

#### Advantages
- Clean mathematical approach that directly implements the specification
- Naturally handles inversion (Below > Above) with signed arithmetic
- Clear separation between input processing and threshold logic

#### Risks
- Changes the fundamental voltage comparison logic
- Could introduce floating-point precision issues
- Requires careful handling of edge cases (active_window near zero)
- Potential compatibility issues with existing sequencing behavior

#### Test Cases
- Default values (-5V, +5V) should behave identically to current implementation
- Custom voltage ranges should scale input appropriately
- Inversion (Below > Above) should reverse sequence direction

### Option 2: Threshold Transformation Approach

#### Description
Keep the input processing unchanged, but transform the calculated threshold voltages based on the user's voltage window.

#### Implementation Steps
1. Add `RangeHigh` (Above) and `RangeLow` (Below) parameters to DiscreteMapSequence
2. Update `getThresholdVoltage()` to apply voltage window transformation:
   ```cpp
   float getThresholdVoltage(int stageIndex) {
       if (_sequence->thresholdMode() == DiscreteMapSequence::ThresholdMode::Position) {
           // Transform from default -5V..+5V range to custom rangeLow..rangeHigh
           float default_voltage = /* calculation from threshold value */;
           float ratio = (default_voltage - (-5.0f)) / 10.0f; // Normalize to 0..1
           return _sequence->rangeLow() + ratio * (_sequence->rangeHigh() - _sequence->rangeLow());
       } else {
           // For Length mode: transform the pre-calculated _lengthThresholds
           float default_voltage = _lengthThresholds[stageIndex];
           float ratio = (default_voltage - (-5.0f)) / 10.0f; // Normalize to 0..1
           return _sequence->rangeLow() + ratio * (_sequence->rangeHigh() - _sequence->rangeLow());
       }
   }
   ```
3. Update `recalculateLengthThresholds()` to work with the new voltage range

#### Advantages
- Preserves existing input processing and threshold crossing logic
- Maintains same threshold comparison algorithm
- Simpler to integrate without disturbing core engine logic
- Less risk of breaking existing functionality

#### Risks
- More complex transformation logic for Length mode thresholds
- Requires changes to both Position and Length mode calculations
- Threshold cache invalidation might need to be more complex
- Potential for errors if transformations not applied consistently

#### Test Cases
- Default values (-5V, +5V) should behave identically to current implementation
- Custom voltage ranges should shift threshold positions appropriately
- Inversion (Below > Above) should affect threshold ordering

## Recommended Approach

**Option 2 (Threshold Transformation)** is recommended because:
- It's less disruptive to the core engine logic
- Maintains compatibility with existing sequencing behavior
- Reduces risk of floating-point precision issues
- Easier to validate against current functionality

## Implementation Sequence
1. Add parameters to DiscreteMapSequence model
2. Implement threshold transformation logic
3. Update UI to expose parameters in list model
4. Add routing support