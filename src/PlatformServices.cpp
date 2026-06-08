#include "Engine/Platform/PlatformServices.hpp"

#include "Engine/Log.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <climits>
#include <memory>
#include <string>
#include <vector>

namespace
{
float clampScale(float scale)
{
    return std::max(scale, 0.01f);
}

int logicalFloor(float pixelPos, float scale)
{
    return static_cast<int>(std::floor(pixelPos / scale));
}

int logicalCeil(float pixelPos, float scale)
{
    return static_cast<int>(std::ceil(pixelPos / scale));
}

#ifdef PET_P2_TESTS
class TestWindowEnumerator : public IWindowEnumerator
{
public:
    void init() override {}

    void onMonitorConnectionChanged(void*, int) override {}

    int getMonitorsCount() const override
    {
        return 0;
    }

    int getPrimaryMonitorIndex() const override
    {
        return -1;
    }

    void getMainMonitorWorkingArea(Vec2i& position, Vec2i& size) const override
    {
        position = Vec2i::zero();
        size     = Vec2i::zero();
    }

    Vec2i getMonitorsSize() const override
    {
        return Vec2i::zero();
    }

    void getMonitorPixelPosition(int, Vec2i& position) const override
    {
        position = Vec2i::zero();
    }

    void getMonitorPixelSize(int, Vec2i& size) const override
    {
        size = Vec2i::zero();
    }

    void getMonitorContentScale(int, Vec2& scale) const override
    {
        scale = Vec2::one();
    }

    void getMonitorPosition(int, Vec2i& position) const override
    {
        position = Vec2i::zero();
    }

    void getMonitorSize(int, Vec2i& size) const override
    {
        size = Vec2i::zero();
    }

    Vec2i getMonitorPhysicalSize() const override
    {
        return Vec2i::zero();
    }

    Vec2i getMonitorPhysicalSize(int) const override
    {
        return Vec2i::zero();
    }
};
#endif
} // namespace

