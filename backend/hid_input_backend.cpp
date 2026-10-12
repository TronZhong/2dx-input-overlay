#include "hid_input_backend.h"

#include "backend/device_manager.h"
#include "backend/state_publisher.h"
#include "backend/auto_config.h"
#include "adapter/my_hid_adapter.h"
#include "platform/raw_input/raw_input_window.h"
#include "platform/raw_input/raw_input_registry.h"
#include "platform/raw_input/raw_input_parser.h"

#include <atomic>
#include <thread>
#include <mutex>
#include <memory>

struct HidInputBackend::Impl {
    MyHidConfig config;
    MyHidAdapter adapter;
    DeviceManager deviceManager;
    StatePublisher statePublisher;
    std::unique_ptr<RawInputWindow> rawInputWindow;
    std::thread workerThread;
    std::atomic<bool> running{false};
    DWORD workerThreadId = 0;
    HANDLE readyEvent = nullptr;
    mutable std::mutex configMutex;
    MyHidState masterState;

    explicit Impl(MyHidConfig cfg)
        : config(std::move(cfg))
        , adapter(config)
        , deviceManager(config) {}
    ~Impl() {
        stopWorker();
    }

    void startWorker();
    void stopWorker();
    void onRawInput(LPARAM lParam);
    void onDeviceChange(WPARAM wParam, LPARAM lParam);
    void publishMasterState();
};

HidInputBackend::HidInputBackend(MyHidConfig cfg)
    : impl_(new Impl(std::move(cfg))) {}

HidInputBackend::~HidInputBackend() {
    delete impl_;
    impl_ = nullptr;
}

bool HidInputBackend::start() {
    impl_->startWorker();
    return true;
}

void HidInputBackend::stop() {
    impl_->stopWorker();
}

bool HidInputBackend::tryGetLatest(HidOverlayState& out) const {
    return impl_->statePublisher.tryGetLatest(out);
}

void HidInputBackend::setTargetVidPid(uint16_t vid, uint16_t pid) {
    std::lock_guard<std::mutex> lock(impl_->configMutex);
    impl_->config.vid = vid;
    impl_->config.pid = pid;
    impl_->adapter = MyHidAdapter(impl_->config);
    impl_->deviceManager.setConfig(impl_->config);
    impl_->masterState = MyHidState{};
    impl_->deviceManager.scanTargetDevices();
}

bool HidInputBackend::autoConfigure(uint16_t vid, uint16_t pid, HidCapabilities* outCaps) {
    std::lock_guard<std::mutex> lock(impl_->configMutex);
    impl_->config.vid = vid;
    impl_->config.pid = pid;
    impl_->adapter = MyHidAdapter(impl_->config);
    impl_->deviceManager.setConfig(impl_->config);
    impl_->masterState = MyHidState{};
    
    bool result = AutoConfig::configure(vid, pid, impl_->deviceManager, impl_->adapter, outCaps);
    if (result) {
        impl_->config = impl_->adapter.config();
    }
    return result;
}

MyHidConfig HidInputBackend::getConfig() const {
    std::lock_guard<std::mutex> lock(impl_->configMutex);
    return impl_->config;
}

void HidInputBackend::Impl::startWorker() {
    if (running.load()) return;

    readyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!readyEvent) return;

    running.store(true);
    
    rawInputWindow = std::make_unique<RawInputWindow>(
        [this](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            switch (msg) {
                case WM_INPUT:
                    onRawInput(lParam);
                    return 0;
                case WM_INPUT_DEVICE_CHANGE:
                    onDeviceChange(wParam, lParam);
                    return 0;
                case WM_DESTROY:
                    PostQuitMessage(0);
                    return 0;
                default:
                    return DefWindowProc(hwnd, msg, wParam, lParam);
            }
        });

    if (!rawInputWindow->create(GetModuleHandleW(nullptr))) {
        stopWorker();
        return;
    }

    if (!RawInputRegistry::registerAll(rawInputWindow->hwnd())) {
        stopWorker();
        return;
    }

    workerThread = std::thread([this]() {
        workerThreadId = GetCurrentThreadId();
        rawInputWindow->runMessageLoop(running);
    });

    DWORD waitRes = WaitForSingleObject(readyEvent, 5000);
    if (waitRes != WAIT_OBJECT_0 || rawInputWindow->hwnd() == nullptr) {
        stopWorker();
        return;
    }

    deviceManager.scanTargetDevices();
    if (deviceManager.devices().empty()) {
        statePublisher.publishDisconnectedState();
    }
}

