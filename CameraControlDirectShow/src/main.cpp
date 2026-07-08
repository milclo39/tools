#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <dshow.h>
#include <ks.h>
#include <ksmedia.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "comctl32.lib")

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() { reset(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    T** operator&() { reset(); return &ptr_; }
    T* operator->() const { return ptr_; }
    T* get() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }

    void reset(T* value = nullptr) {
        if (ptr_) {
            ptr_->Release();
        }
        ptr_ = value;
    }

private:
    T* ptr_ = nullptr;
};

struct CameraDevice {
    std::wstring name;
    ComPtr<IMoniker> moniker;
};

struct Resolution {
    const wchar_t* label;
    long width;
    long height;
};

struct ControlRange {
    long minValue = 0;
    long maxValue = 0;
    long step = 1;
    long defaultValue = 0;
    long flags = 0;
    bool supported = false;
};

constexpr int IDC_CAMERA = 1001;
constexpr int IDC_SCAN = 1002;
constexpr int IDC_RESOLUTION = 1003;
constexpr int IDC_APPLY_RESOLUTION = 1004;
constexpr int IDC_AUTOFOCUS = 1005;
constexpr int IDC_ZOOM = 1101;
constexpr int IDC_FOCUS = 1102;
constexpr int IDC_BRIGHTNESS = 1103;
constexpr int IDC_EXPOSURE = 1104;
constexpr int IDC_ZOOM_VALUE = 1201;
constexpr int IDC_FOCUS_VALUE = 1202;
constexpr int IDC_BRIGHTNESS_VALUE = 1203;
constexpr int IDC_EXPOSURE_VALUE = 1204;

const Resolution kResolutions[] = {
    {L"VGA (640 x 480)", 640, 480},
    {L"720p (1280 x 720)", 1280, 720},
    {L"1080p (1920 x 1080)", 1920, 1080},
    {L"4K (3840 x 2160)", 3840, 2160},
};

class CameraApp {
public:
    int Run(HINSTANCE instance, int show);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateMainWindow(HINSTANCE instance, int show);
    void CreateControls();
    void LayoutControls();
    void SetStatus(const std::wstring& text);

    void EnumerateCameras();
    void PopulateCameraCombo();
    void StartSelectedCamera();
    void StopGraph();
    bool BuildGraph(int cameraIndex);
    bool ApplyResolution(long width, long height);
    void ResizeVideoWindow();

    void LoadControlRanges();
    void SetupTrackbar(HWND hwndTrackbar, HWND hwndValue, const ControlRange& range, long current);
    void UpdateTrackbarValue(HWND hwndTrackbar, HWND hwndValue);
    void ApplyCameraControl(long property, HWND hwndTrackbar);
    void ApplyVideoProcAmp(long property, HWND hwndTrackbar);
    void ApplyAutofocus();

    bool GetCameraControl(long property, long& value, long& flags);
    bool GetVideoProcAmp(long property, long& value, long& flags);
    ControlRange GetCameraRange(long property);
    ControlRange GetVideoProcAmpRange(long property);

    HRESULT GetCapturePin(IBaseFilter* filter, IPin** pin);
    HRESULT GetStreamConfig(IAMStreamConfig** streamConfig);

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND preview_ = nullptr;
    HWND cameraLabel_ = nullptr;
    HWND cameraCombo_ = nullptr;
    HWND scanButton_ = nullptr;
    HWND resolutionLabel_ = nullptr;
    HWND resolutionCombo_ = nullptr;
    HWND applyResolutionButton_ = nullptr;
    HWND autofocusCheck_ = nullptr;
    HWND zoomLabel_ = nullptr;
    HWND focusLabel_ = nullptr;
    HWND brightnessLabel_ = nullptr;
    HWND exposureLabel_ = nullptr;
    HWND zoomTrack_ = nullptr;
    HWND focusTrack_ = nullptr;
    HWND brightnessTrack_ = nullptr;
    HWND exposureTrack_ = nullptr;
    HWND zoomValue_ = nullptr;
    HWND focusValue_ = nullptr;
    HWND brightnessValue_ = nullptr;
    HWND exposureValue_ = nullptr;
    HWND status_ = nullptr;

