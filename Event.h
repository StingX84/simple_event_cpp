#include <algorithm>
#include <cassert>
#include <list>
#include <memory>

/** Класс EventSubscription - управляет временем жизни подписки на событие из Event.
 *
 * Подписка считается активной с момента ее создания в Event::Observer::subscribe до момента:
 * - вызова reset(),
 * - уничтожения этого объекта EventSubscription,
 * - уничтожения Event, создавшего подписку.
 *
 * Объект, созданный конструктором по умолчанию, не является активной подпиской.
 */
class EventSubscription
{
public:
	EventSubscription() = default;
	EventSubscription(const EventSubscription&) = delete;

	EventSubscription(EventSubscription&& o) noexcept: sub_(std::move(o.sub_)) {}

	EventSubscription& operator= (EventSubscription&& o) noexcept
	{
		if (this != &o)
		{
			reset();
			sub_ = std::move(o.sub_);
		}
		return *this;
	}

	~EventSubscription() noexcept { reset(); }

	void swap(EventSubscription& o) noexcept { sub_.swap(o.sub_); }

	/** Возвращает true, если подписка активна */
	bool isActive() const noexcept { return !sub_.expired(); }

	/** Отменяет подписку */
	void reset() noexcept
	{
		if (auto s = sub_.lock())
			s->reset();
	}

	explicit operator bool() const noexcept { return isActive(); }

	bool operator!() const noexcept { return !isActive(); }

private:
	template<class... TArgs> friend class Event;

	/** Вспомогательная структура для хранения информации о подписке со стертым типом. */
	struct UntypedEntry: std::enable_shared_from_this<UntypedEntry>
	{
		virtual ~UntypedEntry() = default;
		virtual void reset() noexcept = 0;
	};

	/** Внутренний конструктор, вызывающийся из Event::Observer::subscribe */
	EventSubscription(const std::shared_ptr<UntypedEntry>& s): sub_(s) {}

	std::weak_ptr<UntypedEntry> sub_ {};
};

#if defined(__cpp_concepts)
/** Концепт EventHandlerConcept проверяет, что тип F является вызываемым с аргументами TArgs&... и возвращает bool */
template<typename F, typename... TArgs>
concept EventHandlerConcept
	= std::invocable<F, TArgs&...> && std::convertible_to<std::invoke_result_t<F, TArgs&...>, bool>;
#	define EVENT_HANDLER_CONCEPT EventHandlerConcept<TArgs...>
#else
#	define EVENT_HANDLER_CONCEPT class
#endif

/** Класс Event представляет собой источник событий, на которые можно подписываться.
 *
 * Thread safety:
 * Этот класс не является потокобезопасным.
 * Вызовы методов Observer::subscribe и fire должны выполняться из одного потока.
 *
 * Класс поддерживает версию стандарта C++11 и выше.
 *
 * Замечание: Если ваш код ориентирован на C++ < 14, то вместо `auto&` при возврате Observer
 * следует использовать явное указание типа. Например, `Event<std::string_view>::Observer&`.
 *
 * @tparam ...TArgs Типы аргументов, которые будут передаваться в обработчики событий.
 *
 * Пример использования:
 * @code
 * // Класс, создающий событие с аргументом типа std::string_view
 * class MyClass {
 *    Event<std::string_view> eventTest_; // Событие с аргументом типа std::string_view
 * public:
 *    auto& eventTest() const noexcept { return eventTest_.observer(); }
 *
 *    void someInternalFunction() const
 *    {
 *        eventTest_.fire("Hello, World!"); // Вызов события с аргументом
 *    }
 * };
 * // Подписка на событие
 * MyClass obj;
 * EventSubscription sub = obj.eventTest().subscribe([](std::string_view message) {
 *     // Обработка события
 *     return true;
 * });
 * obj.someInternalFunction(); // Вызовет обработчик события
 * @endcode
 * @see EventSubscription
 */
