# Simple Event C++

![Workflow Status](https://github.com/StingX84/simple_event_cpp/actions/workflows/cmake-multi-platform.yml/badge.svg)

A lightweight, header-only C++ event system with subscription lifetime management.

## Features

- **Header-only** — drop `Event.h` into your project
- **Type-safe** — template-based event arguments with full type checking
- **RAII subscriptions** — `EventSubscription` automatically unsubscribes on destruction
- **Move semantics** — events and subscriptions are movable, not copyable
- **Reentrancy-safe** — subscribers can safely unsubscribe during event firing
- **Priority-based dispatch** — subscribers can specify execution order
- **C++11–C++20** compatible, uses concepts when available

## Usage

```cpp
#include "Event.h"

#include <string_view>

class MyClassProvidingEvent {
public:
    /// Expose event Observer to subscribers!
    auto& eventTest() const noexcept { return eventTest_.observer(); }
    auto& eventWithData() const noexcept { return eventWithData_.observer(); }

    void fireEvents()
    {
        eventTest_.fire();
        eventWithData_.fire("Your answer", 42);
    }
private:
    Event<> eventTest_;
    Event<std::string_view, int> eventWithData_;
};

MyClassProvidingEvent myClass;

// Subscribe
const auto sub1 = myClass.eventTest().subscribe([]() {
    std::cout << "Simple event received" << std::endl;
});

const auto sub2 = myClass.eventWithData().subscribe([](std::string_view d1, int d2) {
    std::cout << "Event with data received: " << d1 << ", " << d2 << std::endl;
});

// Fire
myClass.fireEvents();

// Subscription auto-unsubscribes when `sub` goes out of scope
```

## Build

```bash
cmake -G Ninja -B build
cmake --build build
./build/tests
```

Example CMakeLists.txt requires CMake 3.20+ and a C++20-compatible compiler.
