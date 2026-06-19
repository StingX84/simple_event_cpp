#include "Event.h"

#include <iostream>
#include <string>

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
		std::cout << "++ Firing int event with value: " << value << std::endl;
		eventInt_.fire(value);
		std::cout << "-- Int value after firing: " << value << std::endl;
	}

private:
	Event<> eventSimple_;
	Event<int&> eventInt_;
	Event<const std::string&> eventString_;
};

void test_event_source_could_be_moved()
{
	Event<int> source1;
	Event<int> source2(std::move(source1));
	Event<int> source3;
    // Event<int> source4 = source2; // This line should cause a compilation error if uncommented
	source3 = std::move(source2);
	source1.swap(source3);
}

void test_event_subscription_could_be_moved()
{
	Event<int> source;
	EventSubscription sub1 = source.observer().subscribe([](int) { return true; });
	EventSubscription sub2(std::move(sub1));
	EventSubscription sub3;
    // EventSubscription sub4 = sub2; // This line should cause a compilation error if uncommented
	sub3 = std::move(sub2);
	assert(!sub1.isActive() && !sub2.isActive() && sub3.isActive());
}

void test_observer_could_not_be_moved()
{
	Event<int> src;
	using T = Event<int>::Observer;
	T& observer = src.observer(); // OK
	// T observer2{observer}; // This line should cause a compilation error if uncommented
	// T observer3 = observer; // This line should cause a compilation error if uncommented
	// T observer4 = std::move(observer); // This line should cause a compilation error if uncommented
	// T observer5{std::move(observer)}; // This line should cause a compilation error if uncommented
	(void)observer; // To avoid unused variable warning
}

void test_subscription_move_leak()
{
	Event<int> event;
	int callCount1 = 0;
	int callCount2 = 0;

	EventSubscription sub1 = event.observer().subscribe([&callCount1](int) {
		callCount1++;
		return true;
	});

	EventSubscription sub2 = event.observer().subscribe([&callCount2](int) {
		callCount2++;
		return true;
	});

	// Присваивание активной подписке sub1 нового значения через перемещение из sub2.
	// Это должно НЕМЕДЛЕННО удалить старый callback (callCount1) из списка подписок Event.
	sub1 = std::move(sub2);

	event.fire(10);

	// Бывшая подписка sub1 не должна вызываться
	assert(callCount1 == 0);
	// Новая подписка sub1 (перемещенная из sub2) должна сработать ровно один раз
	assert(callCount2 == 1);
}

void test_event_reentrancy_and_has_subscribers()
{
	Event<int> event;
	EventSubscription sub1;
	EventSubscription sub2;
	int callCount = 0;

	sub1 = event.observer().subscribe([&](int) {
		callCount++;
		// Само отписка во время рассылки события (Reentrancy)
		sub1.reset();
		// Метод hasSubscribers() должен динамически понять, что sub2 еще жив, несмотря на то, что sub1 удален
		assert(event.observer().hasSubscribers() == true);
		return true;
	});

	sub2 = event.observer().subscribe([&](int) {
		callCount++;
		// Само отписка второго подписчика
		sub2.reset();
		// Все подписчики удалены, hasSubscribers() возвращает false во время выполнения fire()
		assert(event.observer().hasSubscribers() == false);
		return true;
	});

	assert(event.observer().hasSubscribers() == true);
	event.fire(42);
	assert(callCount == 2);
	assert(event.observer().hasSubscribers() == false);
}

void test_event_move_and_lifetime()
{
	Event<int>* source_ev = new Event<int>();
	bool called = false;

	EventSubscription sub = source_ev->observer().subscribe([&called](int) {
		called = true;
		return true;
	});

	// Перемещаем Event во временный объект, а старый полностью освобождаем в куче
	Event<int> target_ev = std::move(*source_ev);
	delete source_ev;

	// Подписка sub должна остаться невредимой, так как внутренний Observer "приколот" (pinned) на куче
	assert(sub.isActive());
	target_ev.fire(123);
	assert(called);

	// Отмена подписки после смерти родительского контейнера не должна падать или течь
	sub.reset();
	assert(!sub.isActive());
}

int main()
{
	const EventTest test;

	{
		// Test simple events
		auto sub1 = test.eventSimple().subscribe([]()
		{
			std::cout << "void: fired (sub1)" << std::endl;
			return true;
		});

		const auto sub2 = test.eventString().subscribe([](const std::string& message)
		{
			std::cout << "string: fired (sub2) with message = " << message << std::endl;
			return true;
		});

		const auto sub3 = test.eventString().subscribe([](std::string message)
		{
			std::cout << "string: fired (sub3) with message = " << message << std::endl;
			return true;
		});

		const auto sub4 = test.eventInt().subscribe([](int value)
		{
			std::cout << "int: fired (sub4) with value = " << value << std::endl;
			return true;
		});

		std::cout << "++ Firing events for the first time:" << std::endl;
		test.fireSimpleAndStringEvents();

		sub1.reset();

		std::cout << "++ Firing after sub1 unsubscribed:" << std::endl;
		test.fireSimpleAndStringEvents();
	} // here all subscriptions go out of scope and are automatically unsubscribed

	// Test if no subscribers left
	assert(!test.eventString().hasSubscribers());

	// Fire simple event again to see that there are no subscribers
	std::cout << "++ Firing after all subscriptions have been destroyed:" << std::endl;
	test.fireSimpleAndStringEvents();

	// Test int event with priority and parameter modification
	const auto sub5 = test.eventInt().subscribe([](int value)
	{
		std::cout << "int: fired (sub4), default priority, value = " << value << std::endl;
		return true;
	});

	// Add modifier to int event subscription
	const auto sub6 = test.eventInt().subscribe([](int& value)
	{
		std::cout << "int: fired (sub6), medium priority, value = " << value << " (changing to 999)" << std::endl;
		value = 999;
		return true; // Continue firing
	}, 150);		 // Medium priority

	// Add priority to int event subscription
	const auto sub7 = test.eventInt().subscribe([](int value)
	{
		std::cout << "int: fired (sub5), high priority, value = " << value << std::endl;
		return true; // Continue firing
	}, 200);		 // Higher priority


	// Fire events again to see the effect of priority
	test.fireIntEvent();

	// Test if subscriber can outlive the Event
	EventSubscription sub;
	{
		const EventTest test2;
		sub = test2.eventInt().subscribe([](int) { return true; });
		assert(sub.isActive());
	}
	assert(!sub.isActive());

	// Will the subscription stay if moved out
	{
		auto sub8 = test.eventInt().subscribe([](int) { return true; });
		assert(sub8.isActive() && !sub.isActive());
		sub = std::move(sub8);
		assert(!sub8.isActive() && sub.isActive());
	}
	assert(sub.isActive());

	test_event_source_could_be_moved();
	test_event_subscription_could_be_moved();
	test_observer_could_not_be_moved();
	test_subscription_move_leak();
	test_event_reentrancy_and_has_subscribers();
	test_event_move_and_lifetime();

	return 0;
}
