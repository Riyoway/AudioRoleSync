#include <windows.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propidl.h>
#include <strsafe.h>
#include "resource.h"

#include <atomic>
#include <cwchar>

namespace {
constexpr wchar_t kAppName[] = L"AudioRoleSync";
constexpr wchar_t kWindowClass[] = L"AudioRoleSync.HiddenWindow";
constexpr wchar_t kMutexName[] = L"Local\\AudioRoleSync.SingleInstance";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kSettingsKey[] = L"Software\\AudioRoleSync";
constexpr wchar_t kRunValue[] = L"AudioRoleSync";
constexpr wchar_t kSyncEnabledValue[] = L"SyncEnabled";
constexpr wchar_t kStartupConfiguredValue[] = L"StartupConfigured";

constexpr UINT kTrayId = 1;
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kDeviceChangedMessage = WM_APP + 2;
constexpr UINT_PTR kSyncTimerId = 1;
constexpr UINT kSyncDebounceMs = 120;

constexpr UINT kMenuSyncEnabled = 1001;
constexpr UINT kMenuSyncNow = 1002;
constexpr UINT kMenuStartWithWindows = 1003;
constexpr UINT kMenuExit = 1004;

MIDL_INTERFACE("F8679F50-850A-41CF-9C72-430F290290C8")
IPolicyConfig : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(LPCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(LPCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(LPCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(LPCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(LPCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(LPCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(LPCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(LPCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(LPCWSTR, const PROPERTYKEY&, const PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(LPCWSTR, ERole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(LPCWSTR, INT) = 0;
};

constexpr CLSID CLSID_PolicyConfigClient = {
    0x870AF99C, 0x171D, 0x4F9E, {0xAF, 0x0D, 0xE6, 0x3D, 0xF4, 0x0C, 0x2B, 0xC9}
};

class App;

class NotificationClient final : public IMMNotificationClient {
public:
    explicit NotificationClient(App* app) noexcept : app_(app) {}

    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG value = --refs_;
        if (value == 0) delete this;
        return value;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_POINTER;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient)) {
            *object = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override;

private:
    std::atomic<ULONG> refs_{1};
    App* app_;
};

class App final {
public:
    ~App() { Shutdown(); }

    bool Initialize(HINSTANCE instance) {
        instance_ = instance;
        syncEnabled_ = ReadSyncEnabled();
        EnsureStartWithWindowsDefault();
        taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = &App::WindowProcStatic;
        windowClass.hInstance = instance_;
        windowClass.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APP_ICON));
        windowClass.hIconSm = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APP_ICON));
        windowClass.lpszClassName = kWindowClass;

        if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

        window_ = CreateWindowExW(0, kWindowClass, kAppName, WS_OVERLAPPED,
                                  0, 0, 0, 0, nullptr, nullptr, instance_, this);
        if (!window_) return false;

        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&enumerator_));
        if (FAILED(hr)) return false;

        notificationClient_ = new NotificationClient(this);
        hr = enumerator_->RegisterEndpointNotificationCallback(notificationClient_);
        if (FAILED(hr)) return false;
        callbackRegistered_ = true;

        AddTrayIcon();
        if (syncEnabled_) SyncNow();
        return true;
    }

    int Run() {
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return static_cast<int>(message.wParam);
    }

    void NotifyDefaultPlaybackChanged() noexcept {
        if (window_) PostMessageW(window_, kDeviceChangedMessage, 0, 0);
    }

private:
    static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            app = static_cast<App*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        return app ? app->WindowProc(hwnd, message, wParam, lParam)
                   : DefWindowProcW(hwnd, message, wParam, lParam);
    }

    LRESULT WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        if (taskbarCreatedMessage_ != 0 && message == taskbarCreatedMessage_) {
            AddTrayIcon();
            return 0;
        }

        switch (message) {
        case kTrayMessage:
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) ShowTrayMenu();
            return 0;
        case kDeviceChangedMessage:
            if (syncEnabled_) {
                KillTimer(hwnd, kSyncTimerId);
                SetTimer(hwnd, kSyncTimerId, kSyncDebounceMs, nullptr);
            }
            return 0;
        case WM_TIMER:
            if (wParam == kSyncTimerId) {
                KillTimer(hwnd, kSyncTimerId);
                if (syncEnabled_) SyncNow();
            }
            return 0;
        case WM_COMMAND:
            HandleCommand(LOWORD(wParam));
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void HandleCommand(UINT command) {
        switch (command) {
        case kMenuSyncEnabled:
            syncEnabled_ = !syncEnabled_;
            WriteSyncEnabled(syncEnabled_);
            if (syncEnabled_) SyncNow();
            break;
        case kMenuSyncNow:
            SyncNow();
            break;
        case kMenuStartWithWindows:
            SetStartWithWindows(!IsStartWithWindowsEnabled());
            WriteStartupConfigured();
            break;
        case kMenuExit:
            RemoveTrayIcon();
            DestroyWindow(window_);
            window_ = nullptr;
            break;
        }
    }

    bool SyncNow() noexcept {
        if (!enumerator_) return false;

        IMMDevice* multimedia = nullptr;
        HRESULT hr = enumerator_->GetDefaultAudioEndpoint(eRender, eMultimedia, &multimedia);
        if (FAILED(hr) || !multimedia) return false;

        LPWSTR multimediaId = nullptr;
        hr = multimedia->GetId(&multimediaId);
        multimedia->Release();
        if (FAILED(hr) || !multimediaId) return false;

        bool alreadySynced = false;
        IMMDevice* communications = nullptr;
        if (SUCCEEDED(enumerator_->GetDefaultAudioEndpoint(eRender, eCommunications, &communications)) && communications) {
            LPWSTR communicationsId = nullptr;
            if (SUCCEEDED(communications->GetId(&communicationsId)) && communicationsId) {
                alreadySynced = (_wcsicmp(multimediaId, communicationsId) == 0);
                CoTaskMemFree(communicationsId);
            }
            communications->Release();
        }

        bool success = alreadySynced;
        if (!alreadySynced) {
            IPolicyConfig* policy = nullptr;
            hr = CoCreateInstance(CLSID_PolicyConfigClient, nullptr, CLSCTX_ALL,
                                  __uuidof(IPolicyConfig), reinterpret_cast<void**>(&policy));
            if (SUCCEEDED(hr) && policy) {
                success = SUCCEEDED(policy->SetDefaultEndpoint(multimediaId, eCommunications));
                policy->Release();
            }
        }

        CoTaskMemFree(multimediaId);
        return success;
    }

    void AddTrayIcon() noexcept {
        if (!window_) return;
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data);
        data.hWnd = window_;
        data.uID = kTrayId;
        data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        data.uCallbackMessage = kTrayMessage;
        data.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APP_ICON));
        StringCchCopyW(data.szTip, ARRAYSIZE(data.szTip), kAppName);
        Shell_NotifyIconW(NIM_ADD, &data);
    }

    void RemoveTrayIcon() noexcept {
        if (!window_) return;
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data);
        data.hWnd = window_;
        data.uID = kTrayId;
        Shell_NotifyIconW(NIM_DELETE, &data);
    }

    void ShowTrayMenu() {
        HMENU menu = CreatePopupMenu();
        if (!menu) return;

        AppendMenuW(menu, MF_STRING | (syncEnabled_ ? MF_CHECKED : MF_UNCHECKED),
                    kMenuSyncEnabled, L"Automatic sync");
        AppendMenuW(menu, MF_STRING, kMenuSyncNow, L"Sync now");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (IsStartWithWindowsEnabled() ? MF_CHECKED : MF_UNCHECKED),
                    kMenuStartWithWindows, L"Start with Windows");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuExit, L"Exit");

        POINT point{};
        GetCursorPos(&point);
        SetForegroundWindow(window_);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, point.x, point.y, 0, window_, nullptr);
        PostMessageW(window_, WM_NULL, 0, 0);
        DestroyMenu(menu);
    }

    static bool ReadSyncEnabled() noexcept {
        DWORD value = 1;
        DWORD size = sizeof(value);
        DWORD type = 0;
        if (RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, kSyncEnabledValue,
                         RRF_RT_REG_DWORD, &type, &value, &size) != ERROR_SUCCESS) return true;
        return value != 0;
    }

    static void WriteSyncEnabled(bool enabled) noexcept {
        HKEY key = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, nullptr, 0, KEY_SET_VALUE,
                            nullptr, &key, nullptr) != ERROR_SUCCESS) return;
        DWORD value = enabled ? 1u : 0u;
        RegSetValueExW(key, kSyncEnabledValue, 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
    }

    static bool IsStartWithWindowsEnabled() noexcept {
        wchar_t value[4096]{};
        DWORD size = sizeof(value);
        return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ,
                            nullptr, value, &size) == ERROR_SUCCESS && value[0] != L'\0';
    }

    static bool IsStartupConfigured() noexcept {
        DWORD value = 0;
        DWORD size = sizeof(value);
        DWORD type = 0;
        return RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, kStartupConfiguredValue,
                            RRF_RT_REG_DWORD, &type, &value, &size) == ERROR_SUCCESS && value != 0;
    }

    static void WriteStartupConfigured() noexcept {
        HKEY key = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, nullptr, 0, KEY_SET_VALUE,
                            nullptr, &key, nullptr) != ERROR_SUCCESS) return;
        const DWORD value = 1u;
        RegSetValueExW(key, kStartupConfiguredValue, 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
    }

    static void EnsureStartWithWindowsDefault() noexcept {
        if (IsStartupConfigured()) return;
        SetStartWithWindows(true);
        WriteStartupConfigured();
    }

    static void SetStartWithWindows(bool enabled) noexcept {
        HKEY key = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE,
                            nullptr, &key, nullptr) != ERROR_SUCCESS) return;

        if (!enabled) {
            RegDeleteValueW(key, kRunValue);
            RegCloseKey(key);
            return;
        }

        wchar_t path[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
        if (length != 0 && length < ARRAYSIZE(path)) {
            wchar_t command[MAX_PATH + 32]{};
            if (SUCCEEDED(StringCchPrintfW(command, ARRAYSIZE(command), L"\"%s\" --startup", path))) {
                RegSetValueExW(key, kRunValue, 0, REG_SZ,
                               reinterpret_cast<const BYTE*>(command),
                               static_cast<DWORD>((wcslen(command) + 1) * sizeof(wchar_t)));
            }
        }
        RegCloseKey(key);
    }

    void Shutdown() noexcept {
        if (shuttingDown_) return;
        shuttingDown_ = true;

        if (window_) {
            KillTimer(window_, kSyncTimerId);
            RemoveTrayIcon();
        }

        if (enumerator_ && callbackRegistered_ && notificationClient_) {
            enumerator_->UnregisterEndpointNotificationCallback(notificationClient_);
            callbackRegistered_ = false;
        }

        if (notificationClient_) {
            notificationClient_->Release();
            notificationClient_ = nullptr;
        }

        if (enumerator_) {
            enumerator_->Release();
            enumerator_ = nullptr;
        }
    }

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    UINT taskbarCreatedMessage_ = 0;
    IMMDeviceEnumerator* enumerator_ = nullptr;
    NotificationClient* notificationClient_ = nullptr;
    bool callbackRegistered_ = false;
    bool syncEnabled_ = true;
    bool shuttingDown_ = false;
};

HRESULT STDMETHODCALLTYPE NotificationClient::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) {
    if (flow == eRender && (role == eMultimedia || role == eConsole) && app_) {
        app_->NotifyDefaultPlaybackChanged();
    }
    return S_OK;
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (!mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return 0;
    }

    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com)) {
        CloseHandle(mutex);
        return 1;
    }

    int exitCode = 1;
    {
        App app;
        if (app.Initialize(instance)) exitCode = app.Run();
    }

    CoUninitialize();
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return exitCode;
}
