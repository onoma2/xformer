#include "AccumulatorConfig.h"

#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"

AccumulatorConfig::AccumulatorConfig() :
    _scope(static_cast<uint8_t>(Scope::Local)),
    _order(static_cast<uint8_t>(Order::Wrap)),
    _polarity(static_cast<uint8_t>(Polarity::Uni)),
    _reset(0),
    _posLim(7),
    _negLim(7)
{
}

void AccumulatorConfig::write(VersionedSerializedWriter &writer) const {
    uint8_t flags = (_scope << 0) | (_order << 1) | (_polarity << 3) | (_reset << 4);
    writer.write(flags);
    writer.write(_posLim);
    writer.write(_negLim);
}

void AccumulatorConfig::read(VersionedSerializedReader &reader) {
    uint8_t flags;
    reader.read(flags);
    _scope    = (flags >> 0) & 0x01;
    _order    = (flags >> 1) & 0x03;
    _polarity = (flags >> 3) & 0x01;
    _reset    = (flags >> 4) & 0x0F;
    reader.read(_posLim);
    reader.read(_negLim);
}
