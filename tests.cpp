#include "Event.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

// ============================================================================
//  Helper class
// ============================================================================

class EventTest
{
public:
	Event<>::Observer& eventSimple() const { return eventSimple_.observer(); }

	Event<int&>::Observer& eventInt() const { return eventInt_.observer(); }

	Event<const std::string&>::Observer& eventString() const { return eventString_.observer(); }

	void fireSimpleAndStringEvents() const
	{
		eventSimple_.fire();
		eventString_.fire("Hello, World!");
	}

	void fireIntEvent() const
	{
		int value = 42;
		eventInt_.fire(value);
	}

private:
	Event<> eventSimple_;
	Event<int&> eventInt_;
	Event<const std::string&> eventString_;
};

// ============================================================================
//  Test cases
// ============================================================================

TEST_CASE("Event source can be moved")
{
	Event<int> source1;
	Event<int> source2(std::move(source1));
	Event<int> source3;
	// Event<int> source4 = source2; // [compile-time test] copy ctor disabled
	source3 = std::move(source2);
	source1.swap(source3);
}

TEST_CASE("Event subscription can be moved")
{
	Event<int> source;
	EventSubscription sub1 = source.observer().subscribe([](int) { return true; });
	EventSubscription sub2(std::move(sub1));
	EventSubscription sub3;
	// EventSubscription sub4 = sub2; // [compile-time test] copy ctor disabled
	sub3 = std::move(sub2);
	CHECK(!sub1.isActive());
	CHECK(!sub2.isActive());
	CHECK(sub3.isActive());
}

TEST_CASE("Observer cannot be copied or moved")
{
	Event<int> src;
	using T = Event<int>::Observer;
	T& observer = src.observer();
	// T observer2{observer};      // [compile-time test] copy ctor disabled
	// T observer3 = observer;     // [compile-time test] copy ctor disabled
	// T observer4 = std::move(observer); // [compile-time test] move ctor disabled
	// T observer5{std::move(observer)};  // [compile-time test] move ctor disabled
	(void)observer;
}

TEST_CASE("Subscription move does not leak")
{
	Event<int> event;
	int callCount1 = 0;
	int callCount2 = 0;

	EventSubscription sub1 = event.observer().subscribe([&callCount1](int)
	{
		callCount1++;
		return true;
	});

	EventSubscription sub2 = event.observer().subscribe([&callCount2](int)
	{
		callCount2++;
		return true;
	});

	// Assigning an active subscription via move must immediately remove the old
	// callback from the Event's subscriber list.
	sub1 = std::move(sub2);

	event.fire(10);

	CHECK(callCount1 == 0);
	CHECK(callCount2 == 1);
}

TEST_CASE("Event reentrancy and hasSubscribers")
{
	Event<int> event;
	EventSubscription sub1;
	EventSubscription sub2;
	int callCount = 0;

	sub1 = event.observer().subscribe([&](int)
	{
		callCount++;
		sub1.reset();
		CHECK(event.observer().hasSubscribers() == true);
		return true;
	});

	sub2 = event.observer().subscribe([&](int)
	{
		callCount++;
		sub2.reset();
		CHECK(event.observer().hasSubscribers() == false);
		return true;
	});

	CHECK(event.observer().hasSubscribers() == true);
	event.fire(42);
	CHECK(callCount == 2);
	CHECK(event.observer().hasSubscribers() == false);
}

TEST_CASE("Event move and lifetime")
{
	Event<int>* source_ev = new Event<int>();
	bool called = false;

	EventSubscription sub = source_ev->observer().subscribe([&called](int)
	{
		called = true;
		return true;
	});

	Event<int> target_ev = std::move(*source_ev);
	delete source_ev;

	CHECK(sub.isActive());
	target_ev.fire(123);
	CHECK(called);

	sub.reset();
	CHECK(!sub.isActive());
}

TEST_CASE("Subscribe, fire, and unsubscribe")
{
	const EventTest test;
	{
		bool called1 = false;
		bool called2 = false;
		bool called3 = false;
		auto sub1 = test.eventSimple().subscribe([&]()
		{
			CHECK(!called1);
			called1 = true;
		});

		const auto sub2 = test.eventString().subscribe([&](const std::string& message)
		{
			CHECK(!called2);
			CHECK(message == "Hello, World!");
			called2 = true;
		});

		const auto sub3 = test.eventString().subscribe([&](const std::string& message)
		{
			CHECK(!called3);
			CHECK(message == "Hello, World!");
			called3 = true;
		});

		const auto sub4 = test.eventInt().subscribe([&](int) { FAIL("This subscriber should not be called"); });

		test.fireSimpleAndStringEvents();
		CHECK(called1);
		CHECK(called2);
		CHECK(called3);

		sub1.reset();

		called1 = called2 = called3 = false;
		test.fireSimpleAndStringEvents();
		CHECK(!called1);
		CHECK(called2);
		CHECK(called3);
	} // sub2, sub3, sub4 go out of scope here — automatically unsubscribed

	CHECK(!test.eventString().hasSubscribers());

	test.fireSimpleAndStringEvents();
}

TEST_CASE("Event priority and parameter modification")
{
	const EventTest test;

	int eventIndex = 0;

	const auto sub5 = test.eventInt().subscribe([&](int value)
	{
		CHECK_EQ(value, 999);
		CHECK_EQ(eventIndex, 2);
		++eventIndex;
		return true;
	}); // default (low) priority, called last

	const auto sub6 = test.eventInt().subscribe(150, [&](int& value)
	{
		CHECK_EQ(value, 42);
		CHECK_EQ(eventIndex, 1);
		value = 999;
		++eventIndex;
		return true;
	}); // medium priority, called second

	const auto sub7 = test.eventInt().subscribe(200, [&](int value)
	{
		CHECK_EQ(value, 42);
		CHECK_EQ(eventIndex, 0);
		++eventIndex;
		return true;
	}); // high priority, called first

	test.fireIntEvent();
}

TEST_CASE("Subscriber outlives Event")
{
	EventSubscription sub;
	{
		const EventTest test2;
		sub = test2.eventInt().subscribe([](int) { return true; });
		CHECK(sub.isActive());
	}
	CHECK(!sub.isActive());
}

TEST_CASE("Subscription stays alive after move")
{
	const EventTest test;
	EventSubscription sub;

	auto sub8 = test.eventInt().subscribe([](int) { return true; });
	CHECK(sub8.isActive());
	CHECK(!sub.isActive());

	sub = std::move(sub8);
	CHECK(!sub8.isActive());
	CHECK(sub.isActive());
}