#ifdef _WIN32
#include <shobjidl.h>
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace
{
class WindowEnumeratorGLFW : public IWindowEnumerator
{
protected:
    std::vector<GLFWmonitor*> monitors;

    int getIndex(const void* monitor) const
    {
        for (int i = 0; i < static_cast<int>(monitors.size()); ++i)
        {
            if (monitors[i] == monitor)
                return i;
        }
        return -1;
    }

public:
    void init() override
    {
        monitors.clear();

        int            monitorCount;
        GLFWmonitor** pMonitors = glfwGetMonitors(&monitorCount);
        monitors.reserve(std::max(0, monitorCount));

        for (int i = 0; i < monitorCount; ++i)
            monitors.emplace_back(pMonitors[i]);
    }

    void onMonitorConnectionChanged(void* monitor, int event) override
    {
        GLFWmonitor* glfwMonitor = static_cast<GLFWmonitor*>(monitor);
        switch (event)
        {
        case GLFW_CONNECTED:
            if (getIndex(glfwMonitor) == -1)
                monitors.emplace_back(glfwMonitor);
            break;
        case GLFW_DISCONNECTED:
            {
                int index = getIndex(glfwMonitor);
                if (index != -1)
                    monitors.erase(monitors.begin() + index);
            }
            break;
        default:
            break;
        }
    }

    int getMonitorsCount() const override
    {
        return static_cast<int>(monitors.size());
    }

    int getPrimaryMonitorIndex() const override
    {
        GLFWmonitor* primary = glfwGetPrimaryMonitor();
        if (!primary)
            return -1;

        return getIndex(primary);
    }

    void getMainMonitorWorkingArea(Vec2i& position, Vec2i& size) const override
    {
        const int mainMonitor = getPrimaryMonitorIndex();
        if (mainMonitor < 0)
        {
            position = Vec2i::zero();
            size     = Vec2i::zero();
            return;
        }

        getMonitorPosition(mainMonitor, position);
        getMonitorSize(mainMonitor, size);
    }

    void getMonitorPixelPosition(int index, Vec2i& position) const override
    {
        position = Vec2i::zero();
        if (index < 0 || index >= static_cast<int>(monitors.size()))
            return;

        glfwGetMonitorPos(monitors[index], &position.x, &position.y);
    }

    void getMonitorPixelSize(int index, Vec2i& size) const override
    {
        size = Vec2i::zero();
        if (index < 0 || index >= static_cast<int>(monitors.size()))
            return;

        const GLFWvidmode* currentVideoMode = glfwGetVideoMode(monitors[index]);
        if (currentVideoMode)
        {
            size.x = currentVideoMode->width;
            size.y = currentVideoMode->height;
        }
    }

    void getMonitorContentScale(int index, Vec2& scale) const override
    {
        scale = Vec2::one();
        if (index < 0 || index >= static_cast<int>(monitors.size()))
            return;

        float xScale = 1.f;
        float yScale = 1.f;
        glfwGetMonitorContentScale(monitors[index], &xScale, &yScale);
        if (xScale > 0.f && yScale > 0.f)
            scale = {xScale, yScale};
    }

    Vec2i getMonitorsSize() const override
    {
        Vec2i size = Vec2i::zero();
        if (!monitors.size())
            return size;

        int minX = INT_MAX;
        int minY = INT_MAX;
        int maxX = INT_MIN;
        int maxY = INT_MIN;

        for (int i = 0; i < static_cast<int>(monitors.size()); ++i)
        {
            Vec2i monitorPixelPos;
            Vec2i monitorPixelSize;
            getMonitorPixelPosition(i, monitorPixelPos);
            getMonitorPixelSize(i, monitorPixelSize);

            if (!monitorPixelSize.x || !monitorPixelSize.y)
                continue;

            Vec2 scale{1.f, 1.f};
            getMonitorContentScale(i, scale);
            const float safeScaleX = clampScale(scale.x);
            const float safeScaleY = clampScale(scale.y);

            const int logicalLeft = logicalFloor(static_cast<float>(monitorPixelPos.x), safeScaleX);
            const int logicalTop = logicalFloor(static_cast<float>(monitorPixelPos.y), safeScaleY);
            const int logicalRight = logicalCeil(static_cast<float>(monitorPixelPos.x + monitorPixelSize.x), safeScaleX);
            const int logicalBottom = logicalCeil(static_cast<float>(monitorPixelPos.y + monitorPixelSize.y), safeScaleY);

            minX = std::min(minX, logicalLeft);
            minY = std::min(minY, logicalTop);
            maxX = std::max(maxX, logicalRight);
            maxY = std::max(maxY, logicalBottom);
        }

        if (minX == INT_MAX || minY == INT_MAX || maxX == INT_MIN || maxY == INT_MIN)
            return size;

        size.x = maxX - minX;
        size.y = maxY - minY;
        return size;
    }

    void getMonitorPosition(int index, Vec2i& position) const override
    {
        position = Vec2i::zero();
        if (index < 0 || index >= static_cast<int>(monitors.size()))
            return;

        Vec2i pixelPosition = Vec2i::zero();
        getMonitorPixelPosition(index, pixelPosition);

        Vec2 scale{1.f, 1.f};
        getMonitorContentScale(index, scale);
        const float safeScaleX = clampScale(scale.x);
        const float safeScaleY = clampScale(scale.y);

        position.x = logicalFloor(static_cast<float>(pixelPosition.x), safeScaleX);
        position.y = logicalFloor(static_cast<float>(pixelPosition.y), safeScaleY);
    }

    void getMonitorSize(int index, Vec2i& size) const override
    {
        size = Vec2i::zero();
        if (index < 0 || index >= static_cast<int>(monitors.size()))
            return;

        Vec2i pixelSize = Vec2i::zero();
        getMonitorPixelSize(index, pixelSize);

        if (!pixelSize.x || !pixelSize.y)
            return;

        Vec2 scale{1.f, 1.f};
        getMonitorContentScale(index, scale);
        const float safeScaleX = clampScale(scale.x);
        const float safeScaleY = clampScale(scale.y);

        Vec2i pixelPosition = Vec2i::zero();
        getMonitorPixelPosition(index, pixelPosition);
        const int logicalLeft = logicalFloor(static_cast<float>(pixelPosition.x), safeScaleX);
        const int logicalTop  = logicalFloor(static_cast<float>(pixelPosition.y), safeScaleY);
        const int logicalRight = logicalCeil(static_cast<float>(pixelPosition.x + pixelSize.x), safeScaleX);
        const int logicalBottom = logicalCeil(static_cast<float>(pixelPosition.y + pixelSize.y), safeScaleY);

        size.x = std::max(1, logicalRight - logicalLeft);
        size.y = std::max(1, logicalBottom - logicalTop);
    }

    Vec2i getMonitorPhysicalSize() const override
    {
        Vec2i sizeMM = Vec2i::zero();
        for (int i = 0; i < static_cast<int>(monitors.size()); ++i)
        {
            int width_mm = 0;
            int height_mm = 0;
            glfwGetMonitorPhysicalSize(monitors[i], &width_mm, &height_mm);
            sizeMM.x += width_mm;
            sizeMM.y += height_mm;
        }
        return sizeMM;
    }

    Vec2i getMonitorPhysicalSize(int index) const override
    {
        if (index < 0 || index >= static_cast<int>(monitors.size()))
            return Vec2i::zero();

        int widthMM  = 0;
        int heightMM = 0;
        glfwGetMonitorPhysicalSize(monitors[index], &widthMM, &heightMM);
        return {widthMM, heightMM};
    }
};

#ifdef _WIN32
class Win32WindowPlatform : public IWindowPlatform
{
    struct FilterConverter
    {
        std::vector<std::wstring> names;
        std::vector<std::wstring> specs;
        std::vector<COMDLG_FILTERSPEC> nativeFilters;
    };

    static std::wstring toWide(const std::string& value)
    {
        return {value.begin(), value.end()};
    }

    static std::filesystem::path pickDialog(const std::vector<IWindowPlatform::DialogFilter>& filters, bool folderMode,
                                           const std::string& title)
    {
        std::filesystem::path result;

        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(hr))
            return result;

        IFileOpenDialog* pFileOpen = nullptr;
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog,
                              reinterpret_cast<void**>(&pFileOpen));
        if (FAILED(hr))
        {
            CoUninitialize();
            return result;
        }

        if (folderMode)
        {
            if (FAILED(pFileOpen->SetOptions(FOS_PICKFOLDERS)))
            {
                pFileOpen->Release();
                CoUninitialize();
                return result;
            }
        }

        const std::wstring titleWide = toWide(title);
        if (FAILED(pFileOpen->SetTitle(titleWide.c_str())))
        {
            pFileOpen->Release();
            CoUninitialize();
            return result;
        }

        FilterConverter converter;
        if (!folderMode && !filters.empty())
        {
            converter.names.reserve(filters.size());
            converter.specs.reserve(filters.size());
            converter.nativeFilters.reserve(filters.size());

            for (const auto& filter : filters)
            {
                converter.names.push_back(toWide(filter.name));
                converter.specs.push_back(toWide(filter.pattern));
                converter.nativeFilters.push_back(
                    COMDLG_FILTERSPEC{converter.names.back().c_str(), converter.specs.back().c_str()});
            }

            if (FAILED(pFileOpen->SetFileTypes(static_cast<UINT>(converter.nativeFilters.size()),
                                              converter.nativeFilters.data())))
            {
                pFileOpen->Release();
                CoUninitialize();
                return result;
            }
        }

        hr = pFileOpen->Show(NULL);
        if (SUCCEEDED(hr))
        {
            IShellItem* pItem = nullptr;
            hr = pFileOpen->GetResult(&pItem);
            if (SUCCEEDED(hr))
            {
                LPWSTR pszFilePath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr))
                {
                    result = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }

        pFileOpen->Release();
        CoUninitialize();
        return result;
    }

