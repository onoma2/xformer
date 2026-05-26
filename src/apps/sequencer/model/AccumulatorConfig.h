#pragma once

#include <cstdint>

class VersionedSerializedWriter;
class VersionedSerializedReader;

// Per-accumulator config block (PhaseFlux MVP). Shared by note (1..28)
// and pulse (1..8) variants — owner picks the range.
// See docs/accumulator-v2-spec.md §13.3.
class AccumulatorConfig {
public:
    enum class Scope : uint8_t { Local = 0, Track = 1 };
    enum class Order : uint8_t { Wrap = 0, Pendulum = 1, Hold = 2, RTZ = 3 };
    enum class Polarity : uint8_t { Uni = 0, Bi = 1 };

    AccumulatorConfig();

    Scope scope() const { return static_cast<Scope>(_scope); }
    void setScope(Scope scope) { _scope = static_cast<uint8_t>(scope) & 0x01; }

    Order order() const { return static_cast<Order>(_order); }
    void setOrder(Order order) { _order = static_cast<uint8_t>(order) & 0x03; }

    Polarity polarity() const { return static_cast<Polarity>(_polarity); }
    void setPolarity(Polarity polarity) { _polarity = static_cast<uint8_t>(polarity) & 0x01; }

    uint8_t reset() const { return _reset; }
    void setReset(uint8_t reset) { _reset = reset & 0x0F; }

    uint8_t posLim() const { return _posLim; }
    void setPosLim(uint8_t posLim) { _posLim = posLim; }

    uint8_t negLim() const { return _negLim; }
    void setNegLim(uint8_t negLim) { _negLim = negLim; }

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    uint8_t _scope    : 1;
    uint8_t _order    : 2;
    uint8_t _polarity : 1;
    uint8_t _reset    : 4;
    uint8_t _posLim;
    uint8_t _negLim;
};
