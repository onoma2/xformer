#pragma once

#include <array>
#include <utility>
#include <functional>
#include <cstddef>

template<typename T, size_t Capacity, typename Compare = std::less<T>>
class SortedQueue {
public:
    SortedQueue() {
        clear();
    }

    void clear() {
        _read = 0;
        _write = 0;
    }

    size_t capacity() {
        return Capacity;
    }

    size_t size() {
        return (_write - _read) % Capacity;
    }

    bool empty() const {
        return _write == _read;
    }

    void push(const T &value) {
        // Drop-oldest overflow contract (Phase 16 P1, 2026-05-23). The ring
        // buffer reserves one slot for empty/full discrimination, so the
        // effective capacity is `Capacity - 1`. Pushing into a full queue
        // would otherwise wrap _write to meet _read and silently drop every
        // pending element. Instead: drop the oldest unread element first,
        // then insert. Caller never sees a failed push; the most recent
        // (Capacity-1) elements always survive.
        if (size() + 1 >= Capacity) {
            _read = increase(_read);
        }
        insert(value);
    }

    void pushReplace(const T &value) {
        size_t pos = insert(value);
        _write = increase(pos);
    }

    const T &front() const { return _queue[_read]; }
          T &front()       { return _queue[_read]; }

    const T &back() const { return _queue[_write]; }
          T &back()       { return _queue[_write]; }

    void pop() {
        if (size() > 0) {
            _read = increase(_read);
        }
    }

    void popBack() {
        if (size() > 0) {
            _write = decrease(_write);
        }
    }

private:
    // insert into queue and return index of inserted value
    size_t insert(const T &value) {
        Compare compare;
        size_t pos = _write;
        _queue[pos] = value;

        // insert sort
        size_t cur = _write;
        size_t prev = decrease(cur);
        while (cur != _read && compare(_queue[cur], _queue[prev])) {
            std::swap(_queue[cur], _queue[prev]);
            cur = prev;
            prev = decrease(cur);
        }

        _write = increase(_write);

        return cur;
    }

    inline size_t increase(size_t pos) const {
        return (pos + 1) % Capacity;
    }

    inline size_t decrease(size_t pos) const {
        return (pos - 1) % Capacity;
    }

    std::array<T, Capacity> _queue;
    size_t _read;
    size_t _write;
};
