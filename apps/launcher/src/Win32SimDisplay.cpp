#ifdef _WIN32
#include "Win32SimDisplay.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace platypus::sim {

namespace {
constexpr int kWidth = 320;
constexpr int kHeight = 240;
constexpr int kScale = 2;
constexpr const wchar_t* kClassName = L"PlatypusSimDisplay";
}  // namespace

struct Win32SimDisplay::Impl {
    HWND hwnd = nullptr;
    std::vector<std::uint16_t> pixels = std::vector<std::uint16_t>(kWidth * kHeight, 0);
    std::function<void(const hal::TouchEvent&)> touch;
    std::function<void(const hal::ButtonEvent&)> button;
    bool mouseDown = false;

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (msg) {
            case WM_NCCREATE: {
                const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                                  reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return DefWindowProcW(hwnd, msg, wp, lp);
            }
            case WM_PAINT: {
                PAINTSTRUCT ps;
                const HDC dc = BeginPaint(hwnd, &ps);
                if (self) self->blit(dc);
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_MOUSEMOVE:
                if (self) self->handleMouse(hwnd, msg, lp);
                return 0;
            case WM_KEYDOWN:
            case WM_KEYUP:
                // Number keys act as the handheld's physical buttons.
                if (self && self->button && wp >= '0' && wp <= '9') {
                    self->button({static_cast<std::uint8_t>(wp - '0'), msg == WM_KEYDOWN});
                }
                return 0;
            case WM_CLOSE:
                // Simulates the power button: forward as button 0 press.
                if (self && self->button) self->button({0, true});
                return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    void handleMouse(HWND wnd, UINT msg, LPARAM lp) {
        const auto x = static_cast<std::uint16_t>(
            std::max<int>(0, std::min<int>(kWidth - 1, GET_X_LPARAM(lp) / kScale)));
        const auto y = static_cast<std::uint16_t>(
            std::max<int>(0, std::min<int>(kHeight - 1, GET_Y_LPARAM(lp) / kScale)));

        hal::TouchEvent ev{hal::TouchEvent::Type::Move, x, y};
        if (msg == WM_LBUTTONDOWN) {
            mouseDown = true;
            ev.type = hal::TouchEvent::Type::Down;
            SetCapture(wnd);
        } else if (msg == WM_LBUTTONUP) {
            mouseDown = false;
            ev.type = hal::TouchEvent::Type::Up;
            ReleaseCapture();
        } else if (!mouseDown) {
            return;  // hover without press is not touch
        }
        if (touch) touch(ev);
    }

    void blit(HDC dc) {
        // RGB565 DIB: BI_BITFIELDS with explicit channel masks.
        struct Rgb565Header {
            BITMAPINFOHEADER h;
            DWORD masks[3];
        } bmi{};
        bmi.h.biSize = sizeof(BITMAPINFOHEADER);
        bmi.h.biWidth = kWidth;
        bmi.h.biHeight = -kHeight;  // top-down
        bmi.h.biPlanes = 1;
        bmi.h.biBitCount = 16;
        bmi.h.biCompression = BI_BITFIELDS;
        bmi.masks[0] = 0xF800;
        bmi.masks[1] = 0x07E0;
        bmi.masks[2] = 0x001F;
        StretchDIBits(dc, 0, 0, kWidth * kScale, kHeight * kScale,
                      0, 0, kWidth, kHeight, pixels.data(),
                      reinterpret_cast<const BITMAPINFO*>(&bmi),
                      DIB_RGB_COLORS, SRCCOPY);
    }

    void pumpMessages() {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
};

Win32SimDisplay::Win32SimDisplay() : impl_(std::make_unique<Impl>()) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = &Impl::wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
    RegisterClassW(&wc);  // idempotent enough: second registration fails harmlessly

    RECT rect{0, 0, kWidth * kScale, kHeight * kScale};
    AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
    impl_->hwnd = CreateWindowExW(
        0, kClassName, L"PlatypusOS Simulator",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, wc.hInstance, impl_.get());
}

Win32SimDisplay::~Win32SimDisplay() {
    if (impl_->hwnd) DestroyWindow(impl_->hwnd);
}

hal::DisplayInfo Win32SimDisplay::info() const noexcept {
    return {kWidth, kHeight, 16};
}

hal::Status Win32SimDisplay::setBacklight(float) { return {}; }

hal::Status Win32SimDisplay::present(std::span<const std::byte> pixels) {
    const auto expected = static_cast<std::size_t>(kWidth) * kHeight * 2;
    if (pixels.size() != expected) return hal::Error::InvalidArgument;

    std::memcpy(impl_->pixels.data(), pixels.data(), expected);
    if (impl_->hwnd) {
        InvalidateRect(impl_->hwnd, nullptr, FALSE);
        UpdateWindow(impl_->hwnd);
        // The shell loop is single-threaded; presenting is the natural place
        // to service the window's message queue (input arrives here too).
        impl_->pumpMessages();
    }
    return {};
}

hal::Status Win32SimDisplay::onTouch(std::function<void(const hal::TouchEvent&)> handler) {
    impl_->touch = std::move(handler);
    return {};
}

hal::Status Win32SimDisplay::onButton(std::function<void(const hal::ButtonEvent&)> handler) {
    impl_->button = std::move(handler);
    return {};
}

}  // namespace platypus::sim
#endif  // _WIN32
