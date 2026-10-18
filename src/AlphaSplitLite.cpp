#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <Processing.NDI.Lib.h>
#include "AlphaSplitCore.h"
#include <atomic>
#include <cstdint>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

namespace {
constexpr int OUT_W = 2560;
constexpr int OUT_H = 720;
constexpr int HALF_W = OUT_W / 2;
constexpr UINT WM_NEW_FRAME = WM_APP + 10;

std::atomic<bool> g_running{ false };
std::mutex g_frameMutex;
std::vector<uint8_t> g_output(static_cast<size_t>(OUT_W) * OUT_H * 4, 0);
// Before the first NDI frame arrives, show black Fill and white Key rather than
// an all-black canvas that could look like a broken transparency signal.
struct InitialCanvas {
    InitialCanvas() {
        for (int y = 0; y < OUT_H; ++y) {
            for (int x = HALF_W; x < OUT_W; ++x) {
                const size_t i = (static_cast<size_t>(y) * OUT_W + x) * 4;
                g_output[i] = g_output[i + 1] = g_output[i + 2] = 255;
                g_output[i + 3] = 255;
            }
        }
    }
} g_initialCanvas;
HWND g_outputWindow = nullptr;
NDIlib_recv_instance_t g_receiver = nullptr;
std::atomic<long long> g_frameCount{ 0 };
std::atomic<bool> g_frameMessagePending{ false };
std::atomic<bool> g_warnedNoAlpha{ false };
std::atomic<bool> g_warnedUnsupportedFormat{ false };
std::atomic<bool> g_loggedFirstFrame{ false };
std::atomic<bool> g_warnedReceiverError{ false };
std::atomic<ULONGLONG> g_startTick{ 0 };
std::atomic<ULONGLONG> g_lastVideoFrameTick{ 0 };
std::atomic<bool> g_warnedNoVideoFrame{ false };
std::atomic<bool> g_warnedVideoStall{ false };

enum class FrameProcessResult { Processed, InvalidFrame, UnsupportedFormat };

struct MonitorPick { RECT bounds{}; bool primary = false; std::wstring deviceName; };

// Temporarily switch the selected output display to the exact 2560×720 canvas
// when Windows exposes that mode. Restore the user's previous mode on every exit.
class DisplayModeGuard {
public:
    bool TrySet2560x720(const std::wstring& deviceName) {
        deviceName_ = deviceName;
        original_ = {};
        original_.dmSize = sizeof(original_);
        if (!EnumDisplaySettingsExW(deviceName_.c_str(), ENUM_CURRENT_SETTINGS, &original_, 0)) {
            std::wcerr << L"无法读取目标显示器当前分辨率；将按当前模式继续。\n";
            return false;
        }
        if (original_.dmPelsWidth == OUT_W && original_.dmPelsHeight == OUT_H) return true;

        DEVMODEW wanted{};
        wanted.dmSize = sizeof(wanted);
        bool found = false;
        for (DWORD modeIndex = 0; EnumDisplaySettingsExW(deviceName_.c_str(), modeIndex, &wanted, 0); ++modeIndex) {
            if (wanted.dmPelsWidth == OUT_W && wanted.dmPelsHeight == OUT_H && wanted.dmBitsPerPel >= 24) {
                found = true;
                break;
            }
            wanted = {};
            wanted.dmSize = sizeof(wanted);
        }
        if (!found) {
            std::wcerr << L"目标显示器没有向 Windows 提供 2560×720 模式；将按当前分辨率拉伸输出。\n";
            return false;
        }
        // Keep the bit depth reported by the enumerated mode. Forcing 32 bpp
        // here can reject a valid 2560x720 mode that the driver exposes at 24 bpp.
        wanted.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
        wanted.dmPelsWidth = OUT_W;
        wanted.dmPelsHeight = OUT_H;
        const LONG result = ChangeDisplaySettingsExW(deviceName_.c_str(), &wanted, nullptr, CDS_FULLSCREEN, nullptr);
        if (result != DISP_CHANGE_SUCCESSFUL) {
            std::wcerr << L"Windows 未能切换到 2560×720（代码 " << result
                       << L"）；将按当前分辨率拉伸输出。\n";
            return false;
        }
        changed_ = true;
        std::wcout << L"已将目标显示器临时切换为 2560×720。退出程序时会尝试恢复原分辨率。\n";
        return true;
    }