public:
    std::filesystem::path openFileDialog(const std::string& title, const std::vector<DialogFilter>& filter) override
    {
        return pickDialog(filter, false, title);
    }

    std::filesystem::path openFolderDialog(const std::string& title) override
    {
        return pickDialog({}, true, title);
    }

    void recycleToTrash(const std::filesystem::path& path) override
    {
        SHFILEOPSTRUCT fileOp{};
        fileOp.hwnd   = NULL;
        fileOp.wFunc  = FO_DELETE;
        std::string temp = path.string();
        temp.push_back('\0');
        temp.push_back('\0');
        fileOp.pFrom  = temp.c_str();
        fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOERRORUI | FOF_NOCONFIRMATION | FOF_SILENT;
        if (SHFileOperation(&fileOp))
            logf("Failed to move file '%s' to Recycle Bin", path.c_str());
    }

    void openPath(const std::string& path) override
    {
        ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
};

class Win32ScreenCapture : public IScreenCapture
{
public:
    Data capture(int x, int y, int w, int h, bool saveIntoClipboard = false) override
    {
        Data data{};
        if (w * h == 0)
            return data;

        HDC hScreen = GetDC(NULL);
        HDC hDC = CreateCompatibleDC(hScreen);
        if (!hDC)
        {
            ReleaseDC(NULL, hScreen);
            return data;
        }

        HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, w, h);
        if (!hBitmap)
        {
            DeleteDC(hDC);
            ReleaseDC(NULL, hScreen);
            return data;
        }

