#pragma once

#include <windows.h>
#include <functional>
#include <atomic>

class RawInputWindow {
public:
    using MessageHandler = std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)>;

    explicit RawInputWindow(MessageHandler handler);
    ~RawInputWindow();

    bool create(HINSTANCE instance);
    void destroy();
    HWND hwnd() const { return hwnd_; }
    DWORD threadId() const { return thread_id_; }

    void runMessageLoop(std::atomic<bool>& running);

private:
    static LRESULT CALLBACK staticWndProc(HWND, UINT, WPARAM, LPARAM);
    HWND hwnd_ = nullptr;
    DWORD thread_id_ = 0;
    MessageHandler handler_;
    HANDLE ready_event_ = nullptr;
};