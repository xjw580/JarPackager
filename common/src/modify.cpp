/**************************************************************************

Author:肖嘉威

Version:1.0.0

Date:2025/9/14 0:03

Description:

**************************************************************************/
#define UNICODE
#include "modify.h"
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <imagehlp.h>
#include <vector>
#include <format>
#include <expected>
#include <windows.h>

// 文件映射RAII包装类
class FileMappingGuard {
private:
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = nullptr;
    LPVOID pBase = nullptr;

public:
    FileMappingGuard() = default;

    ~FileMappingGuard() {
        close();
    }

    // 禁止拷贝
    FileMappingGuard(const FileMappingGuard&) = delete;
    FileMappingGuard& operator=(const FileMappingGuard&) = delete;

    // 允许移动
    FileMappingGuard(FileMappingGuard&& other) noexcept {
        std::swap(hFile, other.hFile);
        std::swap(hMapping, other.hMapping);
        std::swap(pBase, other.pBase);
    }

    FileMappingGuard& operator=(FileMappingGuard&& other) noexcept {
        if (this != &other) {
            close();
            std::swap(hFile, other.hFile);
            std::swap(hMapping, other.hMapping);
            std::swap(pBase, other.pBase);
        }
        return *this;
    }

    bool open(const std::wstring& path, DWORD access = GENERIC_READ | GENERIC_WRITE) {
        close();

        hFile = CreateFileW(
            path.c_str(),
            access,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            return false;
        }

        DWORD protect = (access & GENERIC_WRITE) ? PAGE_READWRITE : PAGE_READONLY;
        DWORD mapAccess = (access & GENERIC_WRITE) ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;

        hMapping = CreateFileMappingW(
            hFile,
            nullptr,
            protect,
            0,
            0,
            nullptr
        );

        if (!hMapping) {
            close();
            return false;
        }

        pBase = MapViewOfFile(
            hMapping,
            mapAccess,
            0,
            0,
            0
        );

        if (!pBase) {
            close();
            return false;
        }

        return true;
    }

    void close() {
        if (pBase) {
            FlushViewOfFile(pBase, 0);  // 确保写入磁盘
            UnmapViewOfFile(pBase);
            pBase = nullptr;
        }
        if (hMapping) {
            CloseHandle(hMapping);
            hMapping = nullptr;
        }
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
        }
    }

    LPVOID data() const { return pBase; }
    bool isOpen() const { return pBase != nullptr; }

    DWORD getFileSize() const {
        if (hFile == INVALID_HANDLE_VALUE) return 0;
        return GetFileSize(hFile, nullptr);
    }
};

// 获取PE文件的实际大小（不包含附加数据）
static DWORD getPEActualSize(const std::wstring& path) {
    FileMappingGuard mapping;
    if (!mapping.open(path, GENERIC_READ)) {
        return 0;
    }

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)mapping.data();
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }

    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)mapping.data() + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }

    // 计算PE文件的实际大小
    DWORD peSize = 0;
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);

    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        DWORD sectionEnd = section[i].PointerToRawData + section[i].SizeOfRawData;
        if (sectionEnd > peSize) {
            peSize = sectionEnd;
        }
    }

    // 如果没有节或计算失败，返回整个文件大小
    if (peSize == 0) {
        peSize = mapping.getFileSize();
    }

    return peSize;
}

// 保存文件尾部的附加数据
static std::vector<BYTE> saveAppendedData(const std::wstring& path) {
    std::vector<BYTE> appendedData;

    // 获取PE实际大小
    DWORD peSize = getPEActualSize(path);
    if (peSize == 0) {
        return appendedData;
    }

    // 获取文件总大小
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return appendedData;
    }

    size_t totalSize = file.tellg();

    // 如果有附加数据
    if (totalSize > peSize) {
        size_t appendedSize = totalSize - peSize;
        appendedData.resize(appendedSize);

        file.seekg(peSize, std::ios::beg);
        file.read(reinterpret_cast<char*>(appendedData.data()), appendedSize);
    }

    file.close();
    return appendedData;
}

// 恢复文件尾部的附加数据
static bool restoreAppendedData(const std::wstring& path, const std::vector<BYTE>& appendedData) {
    if (appendedData.empty()) {
        return true;  // 没有需要恢复的数据
    }

    std::ofstream file(path, std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(appendedData.data()), appendedData.size());
    file.close();

    return true;
}

