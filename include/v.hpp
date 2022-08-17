#pragma once

#include <functional>
#include <memory>
#include <boost/signals2.hpp>

namespace v {

template <class T> using Signal = boost::signals2::signal<T>;
template <class T> using Slot = boost::signals2::slot<T>;
using Connection = boost::signals2::connection;
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
class PropertyObserver
{
public:

	using Slot = boost::signals2::slot<void()>;
	using Connector = std::function<boost::signals2::connection(Slot)>;

	PropertyObserver() = default;
	PropertyObserver(const PropertyObserver& rhs) = default;
	PropertyObserver(PropertyObserver && rhs) = default;
	PropertyObserver& operator=(const PropertyObserver& rhs) = default;
	PropertyObserver& operator=(PropertyObserver && rhs) = default;

	PropertyObserver(const T* value, Connector connector)
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

template <class T> using PropertyConnection = ValueConnection<PropertyObserver<T>>;
template <class T> using GetterConnection = ValueConnection<GetterObserver<T>>;

template <class T> class ReadOnlyProperty;

template <class T>
class PropertySetter
{
public:

	PropertySetter(ReadOnlyProperty<T>* property)
		: property_{ property }
	{
	}

	template <class U>
	auto& operator=(U && value)
	{
		set(std::forward<U>(value));

		return *this;
	}

	template <class U>
	auto set(U && value, bool notify = true, bool force = false) -> void
	{
		property_->set(std::forward<U>(value), notify, force);
	}

private:

	ReadOnlyProperty<T>* property_;
};

template <class T>
class ReadOnlyProperty
{
public:

	ReadOnlyProperty() : value_ {} {}

	template <class U>
	ReadOnlyProperty(U && value) : value_ { std::forward<U>(value) } {}

	ReadOnlyProperty(ReadOnlyProperty<T> && rhs)
		: value_ { std::move(rhs.value_) }
		, signal_{ std::move(rhs.signal_) }
	{}

	bool operator==(const T& value) const { return value_ == value; }

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

		return PropertyObserver<T> { &value_, connect };
	}

	auto& get() const { return value_; }
	auto& operator*() const { return get(); }
	auto operator->() const { return &value_; }

private:

	template <class U>
	auto& operator=(U && value)
	{
		set(std::forward<U>(value));

		return *this;
	}

	template <class U>
	auto set(U && value, bool notify = true, bool force = false) -> void
	{
		if (value == value_ && !force) return;

		value_ = std::forward<U>(value);

		if (notify) this->notify();
	}

	friend class PropertySetter<T>;

	T value_;
	boost::signals2::signal<void()> signal_;
};

template <class T>
class Property : public ReadOnlyProperty<T>
{
public:

	Property()
		: setter_{ this }
	{
	}

	Property(const T & value)
		: ReadOnlyProperty<T> { value }
		, setter_{ this }
	{
	}

	Property(Property && rhs) = default;

	template <class U>
	auto& operator=(U && value)
	{
		setter_.operator=(std::forward<U>(value));

		return *this;
	}

	auto set(const T& value, bool notify = true, bool force = false) -> void
	{
		setter_.set(value, notify, force);
	}

private:

	PropertySetter<T> setter_;
};

template <class T>
class OneShotProperty : public Property<T>
{
public:

	OneShotProperty(const T & value)
		: Property<T> { value }
	{
	}

	template <class U>
	auto& operator=(U && value)
	{
		if (flag_) return *this;

		Property<T>::operator=(std::forward<U>(value));

		flag_ = true;

		return *this;
	}

	auto set(const T& value, bool notify = true, bool force = false) -> void
	{
		if (flag_) return;

		Property<T>::set(value, notify, force);

		flag_ = true;
	}

private:

	bool flag_{ false };
};

template <class T>
class Getter
{
public:

	using GetterFunc = std::function<T()>;

	Getter() = default;
	Getter(GetterFunc getter) : getter_ { getter } {}

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

	auto set(GetterFunc getter) { getter_ = getter; }
	auto get() const { return getter_(); }
	auto operator()() const { return get(); }

private:

	GetterFunc getter_;
	boost::signals2::signal<void()> signal_;
};

} // v
