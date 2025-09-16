﻿/**************************************************************************

Author: 肖嘉威

Version: 3.0.0

Date: 2025/9/14

Description: 纯GDI+实现的启动遮罩，支持DPI缩放和自动进度更新

**************************************************************************/
#define NOMINMAX
#include "splashscreen.h"

#include <algorithm>
#include <format>
#include <vector>

using namespace Gdiplus;

// 静态变量用于GDI+初始化
static ULONG_PTR g_gdiplusToken = 0;
static int g_gdiplusRefCount = 0;

SplashScreen::SplashScreen(const std::vector<char> &pngData, const std::wstring &programName,
                           const std::wstring &programVersion, bool showProgress, bool showProgressText) :
    m_hwnd(nullptr), m_width(400), m_height(300), m_programName(programName), m_programVersion(programVersion),
    m_showProgress(showProgress), m_showProgressText(showProgressText), m_statusText(L"正在初始化..."), m_progress(0),
    m_progressHeight(0), m_autoProgress(false), m_progressStep(0.5), m_progressInterval(50), m_autoCloseDelay(0),
    m_gdiplusBitmap(nullptr), m_cachedBitmap(nullptr) {

    // 初始化GDI+
    if (g_gdiplusRefCount == 0) {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);
    }
    g_gdiplusRefCount++;

    // 从PNG数据创建位图
    CreateBitmapFromPNG(pngData);

    // 计算布局
    CalculateLayout();

    // 注册窗口类
    auto className = L"JarPackagerSplashScreenClass";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = className;

    RegisterClassExW(&wc);
    // 创建分层窗口
    m_hwnd = CreateWindowExW(
            // WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            WS_EX_LAYERED | WS_EX_TOOLWINDOW, className, L"JarPackagerSplashScreen", WS_POPUP, CW_USEDEFAULT,
            CW_USEDEFAULT, m_width, m_height, nullptr, nullptr, GetModuleHandle(nullptr), this);

    // 创建缓存位图
    CreateCachedBitmap();
}

SplashScreen::~SplashScreen() {
    StopAutoProgress(); // 停止定时器
    Close();

    // 清理GDI+位图
    if (m_gdiplusBitmap) {
        delete m_gdiplusBitmap;
    }
    if (m_cachedBitmap) {
        delete m_cachedBitmap;
    }

    // 清理GDI+
    g_gdiplusRefCount--;
    if (g_gdiplusRefCount == 0) {
        GdiplusShutdown(g_gdiplusToken);
    }
}

float SplashScreen::GetDPIScale() {
    HDC hdc = GetDC(nullptr);
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);

    return static_cast<float>(dpiX) / 96.0f;
}

void SplashScreen::CreateBitmapFromPNG(const std::vector<char> &pngData) {
    if (pngData.empty()) {
        CreateDefaultBackground();
        return;
    }

    // 创建内存流
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, pngData.size());
    if (!hMem) {
        CreateDefaultBackground();
        return;
    }

    void *pData = GlobalLock(hMem);
    if (!pData) {
        GlobalFree(hMem);
        CreateDefaultBackground();
        return;
    }

    memcpy(pData, pngData.data(), pngData.size());
    GlobalUnlock(hMem);

    IStream *pStream = nullptr;
    HRESULT hr = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
    if (FAILED(hr)) {
        GlobalFree(hMem);
        CreateDefaultBackground();
        return;
    }

    // 创建GDI+位图
    m_gdiplusBitmap = Bitmap::FromStream(pStream);
    pStream->Release();

    if (!m_gdiplusBitmap || m_gdiplusBitmap->GetLastStatus() != Ok) {
        CreateDefaultBackground();
        return;
    }

    // 获取屏幕尺寸并计算窗口大小
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int targetHeight = static_cast<int>(screenHeight / 4.0f);

    UINT imgWidth = m_gdiplusBitmap->GetWidth();
    UINT imgHeight = m_gdiplusBitmap->GetHeight();

    // 根据图片比例计算宽度
    int targetWidth = static_cast<int>(targetHeight * (static_cast<float>(imgWidth) / imgHeight));

    m_width = targetWidth;
    m_height = targetHeight;
}