    std::vector<CameraDevice> cameras_;
    ComPtr<IGraphBuilder> graph_;
    ComPtr<ICaptureGraphBuilder2> captureBuilder_;
    ComPtr<IBaseFilter> captureFilter_;
    ComPtr<IMediaControl> mediaControl_;
    ComPtr<IVideoWindow> videoWindow_;
    ComPtr<IAMCameraControl> cameraControl_;
    ComPtr<IAMVideoProcAmp> videoProcAmp_;
};

std::wstring HResultText(HRESULT hr) {
    std::wstringstream ss;
    ss << L"0x" << std::hex << static_cast<unsigned long>(hr);
    return ss.str();
}

void FreeMediaType(AM_MEDIA_TYPE& mt) {
    if (mt.cbFormat != 0) {
        CoTaskMemFree(mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = nullptr;
    }
    if (mt.pUnk) {
        mt.pUnk->Release();
        mt.pUnk = nullptr;
    }
}

void DeleteMediaType(AM_MEDIA_TYPE* mt) {
    if (!mt) {
        return;
    }
    FreeMediaType(*mt);
    CoTaskMemFree(mt);
}

bool IsVideoInfo(const AM_MEDIA_TYPE* mt) {
    return mt && mt->formattype == FORMAT_VideoInfo && mt->cbFormat >= sizeof(VIDEOINFOHEADER) && mt->pbFormat;
}

DWORD FourCcFromSubtype(const GUID& subtype) {
    if (subtype.Data2 == 0x0000 && subtype.Data3 == 0x0010 &&
        subtype.Data4[0] == 0x80 && subtype.Data4[1] == 0x00 &&
        subtype.Data4[2] == 0x00 && subtype.Data4[3] == 0xaa &&
        subtype.Data4[4] == 0x00 && subtype.Data4[5] == 0x38 &&
        subtype.Data4[6] == 0x9b && subtype.Data4[7] == 0x71) {
        return subtype.Data1;
    }
    return 0;
}

bool IsMjpg(const GUID& subtype) {
    return FourCcFromSubtype(subtype) == MAKEFOURCC('M', 'J', 'P', 'G');
}

int CameraApp::Run(HINSTANCE instance, int show) {
    instance_ = instance;
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        MessageBoxW(nullptr, L"COM initialization failed.", L"CameraControlDirectShow", MB_ICONERROR);
        return 1;
    }

    const int result = CreateMainWindow(instance, show) ? 0 : 1;
    if (hwnd_) {
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    StopGraph();
    CoUninitialize();
    return result;
}

bool CameraApp::CreateMainWindow(HINSTANCE instance, int show) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = CameraApp::WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"CameraControlDirectShowWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc)) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"DirectShow Camera Control",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1180,
        760,
        nullptr,
        nullptr,
        instance,
        this);

    if (!hwnd_) {
        return false;
    }

    CreateControls();
    EnumerateCameras();
    PopulateCameraCombo();
    ShowWindow(hwnd_, show);
    UpdateWindow(hwnd_);
    StartSelectedCamera();
    return true;
}

LRESULT CALLBACK CameraApp::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CameraApp* app = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<CameraApp*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->hwnd_ = hwnd;
    } else {
        app = reinterpret_cast<CameraApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (app) {
        return app->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CameraApp::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        LayoutControls();
        ResizeVideoWindow();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_SCAN:
            EnumerateCameras();
            PopulateCameraCombo();
            StartSelectedCamera();
            return 0;
        case IDC_CAMERA:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                StartSelectedCamera();
            }
            return 0;
        case IDC_APPLY_RESOLUTION: {
            int index = static_cast<int>(SendMessageW(resolutionCombo_, CB_GETCURSEL, 0, 0));
            if (index >= 0 && index < static_cast<int>(std::size(kResolutions))) {
                const auto& res = kResolutions[index];
                if (ApplyResolution(res.width, res.height)) {
                    SetStatus(std::wstring(L"Resolution applied: ") + res.label);
                }
            }
            return 0;
        }
        case IDC_AUTOFOCUS:
            ApplyAutofocus();
            return 0;
        default:
            break;
        }
        break;
    case WM_HSCROLL: {
        HWND source = reinterpret_cast<HWND>(lParam);
        if (source == zoomTrack_) {
            ApplyCameraControl(CameraControl_Zoom, zoomTrack_);
            UpdateTrackbarValue(zoomTrack_, zoomValue_);
        } else if (source == focusTrack_) {
            ApplyCameraControl(CameraControl_Focus, focusTrack_);
            UpdateTrackbarValue(focusTrack_, focusValue_);
        } else if (source == brightnessTrack_) {
            ApplyVideoProcAmp(VideoProcAmp_Brightness, brightnessTrack_);
            UpdateTrackbarValue(brightnessTrack_, brightnessValue_);
        } else if (source == exposureTrack_) {
            ApplyCameraControl(CameraControl_Exposure, exposureTrack_);
            UpdateTrackbarValue(exposureTrack_, exposureValue_);
        }
        return 0;
    }
    case WM_CLOSE:
        StopGraph();
        DestroyWindow(hwnd_);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

