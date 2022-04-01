#pragma once

#include <functional>
#include <memory>
#include <boost/signals2.hpp>

namespace v {

template <class T> using Signal = boost::signals2::signal<T>;
template <class T> using Slot = boost::signals2::slot<T>;
using ScopedConnection = boost::signals2::scoped_connection;

template <class T>
struct Observable
{
	using Signal = v::Signal<T>;
	using Slot = v::Slot<T>;

	auto observe(Slot slot) { return signal_.connect(slot); }

	template <class ... Args>
	auto notify(Args... args) { return signal_(args...); }

	template <class ... Args>
	auto operator()(Args... args) { return notify(args...); }

private:

	Signal signal_;
};

template <class Observer>
class ValueConnection
{
public:

	using Slot = std::function<void()>;

	ValueConnection() = default;
	ValueConnection(const ValueConnection& rhs) = default;
	ValueConnection(ValueConnection && rhs) = default;
	ValueConnection& operator=(const ValueConnection& rhs) = default;
	ValueConnection& operator=(ValueConnection && rhs) = default;

	ValueConnection(Observer observer, Slot slot, bool start_disconnected = false)
		: observer_ { observer }
		, slot_ { slot }
	{
		if (!start_disconnected) connect();
	}

	auto connect()
	{
		connection_ = observer_.observe(slot_);
	}

	auto disconnect()
	{
		connection_.disconnect();
	}

	auto slot() const { slot_(); }
	auto get() const { return observer_.get(); }
	auto operator*() const { return get(); }

private:

	Observer observer_;
	Slot slot_;
	ScopedConnection connection_;
};

template <class T>
class ValueObserver
{
public:

	using Slot = boost::signals2::slot<void()>;
	using Connector = std::function<boost::signals2::connection(Slot)>;

	ValueObserver() = default;
	ValueObserver(const ValueObserver& rhs) = default;
	ValueObserver(ValueObserver && rhs) = default;
	ValueObserver& operator=(const ValueObserver& rhs) = default;
	ValueObserver& operator=(ValueObserver && rhs) = default;

	ValueObserver(const T* value, Connector connector)
		: value_ { value }
		, connector_ { connector }
	{}

	auto get() const { return *value_; }
	auto operator*() const { return get(); }
	auto observe(Slot slot) { return connector_(slot); }

private:

	const T* value_ {};
	Connector connector_;
};

template <class T>
class GetterObserver
{
public:

	using Getter = std::function<T()>;
	using Slot = boost::signals2::slot<void()>;
	using Connector = std::function<boost::signals2::connection(Slot)>;

	GetterObserver() = default;
	GetterObserver(const GetterObserver& rhs) = default;
	GetterObserver(GetterObserver && rhs) = default;
	GetterObserver& operator=(const GetterObserver& rhs) = default;
	GetterObserver& operator=(GetterObserver && rhs) = default;

	GetterObserver(Getter getter, Connector connector)
		: getter_ { getter }
		, connector_ { connector }
	{
	}

	auto get() const { return getter_(); }
	auto operator()() const { return get(); }
	auto observe(Slot slot) { return connector_(slot); }
	operator bool() const { return bool(getter_); }

private:

	Getter getter_;
	Connector connector_;
};

template <class T>
class ObservableValue
{
public:

	ObservableValue(const T & value) : value_ { value } {}

	bool operator==(const T& value) const { return value_ == value; }

	auto& operator=(const T& value)
	{
		set(value);

		return *this;
	}

	auto set(const T& value, bool notify = true, bool force = false) -> void
	{
		if (value == value_ && !force) return;

		value_ = value;

		if (notify) this->notify();
	}

	auto notify() -> void
	{
		signal_();
	}

	auto observe(boost::signals2::slot<void()> slot)
	{
		return signal_.connect(slot);
	}

	auto observer()
	{
		const auto connect { [this](boost::signals2::slot<void()> slot)
		{
			return signal_.connect(slot);
		}};

		return ValueObserver<T> { &value_, connect };
	}

	auto& get() const { return value_; }
	auto& operator*() const { return get(); }
	auto operator->() const { return &value_; }

private:

	T value_;
	boost::signals2::signal<void()> signal_;
};

template <class T>
class ObservableGetter
{
public:

	using Getter = std::function<T()>;

	ObservableGetter(Getter getter) : getter_ { getter } {}

	auto notify() -> void
	{
		signal_();
	}

	auto observe(boost::signals2::slot<void()> slot)
	{
		return signal_.connect(slot);
	}

	auto observer()
	{
		const auto connect { [this](boost::signals2::slot<void()> slot)
		{
			return signal_.connect(slot);
		}};

		return GetterObserver<T> { getter_, connect };
	}

	auto get() const { return getter_(); }
	auto operator()() const { return get(); }

private:

	Getter getter_;
	boost::signals2::signal<void()> signal_;
};

} // v