template<class... TArgs> class Event
{
public:
	Event() = default;
	Event(const Event&) = delete;
	Event(Event&& o) = default;
	Event& operator= (Event&& o) = default;

	void swap(Event& o) noexcept { observer_.swap(o.observer_); }

	/** Класс Observer позволяет подписываться на события */
	class Observer
	{
	public:

		/** Подписывает на событие с функцией обратного вызова и приоритетом (весом).
		 * @tparam F Тип функции обратного вызова.
		 * @param fn Функция обратного вызова. Сигнатура функции должна быть
		 * совместима с `bool(TArgs&...)`.
		 * @param weight Приоритет (вес) подписки. Чем больше вес, тем выше приоритет.
		 * @return Объект, сохраняющий подписку. Когда объект уничтожается, подписка
		 * автоматически отменяется.
		 */
		template<EVENT_HANDLER_CONCEPT F> EventSubscription subscribe(F fn, int weight = 100) noexcept
		{
			auto it = std::find_if(subscribers_.begin(), subscribers_.end(),
								   [weight](const std::shared_ptr<Entry>& e) { return e->weight_ < weight; });

			auto entry = std::make_shared<EntryCallable<F>>(*this, subscribers_.end(), weight, std::move(fn));
			if (it == subscribers_.end())
			{
				subscribers_.push_back(std::move(entry));
				auto& newEntry = subscribers_.back();
				newEntry->entryIt_ = std::prev(subscribers_.end());
				return {newEntry};
			}
			else
			{
				auto itNewEntry = subscribers_.insert(it, std::move(entry));
				(*itNewEntry)->entryIt_ = itNewEntry;
				return {*itNewEntry};
			}
		}

		/** Возвращает @c true, если есть хотя бы один подписчик на событие, иначе @c false. */
		bool hasSubscribers() const noexcept
		{
			return someEntriesRemoved_
					 ? std::any_of(subscribers_.begin(), subscribers_.end(),
								   [](const std::shared_ptr<Entry>& entry) { return static_cast<bool>(entry); })
					 : !subscribers_.empty();
		}

	private:
		Observer() = default;
		Observer(const Observer&) = delete;

		struct Entry;
		using EntryList = std::list<std::shared_ptr<Entry>>;

		/** Внутренняя структура Entry хранения информацию о подписке с возможностью вызова подписчика. */
		struct Entry: EventSubscription::UntypedEntry
		{
			Entry(Observer& observer, typename EntryList::iterator entryIt, int weight) noexcept
				: observer_(observer), entryIt_(entryIt), weight_(weight)
			{}

			virtual ~Entry() override = default;

			virtual void reset() noexcept override
			{
				if (observer_.enumerating_)
				{
					observer_.someEntriesRemoved_ = true;
					entryIt_->reset(); // Устанавливаем shared_ptr в nullptr, чтобы пометить запись как удаленную.
				}
				else
				{
					observer_.subscribers_.erase(entryIt_);
				}
			}

			virtual bool operator() (TArgs&...) = 0;

			Observer& observer_;
			typename EntryList::iterator entryIt_;
			int weight_;
		};

		/** Внутренняя структура EntryCallable представляет собой запись о подписке на событие с конкретным
		 * пользовательским функтором. Избавляет от необходимости использования std::function. */
		template<class F> struct EntryCallable final: Entry
		{
			EntryCallable(Observer& observer, typename EntryList::iterator it, int weight, F fn) noexcept
				: Entry {observer, it, weight}, fn_(std::forward<F>(fn))
			{}

			virtual ~EntryCallable() override = default;

			virtual bool operator() (TArgs&... args) override { return fn_(args...); }

			F fn_;
		};

		friend struct Entry;
		friend class Event;

		/** Множество подписчиков, отсортированное по приоритету (весу). */
		std::list<std::shared_ptr<Entry>> subscribers_;
		/** Счетчик, указывающий, что в данный момент происходит перечисление подписчиков. */
		int enumerating_ = 0;
		/** Флаг, указывающий, что во время перечисления были удалены некоторые подписчики. */
		bool someEntriesRemoved_ = false;
	};

	/** Возвращает объект, который позволяет подписываться на событие. */
	Observer& observer() const noexcept
	{
		assert(observer_); // Защита на случай, если объект Event был перемещен и состоит в "пустом" состоянии.
		return *observer_;
	}

	/** Вызывает всех подписчиков с заданными аргументами.
	 *
	 * Цепочка вызовов подписчиков прерывается, если какой-либо подписчик возвращает @c false.
	 *
	 * @param ...args Аргументы, передаваемые подписчикам.
	 * @return @c true, если все подписчики вернули @c true, иначе @c false.
	 */
	bool fire(TArgs... args) const noexcept
	{
		assert(observer_);
		bool rv = true;
		if (observer_)
		{
			++observer_->enumerating_;

			struct Cleanup
			{
				Observer& observer_;

				~Cleanup()
				{
					--observer_.enumerating_;
					if (observer_.enumerating_ == 0 && observer_.someEntriesRemoved_)
					{
						observer_.someEntriesRemoved_ = false;
						observer_.subscribers_.remove_if([](const std::shared_ptr<typename Observer::Entry>& entry)
						{ return !entry; });
					}
				}
			} cleanup {*observer_};

			for (const auto& s : observer_->subscribers_)
			{
				if (s && !static_cast<bool>((*s)(args...)))
				{
					rv = false;
					break;
				}
			}
		}
		return rv;
	}

	/** Возвращает @c true, если есть хотя бы один подписчик на событие, иначе @c false. */
	bool hasSubscribers() const noexcept
	{
		assert(observer_);
		return observer_ && observer_->hasSubscribers();
	}

private:
	std::unique_ptr<Observer> observer_ {new Observer()};
};
