# TDD Methodology - PEW|FORMER Project Conventions

**Date**: 2025-11-18
**Purpose**: Document TDD practices, conventions, and lessons learned from implementation work

---

## Overview

The PEW|FORMER project follows strict **Test-Driven Development (TDD)** methodology for all feature implementations and bug fixes. This document captures the conventions, patterns, and key considerations discovered during development.

---

## Core TDD Cycle

### 1. RED → GREEN → REFACTOR

**RED**: Write a failing test that exposes the bug or validates the new feature
- Test should fail for the right reason
- Verify the test actually runs (avoid false passes)
- Keep tests focused and minimal

**GREEN**: Write minimal code to make the test pass
- Don't over-engineer
- Hard-code if necessary to pass the test
- Add only what's needed for this specific test

**REFACTOR**: Clean up code while keeping tests green
- Remove duplication
- Improve naming
- Extract methods
- Optimize algorithms

### 2. Iterative Approach

Build features **incrementally**:
1. Start with simplest possible test case
2. Add one test at a time
3. Implement minimal code to pass
4. Refactor before moving to next test
5. Repeat until feature complete

---

## Test Framework: UnitTest.h

### Framework Structure

PEW|FORMER uses a **custom lightweight test framework** (`src/test/UnitTest.h`), NOT external frameworks like Catch2 or Google Test.

**Basic Structure**:
```cpp
#include "UnitTest.h"

UNIT_TEST("TestName") {

CASE("test case description") {
    // Arrange
    int expected = 5;

    // Act
    int actual = myFunction();

    // Assert
    expectEqual(actual, expected, "values should match");
}

CASE("another test case") {
    // ...
}

} // End UNIT_TEST
```

### Key Macros

**Test Declaration**:
```cpp
UNIT_TEST("TestName")  // Defines a test suite
```

**Test Cases**:
```cpp
CASE("description")    // Individual test case within suite
```

**Assertions**:
```cpp
expectEqual(a, b, "message")       // Assert a == b
expectTrue(condition, "message")   // Assert condition is true
expectFalse(condition, "message")  // Assert condition is false
```

**Important**: NO `REQUIRE()`, `CHECK()`, `SECTION()` - those are Catch2/Catch, NOT UnitTest.h!

---

## Type Safety Considerations

### The expectEqual() Template

The `expectEqual()` macro uses a **template that requires exact type matches**:

```cpp
template<typename T>
static void expectEqual(T a, T b, const Location &location, const char *msg = nullptr) {
    // ...
}
```

**Key Insight**: Both arguments must be the **exact same type** or the template will fail to match.

### Common Type Mismatches

#### 1. uint8_t vs unsigned int

**Problem**: `uint8_t` (unsigned char) doesn't match `unsigned int` literals

```cpp
// ❌ WRONG - Type mismatch
uint8_t value = 5;
expectEqual(value, 5u, "should be 5");  // Error: uint8_t vs unsigned int
```

**Solution**: Cast to match types

```cpp
// ✅ CORRECT
uint8_t value = 5;
expectEqual((unsigned int)value, 5u, "should be 5");
```

#### 2. int16_t vs int

**Problem**: `int16_t` (short) doesn't match `int` literals

```cpp
// ❌ WRONG - Type mismatch
int16_t currentValue = accumulator.currentValue();  // Returns int16_t
expectEqual(currentValue, 0, "should be 0");  // Error: int16_t vs int
```

**Solution**: Cast to int

```cpp
// ✅ CORRECT
expectEqual((int)accumulator.currentValue(), 0, "should be 0");
```

#### 3. bool is fine

Booleans work without casting:

```cpp
// ✅ CORRECT
bool enabled = true;
expectEqual(enabled, true, "should be enabled");
expectTrue(enabled, "should be enabled");  // Even better!
```

### Type Casting Best Practices

**Pattern**: Cast the **variable**, not the literal

```cpp
// ✅ GOOD - Cast variable to match literal type
expectEqual((int)value, 42, "message");
expectEqual((unsigned int)id, 0u, "message");

// ❌ AVOID - Confusing to cast literals
expectEqual(value, (int16_t)42, "message");
```

**Rationale**: Casting the variable makes intent clearer and matches the literal's natural type.

---

## Naming Conventions

### Test File Naming

**Pattern**: `Test<ComponentName>.cpp`

