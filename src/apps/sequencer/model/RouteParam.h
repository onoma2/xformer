#pragma once

#include <cstdint>
#include <cstddef>

// Name-agnostic routing core (U1 of the routing mod-matrix overhaul).
//
// A route delivers a normalized value into an addressed param slot. The engine
// resolves (scope, paramKey) -> Row and calls the row's apply hook; it never
// sees the target's name, range, or meaning. This replaces the flat Target enum
// + targetInfos + per-name dispatch switch + isXxxTarget predicates.
//
// Representation: a Table is a constexpr view over a static Row[] array, so the
// rows live in flash/rodata (one table per scope, shared across instances) and
// each row carries its own apply hook as a function pointer -- zero per-row RAM,
// one indirect call on the hot path, no switch.
//
// See docs/plans/2026-05-31-002-routing-mod-matrix-overhaul-plan.md.

struct RouteParam {

    // Per-param safety/semantics flags (R4). structural = UI-only, never routable.
    enum Flag : uint8_t {
        Fixed      = 1 << 0,   // hot-path Fixed Core param
        Continuous = 1 << 1,   // smooth-modulatable
        Discrete   = 1 << 2,   // stepped value domain
        Structural = 1 << 3,   // UI-only, never routable
        Inlet      = 1 << 4,   // fills a per-track CV bus, not a direct param (R12)
        Trigger    = 1 << 5,   // acts on the rising edge of its routed value, not the level
    };

    // Rising-edge detector for Trigger-kind rows. gateMask holds one "was high"
    // bit per track (bit 0 for global triggers); returns true on a low->high
    // crossing and updates the stored bit. One uniform mechanism for what used
    // to be scattered per-target latches (o_C IOFrame model: the edge is a
    // property of the input, detected once -- reset is just another clocked
    // input, no different from clock).
    static bool gateRisingEdge(uint8_t &gateMask, int trackIndex, float value, float threshold = 0.5f) {
        uint8_t bit = uint8_t(1) << (trackIndex & 7);
        bool wasHigh = (gateMask & bit) != 0;
        bool isHigh = value > threshold;
        if (isHigh) {
            gateMask |= bit;
        } else {
            gateMask &= ~bit;
        }
        return isHigh && !wasHigh;
    }

    struct Range {
        float min;
        float max;
    };

    // A route resolved to a single addressed object for one track. The slot's
    // track mask (global pool) lives on the slot, not here; by apply time the
    // engine has already resolved scope down to one object + track index.
    struct Scope {
        enum class Kind : uint8_t { Global, Track, Group };
        Kind kind = Kind::Global;
        void *object = nullptr;     // concrete scope object the hook writes
        uint8_t trackIndex = 0;     // resolved track within the scope (0..7)
        uint16_t groupMask = 0;     // cell/stage group (Group scope, U6)
    };

    // One routable parameter. key is an append-only numeric paramKey, never the
    // array index, so reordering/adding rows never shifts saved routes (F6).
    // The hook receives the row's range (single source of truth) so it can
    // denormalize the [0,1] value into the param's domain itself.
    struct Row {
        uint8_t key;
        const char *name;
        Range range;
        uint8_t flags;
        void (*applyRouted)(const Scope &scope, const Range &range, float normalized);
    };

    // Per-scope param table: the single source of truth for label, range, flags,
    // and the apply hook. Resolution is by key (linear scan over the small set).
    // The table carries the Scope::Kind its hooks expect (e.g. the global table
    // expects Global, whose object is a Project*); applyRouted fails closed on a
    // null object or a kind mismatch so a bad resolved scope can never reach a
    // hook's unchecked cast.
    class Table {
    public:
        constexpr Table(Scope::Kind expectedKind, const Row *rows, size_t count)
            : _expectedKind(expectedKind), _rows(rows), _count(count) {}

        const Row *find(uint8_t key) const {
            if (key == 0) {
                return nullptr;
            }
            for (size_t i = 0; i < _count; ++i) {
                if (_rows[i].key == key) {
                    return &_rows[i];
                }
            }
            return nullptr;
        }

        bool applyRouted(const Scope &scope, uint8_t key, float normalized) const {
            if (scope.object == nullptr || scope.kind != _expectedKind) {
                return false;
            }
            const Row *row = find(key);
            if (!row || (row->flags & Structural)) {
                return false;
            }
            row->applyRouted(scope, row->range, normalized);
            return true;
        }

        Scope::Kind expectedKind() const { return _expectedKind; }
        const Row *rows() const { return _rows; }
        size_t count() const { return _count; }

    private:
        Scope::Kind _expectedKind;
        const Row *_rows;
        size_t _count;
    };
};
