﻿#include "stylecommon.h"
#define NOMINMAX
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <jni.h>
#include <windows.h>
#include "jarcommon.h"
#include "splashscreen.h"

import std;

// UTF-8和宽字符转换辅助函数
std::string wstringToUtf8(const std::wstring &wstr) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(wstr);
}

std::wstring utf8ToWstring(const std::string &utf8) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(utf8);
}

// 辅助函数：分割宽字符串
std::vector<std::wstring> splitWString(const std::wstring &str, wchar_t delimiter) {
    std::vector<std::wstring> result;
    std::wstringstream ss(str);
    std::wstring item;

    while (std::getline(ss, item, delimiter)) {
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

// 获取当前可执行文件路径
std::expected<std::wstring, std::wstring> getCurrentExecutablePath() {
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) {
        return std::unexpected{L"无法获取当前可执行文件路径"};
    }
    return std::wstring(path);
}

// 查找Java路径
std::expected<std::wstring, std::wstring> findJavaPath() {
    // 首先检查环境变量JAVA_HOME
    wchar_t javaHome[MAX_PATH];
    if (GetEnvironmentVariableW(L"JAVA_HOME", javaHome, MAX_PATH) > 0) {
        if (const auto javaPath = std::filesystem::path(javaHome) / L"bin" / JarCommon::JAVA_EXE_NAME;
            std::filesystem::exists(javaPath)) {
            return javaPath;
        }
    }

    // 搜索PATH环境变量
    if (SearchPathW(nullptr, JarCommon::JAVA_EXE_NAME, nullptr, MAX_PATH, javaHome, nullptr)) {
        return std::wstring(javaHome);
    }

    // 检查常见安装路径
    std::vector<std::wstring> searchPaths = {
            L"C:\\Program Files\\Java", L"C:\\Program Files (x86)\\Java", L"C:\\Program Files\\Eclipse Adoptium",
            L"C:\\Program Files\\Amazon Corretto", L"C:\\Program Files\\Microsoft\\jdk"};

    for (const auto &basePath: searchPaths) {
        if (std::filesystem::exists(basePath)) {
            for (const auto &entry: std::filesystem::directory_iterator(basePath)) {
                if (entry.is_directory()) {
                    if (auto javaExe = entry.path() / "bin" / JarCommon::JAVA_EXE_NAME;
                        std::filesystem::exists(javaExe)) {
                        return javaExe.wstring();
                    }
                }
            }
        }
    }

    return std::unexpected{L"未找到Java运行时环境"};
}

// 查找JVM.dll路径
std::expected<std::wstring, std::wstring> findJvmPath() {
    // 检查常见JVM路径
    std::vector<std::wstring> searchPaths = {L"C:\\Program Files\\Java", L"C:\\Program Files (x86)\\Java",
                                             L"C:\\Program Files\\Eclipse Adoptium",
                                             L"C:\\Program Files\\Amazon Corretto"};

    for (const auto &basePath: searchPaths) {
        if (std::filesystem::exists(basePath)) {
            for (const auto &entry: std::filesystem::directory_iterator(basePath)) {
                if (entry.is_directory()) {
                    // 优先查找server版本
                    auto serverJvm = entry.path() / "bin" / "server" / JarCommon::JVM_DLL_NAME;
                    if (std::filesystem::exists(serverJvm)) {
                        return serverJvm.wstring();
                    }
                    // 备选client版本
                    auto clientJvm = entry.path() / "bin" / "client" / JarCommon::JVM_DLL_NAME;
                    if (std::filesystem::exists(clientJvm)) {
                        return clientJvm.wstring();
                    }
                }
            }
        }
    }

    return std::unexpected{L"未找到JVM动态库"};
}

std::expected<bool, std::wstring> extractJarInfo(const std::wstring &filePath, uint64_t &jarOffset, uint64_t &jarSize,
                                                 uint64_t &splashImageSize, uint32_t &javaVersion,
                                                 std::wstring &mainClass, std::vector<std::wstring> &jvmArgs,
                                                 std::vector<std::wstring> &programArgs, std::wstring &md5Hash,
                                                 std::wstring &javaPath, std::wstring &jarExtractPath,
                                                 std::wstring &splashProgramName, std::wstring &splashProgramVersion,
                                                 JarCommon::LaunchMode &launchMode) {

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected{L"无法打开文件: " + filePath};
    }

    file.seekg(0, std::ios::end);
    const int64_t fileSize = file.tellg();

    if (fileSize < static_cast<int64_t>(sizeof(JarCommon::JarFooter))) {
        return std::unexpected{L"文件太小，不包含有效的JAR信息"};
    }

    file.seekg(fileSize - sizeof(JarCommon::JarFooter));
    JarCommon::JarFooter footer{};
    file.read(reinterpret_cast<char *>(&footer), sizeof(footer));

    if (!file.good() || footer.magic != JarCommon::JAR_MAGIC) {
        return std::unexpected{L"无效的JAR文件格式"};
    }

    const int64_t stringsOffset = fileSize - sizeof(JarCommon::JarFooter) - footer.mainClassLength -
                                  footer.jvmArgsLength - footer.programArgsLength - footer.md5Length -
                                  footer.javaPathLength - footer.jarExtractPathLength - footer.splashProgramNameLength -
                                  footer.splashProgramVersionLength;
    file.seekg(stringsOffset);

    auto readUtf8String = [&](const uint32_t length) -> std::wstring {
        if (length == 0)
            return L"";
        std::vector<char> buffer(length);
        file.read(buffer.data(), length);
        std::string utf8Str(buffer.begin(), buffer.end());
        return utf8ToWstring(utf8Str);
    };

    mainClass = readUtf8String(footer.mainClassLength);

    if (footer.jvmArgsLength > 0) {
        const std::wstring jvmArgsStr = readUtf8String(footer.jvmArgsLength);
        jvmArgs = splitWString(jvmArgsStr, L'\n');
    } else {
        jvmArgs.clear();
    }

    if (footer.programArgsLength > 0) {
        const std::wstring programArgsStr = readUtf8String(footer.programArgsLength);
        programArgs = splitWString(programArgsStr, L'\n');
    } else {
        programArgs.clear();
    }

    md5Hash = readUtf8String(footer.md5Length);
    javaPath = readUtf8String(footer.javaPathLength);
    jarExtractPath = readUtf8String(footer.jarExtractPathLength);
    splashProgramName = readUtf8String(footer.splashProgramNameLength);
    splashProgramVersion = readUtf8String(footer.splashProgramVersionLength);

    jarOffset = footer.jarOffset;
    jarSize = footer.jarSize;
    splashImageSize = footer.splashImageSize;
    javaVersion = footer.javaVersion;
    launchMode = footer.launchMode;

    return true;
}