Examples:
- `TestGateStruct.cpp` - Tests for Gate struct
- `TestRTrigAccumTicking.cpp` - Tests for RTRIG accumulator ticking
- `TestRTrigEdgeCases.cpp` - Tests for edge cases
- `TestAccumulator.cpp` - Tests for Accumulator class

### Test Case Naming

**Pattern**: Descriptive sentence fragments in lowercase

```cpp
CASE("basic gate fields")
CASE("experimental shouldTickAccumulator field")
CASE("gate metadata logic - RTRIG mode with accumulator enabled")
CASE("sequence validation - null sequence handling")
```

**Guidelines**:
- Use lowercase
- Use spaces (not underscores or camelCase)
- Be specific about what's being tested
- Include context when testing modes/configurations

### Assertion Message Naming

**Pattern**: Describe the expected outcome in present tense

```cpp
expectEqual(value, 5, "value should be 5");
expectTrue(enabled, "accumulator should be enabled");
expectFalse(condition, "condition should be false");
```

**NOT**:
```cpp
// ❌ Avoid past tense
expectEqual(value, 5, "value was set to 5");

// ❌ Avoid implementation details
expectEqual(value, 5, "setValue() was called");

// ✅ Focus on expected state
expectEqual(value, 5, "value should be 5");
```

---

## API Discovery Patterns

### Don't Assume Method Names

When working with existing classes, **always check the actual API**:

```cpp
// ❌ WRONG - Assumed API
accumulator.setMin(0);        // Doesn't exist!
accumulator.setMax(10);       // Doesn't exist!
accumulator.value();          // Doesn't exist!

// ✅ CORRECT - Actual API (discovered via header inspection)
accumulator.setMinValue(0);
accumulator.setMaxValue(10);
accumulator.currentValue();
```

**Lesson Learned**: When encountering compilation errors about missing methods:
1. Read the class header file
2. Check actual method signatures
3. Don't trust naming conventions from other languages/frameworks

### Method Name Patterns Observed

**Setters**:
- `setValue()` - Simple value setter
- `setMinValue()` / `setMaxValue()` - Explicit value setters
- `setEnabled()` / `setDirection()` - Boolean/enum setters

**Getters**:
- `value()` - Common but not universal
- `currentValue()` - More explicit variant
- `enabled()` / `direction()` - Direct property names (no "get" prefix)

**Takeaway**: This project avoids "get" prefixes and uses explicit names.

---

## Feature Flag Testing

### Pattern: Conditional Compilation

Tests must work with **both flag states** (0 and 1):

```cpp
CASE("basic functionality") {
    // This test runs regardless of flag state
    Gate gate = { 100, true };
    expectEqual(gate.tick, 100u, "tick should be 100");
}

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
CASE("experimental feature") {
    // This test only runs when flag=1
    Gate gate = { 100, true, true, 0 };
    expectEqual(gate.shouldTickAccumulator, true, "should tick");
}
#endif
```

### Backward Compatibility Testing

**Critical**: Tests must pass with **flag=0** (stable mode):

```cpp
// Test counts change based on flag:
// flag=0: 4 basic tests run
// flag=1: 9 tests run (4 basic + 5 experimental)
```

**Verification Process**:
1. Run tests with flag=1 → all experimental features tested
2. Change flag to 0
3. Run tests again → only basic features tested
4. Verify **zero test failures** in either mode

---

## Common Pitfalls & Solutions

### 1. Wrong Test Framework

**Problem**: Using Catch2 syntax in UnitTest.h project

```cpp
// ❌ WRONG - Catch2 syntax
#include "catch.hpp"
TEST_CASE("description") {
    SECTION("subsection") {
        REQUIRE(value == 5);
    }
}
```

**Solution**: Use UnitTest.h syntax

```cpp
// ✅ CORRECT - UnitTest.h syntax
#include "UnitTest.h"
UNIT_TEST("TestName") {
CASE("test case") {
    expectEqual(value, 5, "should be 5");
}
}
```

### 2. Private Member Access

**Problem**: Test needs to access private struct/class members

```cpp
// ❌ Compilation error
NoteTrackEngine::Gate gate;  // Error: Gate is private
```

**Solution**: Move struct to public section (if appropriate for testing)

```cpp
// In NoteTrackEngine.h
public:
    struct Gate {
        uint32_t tick;
        bool gate;
    };
```

### 3. Static Constexpr in Implementation Files

**Problem**: Using static constexpr members without class qualification

