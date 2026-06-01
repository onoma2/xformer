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
    };

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
    class Table {
    public:
        constexpr Table(const Row *rows, size_t count) : _rows(rows), _count(count) {}

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
            const Row *row = find(key);
            if (!row || (row->flags & Structural)) {
                return false;
            }
            row->applyRouted(scope, row->range, normalized);
            return true;
        }

        const Row *rows() const { return _rows; }
        size_t count() const { return _count; }

    private:
        const Row *_rows;
        size_t _count;
    };
};