    ~DisplayModeGuard() {
        if (changed_) {
            const LONG result = ChangeDisplaySettingsExW(deviceName_.c_str(), &original_, nullptr, 0, nullptr);
            if (result != DISP_CHANGE_SUCCESSFUL)
                std::wcerr << L"提示：未能自动恢复显示器原分辨率，请在 Windows 显示设置中手动恢复。\n";
        }
    }

    DisplayModeGuard() = default;
    DisplayModeGuard(const DisplayModeGuard&) = delete;
    DisplayModeGuard& operator=(const DisplayModeGuard&) = delete;

private:
    std::wstring deviceName_;
    DEVMODEW original_{};
    bool changed_ = false;
};

BOOL CALLBACK EnumMonitorProc(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    auto* list = reinterpret_cast<std::vector<MonitorPick>*>(data);
    MONITORINFOEXW info{}; info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, reinterpret_cast<LPMONITORINFO>(&info))) {
        list->push_back({ info.rcMonitor, (info.dwFlags & MONITORINFOF_PRIMARY) != 0, info.szDevice });
    }
    return TRUE;
}

int SelectMonitor(const std::vector<MonitorPick>& monitors) {
    if (monitors.empty()) return -1;
    int defaultIndex = 0;
    for (size_t i = 0; i < monitors.size(); ++i) {
        if (!monitors[i].primary) { defaultIndex = static_cast<int>(i); break; }
    }
    std::wcout << L"\n检测到以下显示器：\n";
    for (size_t i = 0; i < monitors.size(); ++i) {
        const auto& m = monitors[i];
        const int width = m.bounds.right - m.bounds.left;
        const int height = m.bounds.bottom - m.bounds.top;
        std::wcout << L"  [" << i + 1 << L"] " << m.deviceName << L"  "
                   << width << L"×" << height
                   << (m.primary ? L"（主显示器）" : L"（扩展显示器）")
                   << (static_cast<int>(i) == defaultIndex ? L"  ← 默认" : L"") << L"\n";
    }
    std::wcout << L"选择字幕输出显示器编号，直接输入 0 使用默认显示器：";
    unsigned int choice = 0;
    if (!(std::wcin >> choice)) return -1;
    if (choice == 0) return defaultIndex;
    if (choice > monitors.size()) return -1;
    return static_cast<int>(choice - 1);
}

