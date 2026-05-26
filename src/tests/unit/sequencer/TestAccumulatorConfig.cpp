#include "UnitTest.h"

#include "apps/sequencer/model/AccumulatorConfig.h"
#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"
#include "../core/io/MemoryReaderWriter.h"
#include "model/ProjectVersion.h"

#include <cstring>

static void writeConfig(const AccumulatorConfig &cfg, uint8_t *buf, size_t len) {
    MemoryWriter memoryWriter(buf, len);
    VersionedSerializedWriter writer([&memoryWriter](const void *data, size_t sz) {
        memoryWriter.write(data, sz);
    }, ProjectVersion::Latest);
    cfg.write(writer);
}

static void readConfig(AccumulatorConfig &cfg, const uint8_t *buf, size_t len) {
    MemoryReader memoryReader(buf, len);
    VersionedSerializedReader reader([&memoryReader](void *data, size_t sz) {
        memoryReader.read(data, sz);
    }, ProjectVersion::Latest);
    cfg.read(reader);
}

UNIT_TEST("AccumulatorConfig") {

CASE("defaults_match_spec") {
    AccumulatorConfig cfg;
    expectEqual(int(cfg.scope()), int(AccumulatorConfig::Scope::Local), "default scope");
    expectEqual(int(cfg.order()), int(AccumulatorConfig::Order::Wrap), "default order");
    expectEqual(int(cfg.polarity()), int(AccumulatorConfig::Polarity::Uni), "default polarity");
    expectEqual(int(cfg.reset()), 0, "default reset");
    expectEqual(int(cfg.posLim()), 7, "default posLim");
    expectEqual(int(cfg.negLim()), 7, "default negLim");
}

CASE("default_roundtrip") {
    AccumulatorConfig cfg;

    uint8_t buf[64];
    std::memset(buf, 0, sizeof(buf));
    writeConfig(cfg, buf, sizeof(buf));

    AccumulatorConfig restored;
    restored.setScope(AccumulatorConfig::Scope::Track);
    restored.setOrder(AccumulatorConfig::Order::RTZ);
    restored.setPolarity(AccumulatorConfig::Polarity::Bi);
    restored.setReset(9);
    restored.setPosLim(20);
    restored.setNegLim(15);
    readConfig(restored, buf, sizeof(buf));

    expectEqual(int(restored.scope()), int(AccumulatorConfig::Scope::Local), "scope default");
    expectEqual(int(restored.order()), int(AccumulatorConfig::Order::Wrap), "order default");
    expectEqual(int(restored.polarity()), int(AccumulatorConfig::Polarity::Uni), "polarity default");
    expectEqual(int(restored.reset()), 0, "reset default");
    expectEqual(int(restored.posLim()), 7, "posLim default");
    expectEqual(int(restored.negLim()), 7, "negLim default");
}

CASE("every_field_roundtrips") {
    AccumulatorConfig cfg;
    cfg.setScope(AccumulatorConfig::Scope::Track);
    cfg.setOrder(AccumulatorConfig::Order::Pendulum);
    cfg.setPolarity(AccumulatorConfig::Polarity::Bi);
    cfg.setReset(5);
    cfg.setPosLim(12);
    cfg.setNegLim(3);

    uint8_t buf[64];
    std::memset(buf, 0, sizeof(buf));
    writeConfig(cfg, buf, sizeof(buf));

    AccumulatorConfig r;
    readConfig(r, buf, sizeof(buf));

    expectEqual(int(r.scope()), int(AccumulatorConfig::Scope::Track), "scope");
    expectEqual(int(r.order()), int(AccumulatorConfig::Order::Pendulum), "order");
    expectEqual(int(r.polarity()), int(AccumulatorConfig::Polarity::Bi), "polarity");
    expectEqual(int(r.reset()), 5, "reset");
    expectEqual(int(r.posLim()), 12, "posLim");
    expectEqual(int(r.negLim()), 3, "negLim");
}

CASE("order_hold_roundtrips") {
    AccumulatorConfig cfg;
    cfg.setOrder(AccumulatorConfig::Order::Hold);

    uint8_t buf[64];
    std::memset(buf, 0, sizeof(buf));
    writeConfig(cfg, buf, sizeof(buf));

    AccumulatorConfig r;
    readConfig(r, buf, sizeof(buf));

    expectEqual(int(r.order()), int(AccumulatorConfig::Order::Hold), "order Hold");
}

CASE("order_rtz_roundtrips") {
    AccumulatorConfig cfg;
    cfg.setOrder(AccumulatorConfig::Order::RTZ);

    uint8_t buf[64];
    std::memset(buf, 0, sizeof(buf));
    writeConfig(cfg, buf, sizeof(buf));

    AccumulatorConfig r;
    readConfig(r, buf, sizeof(buf));

    expectEqual(int(r.order()), int(AccumulatorConfig::Order::RTZ), "order RTZ");
}

CASE("reset_field_handles_max_15") {
    AccumulatorConfig cfg;
    cfg.setReset(15);

    uint8_t buf[64];
    std::memset(buf, 0, sizeof(buf));
    writeConfig(cfg, buf, sizeof(buf));

    AccumulatorConfig r;
    readConfig(r, buf, sizeof(buf));

    expectEqual(int(r.reset()), 15, "reset max 15");
}

CASE("pos_lim_handles_max_28") {
    AccumulatorConfig cfg;
    cfg.setPosLim(28);
    cfg.setNegLim(28);

    uint8_t buf[64];
    std::memset(buf, 0, sizeof(buf));
    writeConfig(cfg, buf, sizeof(buf));

    AccumulatorConfig r;
    readConfig(r, buf, sizeof(buf));

    expectEqual(int(r.posLim()), 28, "posLim max 28");
    expectEqual(int(r.negLim()), 28, "negLim max 28");
}

CASE("pos_lim_handles_min_1") {
    AccumulatorConfig cfg;
    cfg.setPosLim(1);
    cfg.setNegLim(1);

    uint8_t buf[64];
    std::memset(buf, 0, sizeof(buf));
    writeConfig(cfg, buf, sizeof(buf));

    AccumulatorConfig r;
    readConfig(r, buf, sizeof(buf));

    expectEqual(int(r.posLim()), 1, "posLim min 1");
    expectEqual(int(r.negLim()), 1, "negLim min 1");
}

CASE("edge_values_all_fields_combo") {
    // Drive every packed field to its non-default max simultaneously.
    // Catches bit-pack collisions: if two fields share bits, one will clobber
    // the other on round-trip.
    AccumulatorConfig cfg;
    cfg.setScope(AccumulatorConfig::Scope::Track);
    cfg.setOrder(AccumulatorConfig::Order::RTZ);
    cfg.setPolarity(AccumulatorConfig::Polarity::Bi);
    cfg.setReset(15);
    cfg.setPosLim(28);
    cfg.setNegLim(28);

    uint8_t buf[64];
    std::memset(buf, 0, sizeof(buf));
    writeConfig(cfg, buf, sizeof(buf));

    AccumulatorConfig r;
    readConfig(r, buf, sizeof(buf));

    expectEqual(int(r.scope()), int(AccumulatorConfig::Scope::Track), "scope max");
    expectEqual(int(r.order()), int(AccumulatorConfig::Order::RTZ), "order max");
    expectEqual(int(r.polarity()), int(AccumulatorConfig::Polarity::Bi), "polarity max");
    expectEqual(int(r.reset()), 15, "reset max");
    expectEqual(int(r.posLim()), 28, "posLim max");
    expectEqual(int(r.negLim()), 28, "negLim max");
}

CASE("pulse_variant_lim_8_roundtrips") {
    // Same struct is shared by note (1..28) and pulse (1..8) accumulators.
    // Confirm pulse-range values traverse the same path.
    AccumulatorConfig cfg;
    cfg.setPosLim(8);
    cfg.setNegLim(8);

    uint8_t buf[64];
    std::memset(buf, 0, sizeof(buf));
    writeConfig(cfg, buf, sizeof(buf));

    AccumulatorConfig r;
    readConfig(r, buf, sizeof(buf));

    expectEqual(int(r.posLim()), 8, "pulse posLim 8");
    expectEqual(int(r.negLim()), 8, "pulse negLim 8");
}

} // UNIT_TEST("AccumulatorConfig")
