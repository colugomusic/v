#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <boost/signals2.hpp>

namespace v {

template <class T> using slot = boost::signals2::slot<T>;
using cn = boost::signals2::connection;
using scoped_cn = boost::signals2::scoped_connection;

template <class T>
struct signal
{
	template <typename Slot>
	auto observe(Slot && slot) { return signal_.connect(std::forward<Slot>(slot)); }

	template <typename Slot>
	auto operator>>(Slot && slot) { return observe(std::forward<Slot>(slot)); }

	template <class ... Args>
	auto notify(Args && ... args) { return signal_(std::forward<Args>(args)...); }

	template <class ... Args>
	auto operator()(Args && ... args) { return notify(std::forward<Args>(args)...); }

private:

	boost::signals2::signal<T> signal_;
};

class store
{
public:

	auto operator+=(scoped_cn && c) -> void
	{
		connections_.push_back(std::move(c));
	}

private:

	std::vector<scoped_cn> connections_;
};

template <class Observer>
class value_cn
{
public:

	using slot_t = std::function<void()>;

	value_cn() = default;
	value_cn(const value_cn& rhs) = default;
	value_cn(value_cn && rhs) = default;
	value_cn& operator=(const value_cn& rhs) = default;
	value_cn& operator=(value_cn && rhs) = default;

	value_cn(Observer observer, slot_t slot, bool start_disconnected = false)
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
	slot_t slot_;
	scoped_cn connection_;
};

template <class T>
class property_observer
{
public:

	using connector_t = std::function<cn(slot<void()>)>;

	property_observer() = default;
	property_observer(const property_observer& rhs) = default;
	property_observer(property_observer && rhs) = default;
	property_observer& operator=(const property_observer& rhs) = default;
	property_observer& operator=(property_observer && rhs) = default;

	property_observer(const T* value, connector_t connector)
		: value_ { value }
		, connector_ { connector }
	{}

	auto get() const { return *value_; }
	auto operator*() const { return get(); }

	template <typename Slot>
	auto observe(Slot && slot) { return connector_(std::forward<Slot>(slot)); }

	template <typename Slot>
	auto operator>>(Slot && slot) { return observe(std::forward<Slot>(slot)); }

private:

	const T* value_ {};
	connector_t connector_;
};

template <class T>
class getter_observer
{
public:

	using getter_t = std::function<T()>;
	using connector_t = std::function<cn(slot<void()>)>;

	getter_observer() = default;
	getter_observer(const getter_observer& rhs) = default;
	getter_observer(getter_observer && rhs) = default;
	getter_observer& operator=(const getter_observer& rhs) = default;
	getter_observer& operator=(getter_observer && rhs) = default;

	getter_observer(getter_t getter, connector_t connector)
		: getter_ { getter }
		, connector_ { connector }
	{
	}

	auto get() const { return getter_(); }
	auto operator()() const { return get(); }
	operator bool() const { return bool(getter_); }

	template <typename Slot>
	auto observe(Slot && slot) { return connector_(std::forward<Slot>(slot)); }

	template <typename Slot>
	auto operator>>(Slot && slot) { return observe(std::forward<Slot>(slot)); }

private:

	getter_t getter_;
	connector_t connector_;
};

template <class T> using property_cn = value_cn<property_observer<T>>;
template <class T> using getter_cn = value_cn<getter_observer<T>>;

template <class T> class read_only_property;

template <class T>
class property_setter
{
public:

	property_setter(property_setter && rhs) = default;

	property_setter(read_only_property<T>* property)
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

	read_only_property<T>* property_;
};

template <class T>
class read_only_property
{
public:

	read_only_property() : value_ {} {}
	read_only_property(T value) : value_ { value } {}
	read_only_property(read_only_property<T> && rhs) = default;

	bool operator==(const T& value) const { return value_ == value; }

	auto notify() -> void
	{
		signal_();
	}

	template <typename Slot>
	auto observe(Slot && slot) { return signal_.connect(std::forward<Slot>(slot)); }