FrameProcessResult ProcessFrame(const NDIlib_video_frame_v2_t& frame) {
    if (!frame.p_data || frame.xres <= 0 || frame.yres <= 0) return FrameProcessResult::InvalidFrame;
    AlphaSplitCore::PixelFormat format;
    bool hasAlpha = false;
    switch (frame.FourCC) {
    case NDIlib_FourCC_type_BGRA: format = AlphaSplitCore::PixelFormat::BGRA; hasAlpha = true; break;
    case NDIlib_FourCC_type_BGRX: format = AlphaSplitCore::PixelFormat::BGRX; break;
    case NDIlib_FourCC_type_RGBA: format = AlphaSplitCore::PixelFormat::RGBA; hasAlpha = true; break;
    case NDIlib_FourCC_type_RGBX: format = AlphaSplitCore::PixelFormat::RGBX; break;
    default: return FrameProcessResult::UnsupportedFormat;
    }

    const int64_t minimumStride = static_cast<int64_t>(frame.xres) * 4;
    if (minimumStride > INT_MAX) return FrameProcessResult::InvalidFrame;
    // Negative stride is used by some explicitly flipped NDI formats. This receiver
    // requests the non-flipped format, so reject it rather than treating it as packed.
    if (frame.line_stride_in_bytes < 0) return FrameProcessResult::InvalidFrame;
    const int srcStride = frame.line_stride_in_bytes == 0
        ? static_cast<int>(minimumStride) : frame.line_stride_in_bytes;
    if (srcStride < minimumStride) return FrameProcessResult::InvalidFrame;

    if (!g_loggedFirstFrame.exchange(true)) {
        const wchar_t* formatName = L"unknown";
        switch (format) {
        case AlphaSplitCore::PixelFormat::BGRA: formatName = L"BGRA"; break;
        case AlphaSplitCore::PixelFormat::BGRX: formatName = L"BGRX"; break;
        case AlphaSplitCore::PixelFormat::RGBA: formatName = L"RGBA"; break;
        case AlphaSplitCore::PixelFormat::RGBX: formatName = L"RGBX"; break;
        }
        std::wcout << L"首次收到视频帧：" << frame.xres << L"×" << frame.yres
                   << L"，格式 " << formatName
                   << (hasAlpha ? L"（含 Alpha 通道）\n" : L"（不含 Alpha 通道）\n");
    }
    if (!hasAlpha && !g_warnedNoAlpha.exchange(true)) {
        std::wcerr << L"警告：当前 NDI 帧格式不含 Alpha（BGRX/RGBX）。Key 将全白；请确认上游发送了透明通道。\n";
    }

    // Convert into a receiver-thread-local back buffer without holding the UI mutex.
    // On the target i5-7200U, keeping the mutex around the full pixel loop could make
    // WM_PAINT wait for an entire frame conversion. Swap buffers under the lock only
    // after a complete frame is ready; the swap itself is constant-time.
    thread_local std::vector<uint8_t> convertedOutput(g_output.size(), 0);
    if (!AlphaSplitCore::BuildFillKey(frame.p_data, frame.xres, frame.yres, srcStride,
                                      format, convertedOutput.data(), OUT_W, OUT_H)) {
        return FrameProcessResult::InvalidFrame;
    }
    {
        std::lock_guard<std::mutex> lock(g_frameMutex);
        g_output.swap(convertedOutput);
    }
    g_lastVideoFrameTick.store(GetTickCount64(), std::memory_order_relaxed);
    g_frameCount.fetch_add(1, std::memory_order_relaxed);
    return FrameProcessResult::Processed;
}

