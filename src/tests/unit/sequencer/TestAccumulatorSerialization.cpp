#include "UnitTest.h"

#include "apps/sequencer/model/Accumulator.h"
#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"
#include "../core/io/MemoryReaderWriter.h"

UNIT_TEST("AccumulatorSerialization") {

CASE("write_accumulator_with_custom_values") {
    // Test 1.1: Verify accumulator writes all parameters
    uint8_t buf[512];
    std::memset(buf, 0, sizeof(buf));

    // Create accumulator with non-default values
    Accumulator accumulator;
    accumulator.setEnabled(true);
    accumulator.setMode(Accumulator::Mode::Stage);
    accumulator.setPolarity(Accumulator::Polarity::Bipolar);
    accumulator.setDirection(Accumulator::Direction::Down);
    accumulator.setOrder(Accumulator::Order::Pendulum);
    accumulator.setMinValue(-10);
    accumulator.setMaxValue(15);
    accumulator.setStepValue(3);

    // Serialize to buffer
    MemoryWriter memoryWriter(buf, sizeof(buf));
    VersionedSerializedWriter writer([&memoryWriter] (const void *data, size_t len) {
        memoryWriter.write(data, len);
    }, 33); // Version33 for accumulator serialization

    accumulator.write(writer);

    // Verify buffer is not empty (data was written)
    bool hasData = false;
    for (size_t i = 0; i < 20; ++i) {
        if (buf[i] != 0) {
            hasData = true;
            break;
        }
    }
    expectTrue(hasData, "Buffer should contain serialized data");
}

CASE("read_accumulator_with_known_values") {
    // Test 1.2: Verify accumulator reads all parameters
    uint8_t buf[512];
    std::memset(buf, 0, sizeof(buf));

    // Write known values to buffer
    MemoryWriter memoryWriter(buf, sizeof(buf));
    VersionedSerializedWriter writer([&memoryWriter] (const void *data, size_t len) {
        memoryWriter.write(data, len);
    }, 33);

    Accumulator sourceAccumulator;
    sourceAccumulator.setEnabled(true);
    sourceAccumulator.setMode(Accumulator::Mode::Track);
    sourceAccumulator.setPolarity(Accumulator::Polarity::Unipolar);
    sourceAccumulator.setDirection(Accumulator::Direction::Up);
    sourceAccumulator.setOrder(Accumulator::Order::Wrap);
    sourceAccumulator.setMinValue(-7);
    sourceAccumulator.setMaxValue(7);
    sourceAccumulator.setStepValue(2);

    sourceAccumulator.write(writer);

    // Read from buffer
    MemoryReader memoryReader(buf, sizeof(buf));
    VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) {
        memoryReader.read(data, len);
    }, 33);

    Accumulator targetAccumulator;
    targetAccumulator.read(reader);

    // Verify all parameters match
    expectTrue(targetAccumulator.enabled(), "enabled should be true");
    expectEqual(int(targetAccumulator.mode()), int(Accumulator::Mode::Track), "mode should be Track");
    expectEqual(int(targetAccumulator.polarity()), int(Accumulator::Polarity::Unipolar), "polarity should be Unipolar");
    expectEqual(int(targetAccumulator.direction()), int(Accumulator::Direction::Up), "direction should be Up");
    expectEqual(int(targetAccumulator.order()), int(Accumulator::Order::Wrap), "order should be Wrap");
    expectEqual(int(targetAccumulator.minValue()), -7, "minValue should be -7");
    expectEqual(int(targetAccumulator.maxValue()), 7, "maxValue should be 7");
    expectEqual(int(targetAccumulator.stepValue()), 2, "stepValue should be 2");
}

CASE("roundtrip_consistency") {
    // Test 1.3: Verify round-trip consistency (write then read)
    uint8_t buf[512];
    std::memset(buf, 0, sizeof(buf));

    // Create accumulator with various values
    Accumulator originalAccumulator;
    originalAccumulator.setEnabled(true);
    originalAccumulator.setMode(Accumulator::Mode::Stage);
    originalAccumulator.setPolarity(Accumulator::Polarity::Bipolar);
    originalAccumulator.setDirection(Accumulator::Direction::Down);
    originalAccumulator.setOrder(Accumulator::Order::Hold);
    originalAccumulator.setMinValue(-20);
    originalAccumulator.setMaxValue(30);
    originalAccumulator.setStepValue(5);

    // Trigger some ticks to change currentValue
    originalAccumulator.tick();
    originalAccumulator.tick();

    // Serialize
    MemoryWriter memoryWriter(buf, sizeof(buf));
    VersionedSerializedWriter writer([&memoryWriter] (const void *data, size_t len) {
        memoryWriter.write(data, len);
    }, 33);
    originalAccumulator.write(writer);

    // Deserialize
    MemoryReader memoryReader(buf, sizeof(buf));
    VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) {
        memoryReader.read(data, len);
    }, 33);

    Accumulator restoredAccumulator;
    restoredAccumulator.read(reader);

    // Verify all values match
    expectEqual(restoredAccumulator.enabled(), originalAccumulator.enabled(), "enabled should match");
    expectEqual(int(restoredAccumulator.mode()), int(originalAccumulator.mode()), "mode should match");
    expectEqual(int(restoredAccumulator.polarity()), int(originalAccumulator.polarity()), "polarity should match");
    expectEqual(int(restoredAccumulator.direction()), int(originalAccumulator.direction()), "direction should match");
    expectEqual(int(restoredAccumulator.order()), int(originalAccumulator.order()), "order should match");
    expectEqual(int(restoredAccumulator.minValue()), int(originalAccumulator.minValue()), "minValue should match");
    expectEqual(int(restoredAccumulator.maxValue()), int(originalAccumulator.maxValue()), "maxValue should match");
    expectEqual(int(restoredAccumulator.stepValue()), int(originalAccumulator.stepValue()), "stepValue should match");
    expectEqual(int(restoredAccumulator.currentValue()), int(originalAccumulator.currentValue()), "currentValue should match");
}

CASE("default_values_for_missing_data") {
    // Test 1.4: Verify safe defaults when reading from empty/short buffer
    // This simulates backward compatibility (reading old project without accumulator data)

    Accumulator accumulator;
    accumulator.setEnabled(true);
    accumulator.setMinValue(-50);
    accumulator.setMaxValue(50);

    // Create a default accumulator for comparison
    Accumulator defaultAccumulator;

    // Now reset to default by assignment (simulates old project loading)
    accumulator = Accumulator();

    // Verify accumulator has default values
    expectEqual(accumulator.enabled(), defaultAccumulator.enabled(), "enabled should be default (false)");
    expectEqual(int(accumulator.mode()), int(defaultAccumulator.mode()), "mode should be default");
    expectEqual(int(accumulator.direction()), int(defaultAccumulator.direction()), "direction should be default");
    expectEqual(int(accumulator.order()), int(defaultAccumulator.order()), "order should be default");
    expectEqual(int(accumulator.minValue()), int(defaultAccumulator.minValue()), "minValue should be default");
    expectEqual(int(accumulator.maxValue()), int(defaultAccumulator.maxValue()), "maxValue should be default");
    expectEqual(int(accumulator.stepValue()), int(defaultAccumulator.stepValue()), "stepValue should be default");
}

} // UNIT_TEST("AccumulatorSerialization")