void CameraApp::CreateControls() {
    preview_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
        10, 10, 760, 600, hwnd_, nullptr, instance_, nullptr);

    cameraLabel_ = CreateWindowExW(0, L"STATIC", L"Camera", WS_CHILD | WS_VISIBLE,
        790, 10, 100, 20, hwnd_, nullptr, instance_, nullptr);
    cameraCombo_ = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        790, 30, 340, 200, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CAMERA)), instance_, nullptr);
    scanButton_ = CreateWindowExW(0, L"BUTTON", L"Scan", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        790, 64, 90, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SCAN)), instance_, nullptr);

    resolutionLabel_ = CreateWindowExW(0, L"STATIC", L"Resolution", WS_CHILD | WS_VISIBLE,
        790, 110, 100, 20, hwnd_, nullptr, instance_, nullptr);
    resolutionCombo_ = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        790, 130, 220, 200, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RESOLUTION)), instance_, nullptr);
    applyResolutionButton_ = CreateWindowExW(0, L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        1020, 130, 110, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_APPLY_RESOLUTION)), instance_, nullptr);
    for (const auto& res : kResolutions) {
        SendMessageW(resolutionCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(res.label));
    }
    SendMessageW(resolutionCombo_, CB_SETCURSEL, 1, 0);

    autofocusCheck_ = CreateWindowExW(0, L"BUTTON", L"Autofocus", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        790, 184, 160, 24, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_AUTOFOCUS)), instance_, nullptr);

    zoomLabel_ = CreateWindowExW(0, L"STATIC", L"Zoom", WS_CHILD | WS_VISIBLE,
        790, 230, 160, 20, hwnd_, nullptr, instance_, nullptr);
    focusLabel_ = CreateWindowExW(0, L"STATIC", L"Focus", WS_CHILD | WS_VISIBLE,
        790, 300, 160, 20, hwnd_, nullptr, instance_, nullptr);
    brightnessLabel_ = CreateWindowExW(0, L"STATIC", L"Brightness", WS_CHILD | WS_VISIBLE,
        790, 370, 160, 20, hwnd_, nullptr, instance_, nullptr);
    exposureLabel_ = CreateWindowExW(0, L"STATIC", L"Exposure", WS_CHILD | WS_VISIBLE,
        790, 440, 160, 20, hwnd_, nullptr, instance_, nullptr);
    zoomTrack_ = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
        790, 250, 260, 32, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ZOOM)), instance_, nullptr);
    focusTrack_ = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
        790, 320, 260, 32, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FOCUS)), instance_, nullptr);
    brightnessTrack_ = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
        790, 390, 260, 32, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BRIGHTNESS)), instance_, nullptr);
    exposureTrack_ = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
        790, 460, 260, 32, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EXPOSURE)), instance_, nullptr);

    zoomValue_ = CreateWindowExW(0, L"STATIC", L"-", WS_CHILD | WS_VISIBLE,
        1060, 250, 70, 24, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ZOOM_VALUE)), instance_, nullptr);
    focusValue_ = CreateWindowExW(0, L"STATIC", L"-", WS_CHILD | WS_VISIBLE,
        1060, 320, 70, 24, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FOCUS_VALUE)), instance_, nullptr);
    brightnessValue_ = CreateWindowExW(0, L"STATIC", L"-", WS_CHILD | WS_VISIBLE,
        1060, 390, 70, 24, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BRIGHTNESS_VALUE)), instance_, nullptr);
    exposureValue_ = CreateWindowExW(0, L"STATIC", L"-", WS_CHILD | WS_VISIBLE,
        1060, 460, 70, 24, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EXPOSURE_VALUE)), instance_, nullptr);

    status_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Ready", WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 640, 1120, 28, hwnd_, nullptr, instance_, nullptr);
}