```cpp
// In .cpp file
// ❌ WRONG
if (event.sequenceId == MainSequenceId) { }

// ✅ CORRECT
if (event.sequenceId == NoteTrackEngine::MainSequenceId) { }
```

**Rule**: Static constexpr members need class qualification in .cpp files.

### 4. Delayed First Tick Feature

**Problem**: Accumulator doesn't increment on first tick after reset()

```cpp
// ❌ Test fails
sequence.accumulator().reset();
const_cast<Accumulator&>(sequence.accumulator()).tick();
expectEqual(sequence.accumulator().currentValue(), 1, "should be 1");
// FAILS: currentValue() is still 0!
```

**Reason**: Delayed first tick feature prevents jump on playback start

**Solution**: Account for delayed tick in tests

```cpp
// ✅ Test passes
sequence.accumulator().reset();

// First tick is skipped (feature behavior)
const_cast<Accumulator&>(sequence.accumulator()).tick();
expectEqual((int)sequence.accumulator().currentValue(), 0, "still 0 after delayed first tick");

// Subsequent ticks work normally
const_cast<Accumulator&>(sequence.accumulator()).tick();
expectEqual((int)sequence.accumulator().currentValue(), 1, "now 1");
```

---

## Test Organization

### File Structure

```
src/tests/unit/sequencer/
├── CMakeLists.txt              # Test registration
├── TestAccumulator.cpp         # Core feature tests
├── TestGateStruct.cpp          # Struct extension tests
├── TestRTrigAccumTicking.cpp   # Integration tests
└── TestRTrigEdgeCases.cpp      # Edge case tests
```

### Test Registration

Add to `CMakeLists.txt`:

```cmake
register_sequencer_test(TestGateStruct TestGateStruct.cpp)
```

**Pattern**: `register_sequencer_test(<executable_name> <source_file>)`

### Build & Run

```bash
cd build/sim/debug

# Clean rebuild (important after flag changes!)
rm -rf CMakeFiles src/tests/unit/sequencer/CMakeFiles

# Configure
cmake ../../..

# Build specific test
make -j TestGateStruct

# Run test
./src/tests/unit/sequencer/TestGateStruct
```

**Important**: Always clean rebuild after changing feature flags!

---

## Test Coverage Guidelines

### What to Test

**1. Core Functionality**:
- Happy path (expected behavior)
- Struct field access and initialization
- Method return values
- State transitions

**2. Edge Cases**:
- Null pointers
- Invalid IDs
- Boundary values (min/max)
- State validation chains

**3. Integration Points**:
- Cross-component interactions
- Queue behavior
- Sequence lookups
- Feature flag combinations

**4. Backward Compatibility**:
- Default values
- 2-arg vs 4-arg constructors
- Flag=0 vs flag=1 behavior

### Test Case Organization

**Pattern**: Organize by feature/concern, not by method

```cpp
UNIT_TEST("ComponentName") {

// Basic functionality
CASE("basic field access") { }
CASE("construction with defaults") { }

// Feature-specific
CASE("experimental feature behavior") { }
CASE("feature flag=0 compatibility") { }

// Edge cases
CASE("null handling") { }
CASE("boundary values") { }

} // End test
```

---

## Lessons Learned

### From RTRIG Spread-Ticks Implementation

**1. Type Safety Matters**:
- Always cast to match template types
- Prefer explicit types over auto/inference
- Read compiler errors carefully

**2. Know Your Framework**:
- Don't assume Catch2 syntax
- Check actual project test framework
- Verify macro names before using

**3. API Discovery**:
- Read header files first
- Don't guess method names
- Check return types

**4. Feature Flags**:
- Test both enabled and disabled states
- Verify zero regressions with flag=0
- Clean rebuild after flag changes

**5. Domain Knowledge**:
- Understand "delayed first tick" feature
- Know accumulator behavior patterns
- Respect existing architectural decisions

**6. Incremental Development**:
- One test case at a time
- Small commits with clear messages
- Verify tests pass before moving on

---

## Quick Reference

### Common Code Patterns

**Test Structure**:
```cpp
UNIT_TEST("TestName") {
CASE("description") {
    // Arrange
    Component component;
    component.setup();

    // Act
    int result = component.method();

    // Assert
    expectEqual(result, expected, "should match");
}
}
```