static std::string generateManifest(const ExecutionLevel level) {
    const char* levelStr = (level == ExecutionLevel::RequireAdmin)
        ? "requireAdministrator"
        : "asInvoker";

    std::ostringstream manifest;
    manifest << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
             << R"(<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">)"
             << R"(<trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">)"
             << R"(<security>)"
             << R"(<requestedPrivileges>)"
             << R"(<requestedExecutionLevel level=")" << levelStr << R"(" uiAccess="false"/>)"
             << R"(</requestedPrivileges>)"
             << R"(</security>)"
             << R"(</trustInfo>)"
             // 加上新版 Windows 控件依赖
             << R"(<dependency>)"
             << R"(<dependentAssembly>)"
             << R"(<assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0")"
             << R"( processorArchitecture="*" publicKeyToken="6595b64144ccf1df" language="*"/>)"
             << R"(</dependentAssembly>)"
             << R"(</dependency>)"
             << R"(</assembly>)";

    return manifest.str();
}


static bool updateChecksum(const std::wstring& path) {
    DWORD headerSum = 0;
    DWORD checkSum = 0;

    DWORD result = MapFileAndCheckSumW(
        path.c_str(),
        &headerSum,
        &checkSum
    );

    if (result != CHECKSUM_SUCCESS) {
        return false;
    }

    // 使用文件映射直接更新校验和
    FileMappingGuard mapping;
    if (!mapping.open(path)) {
        return false;
    }

    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)mapping.data();
    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((BYTE*)mapping.data() + pDosHeader->e_lfanew);

    // 更新校验和
    pNtHeaders->OptionalHeader.CheckSum = checkSum;

    return true;
}

PEModifier::PEModifier(const std::wstring &path) : filePath(path) {}

std::expected<bool, std::wstring> PEModifier::loadFile() {
    // 优化：不再加载整个文件到内存，只验证PE结构
    return validatePE();
}

std::expected<bool, std::wstring> PEModifier::validatePE() {
    FileMappingGuard mapping;
    if (!mapping.open(filePath, GENERIC_READ)) {
        return std::unexpected{L"无法打开文件: " + filePath};
    }

    DWORD fileSize = mapping.getFileSize();
    if (fileSize < sizeof(IMAGE_DOS_HEADER)) {
        return std::unexpected{L"文件太小，不是有效的PE文件"};
    }

    PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(mapping.data());
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return std::unexpected{L"不是有效的PE文件：DOS签名错误"};
    }

    peHeaderOffset = dosHeader->e_lfanew;
    if (peHeaderOffset >= fileSize || peHeaderOffset + sizeof(IMAGE_NT_HEADERS) > fileSize) {
        return std::unexpected{L"PE头偏移无效"};
    }

    PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>((BYTE*)mapping.data() + peHeaderOffset);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return std::unexpected{L"不是有效的PE文件：NT签名错误"};
    }

    optionalHeaderOffset = peHeaderOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);

    // 保存文件大小供后续使用
    fileData.resize(1); // 仅作为标记，表示文件已验证

    // 检测是否有附加数据
    DWORD peSize = getPEActualSize(filePath);
    if (peSize > 0 && fileSize > peSize) {
        std::wcout << L"注意：检测到文件尾部有 " << (fileSize - peSize) << L" 字节的附加数据" << std::endl;
    }

    return true;
}

std::expected<WORD, std::wstring> PEModifier::getCurrentSubsystem() {
    if (fileData.empty()) {
        return std::unexpected{L"PE文件未加载或无效"};
    }

    FileMappingGuard mapping;
    if (!mapping.open(filePath, GENERIC_READ)) {
        return std::unexpected{L"无法打开文件进行读取"};
    }

    PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>((BYTE*)mapping.data() + peHeaderOffset);

    if (ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        PIMAGE_OPTIONAL_HEADER32 optHeader = reinterpret_cast<PIMAGE_OPTIONAL_HEADER32>(&ntHeaders->OptionalHeader);
        return optHeader->Subsystem;
    } else if (ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        PIMAGE_OPTIONAL_HEADER64 optHeader64 = reinterpret_cast<PIMAGE_OPTIONAL_HEADER64>(&ntHeaders->OptionalHeader);
        return optHeader64->Subsystem;
    } else {
        return std::unexpected{L"不支持的PE格式"};
    }
}