void CameraApp::LayoutControls() {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const int margin = 10;
    const int panelWidth = 360;
    const int statusHeight = 30;
    const int previewWidth = std::max(200, static_cast<int>(rc.right - rc.left) - panelWidth - margin * 3);
    const int previewHeight = std::max(160, static_cast<int>(rc.bottom - rc.top) - statusHeight - margin * 3);
    const int panelX = margin * 2 + previewWidth;

    MoveWindow(preview_, margin, margin, previewWidth, previewHeight, TRUE);
    MoveWindow(cameraLabel_, panelX, 10, 100, 20, TRUE);
    MoveWindow(cameraCombo_, panelX, 30, panelWidth - 20, 360, TRUE);
    MoveWindow(scanButton_, panelX, 64, 90, 28, TRUE);
    MoveWindow(resolutionLabel_, panelX, 110, 100, 20, TRUE);
    MoveWindow(resolutionCombo_, panelX, 130, 220, 200, TRUE);
    MoveWindow(applyResolutionButton_, panelX + 230, 130, 110, 28, TRUE);
    MoveWindow(autofocusCheck_, panelX, 184, 160, 24, TRUE);

    const HWND textLabels[] = { zoomLabel_, focusLabel_, brightnessLabel_, exposureLabel_ };
    const HWND labels[] = { zoomTrack_, focusTrack_, brightnessTrack_, exposureTrack_ };
    const HWND values[] = { zoomValue_, focusValue_, brightnessValue_, exposureValue_ };
    for (int i = 0; i < 4; ++i) {
        const int y = 250 + i * 70;
        MoveWindow(textLabels[i], panelX, y - 20, 160, 20, TRUE);
        MoveWindow(labels[i], panelX, y, 260, 32, TRUE);
        MoveWindow(values[i], panelX + 270, y, 70, 24, TRUE);
    }
    MoveWindow(status_, margin, rc.bottom - statusHeight - margin, rc.right - rc.left - margin * 2, statusHeight, TRUE);
}

void CameraApp::SetStatus(const std::wstring& text) {
    SetWindowTextW(status_, text.c_str());
}

void CameraApp::EnumerateCameras() {
    cameras_.clear();

    ComPtr<ICreateDevEnum> devEnum;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&devEnum));
    if (FAILED(hr)) {
        SetStatus(L"Failed to create DirectShow device enumerator: " + HResultText(hr));
        return;
    }

    ComPtr<IEnumMoniker> enumMoniker;
    hr = devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMoniker, 0);
    if (hr != S_OK) {
        SetStatus(L"No DirectShow video input devices found.");
        return;
    }

    IMoniker* rawMoniker = nullptr;
    while (enumMoniker->Next(1, &rawMoniker, nullptr) == S_OK) {
        CameraDevice device;
        device.moniker.reset(rawMoniker);

        ComPtr<IPropertyBag> bag;
        hr = device.moniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&bag));
        if (SUCCEEDED(hr)) {
            VARIANT value;
            VariantInit(&value);
            if (SUCCEEDED(bag->Read(L"FriendlyName", &value, nullptr)) && value.vt == VT_BSTR) {
                device.name = value.bstrVal;
            }
            VariantClear(&value);
        }
        if (device.name.empty()) {
            device.name = L"Video Capture Device";
        }
        cameras_.push_back(std::move(device));
    }
}

void CameraApp::PopulateCameraCombo() {
    SendMessageW(cameraCombo_, CB_RESETCONTENT, 0, 0);
    for (const auto& camera : cameras_) {
        SendMessageW(cameraCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(camera.name.c_str()));
    }
    if (!cameras_.empty()) {
        SendMessageW(cameraCombo_, CB_SETCURSEL, 0, 0);
        SetStatus(std::to_wstring(cameras_.size()) + L" camera(s) found.");
    } else {
        SetStatus(L"No camera found. Check USB connection and Windows camera privacy settings.");
    }
}

void CameraApp::StartSelectedCamera() {
    int index = static_cast<int>(SendMessageW(cameraCombo_, CB_GETCURSEL, 0, 0));
    if (index < 0 || index >= static_cast<int>(cameras_.size())) {
        return;
    }
    StopGraph();
    if (!BuildGraph(index)) {
        return;
    }
    LoadControlRanges();
}