void HidInputBackend::Impl::stopWorker() {
    if (!running.exchange(false)) return;

    if (workerThreadId != 0) {
        PostThreadMessage(workerThreadId, WM_QUIT, 0, 0);
    }

    if (workerThread.joinable()) {
        workerThread.join();
    }

    rawInputWindow.reset();
    workerThreadId = 0;
    deviceManager.clear();

    if (readyEvent) {
        CloseHandle(readyEvent);
        readyEvent = nullptr;
    }

    statePublisher.publishDisconnectedState();
}

void HidInputBackend::Impl::onRawInput(LPARAM lParam) {
    RawInputData data;
    if (!RawInputParser::parseRawInput(lParam, data)) return;

    auto& devices = deviceManager.devices();
    auto it = devices.find(data.deviceHandle);
    if (it == devices.end()) {
        if (!deviceManager.tryAddTargetDevice(data.deviceHandle)) return;
        it = devices.find(data.deviceHandle);
        if (it == devices.end()) return;
    }

    const auto& dev = it->second;
    MyHidState slice{};
    if (!adapter.updateFromReport(
            reinterpret_cast<PHIDP_PREPARSED_DATA>(const_cast<uint8_t*>(dev.preparsed.data())),
            data.rawData.data(),
            data.rawData.size(),
            slice)) {
        return;
    }

    // Resize master state arrays if config changed
    const auto& btns = config.buttons;
    const auto& axes = config.axes;
    if (masterState.dynamicButtons.size() != btns.size()) {
        masterState.dynamicButtons.assign(btns.size(), false);
    }
    if (masterState.dynamicAxesNorm.size() != axes.size()) {
        masterState.dynamicAxesNorm.assign(axes.size(), 0.0f);
        masterState.dynamicAxesRaw.assign(axes.size(), 0);
        masterState.dynamicAxesDir.assign(axes.size(), 0);
    }

    // Merge slice into master state based on ownership
    for (size_t i = 0; i < btns.size() && i < slice.dynamicButtons.size(); ++i) {
        if (deviceManager.ownsButton(dev, btns[i].usage)) {
            masterState.dynamicButtons[i] = slice.dynamicButtons[i];
        }
    }
    for (size_t i = 0; i < axes.size() && i < slice.dynamicAxesRaw.size(); ++i) {
        if (deviceManager.ownsAxis(dev, axes[i].usagePage, axes[i].usage)) {
            masterState.dynamicAxesNorm[i] = slice.dynamicAxesNorm[i];
            masterState.dynamicAxesRaw[i] = slice.dynamicAxesRaw[i];
            masterState.dynamicAxesDir[i] = slice.dynamicAxesDir[i];
        }
    }

    masterState.connected = true;
    publishMasterState();
}

void HidInputBackend::Impl::onDeviceChange(WPARAM wParam, LPARAM lParam) {
    HANDLE handle = reinterpret_cast<HANDLE>(lParam);
    if (wParam == GIDC_ARRIVAL) {
        deviceManager.tryAddTargetDevice(handle);
    } else if (wParam == GIDC_REMOVAL) {
        deviceManager.removeTargetDevice(handle);
        if (deviceManager.devices().empty()) {
            statePublisher.publishDisconnectedState();
        }
    }
}

void HidInputBackend::Impl::publishMasterState() {
    statePublisher.publishState(masterState, config);
}