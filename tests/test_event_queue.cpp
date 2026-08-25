// EventQueue contract tests: FIFO dispatch, bounded overflow, and concurrent producers.
#include <platypus/appfw/EventQueue.hpp>
#include <platypus/appfw/IApp.hpp>

#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

using namespace platypus;

class RecordingApp final : public appfw::IApp {
public:
    const appfw::AppManifest& manifest() const noexcept override { return manifest_; }
    void onStart(appfw::AppContext&) override {}
    void onStop() override {}
    void onFrame(appfw::AppContext&, std::chrono::milliseconds) override {}

    void onTouch(const hal::TouchEvent&) override {
        order_.push_back('T');
        ++touchCount_;
    }

    void onButton(const hal::ButtonEvent&) override {
        order_.push_back('B');
        ++buttonCount_;
    }

    std::vector<char> order_;
    std::size_t touchCount_ = 0;
    std::size_t buttonCount_ = 0;

private:
    appfw::AppManifest manifest_{"test.events", "Events", "1.0.0", false, false};
};

}  // namespace

void test_event_queue() {
    {
        appfw::EventQueue queue(4);
        RecordingApp app;

        assert(queue.post(hal::TouchEvent{hal::TouchEvent::Type::Down, 10, 20}));
        assert(queue.post(hal::ButtonEvent{1, true}));
        assert(queue.size() == 2);
        assert(queue.dispatchPending(app) == 2);
        assert((app.order_ == std::vector<char>{'T', 'B'}));
        assert(queue.size() == 0);
        assert(queue.droppedCount() == 0);
    }

    {
        appfw::EventQueue queue(2);
        assert(queue.post(hal::ButtonEvent{1, true}));
        assert(queue.post(hal::ButtonEvent{1, false}));
        assert(!queue.post(hal::ButtonEvent{2, true}));
        assert(queue.size() == 2);
        assert(queue.droppedCount() == 1);
    }

    {
        constexpr std::size_t kProducerCount = 4;
        constexpr std::size_t kEventsPerProducer = 250;
        appfw::EventQueue queue(kProducerCount * kEventsPerProducer);
        std::vector<std::thread> producers;

        for (std::size_t producer = 0; producer < kProducerCount; ++producer) {
            producers.emplace_back([&queue, producer] {
                for (std::size_t event = 0; event < kEventsPerProducer; ++event) {
                    const auto id = static_cast<std::uint8_t>((producer + event) % 255);
                    assert(queue.post(hal::ButtonEvent{id, true}));
                }
            });
        }
        for (auto& producer : producers) {
            producer.join();
        }

        RecordingApp app;
        assert(queue.size() == kProducerCount * kEventsPerProducer);
        assert(queue.dispatchPending(app) == kProducerCount * kEventsPerProducer);
        assert(app.buttonCount_ == kProducerCount * kEventsPerProducer);
        assert(queue.droppedCount() == 0);
    }

    std::puts("test_event_queue: OK");
}