void CameraApp::StopGraph() {
    if (mediaControl_) {
        mediaControl_->Stop();
    }
    if (videoWindow_) {
        videoWindow_->put_Visible(OAFALSE);
        videoWindow_->put_Owner(static_cast<OAHWND>(0));
    }
    cameraControl_.reset();
    videoProcAmp_.reset();
    videoWindow_.reset();
    mediaControl_.reset();
    captureFilter_.reset();
    captureBuilder_.reset();
    graph_.reset();
}

bool CameraApp::BuildGraph(int cameraIndex) {
    HRESULT hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&graph_));
    if (FAILED(hr)) {
        SetStatus(L"Failed to create filter graph: " + HResultText(hr));
        return false;
    }

    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&captureBuilder_));
    if (FAILED(hr)) {
        SetStatus(L"Failed to create capture graph builder: " + HResultText(hr));
        return false;
    }
    captureBuilder_->SetFiltergraph(graph_.get());

    hr = cameras_[cameraIndex].moniker->BindToObject(nullptr, nullptr, IID_PPV_ARGS(&captureFilter_));
    if (FAILED(hr)) {
        SetStatus(L"Failed to bind selected camera: " + HResultText(hr));
        return false;
    }

    hr = graph_->AddFilter(captureFilter_.get(), cameras_[cameraIndex].name.c_str());
    if (FAILED(hr)) {
        SetStatus(L"Failed to add camera filter: " + HResultText(hr));
        return false;
    }

    const int selectedResolution = static_cast<int>(SendMessageW(resolutionCombo_, CB_GETCURSEL, 0, 0));
    if (selectedResolution >= 0 && selectedResolution < static_cast<int>(std::size(kResolutions))) {
        const auto& res = kResolutions[selectedResolution];
        ApplyResolution(res.width, res.height);
    }

    hr = captureBuilder_->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, captureFilter_.get(), nullptr, nullptr);
    if (FAILED(hr)) {
        hr = captureBuilder_->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, captureFilter_.get(), nullptr, nullptr);
    }
    if (FAILED(hr)) {
        SetStatus(L"Failed to render preview stream: " + HResultText(hr));
        return false;
    }

    graph_->QueryInterface(IID_PPV_ARGS(&mediaControl_));
    graph_->QueryInterface(IID_PPV_ARGS(&videoWindow_));
    captureFilter_->QueryInterface(IID_PPV_ARGS(&cameraControl_));
    captureFilter_->QueryInterface(IID_PPV_ARGS(&videoProcAmp_));

    if (videoWindow_) {
        videoWindow_->put_Owner(reinterpret_cast<OAHWND>(preview_));
        videoWindow_->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS);
        videoWindow_->put_Visible(OATRUE);
        ResizeVideoWindow();
    }

    if (mediaControl_) {
        hr = mediaControl_->Run();
        if (FAILED(hr)) {
            SetStatus(L"Failed to run preview graph: " + HResultText(hr));
            return false;
        }
    }

    SetStatus(L"Started: " + cameras_[cameraIndex].name);
    return true;
}

bool CameraApp::ApplyResolution(long width, long height) {
    if (!captureBuilder_ || !captureFilter_) {
        return false;
    }

    ComPtr<IAMStreamConfig> streamConfig;
    HRESULT hr = GetStreamConfig(&streamConfig);
    if (FAILED(hr)) {
        SetStatus(L"Failed to get stream config: " + HResultText(hr));
        return false;
    }

    int count = 0;
    int size = 0;
    hr = streamConfig->GetNumberOfCapabilities(&count, &size);
    if (FAILED(hr)) {
        SetStatus(L"Failed to inspect camera media types: " + HResultText(hr));
        return false;
    }

    std::vector<BYTE> caps(static_cast<size_t>(size));
    AM_MEDIA_TYPE* best = nullptr;
    AM_MEDIA_TYPE* fallback = nullptr;

    for (int i = 0; i < count; ++i) {
        AM_MEDIA_TYPE* mt = nullptr;
        if (FAILED(streamConfig->GetStreamCaps(i, &mt, caps.data()))) {
            continue;
        }

        if (!IsVideoInfo(mt)) {
            DeleteMediaType(mt);
            continue;
        }

        auto vih = reinterpret_cast<VIDEOINFOHEADER*>(mt->pbFormat);
        const BITMAPINFOHEADER& bmi = vih->bmiHeader;
        if (bmi.biWidth == width && labs(bmi.biHeight) == height) {
            if (IsMjpg(mt->subtype)) {
                best = mt;
                break;
            }
            if (!fallback) {
                fallback = mt;
                continue;
            }
        }
        DeleteMediaType(mt);
    }

    AM_MEDIA_TYPE* chosen = best ? best : fallback;
    if (!chosen) {
        SetStatus(L"Selected resolution is not advertised by this camera.");
        return false;
    }

    hr = streamConfig->SetFormat(chosen);
    DeleteMediaType(chosen);
    if (fallback && fallback != chosen) {
        DeleteMediaType(fallback);
    }

    if (FAILED(hr)) {
        SetStatus(L"Failed to set resolution: " + HResultText(hr));
        return false;
    }
    return true;
}

