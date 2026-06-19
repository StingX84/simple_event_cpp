# Simple Event C++

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

#include <string>

class MyClassProvidingEvent {
public:
    /// Expose event Observer to subscribers!
    auto& eventTest() const noexcept { return eventTest_.observer(); }

    void fireEventTest()
    {
        eventTest_.fire("Hello world!");
    }
private:
    Event<const std::string&> eventTest_;
};

MyClassProvidingEvent myClass;

// Subscribe
const auto sub = myClass.eventTest().subscribe([](const std::string& msg) {
    std::cout << msg << std::endl;
    return true; // continue dispatching
});

// Fire
myClass.fireEventTest();

// Subscription auto-unsubscribes when `sub` goes out of scope
```

## Build

```bash
cmake -B build
cmake --build build
./build/app
```

Example CMakeLists.txt requires CMake 3.20+ and a C++20-compatible compiler.
