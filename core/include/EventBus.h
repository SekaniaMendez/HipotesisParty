#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hp {

// Single-threaded typed events. Dispatch snapshots each listener list, so
// callbacks can subscribe/unsubscribe safely. Events published from a callback
// wait for the next Dispatch, preventing recursive cascades in the game loop.
class EventBus {
 public:
  using Subscription = std::uint64_t;
  EventBus() = default;
  EventBus(const EventBus&) = delete;
  EventBus& operator=(const EventBus&) = delete;

  template <class Event, class Callback>
  Subscription Subscribe(Callback&& callback) {
    const Subscription id = nextId_++;
    auto listener = std::make_shared<Listener>();
    listener->id = id;
    listener->callback = [fn = std::function<void(const Event&)>(
                              std::forward<Callback>(callback))](const void* data) {
      fn(*static_cast<const Event*>(data));
    };
    listeners_[typeid(Event)].push_back(std::move(listener));
    return id;
  }

  void Unsubscribe(Subscription id) {
    for (auto& [type, listeners] : listeners_) {
      for (auto& listener : listeners) {
        if (listener->id == id) listener->active = false;
      }
      std::erase_if(listeners, [](const auto& p) { return !p->active; });
    }
  }

  template <class Event>
  void Publish(Event event) {
    pending_.push_back([this, event = std::move(event)] {
      const auto found = listeners_.find(typeid(Event));
      if (found == listeners_.end()) return;
      const auto snapshot = found->second;
      for (const auto& listener : snapshot) {
        if (listener->active) listener->callback(&event);
      }
    });
  }

  std::size_t Dispatch(std::size_t maxEvents =
                           std::numeric_limits<std::size_t>::max()) {
    if (dispatching_) return 0;
    struct Guard {
      bool& flag;
      explicit Guard(bool& flag) : flag(flag) { flag = true; }
      ~Guard() { flag = false; }
    } guard(dispatching_);
    const std::size_t count = std::min(maxEvents, pending_.size());
    for (std::size_t i = 0; i < count; ++i) {
      auto event = std::move(pending_.front());
      pending_.pop_front();
      event();
    }
    return count;
  }

  std::size_t PendingCount() const { return pending_.size(); }

 private:
  struct Listener {
    Subscription id = 0;
    bool active = true;
    std::function<void(const void*)> callback;
  };
  std::unordered_map<std::type_index, std::vector<std::shared_ptr<Listener>>>
      listeners_;
  std::deque<std::function<void()>> pending_;
  Subscription nextId_ = 1;
  bool dispatching_ = false;
};

}  // namespace hp
