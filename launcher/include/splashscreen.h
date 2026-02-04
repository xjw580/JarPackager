#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include <gdiplus.h>
#include "jarcommon.h"

class SplashScreen {
private:
    HWND m_hwnd;

    int m_width;
    int m_height;
    std::wstring m_programName;
    std::wstring m_programVersion;
    bool m_showProgress;
    bool m_showProgressText;
    std::wstring m_statusText;

    // 进度条相关
    double m_progress; // 0-100
    int m_progressHeight; // 进度条高度

    // 定时器相关
    static const UINT_PTR PROGRESS_TIMER_ID = 1;
    static const UINT_PTR AUTO_CLOSE_TIMER_ID = 2;

    bool m_autoProgress;
    double m_progressStep;
    DWORD m_progressInterval;
    DWORD m_autoCloseDelay;

    // 文本绘制位置
    Gdiplus::RectF m_titleRect;
    Gdiplus::RectF m_versionRect;
    Gdiplus::RectF m_statusRect;
    Gdiplus::RectF m_progressRect;

    // 文本位置百分比 (0-100)
    float m_titlePosX;
    float m_titlePosY;
    float m_versionPosX;
    float m_versionPosY;
    float m_statusPosX;
    float m_statusPosY;

    // 字体大小百分比
    float m_titleFontSizePercent;
    float m_versionFontSizePercent;
    float m_statusFontSizePercent;

    // GDI+相关
    Gdiplus::Bitmap *m_gdiplusBitmap;
    Gdiplus::Bitmap *m_cachedBitmap;

    // 窗口过程
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static float CalculateOptimalFontSize(const Gdiplus::Graphics *graphics, const Gdiplus::FontFamily *fontFamily,
                                          const std::wstring &text, const Gdiplus::RectF &targetRect, float maxFontSize,
                                          Gdiplus::FontStyle fontStyle);

    // DPI相关
    float GetDPIScale();

    // 从PNG数据创建位图
    void CreateBitmapFromPNG(const std::vector<char> &pngData);

    // 创建默认背景
    void CreateDefaultBackground();

    // 创建缓存的背景位图
    void CreateCachedBitmap();

    // 绘制所有内容到缓存位图
    void DrawToCachedBitmap();

    // 更新分层窗口
    void UpdateDisplay();

    // 计算布局
    void CalculateLayout();


public:
    SplashScreen(const std::vector<char> &pngData, const std::wstring &programName, const std::wstring &programVersion,
                 bool showProgress = true, bool showProgressText = true, float titlePosX = 50.0f,
                 float titlePosY = 33.0f, float versionPosX = 50.0f, float versionPosY = 45.0f, float statusPosX = 5.0f,
                 float statusPosY = 85.0f, float titleFontSizePercent = 15.0f, float versionFontSizePercent = 9.0f,
                 float statusFontSizePercent = 5.5f);
    ~SplashScreen();

    // 显示启动遮罩
    bool Show();

    // 隐藏启动遮罩
    void Hide();

    // 关闭启动遮罩
    void Close();

    // 启动自动进度
    void StartAutoProgress(double stepSize = 0.5, DWORD intervalMs = 50);

    // 停止自动进度
    void StopAutoProgress();

    // 设置自动关闭延迟
    void SetAutoCloseDelay(DWORD delayMs);

    // 更新进度（0-100）
    void SetProgress(double progress);

    // 更新状态文本
    void SetStatusText(const std::wstring &statusText);

    // 同时更新进度和状态文本
    void UpdateProgress(int progress, const std::wstring *statusText = nullptr);

    // 获取窗口句柄
    [[nodiscard]] HWND GetHandle() const { return m_hwnd; }

    // 获取当前进度
    [[nodiscard]] double GetProgress() const { return m_progress; }
};