std::expected<bool, std::wstring> PEModifier::setSubsystem(WORD subsystem) {
    if (fileData.empty()) {
        return std::unexpected{L"PE文件未加载或无效"};
    }

    // 创建备份（可选）
    std::wstring backupPath = filePath + L".backup";
    bool backupCreated = false;
    try {
        std::filesystem::copy_file(filePath, backupPath, std::filesystem::copy_options::overwrite_existing);
        backupCreated = true;
    } catch (const std::exception &e) {
        std::wcout << L"警告：无法创建备份文件: " << e.what() << std::endl;
    }

    // 直接在文件上进行修改
    FileMappingGuard mapping;
    if (!mapping.open(filePath, GENERIC_READ | GENERIC_WRITE)) {
        if (backupCreated) std::filesystem::remove(backupPath);
        return std::unexpected{L"无法打开文件进行写入"};
    }

    PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>((BYTE*)mapping.data() + peHeaderOffset);

    // 实时修改子系统值
    if (ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        PIMAGE_OPTIONAL_HEADER32 optHeader = reinterpret_cast<PIMAGE_OPTIONAL_HEADER32>(&ntHeaders->OptionalHeader);
        optHeader->Subsystem = subsystem;
    } else if (ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        PIMAGE_OPTIONAL_HEADER64 optHeader64 = reinterpret_cast<PIMAGE_OPTIONAL_HEADER64>(&ntHeaders->OptionalHeader);
        optHeader64->Subsystem = subsystem;
    } else {
        if (backupCreated) std::filesystem::remove(backupPath);
        return std::unexpected{L"不支持的PE格式"};
    }

    // 文件映射会在 mapping 析构时自动保存更改
    mapping.close(); // 显式关闭以确保写入

    // 更新校验和
    updateChecksum(filePath);

    // 删除备份文件（如果操作成功）
    if (backupCreated) {
        std::filesystem::remove(backupPath);
    }

    return true;
}

std::expected<bool, std::wstring> PEModifier::setExecutionLevel(const ExecutionLevel level) const {
    // 检查文件是否存在
    DWORD attrs = GetFileAttributesW(filePath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return std::unexpected(L"文件不存在或无法访问");
    }

    // 重要：先保存附加数据
    std::vector<BYTE> appendedData = saveAppendedData(filePath);
    if (!appendedData.empty()) {
        std::wcout << L"已保存 " << appendedData.size() << L" 字节的附加数据" << std::endl;
    }

    // 生成清单内容
    std::string manifestContent = generateManifest(level);

    // 开始更新资源
    HANDLE hUpdate = BeginUpdateResourceW(filePath.c_str(), FALSE);
    if (!hUpdate) {
        DWORD error = GetLastError();
        std::wstring errorMsg = L"无法开始资源更新，错误代码: " + std::to_wstring(error);
        return std::unexpected(errorMsg);
    }

    // 更新清单资源
    BOOL result = UpdateResourceW(
        hUpdate,
        MAKEINTRESOURCEW(24),  // RT_MANIFEST
        MAKEINTRESOURCEW(1),    // 资源ID
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        (LPVOID)manifestContent.c_str(),
        static_cast<DWORD>(manifestContent.size())
    );

    if (!result) {
        EndUpdateResourceW(hUpdate, TRUE);  // 取消更新
        return std::unexpected(L"更新清单资源失败");
    }

    // 提交资源更新
    if (!EndUpdateResourceW(hUpdate, FALSE)) {
        DWORD error = GetLastError();
        std::wstring errorMsg = L"提交资源更新失败，错误代码: " + std::to_wstring(error);
        return std::unexpected(errorMsg);
    }

    // 重要：恢复附加数据
    if (!appendedData.empty()) {
        if (restoreAppendedData(filePath, appendedData)) {
            std::wcout << L"已恢复 " << appendedData.size() << L" 字节的附加数据" << std::endl;
        } else {
            std::wcout << L"警告：无法恢复附加数据！" << std::endl;
            // 可以考虑将附加数据保存到单独的文件
            std::wstring dataPath = filePath + L".appended_data";
            std::ofstream dataFile(dataPath, std::ios::binary);
            if (dataFile.is_open()) {
                dataFile.write(reinterpret_cast<const char*>(appendedData.data()), appendedData.size());
                dataFile.close();
                std::wcout << L"附加数据已保存到: " << dataPath << std::endl;
            }
        }
    }

    // 更新PE文件校验和
    updateChecksum(filePath);

    return true;
}

