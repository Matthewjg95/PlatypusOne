// AppRegistry contract tests: unique ids, factory dispatch, unknown lookup.
#include <platypus/appfw/AppRegistry.hpp>

#include <cassert>
#include <cstdio>

namespace {

using namespace platypus;

class DummyApp final : public appfw::IApp {
public:
    const appfw::AppManifest& manifest() const noexcept override { return m_; }
    void onStart(appfw::AppContext&) override {}
    void onStop() override {}
    void onFrame(appfw::AppContext&, std::chrono::milliseconds) override {}

private:
    appfw::AppManifest m_{"test.dummy", "Dummy", "1.0.0", false, false};
};

std::unique_ptr<appfw::IApp> makeDummy() { return std::make_unique<DummyApp>(); }

}  // namespace

void test_app_registry() {
    appfw::AppRegistry registry;
    appfw::AppManifest manifest{"test.dummy", "Dummy", "1.0.0", false, false};

    assert(registry.add(manifest, &makeDummy));
    assert(!registry.add(manifest, &makeDummy));  // duplicate id rejected
    assert(!registry.add({"x", "X", "1", false, false}, nullptr));  // null factory

    assert(registry.manifests().size() == 1);
    assert(registry.create("test.dummy") != nullptr);
    assert(registry.create("test.unknown") == nullptr);

    std::puts("test_app_registry: OK");
}
