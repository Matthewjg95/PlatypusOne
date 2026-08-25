// PlatypusOS app framework — thread-safe input handoff.
//
// Driver callbacks may run on transport or hardware threads. EventQueue copies
// their small value events into a bounded FIFO so IApp callbacks execute only
// when the UI thread explicitly dispatches them.
#pragma once

#include <platypus/hal/IDisplay.hpp>

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <variant>

namespace platypus::appfw {

class IApp;

class EventQueue {
public:
    explicit EventQueue(std::size_t capacity = 128);

    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;

    /// Copies an event into the queue. Returns false when the bounded queue is
    /// full; the rejected event is counted by droppedCount().
    [[nodiscard]] bool post(const hal::TouchEvent& event);
    [[nodiscard]] bool post(const hal::ButtonEvent& event);

    /// Delivers every currently queued event to app on the calling thread.
    /// Events posted concurrently remain safe and may be delivered by this or
    /// a later call.
    std::size_t dispatchPending(IApp& app);

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t droppedCount() const;

private:
    using Event = std::variant<hal::TouchEvent, hal::ButtonEvent>;

    [[nodiscard]] bool postEvent(Event event);
    [[nodiscard]] std::optional<Event> tryPop();

    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<Event> events_;
    std::size_t droppedCount_ = 0;
};

}  // namespace platypus::appfw