**Type-Safe Comparisons**:
```cpp
expectEqual((int)int16Value, 42, "message");
expectEqual((unsigned int)uint8Value, 5u, "message");
expectTrue(boolValue, "message");
```

**Feature Flag Tests**:
```cpp
CASE("always runs") { /* ... */ }

#if CONFIG_EXPERIMENTAL_FEATURE
CASE("only when flag=1") { /* ... */ }
#endif
```

**Null Safety**:
```cpp
const Type* ptr = nullptr;
bool isValid = (ptr != nullptr);
expectEqual(isValid, false, "should be null");
```

---

## Simulator Interface for TDD

**Simulator Interface** - Critical for TDD workflow:
- Complete virtual hardware interface for testing
- All physical hardware interactions simulated
- Runs exact same firmware code as hardware
- Accurate development and testing environment

**Development Workflow** (TDD-focused):
- Simulator-first development is recommended for TDD
  1. Develop and test features in simulator (`build/sim/debug`)
  2. Better debugging experience with native tools
  3. Faster iteration cycle for TDD cycles
  4. Port to hardware once stable

**When modifying timing-critical code:**
- Test on actual hardware after simulator validation - simulator timing differs
- Check `Engine::update()` execution time during TDD
- Verify against task priorities and stack sizes during integration testing

**When adding UI features:**
- Use simulator to test UI behavior before hardware validation
- Consider noise reduction impact (pixel count affects audio noise)
- Test with different brightness/screensaver settings
- Use simulator screenshot feature to document UI changes

---

## TDD Application Examples

### Feature Implementation Process
Real-world TDD application examples from the project:

**Accumulator Feature** (TDD-verified):
- Phase 1: Model and Engine Integration
- Phase 2: UI Implementation
- Phase 3: Modulation Implementation
- All unit tests pass with TDD approach
- ✅ **Successfully tested and verified on actual hardware**

**Pulse Count Feature** (TDD-verified):
- Model layer implementation with 7 unit tests
- Integration with engine timing logic
- UI integration with encoder support
- ✅ **Fully tested and verified on actual hardware**

**Gate Mode Feature** (TDD-verified):
- Followed strict TDD methodology (RED-GREEN-REFACTOR):
  - Phase 1 (Model Layer): 6 unit tests written first, all passing
  - Phase 2 (Engine Layer): Design documented, then implemented
  - Phase 3 (UI Integration): Button cycling, visual display, encoder support
- Two critical bugs discovered and fixed during TDD process
- ✅ **Production ready**

**Harmony Feature** (TDD-verified):
- Total: 19 Passing Unit Tests
- HarmonyEngine Tests (13): Default values, scale intervals, chord quality, etc.
- NoteSequence Harmony Tests (3): Default properties, validation, clamping
- Model Integration Tests (3): Engine accessor methods, coordination contracts
- ✅ **19/19 unit tests passing (100%)**

### Testing Status Documentation
Each feature includes explicit testing verification:
- Unit tests passing counts
- Integration testing confirmation
- Hardware verification results
- Bug discovery and fixes during TDD

---

## Testing Conventions and Common Errors

### Test Framework

**CRITICAL**: This project uses a **custom UnitTest.h framework**, NOT Catch2 or Google Test.

**Correct Test Structure:**
```cpp
#include "UnitTest.h"

UNIT_TEST("TestName") {

CASE("test_case_name") {
    // Test code here
    expectEqual(actual, expected, "optional message");
    expectTrue(condition, "optional message");
    expectFalse(condition, "optional message");
}

} // UNIT_TEST("TestName")
```

**Common Test Framework Errors:**

❌ **WRONG** (Catch2 style):
```cpp
#include "catch.hpp"

TEST_CASE("Description", "[tag]") {
    REQUIRE(condition);
}
```

✅ **CORRECT** (UnitTest.h style):
```cpp
#include "UnitTest.h"

UNIT_TEST("TestName") {
CASE("description") {
    expectTrue(condition, "message");
}
}
```

**Assertion Functions:**
- `expectEqual(a, b, msg)` - Compare values (int, float, const char*)
- `expectTrue(condition, msg)` - Assert true
- `expectFalse(condition, msg)` - Assert false
- `expect(condition, msg)` - Generic assertion

**Enum Comparison:**
- Always cast enums to `int` for expectEqual:
```cpp
expectEqual(static_cast<int>(actual), static_cast<int>(expected), "message");
```

### Type System Conventions

**Clamp Function Type Matching:**