void CameraApp::ResizeVideoWindow() {
    if (!videoWindow_ || !preview_) {
        return;
    }
    RECT rc{};
    GetClientRect(preview_, &rc);
    videoWindow_->SetWindowPosition(0, 0, rc.right - rc.left, rc.bottom - rc.top);
}

void CameraApp::LoadControlRanges() {
    const ControlRange zoom = GetCameraRange(CameraControl_Zoom);
    const ControlRange focus = GetCameraRange(CameraControl_Focus);
    const ControlRange brightness = GetVideoProcAmpRange(VideoProcAmp_Brightness);
    const ControlRange exposure = GetCameraRange(CameraControl_Exposure);

    long value = 0;
    long flags = 0;
    SetupTrackbar(zoomTrack_, zoomValue_, zoom, GetCameraControl(CameraControl_Zoom, value, flags) ? value : zoom.defaultValue);
    SetupTrackbar(focusTrack_, focusValue_, focus, GetCameraControl(CameraControl_Focus, value, flags) ? value : focus.defaultValue);
    SetupTrackbar(brightnessTrack_, brightnessValue_, brightness, GetVideoProcAmp(VideoProcAmp_Brightness, value, flags) ? value : brightness.defaultValue);
    SetupTrackbar(exposureTrack_, exposureValue_, exposure, GetCameraControl(CameraControl_Exposure, value, flags) ? value : exposure.defaultValue);

    if (GetCameraControl(CameraControl_Focus, value, flags)) {
        Button_SetCheck(autofocusCheck_, (flags & CameraControl_Flags_Auto) ? BST_CHECKED : BST_UNCHECKED);
    }
}

void CameraApp::SetupTrackbar(HWND hwndTrackbar, HWND hwndValue, const ControlRange& range, long current) {
    EnableWindow(hwndTrackbar, range.supported ? TRUE : FALSE);
    if (!range.supported) {
        SetWindowTextW(hwndValue, L"N/A");
        return;
    }

    SendMessageW(hwndTrackbar, TBM_SETRANGEMIN, TRUE, range.minValue);
    SendMessageW(hwndTrackbar, TBM_SETRANGEMAX, TRUE, range.maxValue);
    SendMessageW(hwndTrackbar, TBM_SETTICFREQ, std::max(1L, range.step), 0);
    SendMessageW(hwndTrackbar, TBM_SETPOS, TRUE, std::clamp(current, range.minValue, range.maxValue));
    UpdateTrackbarValue(hwndTrackbar, hwndValue);
}

void CameraApp::UpdateTrackbarValue(HWND hwndTrackbar, HWND hwndValue) {
    long pos = static_cast<long>(SendMessageW(hwndTrackbar, TBM_GETPOS, 0, 0));
    SetWindowTextW(hwndValue, std::to_wstring(pos).c_str());
}

void CameraApp::ApplyCameraControl(long property, HWND hwndTrackbar) {
    if (!cameraControl_) {
        return;
    }
    const long value = static_cast<long>(SendMessageW(hwndTrackbar, TBM_GETPOS, 0, 0));
    const long flags = (property == CameraControl_Focus && Button_GetCheck(autofocusCheck_) == BST_CHECKED)
        ? CameraControl_Flags_Auto
        : CameraControl_Flags_Manual;
    cameraControl_->Set(property, value, flags);
}