	template <typename Slot>
	auto operator>>(Slot && slot) { return observe(std::forward<Slot>(slot)); }

	auto observer()
	{
		const auto connect { [this](boost::signals2::slot<void()> slot)
		{
			return signal_.connect(slot);
		}};

		return property_observer<T> { &value_, connect };
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

	friend class property_setter<T>;

	T value_;
	boost::signals2::signal<void()> signal_;
};

template <class T>
class property : public read_only_property<T>
{
public:

	property()
		: setter_{ this }
	{
	}

	property(const T & value)
		: read_only_property<T> { value }
		, setter_{ this }
	{
	}

	property(property && rhs)
		: read_only_property<T> { std::move(rhs) }
		, setter_{ this }
	{
	}

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

	property_setter<T> setter_;
};

template <class T>
class one_shot_property : public property<T>
{
public:

	one_shot_property(const T & value)
		: property<T> { value }
	{
	}

	template <class U>
	auto& operator=(U && value)
	{
		if (flag_) return *this;

		property<T>::operator=(std::forward<U>(value));

		flag_ = true;

		return *this;
	}

	auto set(const T& value, bool notify = true, bool force = false) -> void
	{
		if (flag_) return;

		property<T>::set(value, notify, force);

		flag_ = true;
	}

private:

	bool flag_{ false };
};

template <class T>
class getter
{
public:

	using getter_fn = std::function<T()>;

	getter() = default;
	getter(getter_fn getter) : getter_ { getter } {}

	auto notify() -> void
	{
		signal_();
	}

	template <typename Slot>
	auto observe(Slot && slot) { return signal_.connect(std::forward<Slot>(slot)); }

	template <typename Slot>
	auto operator>>(Slot && slot) { return observe(std::forward<Slot>(slot)); }

	auto observer()
	{
		const auto connect { [this](boost::signals2::slot<void()> slot)
		{
			return signal_.connect(slot);
		}};

		return getter_observer<T> { getter_, connect };
	}

	auto set(getter_fn getter) { getter_ = getter; }
	auto get() const { return getter_(); }
	auto operator()() const { return get(); }
	auto operator*() const { return get(); }

private:

	getter_fn getter_;
	boost::signals2::signal<void()> signal_;
};

class expiry_token
{
public:

	auto expire() -> void
	{
		if (expired_) return;

		expired_ = true;
		signal_();
	}

	auto is_expired() const -> bool
	{
		return expired_;
	}

	template <typename Slot>
	auto observe_expiry(Slot && slot)
	{
		return signal_ >> std::forward<Slot>(slot);
	}

private:

	signal<void()> signal_;
	bool expired_{false};
};

struct with_expiry_token
{
	auto get_expiry_token() -> expiry_token&
	{
		return token_;
	}

	auto get_expiry_token() const -> const expiry_token&
	{
		return token_;
	}

private:

	expiry_token token_;
};

struct custom_expiry_token
{
	virtual auto get_expiry_token() -> expiry_token& = 0;
	virtual auto get_expiry_token() const -> const expiry_token& = 0;
};

template <typename TokenPolicy = with_expiry_token>
class expirable : public TokenPolicy
{
public:

	auto expire() -> void
	{
		get_expiry_token().expire();
	}

	auto is_expired() const -> bool
	{
		return get_expiry_token().is_expired();
	}

	template <typename Slot>
	auto observe_expiry(Slot && slot)
	{
		return get_expiry_token().observe_expiry(slot);
	}
};

template <typename T>
class attacher
{
public:

	template <typename U>
	auto operator<<(U* object) -> void
	{
		const auto on_expired = [=]()
		{
			attached_objects_.erase(reinterpret_cast<intptr_t>(object));
			static_cast<T*>(this)->detach(object);
		};

		static_cast<T*>(this)->attach(object);
		attached_objects_[reinterpret_cast<intptr_t>(object)] = object->observe_expiry(on_expired);
	}

private:

	std::unordered_map<intptr_t, v::scoped_cn> attached_objects_;
};

} // v
