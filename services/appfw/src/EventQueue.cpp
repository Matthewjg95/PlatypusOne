#include "platypus/appfw/EventQueue.hpp"

#include "platypus/appfw/IApp.hpp"

#include <type_traits>
#include <utility>

namespace platypus::appfw {

EventQueue::EventQueue(std::size_t capacity) : capacity_(capacity) {}

bool EventQueue::post(const hal::TouchEvent& event) { return postEvent(event); }

bool EventQueue::post(const hal::ButtonEvent& event) { return postEvent(event); }

std::size_t EventQueue::dispatchPending(IApp& app) {
    std::size_t dispatched = 0;
    while (auto event = tryPop()) {
        std::visit(
            [&app](const auto& value) {
                using EventType = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<EventType, hal::TouchEvent>) {
                    app.onTouch(value);
                } else if constexpr (std::is_same_v<EventType, hal::ButtonEvent>) {
                    app.onButton(value);
                }
            },
            *event);
        ++dispatched;
    }
    return dispatched;
}

std::size_t EventQueue::size() const {
    const std::scoped_lock lock(mutex_);
    return events_.size();
}

std::size_t EventQueue::droppedCount() const {
    const std::scoped_lock lock(mutex_);
    return droppedCount_;
}

bool EventQueue::postEvent(Event event) {
    const std::scoped_lock lock(mutex_);
    if (events_.size() >= capacity_) {
        ++droppedCount_;
        return false;
    }
    events_.push_back(std::move(event));
    return true;
}

std::optional<EventQueue::Event> EventQueue::tryPop() {
    const std::scoped_lock lock(mutex_);
    if (events_.empty()) {
        return std::nullopt;
    }

    auto event = std::move(events_.front());
    events_.pop_front();
    return event;
}

}  // namespace platypus::appfw
