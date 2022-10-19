#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <boost/signals2.hpp>

namespace v {

template <class T> using slot = boost::signals2::slot<T>;
using cn = boost::signals2::connection;
using scoped_cn = boost::signals2::scoped_connection;

namespace detail {

template <typename SignalType>
struct signal_base
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

	SignalType signal_;
};

template <class T>
using boost_signal = typename boost::signals2::signal_type<T, boost::signals2::keywords::mutex_type<boost::signals2::dummy_mutex>>::type;

template <class T>
using boost_mt_signal = boost::signals2::signal<T>;

} // detail

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

namespace detail {

template <class T, class SignalType>
class read_only_property_base;

template <class T, class SignalType>
class property_setter_base
{
public:

	property_setter_base(property_setter_base && rhs) = default;

	property_setter_base(read_only_property_base<T, SignalType>* property)
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

	read_only_property_base<T, SignalType>* property_;
};

template <class T, class SignalType>
class read_only_property_base
{
public:

	read_only_property_base() : value_ {} {}
	read_only_property_base(T value) : value_ { value } {}
	read_only_property_base(read_only_property_base<T, SignalType> && rhs) = default;

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
		const auto connect { [this](v::slot<void()> slot)
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

	friend class property_setter_base<T, SignalType>;

	T value_;
	SignalType signal_;
};

template <class T, class SignalType>
class property_base : public read_only_property_base<T, SignalType>
{
public:

	property_base()
		: setter_{ this }
	{
	}

	property_base(const T & value)
		: read_only_property_base<T, SignalType> { value }
		, setter_{ this }
	{
	}

	property_base(property_base && rhs)
		: read_only_property_base<T, SignalType> { std::move(rhs) }
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

	property_setter_base<T, SignalType> setter_;
};

template <class T, class SignalType>
class one_shot_property_base : public property_base<T, SignalType>
{
public:

	one_shot_property_base(const T & value)
		: property_base<T, SignalType> { value }
	{
	}

	template <class U>
	auto& operator=(U && value)
	{
		if (flag_) return *this;

		property_base<T, SignalType>::operator=(std::forward<U>(value));

		flag_ = true;

		return *this;
	}

	auto set(const T& value, bool notify = true, bool force = false) -> void
	{
		if (flag_) return;

		property_base<T, SignalType>::set(value, notify, force);

		flag_ = true;
	}

private:

	bool flag_{ false };
};

template <class T, class SignalType>
class getter_base
{
public:

	using getter_fn = std::function<T()>;

	getter_base() = default;
	getter_base(getter_fn getter) : getter_ { getter } {}

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
		const auto connect { [this](v::slot<void()> slot)
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
	SignalType signal_;
};

} // detail

template <typename T> using getter = detail::getter_base<T, detail::boost_signal<void()>>;
template <typename T> using property = detail::property_base<T, detail::boost_signal<void()>>;
template <typename T> using property_setter = detail::property_setter_base<T, detail::boost_signal<void()>>;
template <typename T> using read_only_property = detail::read_only_property_base<T, detail::boost_signal<void()>>;
template <typename T> using signal = detail::signal_base<detail::boost_signal<T>>;

namespace mt {

template <typename T> using mt_getter = detail::getter_base<T, detail::boost_mt_signal<void()>>;
template <typename T> using mt_property = detail::property_base<T, detail::boost_mt_signal<void()>>;
template <typename T> using mt_property_setter = detail::property_setter_base<T, detail::boost_mt_signal<void()>>;
template <typename T> using mt_read_only_property = detail::read_only_property_base<T, detail::boost_mt_signal<void()>>;
template <typename T> using mt_signal = detail::signal_base<detail::boost_mt_signal<T>>;

} // mt

class expiry_token
{
public:

	~expiry_token()
	{
		expire();
	}

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

class expirable
{
public:

	auto expire() -> void
	{
		token_.expire();
	}

	auto is_expired() const -> bool
	{
		return token_.is_expired();
	}

	template <typename Slot>
	auto observe_expiry(Slot && slot)
	{
		return token_.observe_expiry(slot);
	}

	auto get_expiry_token() -> expiry_token& { return token_; }
	auto get_expiry_token() const -> const expiry_token& { return token_; }

private:

	expiry_token token_;
};

class expirable_with_custom_expiry_token
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

private:

	virtual auto get_expiry_token() -> expiry_token& = 0;
	virtual auto get_expiry_token() const -> const expiry_token& = 0;
};

template <typename Expirable, typename Slot>
auto observe_expiry(Expirable* object, Slot && slot) { return object->observe_expiry(std::forward<Slot>(slot)); }

template <typename T> struct attach { T object; auto operator->() const { return object; } operator T() const { return object; } };
template <typename T> struct detach { T object; auto operator->() const { return object; } operator T() const { return object; } };

template <typename T>
class attacher
{
public:

	template <typename U>
	auto operator<<(U object) -> void
	{
		attach(object);
	}

	template <typename U>
	auto operator>>(U object) -> void
	{
		detach(object);
	}

private:

	template <typename U>
	auto attach(U object) -> void
	{
		const auto on_expired = [=]()
		{
			detach(object);
		};

		static_cast<T*>(this)->update(v::attach<U>{object});
		attached_objects_[std::hash<U>()(object)] = observe_expiry(object, on_expired);
	}

	template <typename U>
	auto detach(U object) -> void
	{
		attached_objects_.erase(std::hash<U>()(object));
		static_cast<T*>(this)->update(v::detach<U>{object});
	}

	std::unordered_map<size_t, v::scoped_cn> attached_objects_;
};

} // v