std::expected<bool, std::wstring> extractJarFile(const std::wstring &executablePath, uint64_t jarOffset,
                                                 uint64_t jarSize, const std::wstring &outputPath) {
    std::ifstream inFile(executablePath, std::ios::binary);
    if (!inFile.is_open()) {
        return std::unexpected{L"无法打开可执行文件: " + executablePath};
    }

    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile.is_open()) {
        return std::unexpected{L"无法创建输出文件: " + outputPath};
    }

    inFile.seekg(jarOffset);
    constexpr size_t bufferSize = 64 * 1024;
    std::vector<char> buffer(bufferSize);
    uint64_t remainingBytes = jarSize;

    while (remainingBytes > 0) {
        size_t bytesToRead = std::min(static_cast<uint64_t>(bufferSize), remainingBytes);
        inFile.read(buffer.data(), bytesToRead);

        if (inFile.gcount() != static_cast<std::streamsize>(bytesToRead)) {
            return std::unexpected{L"读取JAR数据时发生错误"};
        }

        outFile.write(buffer.data(), bytesToRead);
        if (outFile.fail()) {
            return std::unexpected{L"写入JAR文件时发生错误"};
        }

        remainingBytes -= bytesToRead;
    }

    return true;
}

std::wstring calculateMD5(const std::vector<std::uint8_t> &data) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string result;

    if (CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
            if (CryptHashData(hHash, data.data(), static_cast<DWORD>(data.size()), 0)) {
                DWORD hash_size = 16;
                BYTE hash[16];

                if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hash_size, 0)) {
                    std::stringstream ss;
                    for (DWORD i = 0; i < hash_size; i++) {
                        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
                    }
                    result = ss.str();
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }

    return utf8ToWstring(result);
}