LRESULT CALLBACK OutputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC hdc = BeginPaint(hwnd, &ps);
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = OUT_W;
        bmi.bmiHeader.biHeight = -OUT_H; // top-down DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        RECT client{}; GetClientRect(hwnd, &client);
        // Keep the shared-frame lock only long enough to take a consistent snapshot.
        // StretchDIBits can take noticeably longer on older integrated GPUs; doing it
        // while holding g_frameMutex would block the NDI receiver from publishing the
        // next converted frame. A UI-thread-local snapshot stays immutable for the draw.
        thread_local std::vector<uint8_t> paintSnapshot(
            static_cast<size_t>(OUT_W) * OUT_H * 4, 0);
        {
            std::lock_guard<std::mutex> lock(g_frameMutex);
            std::copy(g_output.begin(), g_output.end(), paintSnapshot.begin());
        }
        StretchDIBits(hdc, 0, 0, client.right - client.left, client.bottom - client.top,
            0, 0, OUT_W, OUT_H, paintSnapshot.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_NEW_FRAME:
        g_frameMessagePending.store(false, std::memory_order_release);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_TIMER:
        if (wParam == 1) {
            const ULONGLONG now = GetTickCount64();
            // Report the actual rate of successfully processed NDI frames every five
            // seconds. This helps distinguish a healthy 25/30 fps feed from a feed
            // that is arriving slowly or stalling, without printing on every frame.
            static ULONGLONG statsTick = 0;
            static long long statsFrameCount = 0;
            const long long currentFrameCount = g_frameCount.load(std::memory_order_relaxed);
            if (statsTick == 0) {
                statsTick = now;
                statsFrameCount = currentFrameCount;
            } else if (now - statsTick >= 5000) {
                const ULONGLONG elapsed = now - statsTick;
                const long long frames = currentFrameCount - statsFrameCount;
                const long long fpsTenths = elapsed > 0
                    ? (frames * 10000LL + static_cast<long long>(elapsed / 2)) / static_cast<long long>(elapsed)
                    : 0;
                std::wcout << L"NDI 已处理帧率（最近 " << elapsed / 1000 << L" 秒）："
                           << fpsTenths / 10 << L'.' << fpsTenths % 10
                           << L" fps；累计处理 " << currentFrameCount << L" 帧\n";
                statsTick = now;
                statsFrameCount = currentFrameCount;
            }
            const ULONGLONG lastFrame = g_lastVideoFrameTick.load(std::memory_order_relaxed);
            if (lastFrame == 0) {
                const ULONGLONG started = g_startTick.load(std::memory_order_relaxed);
                if (started != 0 && now - started >= 5000 &&
                    !g_warnedNoVideoFrame.exchange(true, std::memory_order_relaxed)) {
                    std::wcerr << L"提示：启动 5 秒后仍未收到可处理的视频帧。请检查 NDI 源是否正在输出，以及网络/像素格式是否正常。\n";
                }
            } else if (now - lastFrame >= 3000) {
                if (!g_warnedVideoStall.exchange(true, std::memory_order_relaxed)) {
                    std::wcerr << L"提示：连续 3 秒未收到新的视频帧，NDI 信号可能中断。程序仍会继续等待。\n";
                }
            } else {
                g_warnedVideoStall.store(false, std::memory_order_relaxed);
            }
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        break;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ReceiverLoop(NDIlib_recv_instance_t receiver, HWND window) {
    while (g_running.load(std::memory_order_relaxed)) {
        NDIlib_video_frame_v2_t video{};
        const NDIlib_frame_type_e type = NDIlib_recv_capture_v3(receiver, &video, nullptr, nullptr, 50);
        if (type == NDIlib_frame_type_video) {
            const FrameProcessResult result = ProcessFrame(video);
            if (result == FrameProcessResult::Processed) {
                if (!g_frameMessagePending.exchange(true, std::memory_order_acq_rel)) {
                    if (!PostMessageW(window, WM_NEW_FRAME, 0, 0))
                        g_frameMessagePending.store(false, std::memory_order_release);
                }
            } else if (result == FrameProcessResult::UnsupportedFormat &&
                       !g_warnedUnsupportedFormat.exchange(true)) {
                std::wcerr << L"收到暂不支持的 NDI 像素格式；当前版本仅处理 BGRA/BGRX/RGBA/RGBX。\n";
            } else if (result == FrameProcessResult::InvalidFrame) {
                static std::atomic<bool> warnedInvalidFrame{ false };
                if (!warnedInvalidFrame.exchange(true))
                    std::wcerr << L"收到参数不合规的 NDI 视频帧；已跳过该帧。\n";
            }
            NDIlib_recv_free_video_v2(receiver, &video);
        } else if (type == NDIlib_frame_type_error) {
            if (!g_warnedReceiverError.exchange(true)) {
                std::wcerr << L"NDI 接收报告错误；程序将继续等待。若持续无画面，请检查 WeMix NDI 源和网络。\n";
            }
            Sleep(200);
        }
    }
}

std::wstring Utf8ToWide(const char* text) {
    if (!text || !*text) return L"(unnamed)";
    const int required = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (required <= 1) return L"(unnamed)";
    std::wstring result(static_cast<size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), required) == 0)
        return L"(unnamed)";
    result.pop_back(); // remove the terminating NUL
    return result;
}

void WaitForEnter() {
    std::wcout << L"按任意键退出……";
    std::system("pause >nul");
}