void SplashScreen::CreateDefaultBackground() {
    m_width = static_cast<int>(600 * GetDPIScale());
    m_height = static_cast<int>(200 * GetDPIScale());

    // 创建默认渐变背景位图
    m_gdiplusBitmap = new Bitmap(m_width, m_height, PixelFormat32bppARGB);
    Graphics g(m_gdiplusBitmap);

    // 创建渐变画刷
    LinearGradientBrush gradientBrush(Point(0, 0), Point(0, m_height), Color(255, 30, 60, 120), // 起始颜色
                                      Color(255, 90, 150, 255) // 结束颜色
    );

    g.FillRectangle(&gradientBrush, 0, 0, m_width, m_height);
}

void SplashScreen::CreateCachedBitmap() {
    if (!m_gdiplusBitmap)
        return;

    // 创建缓存位图
    m_cachedBitmap = new Bitmap(m_width, m_height, PixelFormat32bppARGB);

    DrawToCachedBitmap();
}

void SplashScreen::CalculateLayout() {
    float dpiScale = GetDPIScale();

    // 进度条高度
    m_progressHeight = static_cast<int>(8 * dpiScale);
    float margin = 15.0f * dpiScale;

    // 标题位置（图片区域的上1/3处）
    m_titleRect = RectF(margin, static_cast<float>(m_height) / 3.0f, static_cast<float>(m_width) - 2 * margin,
                        50.0f * dpiScale);

    // 版本位置（标题下方）
    m_versionRect = RectF(margin, m_titleRect.Y + m_titleRect.Height + 10.0f * dpiScale,
                          static_cast<float>(m_width) - 2 * margin, 30.0f * dpiScale);

    // 进度条位置（图片底部）
    m_progressRect = RectF(0, static_cast<float>(m_height - m_progressHeight), static_cast<float>(m_width),
                           static_cast<float>(m_progressHeight));

    // 状态文本位置（进度条上方）
    m_statusRect = RectF(margin, m_progressRect.Y - 35.0f * dpiScale, static_cast<float>(m_width) - 2 * margin,
                         25.0f * dpiScale);
}

float SplashScreen::CalculateOptimalFontSize(const Graphics *graphics, const FontFamily *fontFamily,
                                             const std::wstring &text, const RectF &targetRect, const float maxFontSize,
                                             const FontStyle fontStyle) {
    float fontSize = maxFontSize;
    const float minFontSize = 8.0f;
    const float step = 2.0f;

    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);
    format.SetFormatFlags(StringFormatFlagsNoWrap);

    while (fontSize > minFontSize) {
        Font font(fontFamily, fontSize, fontStyle, UnitPixel);
        RectF boundingBox;

        // 测量文本边界
        graphics->MeasureString(text.c_str(), -1, &font, targetRect, &format, &boundingBox);

        // 检查是否适合目标矩形（留一些边距）
        if (boundingBox.Width <= targetRect.Width * 0.95f && boundingBox.Height <= targetRect.Height * 0.95f) {
            break;
        }

        fontSize -= step;
    }

    return std::max(fontSize, minFontSize);
}

void SplashScreen::DrawToCachedBitmap() {
    if (!m_cachedBitmap || !m_gdiplusBitmap)
        return;

    Graphics g(m_cachedBitmap);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    // 清除背景为透明
    g.Clear(Color(0, 0, 0, 0));

    // 绘制背景图片
    g.DrawImage(m_gdiplusBitmap, 0, 0, m_width, m_height);

    float dpiScale = GetDPIScale();

    // 绘制标题
    if (!m_programName.empty()) {
        FontFamily fontFamily(L"Microsoft YaHei");

        // 计算合适的标题字体大小
        float titleFontSize =
                CalculateOptimalFontSize(&g, &fontFamily, m_programName, m_titleRect, 42.0f * dpiScale, FontStyleBold);

        Font titleFont(&fontFamily, titleFontSize, FontStyleBold, UnitPixel);

        // 阴影
        SolidBrush shadowBrush(Color(128, 0, 0, 0));
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        format.SetTrimming(StringTrimmingEllipsisCharacter);
        format.SetFormatFlags(StringFormatFlagsNoWrap);

        RectF shadowRect = m_titleRect;
        shadowRect.Offset(2.0f * dpiScale, 2.0f * dpiScale);
        g.DrawString(m_programName.c_str(), -1, &titleFont, shadowRect, &format, &shadowBrush);

        // 主文本
        SolidBrush whiteBrush(Color(255, 255, 255, 255));
        g.DrawString(m_programName.c_str(), -1, &titleFont, m_titleRect, &format, &whiteBrush);
    }

    // 绘制版本
    if (!m_programVersion.empty()) {
        FontFamily fontFamily(L"Microsoft YaHei");

        // 计算合适的版本字体大小
        float versionFontSize = CalculateOptimalFontSize(&g, &fontFamily, m_programVersion, m_versionRect,
                                                         24.0f * dpiScale, FontStyleRegular);

        Font versionFont(&fontFamily, versionFontSize, FontStyleRegular, UnitPixel);

        SolidBrush grayBrush(Color(255, 200, 200, 200));
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        format.SetTrimming(StringTrimmingEllipsisCharacter);
        format.SetFormatFlags(StringFormatFlagsNoWrap);

        g.DrawString(m_programVersion.c_str(), -1, &versionFont, m_versionRect, &format, &grayBrush);
    }
}