        HGDIOBJ oldObj = SelectObject(hDC, hBitmap);
        if (!BitBlt(hDC, 0, 0, w, h, hScreen, x, y, SRCCOPY))
            logf("BitBlt has failed");

        if (saveIntoClipboard)
        {
            OpenClipboard(NULL);
            EmptyClipboard();
            SetClipboardData(CF_BITMAP, hBitmap);
            CloseClipboard();
        }

        BITMAP bitmap;
        GetObject(hBitmap, sizeof(BITMAP), &bitmap);

        BITMAPINFOHEADER bi{};
        bi.biSize          = sizeof(BITMAPINFOHEADER);
        bi.biWidth         = bitmap.bmWidth;
        bi.biHeight        = bitmap.bmHeight;
        bi.biPlanes        = bitmap.bmPlanes;
        bi.biBitCount      = bitmap.bmBitsPixel;
        bi.biCompression   = BI_RGB;
        bi.biSizeImage     = 0;
        bi.biXPelsPerMeter = 0;
        bi.biYPelsPerMeter = 0;
        bi.biClrUsed       = 0;
        bi.biClrImportant  = 0;

        if (bi.biBitCount == 0)
            bi.biBitCount = 32;

        const DWORD dwBmpSize = (bitmap.bmWidth * bi.biBitCount + 31) / 32 * 4 * bitmap.bmHeight;

        data.bits        = new char[dwBmpSize];
        data.bitPerPixel = static_cast<std::uint32_t>(bi.biBitCount);
        data.width       = static_cast<std::uint32_t>(bitmap.bmWidth);
        data.height      = static_cast<std::uint32_t>(bitmap.bmHeight);

        GetDIBits(hScreen, hBitmap, 0, static_cast<UINT>(bitmap.bmHeight), data.bits, reinterpret_cast<BITMAPINFO*>(&bi),
                  DIB_RGB_COLORS);

        SelectObject(hDC, oldObj);
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
        DeleteObject(hBitmap);
        return data;
    }

    void release(Data& data) override
    {
        delete[] static_cast<char*>(data.bits);
        data = Data{};
    }
};
#else
class DummyScreenCapture : public IScreenCapture
{
public:
    Data capture(int, int, int, int, bool = false) override
    {
        return Data{};
    }

    void release(Data&) override
    {
    }
};

class DummyWindowPlatform : public IWindowPlatform
{
public:
    std::filesystem::path openFileDialog(const std::string&,
                                        const std::vector<DialogFilter>&) override
    {
        return {};
    }

    std::filesystem::path openFolderDialog(const std::string&) override
    {
        return {};
    }

    void recycleToTrash(const std::filesystem::path&) override
    {
    }

    void openPath(const std::string& path) override
    {
        std::string command = "xdg-open ";
        command += path;
        std::system(command.c_str());
    }
};
#endif
} // namespace

namespace PlatformServices
{
std::unique_ptr<IWindowEnumerator> createWindowEnumerator()
{
#ifdef PET_P2_TESTS
    return std::make_unique<TestWindowEnumerator>();
#else
    return std::make_unique<WindowEnumeratorGLFW>();
#endif
}

std::unique_ptr<IScreenCapture> createScreenCapture()
{
#ifdef _WIN32
    return std::make_unique<Win32ScreenCapture>();
#else
    return std::make_unique<DummyScreenCapture>();
#endif
}

std::unique_ptr<IWindowPlatform> createWindowPlatform()
{
#ifdef _WIN32
    return std::make_unique<Win32WindowPlatform>();
#else
    return std::make_unique<DummyWindowPlatform>();
#endif
}
} // namespace PlatformServices