void CameraApp::ApplyVideoProcAmp(long property, HWND hwndTrackbar) {
    if (!videoProcAmp_) {
        return;
    }
    const long value = static_cast<long>(SendMessageW(hwndTrackbar, TBM_GETPOS, 0, 0));
    videoProcAmp_->Set(property, value, VideoProcAmp_Flags_Manual);
}

void CameraApp::ApplyAutofocus() {
    if (!cameraControl_) {
        return;
    }
    long value = 0;
    long flags = 0;
    GetCameraControl(CameraControl_Focus, value, flags);
    const long newFlags = Button_GetCheck(autofocusCheck_) == BST_CHECKED
        ? CameraControl_Flags_Auto
        : CameraControl_Flags_Manual;
    HRESULT hr = cameraControl_->Set(CameraControl_Focus, value, newFlags);
    if (FAILED(hr)) {
        SetStatus(L"Failed to change autofocus: " + HResultText(hr));
    }
}

bool CameraApp::GetCameraControl(long property, long& value, long& flags) {
    return cameraControl_ && SUCCEEDED(cameraControl_->Get(property, &value, &flags));
}

bool CameraApp::GetVideoProcAmp(long property, long& value, long& flags) {
    return videoProcAmp_ && SUCCEEDED(videoProcAmp_->Get(property, &value, &flags));
}

ControlRange CameraApp::GetCameraRange(long property) {
    ControlRange range{};
    if (!cameraControl_) {
        return range;
    }
    range.supported = SUCCEEDED(cameraControl_->GetRange(
        property, &range.minValue, &range.maxValue, &range.step, &range.defaultValue, &range.flags));
    if (range.step <= 0) {
        range.step = 1;
    }
    return range;
}

ControlRange CameraApp::GetVideoProcAmpRange(long property) {
    ControlRange range{};
    if (!videoProcAmp_) {
        return range;
    }
    range.supported = SUCCEEDED(videoProcAmp_->GetRange(
        property, &range.minValue, &range.maxValue, &range.step, &range.defaultValue, &range.flags));
    if (range.step <= 0) {
        range.step = 1;
    }
    return range;
}

HRESULT CameraApp::GetCapturePin(IBaseFilter* filter, IPin** pin) {
    if (!filter || !pin) {
        return E_POINTER;
    }

    ComPtr<IEnumPins> enumPins;
    HRESULT hr = filter->EnumPins(&enumPins);
    if (FAILED(hr)) {
        return hr;
    }

    IPin* rawPin = nullptr;
    while (enumPins->Next(1, &rawPin, nullptr) == S_OK) {
        ComPtr<IPin> current;
        current.reset(rawPin);

        PIN_DIRECTION direction{};
        if (SUCCEEDED(current->QueryDirection(&direction)) && direction == PINDIR_OUTPUT) {
            IKsPropertySet* propertySet = nullptr;
            if (SUCCEEDED(current->QueryInterface(IID_PPV_ARGS(&propertySet)))) {
                GUID category{};
                DWORD returned = 0;
                HRESULT categoryHr = propertySet->Get(
                    AMPROPSETID_Pin,
                    AMPROPERTY_PIN_CATEGORY,
                    nullptr,
                    0,
                    &category,
                    sizeof(category),
                    &returned);
                propertySet->Release();
                if (SUCCEEDED(categoryHr) && (category == PIN_CATEGORY_CAPTURE || category == PIN_CATEGORY_PREVIEW)) {
                    *pin = current.get();
                    (*pin)->AddRef();
                    return S_OK;
                }
            } else {
                *pin = current.get();
                (*pin)->AddRef();
                return S_OK;
            }
        }
    }

    return E_FAIL;
}

HRESULT CameraApp::GetStreamConfig(IAMStreamConfig** streamConfig) {
    if (!streamConfig) {
        return E_POINTER;
    }
    HRESULT hr = captureBuilder_->FindInterface(
        &PIN_CATEGORY_CAPTURE,
        &MEDIATYPE_Video,
        captureFilter_.get(),
        IID_PPV_ARGS(streamConfig));
    if (SUCCEEDED(hr)) {
        return hr;
    }
    return captureBuilder_->FindInterface(
        &PIN_CATEGORY_PREVIEW,
        &MEDIATYPE_Video,
        captureFilter_.get(),
        IID_PPV_ARGS(streamConfig));
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
    CameraApp app;
    return app.Run(instance, show);
}