void SplashScreen::StartAutoProgress(double stepSize, DWORD intervalMs) {
    m_autoProgress = true;
    m_progressStep = stepSize;
    m_progressInterval = intervalMs;

    if (m_hwnd) {
        SetTimer(m_hwnd, PROGRESS_TIMER_ID, m_progressInterval, nullptr);
    }
}

void SplashScreen::StopAutoProgress() {
    if (m_hwnd && m_autoProgress) {
        KillTimer(m_hwnd, PROGRESS_TIMER_ID);
    }
    m_autoProgress = false;
}

void SplashScreen::SetAutoCloseDelay(const DWORD delayMs) {
    m_autoCloseDelay = delayMs;

    if (m_hwnd && delayMs > 0) {
        SetTimer(m_hwnd, AUTO_CLOSE_TIMER_ID, delayMs, nullptr);
    }
}

void SplashScreen::UpdateDisplay() {
    if (!m_hwnd || !m_cachedBitmap)
        return;

    // 创建临时位图用于绘制当前帧
    Bitmap *tempBitmap = new Bitmap(m_width, m_height, PixelFormat32bppARGB);
    Graphics g(tempBitmap);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    // 绘制缓存的背景
    g.DrawImage(m_cachedBitmap, 0, 0);

    float dpiScale = GetDPIScale();
    // 绘制状态文本
    if (m_showProgressText &&!m_statusText.empty()) {
        const FontFamily fontFamily(L"Microsoft YaHei");

        // 计算合适的状态文本字体大小
        const float statusFontSize = CalculateOptimalFontSize(&g, &fontFamily, m_statusText, m_statusRect,
                                                              15.0f * dpiScale, FontStyleRegular);

        const Font statusFont(&fontFamily, statusFontSize, FontStyleRegular, UnitPixel);

        SolidBrush lightGrayBrush(Color(255, 180, 180, 180));
        StringFormat format;
        format.SetAlignment(StringAlignmentNear);
        format.SetLineAlignment(StringAlignmentCenter);
        format.SetTrimming(StringTrimmingEllipsisCharacter);
        format.SetFormatFlags(StringFormatFlagsNoWrap);

        g.DrawString(m_statusText.c_str(), -1, &statusFont, m_statusRect, &format, &lightGrayBrush);
    }

    // 绘制进度条
    if (m_showProgress && m_progress > 0) {
        const float barWidth = static_cast<float>(m_width) * static_cast<float>(m_progress) / 100.0f;
        SolidBrush barBrush(Color(255, 0, 117, 255)); // 蓝色进度条

        g.FillRectangle(&barBrush, 0.0f, m_progressRect.Y, barWidth, m_progressRect.Height);
    }

    // 更新分层窗口
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    HBITMAP hBitmap = nullptr;
    tempBitmap->GetHBITMAP(Color(0, 0, 0, 0), &hBitmap);
    HBITMAP hOldBitmap = (HBITMAP) SelectObject(hdcMem, hBitmap);

    POINT ptSrc = {0, 0};
    SIZE size = {m_width, m_height};

    RECT windowRect;
    GetWindowRect(m_hwnd, &windowRect);
    POINT ptDst = {windowRect.left, windowRect.top};

    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

    UpdateLayeredWindow(m_hwnd, hdcScreen, &ptDst, &size, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    // 清理
    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    delete tempBitmap;
}

bool SplashScreen::Show() {
    if (!m_hwnd)
        return false;

    // 重新计算布局
    CalculateLayout();

    // 重新创建缓存位图
    if (m_cachedBitmap) {
        delete m_cachedBitmap;
    }
    CreateCachedBitmap();

    // 获取屏幕尺寸，居中显示
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - m_width) / 2;
    int y = (screenHeight - m_height) / 2;

    // 设置窗口位置和大小
    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, m_width, m_height, SWP_NOACTIVATE);

    // 绘制并显示窗口
    UpdateDisplay();
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    return true;
}

