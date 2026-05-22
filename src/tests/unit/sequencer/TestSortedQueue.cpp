// Phase 16 P1 — Queue contract audit.
//
// SortedQueue<T, Capacity> is a ring buffer with insert-sort. Both
// StochasticTrackEngine queues (gates, CVs) and other engines depend on it.
// Original implementation silently dropped ALL pending elements on overflow
// (push past Capacity wraps _write to meet _read; size() returns 0; queue
// "empties"). User decision 2026-05-23: drop OLDEST on overflow, preserve
// most recent (Capacity-1) elements.
//
// Tests pin the contract:
//   - Standard fill / drain still works.
//   - Overflow drops oldest, keeps newest.
//   - Sort order maintained across overflow.
//   - Same-tick ordering preserved (stable for equal keys).

#include "UnitTest.h"

#include "apps/sequencer/engine/SortedQueue.h"

namespace {

struct TimedItem {
    uint32_t tick;
    int payload;

    bool operator==(const TimedItem &o) const {
        return tick == o.tick && payload == o.payload;
    }
};

struct ByTick {
    bool operator()(const TimedItem &a, const TimedItem &b) const {
        return a.tick < b.tick;
    }
};

} // anonymous namespace


UNIT_TEST("SortedQueue") {

CASE("empty_queue_size_zero") {
    SortedQueue<TimedItem, 4, ByTick> q;
    expectEqual(int(q.size()), 0);
    expectTrue(q.empty(), "fresh queue is empty");
}

CASE("push_then_pop_in_sort_order") {
    SortedQueue<TimedItem, 8, ByTick> q;
    q.push({ 30, 'C' });
    q.push({ 10, 'A' });
    q.push({ 20, 'B' });

    expectEqual(int(q.size()), 3);
    expectEqual(int(q.front().tick), 10);
    q.pop();
    expectEqual(int(q.front().tick), 20);
    q.pop();
    expectEqual(int(q.front().tick), 30);
    q.pop();
    expectTrue(q.empty(), "queue empty after draining");
}

CASE("clear_resets_state") {
    SortedQueue<TimedItem, 4, ByTick> q;
    q.push({ 10, 1 });
    q.push({ 20, 2 });
    q.clear();
    expectTrue(q.empty(), "clear empties queue");
    expectEqual(int(q.size()), 0);
}

CASE("overflow_drops_oldest_preserves_most_recent") {
    // Phase 16 P1 contract: pushing into a full queue drops the OLDEST
    // unread element to make room. With Capacity=4 the effective
    // size cap is 3 (one slot reserved for empty/full discrimination).
    // After pushing 5 distinct ticks, only the 3 most recent (by insert
    // order) should remain.
    SortedQueue<TimedItem, 4, ByTick> q;
    q.push({ 10, 1 });   // size 1
    q.push({ 20, 2 });   // size 2
    q.push({ 30, 3 });   // size 3 (cap)
    q.push({ 40, 4 });   // overflow → drops {10,1}, keeps {20,30,40}
    q.push({ 50, 5 });   // overflow → drops {20,2}, keeps {30,40,50}

    expectEqual(int(q.size()), 3);

    // Pop in sort order. Should be 30, 40, 50.
    expectEqual(int(q.front().tick), 30);
    expectEqual(q.front().payload, 3);
    q.pop();
    expectEqual(int(q.front().tick), 40);
    q.pop();
    expectEqual(int(q.front().tick), 50);
    q.pop();
    expectTrue(q.empty(), "all three remaining items popped");
}

CASE("overflow_with_out_of_order_inserts_still_sorts") {
    // Drop-oldest must work with the insert-sort path too. Push elements
    // in non-monotonic tick order; verify post-overflow set is correct.
    SortedQueue<TimedItem, 4, ByTick> q;
    q.push({ 100, 1 });
    q.push({ 50,  2 });
    q.push({ 200, 3 });
    // size 3 (full). Next push triggers drop-oldest.
    // "Oldest" = the element at _read position = tick 50 (lowest after sort).
    // After insert-sort: {50, 100, 200}. _read points at 50.
    // Push {75, 4}: drop _read element (50), then insert 75.
    // Remaining: {75, 100, 200}.
    q.push({ 75, 4 });

    expectEqual(int(q.size()), 3);
    expectEqual(int(q.front().tick), 75);
    q.pop();
    expectEqual(int(q.front().tick), 100);
    q.pop();
    expectEqual(int(q.front().tick), 200);
}

CASE("same_tick_inserts_preserve_relative_order") {
    // Two elements at the same tick: insert-sort with strict less-than
    // comparator leaves the SECOND insert after the first. Verify that
    // contract — gate ON pushed at tick T, gate OFF pushed at tick T
    // (both same key) should pop in the order they were inserted.
    SortedQueue<TimedItem, 8, ByTick> q;
    q.push({ 50, 0xAA });   // first at tick 50
    q.push({ 50, 0xBB });   // second at tick 50
    q.push({ 50, 0xCC });   // third at tick 50

    expectEqual(int(q.size()), 3);
    expectEqual(q.front().payload, 0xAA);
    q.pop();
    expectEqual(q.front().payload, 0xBB);
    q.pop();
    expectEqual(q.front().payload, 0xCC);
}

CASE("fill_drain_fill_cycle_round_trip") {
    // Exercise wraparound behavior of the ring buffer's read/write
    // indices. Fill, drain, fill again — internals must stay sane.
    SortedQueue<TimedItem, 4, ByTick> q;
    for (int round = 0; round < 5; ++round) {
        for (int i = 0; i < 3; ++i) {
            q.push({ uint32_t(100 + i + round * 10), i });
        }
        expectEqual(int(q.size()), 3);
        while (!q.empty()) q.pop();
        expectEqual(int(q.size()), 0);
    }
}

CASE("massive_overflow_keeps_only_capacity_minus_one") {
    // Push 100 elements into a Cap=4 queue. Only the most recent 3
    // should survive. Validates that drop-oldest scales — not just a
    // single-overflow fix.
    SortedQueue<TimedItem, 4, ByTick> q;
    for (int i = 0; i < 100; ++i) {
        q.push({ uint32_t(i * 10), i });
    }
    expectEqual(int(q.size()), 3);

    // The three most recent are i=97 (tick 970), i=98 (tick 980), i=99 (tick 990).
    expectEqual(int(q.front().tick), 970);
    q.pop();
    expectEqual(int(q.front().tick), 980);
    q.pop();
    expectEqual(int(q.front().tick), 990);
}

} // UNIT_TEST("SortedQueue")
