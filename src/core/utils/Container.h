#pragma once

#include <cstdint>
#include <utility>

template<typename... Ts>
struct maxsizeof {
    static constexpr size_t value = 0;
};

template<typename T, typename... Ts>
struct maxsizeof<T, Ts...> {
    static constexpr size_t value = sizeof(T) > maxsizeof<Ts...>::value ? sizeof(T) : maxsizeof<Ts...>::value;
};

/*
 * Tagged-storage discriminated union for a fixed set of types.
 *
 * create<U>() runs the current occupant's destructor (if any) before
 * placement-new'ing U. This avoids destructor-skip leaks on type swaps,
 * which previously required callers to remember to call destroy() first
 * and which silently corrupted heap state when forgotten (see e.g.
 * StochasticTrackEngine::_lockedParents heap leak that surfaced as a
 * second-layout-commit reboot on STM32).
 *
 * Container also destructs its current occupant on its own destruction,
 * so scope-exit cleanup is automatic.
 */
template<typename... Ts>
class Container {
public:
    static constexpr size_t Size = maxsizeof<Ts...>::value;

    Container() = default;

    ~Container() {
        destroy();
    }

    template<typename U, typename... Args>
    U *create(Args&&... args) {
        // Destruct prior occupant via the type-erased destructor pointer
        // stored from the previous create().
        if (_destructor) {
            _destructor(_data);
            _destructor = nullptr;
        }
        U *obj = new(_data) U(std::forward<Args>(args)...);
        _destructor = &destructorThunk<U>;
        return obj;
    }

    // Explicit destroy without re-creating. Idempotent.
    void destroy() {
        if (_destructor) {
            _destructor(_data);
            _destructor = nullptr;
        }
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"

    template<typename U>
    U &as() {
        return *reinterpret_cast<U *>(_data);
    }

    template<typename U>
    const U &as() const {
        return *reinterpret_cast<const U *>(_data);
    }

#pragma GCC diagnostic pop

private:
    template<typename U>
    static void destructorThunk(void *p) {
        static_cast<U *>(p)->~U();
    }

    using DestructorFn = void (*)(void *);
    DestructorFn _destructor = nullptr;

    // memory aligned to system pointer size
    uintptr_t _data[(Size + sizeof(uintptr_t) - 1) / sizeof(uintptr_t)];
};