int SelectSource(NDIlib_find_instance_t finder, NDIlib_source_t& selected,
                 std::string& selectedName, std::string& selectedUrl) {
    std::wcout << L"正在搜索局域网中的 NDI 信号（等待约 2 秒）……\n";
    NDIlib_find_wait_for_sources(finder, 2000);
    uint32_t count = 0;
    const NDIlib_source_t* sources = NDIlib_find_get_current_sources(finder, &count);
    if (!sources || count == 0) {
        std::wcerr << L"没有发现 NDI 信号。请确认 WeMix 已输出 NDI，且两台设备在可互通的网络中。\n";
        return -1;
    }
    std::wcout << L"\n发现以下 NDI 信号：\n";
    for (uint32_t i = 0; i < count; ++i) {
        std::wcout << L"  [" << i + 1 << L"] " << Utf8ToWide(sources[i].p_ndi_name) << L"\n";
    }
    std::wcout << L"\n输入信号编号并按回车：";
    unsigned int index = 0;
    if (!(std::wcin >> index) || index < 1 || index > count) {
        std::wcerr << L"输入的编号无效。\n"; return -1;
    }
    // The source descriptors returned by the finder are owned by the NDI finder
    // and may become invalid when NDIlib_find_destroy is called. Keep the selected
    // name and URL in caller-owned strings for the full receiver lifetime, then
    // point the descriptor at those stable copies.
    const NDIlib_source_t& found = sources[index - 1];
    selectedName = found.p_ndi_name ? found.p_ndi_name : "";
    selectedUrl = found.p_url_address ? found.p_url_address : "";
    selected.p_ndi_name = selectedName.empty() ? nullptr : selectedName.c_str();
    selected.p_url_address = selectedUrl.empty() ? nullptr : selectedUrl.c_str();
    return static_cast<int>(index - 1);
}
}

