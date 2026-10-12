#include "raw_input_window.h"

#include <windows.h>

static constexpr const wchar_t* kClassName = L"HidInputBackendWnd";

RawInputWindow::RawInputWindow(MessageHandler handler)
    : handler_(std::move(handler)) {}

RawInputWindow::~RawInputWindow() {
    destroy();
}

bool RawInputWindow::create(HINSTANCE instance) {
    ready_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ready_event_) return false;

    WNDCLASSW wc{};
    wc.lpfnWndProc = staticWndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    RegisterClassW(&wc);

    hwnd_ = CreateWindowExW(
        0, kClassName, L"HID Input Backend", 0,
        0, 0, 0, 0, nullptr, nullptr, instance, this);

    if (!hwnd_) return false;

    thread_id_ = GetCurrentThreadId();
    return true;
}

void RawInputWindow::destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
    if (ready_event_) {
        CloseHandle(ready_event_);
        ready_event_ = nullptr;
    }
}

void RawInputWindow::runMessageLoop(std::atomic<bool>& running) {
    if (ready_event_) SetEvent(ready_event_);

    MSG msg{};
    while (running.load() && GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK RawInputWindow::staticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RawInputWindow* self = nullptr;
    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = reinterpret_cast<RawInputWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<RawInputWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (self && self->handler_) {
        return self->handler_(hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}