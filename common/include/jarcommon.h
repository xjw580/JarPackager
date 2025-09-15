#pragma once

#include <string>
#include <unordered_map>

namespace JarCommon {

    enum class LaunchMode { JavaExe = 0, DirectJVM = 1 };

    inline constexpr unsigned int JAR_MAGIC = 0x4A415246; // "JARF"

    inline constexpr auto JVM_DLL_NAME = L"jvm.dll";

    inline constexpr auto JAVA_EXE_NAME = L"java.exe";

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
        int launchTime;//单位：ms
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
    };
#pragma pack(pop)

} // namespace JarCommon
