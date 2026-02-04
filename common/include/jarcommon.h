#pragma once

#include <string>
#include <unordered_map>

namespace JarCommon {
    enum class LaunchMode { JavaExe = 0, DirectJVM = 1 };

    inline constexpr unsigned int JAR_MAGIC = 0x4A415246; // "JARF"

    inline constexpr auto JVM_DLL_NAME = L"jvm.dll";

    inline constexpr auto JAVA_EXE_NAME = L"java.exe";

    namespace SplashLayout {
        // 百分比常量
        inline constexpr float BaseMarginPercent = 0.055f;
        inline constexpr float TitleHeightPercent = 0.185f;
        inline constexpr float VersionHeightPercent = 0.11f;
        inline constexpr float ProgressHeightPercent = 0.015f;
        inline constexpr float StatusYOffsetPercent = 0.13f;
        inline constexpr float StatusHeightPercent = 0.09f;

        inline constexpr float TitleMaxFontSizePercent = 0.15f;
        inline constexpr float VersionMaxFontSizePercent = 0.09f;
        inline constexpr float StatusMaxFontSizePercent = 0.055f;
        inline constexpr float ShadowRectOffsetPercent = 0.06f;
    } // namespace SplashLayout

    extern const std::unordered_map<std::string, unsigned int> JAVA_VERSION_MAP;

    /**
     * 完整结构
     * exe
     * jar
     * image (png格式)
     * mainClass + jvmArgs + programArgs + javaPath + jarExtractPath + splashProgramName + splashProgramVersion
     * JarFooter
     */
#pragma pack(push, 1)
    struct JarFooter {
        unsigned int magic;
        unsigned long long jarOffset;
        unsigned long long jarSize;
        unsigned long long splashImageSize;
        bool splashShowProgress;
        bool splashShowProgressText;
        int launchTime; // 单位：ms
        unsigned long long timestamp;
        unsigned int javaVersion;
        unsigned int mainClassLength;
        unsigned int jvmArgsLength;
        unsigned int programArgsLength;
        unsigned int javaPathLength;
        unsigned int jarExtractPathLength;
        unsigned int splashProgramNameLength;
        unsigned int splashProgramVersionLength;
        LaunchMode launchMode;
        // 文本位置百分比 (0-100)
        float titlePosX;
        float titlePosY;
        float versionPosX;
        float versionPosY;
        // 进度文本位置百分比 (0-100)
        float statusPosX;
        float statusPosY;
        // 字体大小百分比 (0-1.0, 相对于窗口高度)
        float titleFontSizePercent;
        float versionFontSizePercent;
        float statusFontSizePercent;
    };
#pragma pack(pop)
} // namespace JarCommon