std::expected<ExecutionLevel, std::wstring> PEModifier::getExecutionLevel() const {
    // 加载文件作为数据文件
    HMODULE hModule = LoadLibraryExW(
        filePath.c_str(),
        nullptr,
        LOAD_LIBRARY_AS_DATAFILE
    );

    if (!hModule) {
        return std::unexpected(L"无法加载PE文件");
    }

    // 查找清单资源
    HRSRC hResource = FindResourceW(
        hModule,
        MAKEINTRESOURCEW(1),
        MAKEINTRESOURCEW(24)  // RT_MANIFEST
    );

    if (!hResource) {
        FreeLibrary(hModule);
        return std::unexpected(L"未找到清单资源");
    }

    // 加载资源
    HGLOBAL hGlobal = LoadResource(hModule, hResource);
    if (!hGlobal) {
        FreeLibrary(hModule);
        return std::unexpected(L"无法加载清单资源");
    }

    // 获取资源数据
    LPVOID pData = LockResource(hGlobal);
    DWORD size = SizeofResource(hModule, hResource);

    if (!pData || size == 0) {
        FreeLibrary(hModule);
        return std::unexpected(L"无法访问清单数据");
    }

    // 转换为字符串并分析
    std::string manifest(static_cast<char*>(pData), size);

    ExecutionLevel level = ExecutionLevel::AsInvoker;
    if (manifest.find("requireAdministrator") != std::string::npos) {
        level = ExecutionLevel::RequireAdmin;
    }

    FreeLibrary(hModule);
    return level;
}

#pragma pack(push, 2)
struct ICONDIR {
    WORD idReserved;
    WORD idType;
    WORD idCount;
};
struct ICONDIRENTRY {
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwBytesInRes;
    DWORD dwImageOffset;
};
struct GRPICONDIRENTRY {
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwBytesInRes;
    WORD nID;
};
#pragma pack(pop)

std::expected<bool, std::wstring> PEModifier::setIcon(const wchar_t* icoFile) const {
    // 保存附加数据
    std::vector<BYTE> appendedData = saveAppendedData(filePath);

    // 尝试读取原 manifest
    std::vector<BYTE> originalManifest;
    {
        HMODULE hModule = LoadLibraryExW(filePath.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE);
        if (hModule) {
            HRSRC hRes = FindResourceW(hModule, MAKEINTRESOURCE(1), MAKEINTRESOURCE(24));
            if (hRes) {
                HGLOBAL hGlobal = LoadResource(hModule, hRes);
                if (hGlobal) {
                    LPVOID pData = LockResource(hGlobal);
                    DWORD size = SizeofResource(hModule, hRes);
                    if (pData && size > 0) {
                        originalManifest.assign((BYTE*)pData, (BYTE*)pData + size);
                    }
                }
            }
            FreeLibrary(hModule);
        }
    }

    // 读取 ICO 文件
    std::ifstream ico(icoFile, std::ios::binary);
    if (!ico) return std::unexpected{std::format(L"无法打开 ICO 文件: {}", icoFile)};

    ICONDIR iconDir{};
    ico.read(reinterpret_cast<char*>(&iconDir), sizeof(iconDir));
    if (iconDir.idType != 1 || iconDir.idCount == 0) return std::unexpected{L"ICO 文件格式错误"};

    std::vector<ICONDIRENTRY> entries(iconDir.idCount);
    ico.read(reinterpret_cast<char*>(entries.data()), iconDir.idCount * sizeof(ICONDIRENTRY));

    // 开始更新资源
    HANDLE hRes = BeginUpdateResourceW(filePath.c_str(), FALSE);
    if (!hRes) return std::unexpected{std::format(L"无法打开 EXE: {}", filePath)};

    // 写入每个 RT_ICON
    for (int i = 0; i < iconDir.idCount; i++) {
        std::vector<char> image(entries[i].dwBytesInRes);
        ico.seekg(entries[i].dwImageOffset, std::ios::beg);
        ico.read(image.data(), entries[i].dwBytesInRes);

        if (!UpdateResourceW(hRes, RT_ICON, MAKEINTRESOURCE(i + 1),
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                             image.data(), entries[i].dwBytesInRes)) {
            EndUpdateResourceW(hRes, TRUE);
            return std::unexpected{L"写入 RT_ICON 失败"};
        }
    }

    // 构造 RT_GROUP_ICON
    struct {
        ICONDIR dir{};
        std::vector<GRPICONDIRENTRY> entries;
    } grp;
    grp.dir.idReserved = 0;
    grp.dir.idType = 1;
    grp.dir.idCount = iconDir.idCount;
    grp.entries.resize(iconDir.idCount);

    for (int i = 0; i < iconDir.idCount; i++) {
        grp.entries[i].bWidth = entries[i].bWidth;
        grp.entries[i].bHeight = entries[i].bHeight;
        grp.entries[i].bColorCount = entries[i].bColorCount;
        grp.entries[i].bReserved = entries[i].bReserved;
        grp.entries[i].wPlanes = entries[i].wPlanes;
        grp.entries[i].wBitCount = entries[i].wBitCount;
        grp.entries[i].dwBytesInRes = entries[i].dwBytesInRes;
        grp.entries[i].nID = i + 1;
    }

    size_t grpSize = sizeof(ICONDIR) + grp.entries.size() * sizeof(GRPICONDIRENTRY);
    std::vector<BYTE> grpData(grpSize);
    memcpy(grpData.data(), &grp.dir, sizeof(ICONDIR));
    memcpy(grpData.data() + sizeof(ICONDIR), grp.entries.data(), grp.entries.size() * sizeof(GRPICONDIRENTRY));

    if (!UpdateResourceW(hRes, RT_GROUP_ICON, MAKEINTRESOURCE(1),
                         MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                         grpData.data(), grpSize)) {
        EndUpdateResourceW(hRes, TRUE);
        return std::unexpected{L"写入 RT_GROUP_ICON 失败"};
    }

    // 恢复原 manifest
    if (!originalManifest.empty()) {
        if (!UpdateResourceW(hRes, RT_MANIFEST, MAKEINTRESOURCE(1),
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                             originalManifest.data(), static_cast<DWORD>(originalManifest.size()))) {
            EndUpdateResourceW(hRes, TRUE);
            return std::unexpected{L"恢复原 manifest 失败"};
        }
    }

    if (!EndUpdateResourceW(hRes, FALSE)) {
        return std::unexpected{L"提交资源更新失败"};
    }

    // 恢复尾部附加数据
    if (!restoreAppendedData(filePath, appendedData)) {
        std::wcout << L"警告：无法恢复附加数据！" << std::endl;
    }

    // 更新校验和
    updateChecksum(filePath);

    return true;
}