void SplashScreen::Hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void SplashScreen::Close() {
    StopAutoProgress(); // 确保停止所有定时器

    if (m_hwnd) {
        KillTimer(m_hwnd, PROGRESS_TIMER_ID);
        KillTimer(m_hwnd, AUTO_CLOSE_TIMER_ID);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void SplashScreen::SetProgress(const double progress) {
    m_progress = std::max(0.0, std::min(100.0, progress));
    UpdateDisplay();
}

void SplashScreen::SetStatusText(const std::wstring &statusText) {
    m_statusText = statusText;
    UpdateDisplay();
}

void SplashScreen::UpdateProgress(int progress, const std::wstring *statusText) {
    m_progress = std::clamp(progress, 0, 100);
    if (statusText) {
        m_statusText = *statusText;
    }
    UpdateDisplay();
}

LRESULT CALLBACK SplashScreen::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    SplashScreen *pThis = nullptr;

    if (uMsg == WM_NCCREATE) {
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        pThis = reinterpret_cast<SplashScreen *>(pCreateStruct->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<SplashScreen *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (uMsg) {
            case WM_TIMER: {
                if (wParam == PROGRESS_TIMER_ID && pThis->m_autoProgress) {
                    // 自动更新进度
                    if (pThis->m_progress < 100.0) {
                        pThis->m_progress += pThis->m_progressStep;
                        if (pThis->m_progress > 100.0) {
                            pThis->m_progress = 100.0;
                        }

                        // 更新状态文本
                        pThis->m_statusText = std::format(L"正在加载... {:.2f}%", pThis->m_progress);

                        // 如果达到100%，停止自动进度
                        if (pThis->m_progress >= 100.0) {
                            pThis->StopAutoProgress();
                        }
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                } else if (wParam == AUTO_CLOSE_TIMER_ID) {
                    // 自动关闭
                    KillTimer(hwnd, AUTO_CLOSE_TIMER_ID);
                    pThis->Close();
                }
                return 0;
            }
            case WM_PAINT: {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                pThis->UpdateDisplay();
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_ERASEBKGND:
                return 1;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// 使用示例 - 自动进度版本
// ============================================================================

/*
// 示例1：完全自动模式
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {

    std::vector<char> pngData;
    // ... 加载PNG数据 ...

    // 创建启动遮罩
    SplashScreen splash(pngData, L"我的应用程序", L"版本 3.0.0");

    // 显示遮罩并启动自动进度
    splash.Show();
    splash.StartAutoProgress(0.5, 50); // 每50ms增加0.5%，约10秒完成
    splash.SetAutoCloseDelay(5000);    // 5秒后自动关闭

    // 简单的消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

// 示例2：混合控制模式
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {

    std::vector<char> pngData;
    SplashScreen splash(pngData, L"我的应用程序", L"版本 3.0.0");

    splash.Show();
    splash.StartAutoProgress(0.2, 100); // 启动缓慢基础进度

    // 模拟实际加载任务
    Sleep(2000);

    // 手动控制关键节点
    splash.StopAutoProgress();
    splash.SetProgress(50);
    splash.SetStatusText(L"正在初始化数据库...");

    Sleep(1000);

    // 继续自动进度
    splash.StartAutoProgress(2.0, 100); // 更快完成

    // 等待完成
    while (splash.GetProgress() < 100) {
        MSG msg;
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(10);
    }

    splash.Close();
    return 0;
}
*/