The `clamp()` function requires all three arguments to be the **same type**.

❌ **WRONG**:
```cpp
_masterTrackIndex = clamp(index, int8_t(0), int8_t(7));  // Type mismatch!
_harmonyScale = clamp(scale, uint8_t(0), uint8_t(6));   // Type mismatch!
```

✅ **CORRECT**:
```cpp
_masterTrackIndex = clamp(index, 0, 7);  // All int
_harmonyScale = clamp(scale, 0, 6);      // All int
```

The compiler will assign to the correct member variable type automatically.

**Enum Conventions in Model Layer:**

Model enums follow specific patterns:

✅ **CORRECT** (plain enum, no Last):
```cpp
enum HarmonyRole {
    HarmonyOff = 0,
    HarmonyMaster = 1,
    HarmonyFollowerRoot = 2,
    // ... no Last member
};
```

❌ **WRONG** (enum class with Last):
```cpp
enum class HarmonyRole {  // Don't use "class"
    HarmonyOff = 0,
    Last  // Don't add Last in model enums
};
```

**Note**: UI/Pages enums DO use `enum class` and `Last` - this convention is specific to model layer.

### Bitfield Packing

**Available Space Check:**
```cpp
// In NoteSequence::Step::_data1 union
// Check comments for remaining bits:
BitField<uint32_t, 20, GateMode::Bits> gateMode;  // bits 20-21
// 10 bits left  <-- Always documented
```

**Serialization Pattern:**
```cpp
// Write (bit-pack multiple values into single byte)
uint8_t flags = (static_cast<uint8_t>(_role) << 0) |
                (static_cast<uint8_t>(_scale) << 3);
writer.write(flags);

// Read (unpack with masks)
uint8_t flags;
reader.read(flags);
_role = static_cast<Role>((flags >> 0) & 0x7);   // 3 bits
_scale = (flags >> 3) & 0x7;                      // 3 bits
```

### Common Compilation Errors

**Error: "no member named 'X' in 'ClassName'"**
- Cause: Using undefined enum or method
- Fix: Check if you're using plain enum (not enum class) for model enums
- Fix: Ensure you've added the member to the class definition

**Error: "no matching function for call to 'clamp'"**
- Cause: Type mismatch in clamp arguments
- Fix: Use `clamp(value, 0, max)` without type casts

**Error: "undefined symbols for architecture"**
- Cause: Missing CMakeLists.txt registration
- Fix: Add `register_sequencer_test(TestName TestName.cpp)` to CMakeLists.txt

### Test Organization

**Unit Test Location:**
- `src/tests/unit/sequencer/` - Model and small unit tests
- `src/tests/integration/` - Integration tests (less common)

**Test Naming:**
- File: `TestFeatureName.cpp`
- Test: `UNIT_TEST("FeatureName")`
- Cases: `CASE("descriptive_lowercase_name")`

**Example Test Structure:**
```cpp
#include "UnitTest.h"
#include "apps/sequencer/model/YourClass.h"

UNIT_TEST("YourClass") {

CASE("default_values") {
    YourClass obj;
    expectEqual(obj.value(), 0, "default value should be 0");
}

CASE("setter_getter") {
    YourClass obj;
    obj.setValue(42);
    expectEqual(obj.value(), 42, "value should be 42");
}

CASE("clamping") {
    YourClass obj;
    obj.setValue(1000);  // Over max
    expectEqual(obj.value(), 127, "value should clamp to 127");
}

} // UNIT_TEST("YourClass")
```

### Reference Examples

**Good test examples to copy from:**
- `src/tests/unit/sequencer/TestAccumulator.cpp` - Model testing
- `src/tests/unit/sequencer/TestNoteSequence.cpp` - Property testing
- `src/tests/unit/sequencer/TestHarmonyEngine.cpp` - Lookup table testing
- `src/tests/unit/sequencer/TestPulseCount.cpp` - Feature testing

---

## Conclusion

Successful TDD in PEW|FORMER requires:
1. ✅ Using correct test framework (UnitTest.h)
2. ✅ Type-safe assertions (cast to match types)
3. ✅ Knowing actual API (read headers)
4. ✅ Testing both flag states (backward compatibility)
5. ✅ Understanding domain features (delayed ticks, etc.)
6. ✅ Clean rebuilds (after flag changes)

**Remember**: TDD is about confidence. Each green test is proof your code works.