std::expected<std::wstring, std::wstring> calculateFileMD5(const std::filesystem::path &file_path) {
    try {
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            return std::unexpected{L"无法打开文件计算MD5"};
        }

        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<std::uint8_t> buffer(file_size);
        file.read(reinterpret_cast<char *>(buffer.data()), file_size);
        file.close();

        return calculateMD5(buffer);

    } catch (const std::exception &e) {
        return std::unexpected{L"计算MD5时发生异常: " + utf8ToWstring(e.what())};
    }
}

std::expected<bool, std::wstring> verifyJarFile(const std::wstring &jarPath, const std::wstring &expectedMD5) {
    if (expectedMD5.empty()) {
        return true; // 没有MD5信息，跳过验证
    }

    auto actualMD5Result = calculateFileMD5(jarPath);
    if (!actualMD5Result) {
        return std::unexpected{actualMD5Result.error()};
    }

    return actualMD5Result.value() == expectedMD5;
}

// 使用java.exe启动JAR
std::expected<bool, std::wstring> launchWithJavaExe(const std::wstring &javaPath, const std::wstring &jarPath,
                                                    const std::vector<std::wstring> &jvmArgs,
                                                    const std::vector<std::wstring> &programArgs) {
    std::wstring command = L"\"" + javaPath + L"\"";

    // 添加JVM参数
    for (const auto &arg: jvmArgs) {
        command += L" " + arg;
    }

    // 添加-jar参数
    command += L" -jar \"" + jarPath + L"\"";

    // 添加程序参数
    for (const auto &arg: programArgs) {
        command += L" " + arg;
    }

    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    if (!CreateProcessW(nullptr, const_cast<wchar_t *>(command.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                        &si, &pi)) {
        return std::unexpected{L"启动Java进程失败"};
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

// 使用JVM.dll启动JAR
std::expected<bool, std::wstring> launchWithJvmDll(const std::wstring &jvmPath, const std::wstring &jarPath,
                                                   const unsigned int javaVersion, const std::wstring &mainClass,
                                                   const std::vector<std::wstring> &jvmArgs,
                                                   const std::vector<std::wstring> &programArgs) {
    const HMODULE jvmHandle = LoadLibraryW(jvmPath.c_str());
    if (!jvmHandle) {
        return std::unexpected{L"无法加载JVM动态库"};
    }

    typedef jint(JNICALL * CreateJavaVM_t)(JavaVM * *pvm, void **penv, void *args);
    CreateJavaVM_t CreateJavaVM_func = (CreateJavaVM_t) GetProcAddress(jvmHandle, "JNI_CreateJavaVM");

    if (!CreateJavaVM_func) {
        FreeLibrary(jvmHandle);
        return std::unexpected{L"无法获取JNI_CreateJavaVM函数"};
    }

    // 构建JVM选项
    std::vector<std::string> options;
    options.push_back("-Djava.class.path=" + wstringToUtf8(jarPath));

    for (const auto &arg: jvmArgs) {
        options.push_back(wstringToUtf8(arg));
    }

    std::vector<JavaVMOption> vmOptions(options.size());
    for (size_t i = 0; i < options.size(); ++i) {
        vmOptions[i].optionString = const_cast<char *>(options[i].c_str());
        vmOptions[i].extraInfo = nullptr;
    }

    JavaVMInitArgs vmArgs;
    vmArgs.version = javaVersion;
    vmArgs.nOptions = static_cast<jint>(vmOptions.size());
    vmArgs.options = vmOptions.data();
    vmArgs.ignoreUnrecognized = JNI_FALSE;

    JavaVM *jvm = nullptr;
    JNIEnv *env = nullptr;
    jint result = CreateJavaVM_func(&jvm, (void **) &env, &vmArgs);

    if (result != JNI_OK) {
        FreeLibrary(jvmHandle);
        return std::unexpected{L"创建JVM失败"};
    }

    // 查找并调用main方法
    if (mainClass.empty()) {
        jvm->DestroyJavaVM();
        FreeLibrary(jvmHandle);
        return std::unexpected{L"未指定主类，无法启动"};
    }

    std::string classPath = wstringToUtf8(mainClass);
    std::replace(classPath.begin(), classPath.end(), '.', '/');

    jclass mainClassObj = env->FindClass(classPath.c_str());
    if (!mainClassObj) {
        jvm->DestroyJavaVM();
        FreeLibrary(jvmHandle);
        return std::unexpected{L"找不到主类: " + mainClass};
    }

    jmethodID mainMethod = env->GetStaticMethodID(mainClassObj, "main", "([Ljava/lang/String;)V");
    if (!mainMethod) {
        jvm->DestroyJavaVM();
        FreeLibrary(jvmHandle);
        return std::unexpected{L"找不到main方法"};
    }

    // 创建参数数组
    jobjectArray javaArgs =
            env->NewObjectArray(static_cast<jsize>(programArgs.size()), env->FindClass("java/lang/String"), nullptr);
    for (size_t i = 0; i < programArgs.size(); ++i) {
        jstring argStr = env->NewStringUTF(wstringToUtf8(programArgs[i]).c_str());
        env->SetObjectArrayElement(javaArgs, static_cast<jsize>(i), argStr);
        env->DeleteLocalRef(argStr);
    }

    // 调用main方法
    env->CallStaticVoidMethod(mainClassObj, mainMethod, javaArgs);

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        jvm->DestroyJavaVM();
        FreeLibrary(jvmHandle);
        return std::unexpected{L"Java程序执行时发生异常"};
    }

    jvm->DestroyJavaVM();
    FreeLibrary(jvmHandle);
    return true;
}

std::wstring parseJavaVersion(const uint32_t javaVersion) {
    std::wstring javaVer;
    if (javaVersion == JNI_VERSION_1_1) {
        javaVer = L"1.1";
    } else if (javaVersion == JNI_VERSION_1_2) {
        javaVer = L"1.2";
    } else if (javaVersion == JNI_VERSION_1_4) {
        javaVer = L"1.4";
    } else if (javaVersion == JNI_VERSION_1_6) {
        javaVer = L"1.6";
    } else if (javaVersion == JNI_VERSION_1_8) {
        javaVer = L"1.8";
    } else if (javaVersion == JNI_VERSION_9) {
        javaVer = L"9";
    } else if (javaVersion == JNI_VERSION_10) {
        javaVer = L"10";
    } else if (javaVersion == JNI_VERSION_19) {
        javaVer = L"19";
    } else if (javaVersion == JNI_VERSION_20) {
        javaVer = L"20";
    } else if (javaVersion == JNI_VERSION_21) {
        javaVer = L"21";
    }
    return javaVer;
}

void showJarInfo(const std::wstring &mainClass, const uint32_t javaVersion, const uint64_t splashImageSize,
                 const std::vector<std::wstring> &jvmArgs, const std::vector<std::wstring> &programArgs,
                 const std::wstring &md5Hash, const std::wstring &javaPath, const std::wstring &jarExtractPath,
                 const std::wstring &splashProgramName, const std::wstring &splashProgramVersion,
                 const JarCommon::LaunchMode launchMode, uint64_t jarOffset, uint64_t jarSize) {
    std::wstringstream info;
    info << L"=== JAR 信息 ===\n";
    info << L"JAR 偏移: " << jarOffset << L"\n";
    info << L"JAR 大小: " << jarSize << L" 字节\n";

    const auto javaVer = parseJavaVersion(javaVersion);
    info << L"Java 版本: " << (javaVer.empty() ? L"未指定" : javaVer) << L"\n";
    info << L"MD5 哈希: " << (md5Hash.empty() ? L"未提供" : md5Hash) << L"\n";
    info << L"启动模式: " << (launchMode == JarCommon::LaunchMode::DirectJVM ? L"direct_jvm" : JarCommon::JAVA_EXE_NAME)
         << L"\n";
    info << L"主类: " << (mainClass.empty() ? L"未指定" : mainClass) << L"\n";
    info << L"Java 路径: " << (javaPath.empty() ? L"未指定" : javaPath) << L"\n";
    info << L"Jar解压路径: " << (jarExtractPath.empty() ? L"未指定" : jarExtractPath) << L"\n";
    info << L"启动页图片 大小: " << splashImageSize << L" 字节\n";
    info << L"启动页名: " << (splashProgramName.empty() ? L"未指定" : splashProgramName) << L"\n";
    info << L"启动页版本: " << (splashProgramVersion.empty() ? L"未指定" : splashProgramVersion) << L"\n";

    if (!jvmArgs.empty()) {
        info << L"\nJVM 参数:\n";
        for (const auto &arg: jvmArgs) {
            info << L"  " << arg << L"\n";
        }
    }

    if (!programArgs.empty()) {
        info << L"\n程序参数:\n";
        for (const auto &arg: programArgs) {
            info << L"  " << arg << L"\n";
        }
    }

    MessageBoxW(nullptr, info.str().c_str(), L"JAR 信息", MB_OK | MB_ICONINFORMATION);
}

void showError(const std::wstring &message) { MessageBoxW(nullptr, message.c_str(), L"错误", MB_OK | MB_ICONERROR); }

std::vector<char> loadImageFromExe(const std::wstring &exePath, const uint64_t imgOffset, const uint64_t imageSize) {
    std::vector<char> imageData;
    if (imageSize == 0)
        return imageData;

    imageData.resize(imageSize);

    std::ifstream file(exePath, std::ios::binary);
    if (!file.is_open()) {
        std::wcerr << L"无法打开文件: " << exePath << std::endl;
        return {};
    }

    file.seekg(imgOffset, std::ios::beg);
    file.read(imageData.data(), imageSize);

    if (!file) {
        std::wcerr << L"读取图片数据失败！" << std::endl;
        imageData.clear();
    }

    return imageData;
}

void updateSplashProgress(SplashScreen *splash) {
    splash->StartAutoProgress(0.2, 5); // 每50ms增加0.5%，约10秒完成
    splash->SetAutoCloseDelay(10000);    // 5秒后自动关闭


    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    MessageBoxA(nullptr, "ext","", MB_OK);
}

int wmain(int argc, wchar_t *argv[]) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);

    try {
        // 检查命令行参数
        bool showInfo = false;
        if (argc > 1 && std::wstring(argv[1]) == L"info") {
            showInfo = true;
        }

        // 获取当前可执行文件路径
        auto executablePathResult = getCurrentExecutablePath();
        if (!executablePathResult) {
            showError(executablePathResult.error());
            return 1;
        }
        std::wstring executablePath = executablePathResult.value();

        // 提取JAR信息
        uint64_t jarOffset, jarSize, imageSize;
        uint32_t javaVersion;
        std::wstring mainClass, md5Hash, javaPath, jarExtractPath, splashProgramName, splashProgramVersion;
        std::vector<std::wstring> jvmArgs, programArgs;
        JarCommon::LaunchMode launchMode;

        auto result = extractJarInfo(executablePath, jarOffset, jarSize, imageSize, javaVersion, mainClass, jvmArgs,
                                     programArgs, md5Hash, javaPath, jarExtractPath, splashProgramName,
                                     splashProgramVersion, launchMode);

        if (!result) {
            showError(result.error());
            return 1;
        }

        // 如果是info命令，显示信息后退出
        if (showInfo) {
            showJarInfo(mainClass, javaVersion, imageSize, jvmArgs, programArgs, md5Hash, javaPath, jarExtractPath,
                        splashProgramName, splashProgramVersion, launchMode, jarOffset, jarSize);
            return 0;
        }


        std::thread t([&] {
            // 检查并提取JAR文件
            auto fileStem = std::filesystem::path(executablePath.c_str()).stem().wstring();
            std::wstring jarFileName = fileStem + L".jar";
            bool needExtract = true;

            if (std::filesystem::exists(jarFileName)) {
                if (auto verifyResult = verifyJarFile(jarFileName, md5Hash); verifyResult && verifyResult.value()) {
                    needExtract = false;
                }
            }

            if (needExtract) {
                if (auto extractResult = extractJarFile(executablePath, jarOffset, jarSize, jarFileName);
                    !extractResult) {
                    showError(extractResult.error());
                    return 1;
                }
            }

            // 根据启动模式启动JAR
            if (launchMode == JarCommon::LaunchMode::DirectJVM) { // direct_jvm
                std::filesystem::path jvmDllPath;

                // 优先在 javaPath 下找 server/client jvm.dll
                auto serverJvm = std::filesystem::path(javaPath) / "server" / JarCommon::JVM_DLL_NAME;
                auto clientJvm = std::filesystem::path(javaPath) / "client" / JarCommon::JVM_DLL_NAME;

                if (std::filesystem::exists(serverJvm)) {
                    jvmDllPath = serverJvm;
                } else if (std::filesystem::exists(clientJvm)) {
                    jvmDllPath = clientJvm;
                }

                // 如果还没找到，调用 findJvmPath
                if (jvmDllPath.empty()) {
                    if (auto res = findJvmPath()) {
                        jvmDllPath = res.value();
                    }
                }

                if (jvmDllPath.empty()) {
                    showError(L"未找到 jvm.dll，正在尝试使用 java.exe 模式...");

                    std::filesystem::path javaExePath = std::filesystem::path(javaPath) / JarCommon::JAVA_EXE_NAME;

                    if (!std::filesystem::exists(javaExePath)) {
                        if (auto res = findJavaPath()) {
                            javaExePath = res.value();
                        }
                    }

                    if (auto launchResult = launchWithJavaExe(javaExePath, jarFileName, jvmArgs, programArgs);
                        !launchResult) {
                        showError(launchResult.error());
                        return 1;
                    }
                } else {
                    if (auto launchResult =
                                launchWithJvmDll(jvmDllPath, jarFileName, javaVersion, mainClass, jvmArgs, programArgs);
                        !launchResult) {
                        showError(L"JVM 模式启动失败: " + launchResult.error());
                        return 1;
                    }
                }

            } else { // java.exe 模式
                std::filesystem::path javaExePath = std::filesystem::path(javaPath) / JarCommon::JAVA_EXE_NAME;

                if (!std::filesystem::exists(javaExePath)) {
                    if (auto res = findJavaPath()) {
                        javaExePath = res.value();
                    }
                }

                if (auto launchResult = launchWithJavaExe(javaExePath, jarFileName, jvmArgs, programArgs);
                    !launchResult) {
                    showError(launchResult.error());
                    return 1;
                }
            }
            return 0;
        });

        const auto imageData = loadImageFromExe(executablePath, jarOffset + jarSize, imageSize);
        const auto splash = new SplashScreen(imageData, splashProgramName, splashProgramVersion);
        splash->Show();
        updateSplashProgress(splash);
        splash->Close();
        t.join();
        return 0;

    } catch (const std::exception &e) {
        showError(L"程序异常: " + utf8ToWstring(e.what()));
        return 1;
    }
}
