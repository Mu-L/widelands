/*
 * Copyright (C) 2006-2025 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef WL_NOTIFICATIONS_NOTIFICATIONS_IMPL_H
#define WL_NOTIFICATIONS_NOTIFICATIONS_IMPL_H

#include <algorithm>
#include <cassert>
#include <list>
#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "base/mutex.h"

namespace Notifications {

// Subscribes to a notification type and unsubscribes on destruction.
template <typename T> class Subscriber {
public:
	Subscriber(uint32_t id, const std::function<void(const T&)>& callback)
	   : id_(id), callback_(callback) {
	}

	~Subscriber();

private:
	friend class NotificationsManager;

	uint32_t id_;
	std::function<void(const T&)> callback_;

	DISALLOW_COPY_AND_ASSIGN(Subscriber);
};

// Singleton that dispatches notifications and keeps track of all subscribers.
// Implementation detail. Instead use the functions from the public header.
class NotificationsManager {
public:
	// Returns the Singleton. Will create it if it does not yet exist.
	static NotificationsManager* get();

	// Creates a subscriber for 'T' with the given 'callback' and returns it.
	template <typename T>
	std::unique_ptr<Subscriber<T>> subscribe(std::function<void(const T&)> callback) {
		MutexLock m(get_mutex(T::note_id()));
		std::list<void*>& subscribers = note_id_to_subscribers_[T::note_id()];
		auto new_subscriber =
		   std::unique_ptr<Subscriber<T>>(new Subscriber<T>(next_subscriber_id_, callback));
		subscribers.push_back(new_subscriber.get());
		++next_subscriber_id_;
		++num_subscribers_;
		return new_subscriber;
	}

	// Publishes 'message' to all subscribers.
	template <typename T> void publish(const T& message) {
		MutexLock m(get_mutex(T::note_id()));
		for (void* p_subscriber : note_id_to_subscribers_[T::note_id()]) {
			Subscriber<T>* subscriber = static_cast<Subscriber<T>*>(p_subscriber);
			subscriber->callback_(message);
		}
	}

	// Unsubscribes 'subscriber'.
	template <typename T> void unsubscribe(Subscriber<T>* subscriber) {
		MutexLock m(get_mutex(T::note_id()));

		std::list<void*>& subscribers = note_id_to_subscribers_.at(T::note_id());
		auto subscribers_it = std::find_if(
		   subscribers.begin(), subscribers.end(), [&subscriber](const void* p_subscriber) {
			   return static_cast<const Subscriber<T>*>(p_subscriber)->id_ == subscriber->id_;
		   });

		assert(subscribers_it != subscribers.end());
		subscribers.erase(subscribers_it);
		--num_subscribers_;
	}

private:
	// Private constructor for Singleton.
	NotificationsManager() = default;

	// Checks that there are no more subscribers.
	~NotificationsManager();

	MutexLock::ID get_mutex(uint32_t id) {
		const auto it = mutexes_.find(id);
		if (it != mutexes_.end()) {
			return it->second;
		}
		MutexLock::ID m = MutexLock::create_custom_mutex();
		mutexes_.emplace(id, m);
		return m;
	}

	uint32_t next_subscriber_id_{1};
	uint32_t num_subscribers_{0U};

	// Ideally we would like to keep a list<Subscriber<T>*> instead of void* to
	// be typesafe. Unfortunately, C++ does not allow for an easy way. I could
	// introduce a base class and dispatch via a virtual function call, but
	// since this framework should be as efficient as possible, I opted for
	// using void* and casting instead.
	std::unordered_map<uint32_t, std::list<void*>> note_id_to_subscribers_;
	std::unordered_map<uint32_t, MutexLock::ID> mutexes_;

	DISALLOW_COPY_AND_ASSIGN(NotificationsManager);
};

template <typename T> Subscriber<T>::~Subscriber() {
	NotificationsManager::get()->unsubscribe<T>(this);
}

}  // namespace Notifications

#endif  // end of include guard: WL_NOTIFICATIONS_NOTIFICATIONS_IMPL_H