void PEModifier::showPEInfo() {
    auto subsystem = getCurrentSubsystem();
    if (!subsystem) {
        std::wcout << L"错误: " << subsystem.error() << std::endl;
        return;
    }

    // 获取文件大小
    HANDLE hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    DWORD fileSize = 0;
    if (hFile != INVALID_HANDLE_VALUE) {
        fileSize = GetFileSize(hFile, nullptr);
        CloseHandle(hFile);
    }

    std::wcout << L"=== PE 文件信息 ===" << std::endl;
    std::wcout << L"文件路径: " << filePath << std::endl;
    std::wcout << L"文件大小: " << fileSize << L" 字节" << std::endl;

    // 检测附加数据
    DWORD peSize = getPEActualSize(filePath);
    if (peSize > 0 && fileSize > peSize) {
        std::wcout << L"PE 大小: " << peSize << L" 字节" << std::endl;
        std::wcout << L"附加数据: " << (fileSize - peSize) << L" 字节" << std::endl;
    }

    std::wcout << L"当前子系统: ";

    switch (subsystem.value()) {
        case IMAGE_SUBSYSTEM_WINDOWS_GUI:
            std::wcout << L"Windows GUI (不显示控制台)";
            break;
        case IMAGE_SUBSYSTEM_WINDOWS_CUI:
            std::wcout << L"Windows Console (显示控制台)";
            break;
        default:
            std::wcout << L"其他 (" << subsystem.value() << L")";
            break;
    }
    std::wcout << std::endl;

    // 读取架构信息
    FileMappingGuard mapping;
    if (mapping.open(filePath, GENERIC_READ)) {
        PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>((BYTE*)mapping.data() + peHeaderOffset);
        std::wcout << L"架构: ";
        if (ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            std::wcout << L"32位";
        } else if (ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            std::wcout << L"64位";
        } else {
            std::wcout << L"未知";
        }
        std::wcout << std::endl;
    }

    // 尝试获取执行级别
    auto execLevel = getExecutionLevel();
    if (execLevel) {
        std::wcout << L"执行级别: ";
        if (execLevel.value() == ExecutionLevel::RequireAdmin) {
            std::wcout << L"需要管理员权限";
        } else {
            std::wcout << L"普通用户权限";
        }
        std::wcout << std::endl;
    }
}