int main() {
    // Keep fullscreen output coordinates in physical pixels on scaled Windows desktops.
    // Available since Windows Vista; safe on the Windows 10 x64 target.
    SetProcessDPIAware();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::wcout << L"AlphaSplit Lite — NDI 接收开发版\n";
    std::wcout << L"输出画布：2560×720；左侧 Fill，右侧 Key；按 Esc 退出。\n\n";

    if (!NDIlib_initialize()) {
        std::wcerr << L"NDI 初始化失败。请检查 NDI Runtime、CPU 支持情况及程序位数。\n";
        WaitForEnter(); return 2;
    }

    NDIlib_find_create_t findDesc{};
    findDesc.show_local_sources = true;
    findDesc.p_groups = nullptr;
    findDesc.p_extra_ips = nullptr;
    NDIlib_find_instance_t finder = NDIlib_find_create_v2(&findDesc);
    if (!finder) {
        std::wcerr << L"无法创建 NDI 搜索器。\n";
        NDIlib_destroy(); return 3;
    }

    NDIlib_source_t selected{};
    // These strings must outlive the receiver because the NDI source descriptor
    // may be consulted asynchronously after NDIlib_recv_create_v3 returns.
    std::string selectedSourceName;
    std::string selectedSourceUrl;
    const int selectedIndex = SelectSource(finder, selected, selectedSourceName, selectedSourceUrl);
    if (selectedIndex < 0) {
        NDIlib_find_destroy(finder); NDIlib_destroy();
        WaitForEnter(); return 4;
    }

    NDIlib_recv_create_v3_t recvDesc{};
    recvDesc.source_to_connect_to = selected;
    recvDesc.color_format = NDIlib_recv_color_format_BGRX_BGRA;
    recvDesc.bandwidth = NDIlib_recv_bandwidth_highest;
    recvDesc.allow_video_fields = false;
    recvDesc.p_ndi_recv_name = "AlphaSplit Lite";
    g_receiver = NDIlib_recv_create_v3(&recvDesc);
    NDIlib_find_destroy(finder);
    if (!g_receiver) {
        std::wcerr << L"创建 NDI 接收器失败。\n"; NDIlib_destroy(); return 5;
    }

    std::vector<MonitorPick> monitors;
    EnumDisplayMonitors(nullptr, nullptr, EnumMonitorProc, reinterpret_cast<LPARAM>(&monitors));
    if (monitors.empty()) {
        std::wcerr << L"Windows 未报告任何可用显示器。\n";
        NDIlib_recv_destroy(g_receiver); NDIlib_destroy();
        WaitForEnter(); return 6;
    }
    const int targetIndex = SelectMonitor(monitors);
    if (targetIndex < 0 || static_cast<size_t>(targetIndex) >= monitors.size()) {
        std::wcerr << L"显示器编号无效，程序退出。\n";
        NDIlib_recv_destroy(g_receiver); NDIlib_destroy();
        WaitForEnter(); return 6;
    }

    DisplayModeGuard displayModeGuard;
    const std::wstring selectedDevice = monitors[static_cast<size_t>(targetIndex)].deviceName;
    displayModeGuard.TrySet2560x720(selectedDevice);
    // Resolution changes can alter monitor bounds; refresh before placing the fullscreen window.
    monitors.clear();
    EnumDisplayMonitors(nullptr, nullptr, EnumMonitorProc, reinterpret_cast<LPARAM>(&monitors));
    int refreshedTargetIndex = -1;
    for (size_t i = 0; i < monitors.size(); ++i) {
        if (monitors[i].deviceName == selectedDevice) {
            refreshedTargetIndex = static_cast<int>(i);
            break;
        }
    }
    if (refreshedTargetIndex < 0) {
        std::wcerr << L"切换分辨率后无法重新识别目标显示器，程序退出。\n";
        NDIlib_recv_destroy(g_receiver); NDIlib_destroy();
        WaitForEnter(); return 6;
    }

    const wchar_t* className = L"AlphaSplitLiteOutputWindow";
    WNDCLASSW wc{}; wc.lpfnWndProc = OutputWndProc; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = className; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        std::wcerr << L"注册输出窗口类失败。\n";
        NDIlib_recv_destroy(g_receiver); NDIlib_destroy();
        WaitForEnter(); return 7;
    }
    const int outputMonitorIndex = refreshedTargetIndex;
    const RECT b = monitors[static_cast<size_t>(outputMonitorIndex)].bounds;
    g_outputWindow = CreateWindowExW(WS_EX_TOPMOST, className, L"AlphaSplit Lite Output",
        WS_POPUP, b.left, b.top, b.right - b.left, b.bottom - b.top,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_outputWindow) {
        std::wcerr << L"创建输出窗口失败。\n";
        NDIlib_recv_destroy(g_receiver); NDIlib_destroy(); return 7;
    }
    ShowWindow(g_outputWindow, SW_SHOW);
    UpdateWindow(g_outputWindow);
    if (!SetTimer(g_outputWindow, 1, 1000, nullptr)) {
        std::wcerr << L"提示：无法启动视频帧状态监测定时器；图像输出仍可继续。\n";
    }
    // The console is the process's original foreground window. Explicitly activate
    // the fullscreen output so the documented Esc-to-exit shortcut works without
    // requiring the operator to click the output monitor first.
    SetForegroundWindow(g_outputWindow);
    SetFocus(g_outputWindow);
    const int monitorW = b.right - b.left;
    const int monitorH = b.bottom - b.top;
    std::wcout << L"\n目标输出显示器：" << monitors[static_cast<size_t>(outputMonitorIndex)].deviceName
               << L"，" << monitorW << L"×" << monitorH
               << (monitors[static_cast<size_t>(outputMonitorIndex)].primary ? L"（主显示器）\n" : L"（扩展显示器）\n");
    if (monitorW != OUT_W || monitorH != OUT_H) {
        std::wcerr << L"注意：目标显示器当前桌面分辨率不是 2560×720，输出会被拉伸。\n"
                      L"请在 Windows 显示设置中将连接分离盒的显示器设为 2560×720，再重新启动程序。\n";
    }
    std::wcout << L"已连接 NDI 源，输出窗口已打开。按 Esc 退出输出。\n";
    g_startTick.store(GetTickCount64(), std::memory_order_relaxed);
    g_running.store(true);
    std::thread receiverThread(ReceiverLoop, g_receiver, g_outputWindow);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    g_running.store(false);
    if (receiverThread.joinable()) receiverThread.join();
    NDIlib_recv_destroy(g_receiver); g_receiver = nullptr;
    NDIlib_destroy();
    std::wcout << L"程序已退出。接收帧数：" << g_frameCount.load() << L"\n";
    return 0;
}
