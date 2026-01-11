#pragma once

#include "Routing.h"
#include "Serialize.h"

#include "core/math/Math.h"

#include <array>
#include <cstdint>

class CvRoute {
public:
    static constexpr int LaneCount = 4;

    enum class InputSource : uint8_t {
        CvIn,
        Bus,
        Off,
        Last
    };

    enum class OutputDest : uint8_t {
        CvOut,
        Bus,
        None,
        Last
    };

    InputSource inputSource(int lane) const { return _inputs[lane]; }
    OutputDest outputDest(int lane) const { return _outputs[lane]; }

    void setInputSource(int lane, InputSource source) {
        if (lane < 0 || lane >= LaneCount) {
            return;
        }
        _inputs[lane] = clampEnum(source);
    }

    void setOutputDest(int lane, OutputDest dest) {
        if (lane < 0 || lane >= LaneCount) {
            return;
        }
        _outputs[lane] = clampEnum(dest);
    }

    int scan() const { return _scan.get(Routing::isRouted(Routing::Target::CvRouteScan)); }
    int route() const { return _route.get(Routing::isRouted(Routing::Target::CvRouteRoute)); }

    void setScan(int value, bool routed = false) {
        _scan.set(clamp(value, 0, 100), routed);
    }

    void setRoute(int value, bool routed = false) {
        _route.set(clamp(value, 0, 100), routed);
    }

    void clear() {
        _inputs.fill(InputSource::CvIn);
        _outputs.fill(OutputDest::CvOut);
        setScan(0);
        setRoute(0);
    }

    void write(VersionedSerializedWriter &writer) const {
        for (auto input : _inputs) {
            writer.writeEnum(input, inputSourceSerialize);
        }
        for (auto output : _outputs) {
            writer.writeEnum(output, outputDestSerialize);
        }
        _scan.write(writer);
        _route.write(writer);
    }

    void read(VersionedSerializedReader &reader) {
        for (auto &input : _inputs) {
            reader.readEnum(input, inputSourceSerialize);
            input = clampEnum(input);
        }
        for (auto &output : _outputs) {
            reader.readEnum(output, outputDestSerialize);
            output = clampEnum(output);
        }
        _scan.read(reader);
        _route.read(reader);
    }

private:
    static uint8_t inputSourceSerialize(InputSource source) {
        switch (source) {
        case InputSource::CvIn: return 0;
        case InputSource::Bus:  return 1;
        case InputSource::Off:  return 2;
        case InputSource::Last: break;
        }
        return 0;
    }

    static uint8_t outputDestSerialize(OutputDest dest) {
        switch (dest) {
        case OutputDest::CvOut: return 0;
        case OutputDest::Bus:   return 1;
        case OutputDest::None:  return 2;
        case OutputDest::Last:  break;
        }
        return 0;
    }

    template<typename Enum>
    static Enum clampEnum(Enum value) {
        return value >= Enum::Last ? static_cast<Enum>(int(Enum::Last) - 1) : value;
    }

    std::array<InputSource, LaneCount> _inputs{};
    std::array<OutputDest, LaneCount> _outputs{};
    Routable<uint8_t> _scan{};
    Routable<uint8_t> _route{};
};
