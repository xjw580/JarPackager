#pragma once

#include <expected>
#include <string>
#include <vector>

namespace std::filesystem {
    class path;
}
inline constexpr unsigned int EXE_MAGIC = 0x65786546; // "EXEF"

#pragma pack(push, 1)
struct ExeFooter {
    unsigned int magic;
    unsigned long long exeOffset;
    unsigned long long exeSize;
};
#pragma pack(pop)

class Attach {
public:
    using Path = std::filesystem::path;
    using ByteArray = std::vector<std::byte>;

    static std::expected<Path, std::wstring> attachExe(const Path &srcEexPath, const Path &attachExePath, const Path &outputPath);

    static std::expected<Path, std::wstring> attachExe(const Path &attachExePath, const Path &outputPath);

    // 将EXE附加到当前程序
    static std::expected<Path, std::wstring> attachExeToCurrent(const Path &attachExePath);

    // 从附加的EXE文件中读取附加内容
    static std::expected<ByteArray, std::wstring> readAttachedExe(const Path &attachedExePath, bool onlyVerify = false);

private:
    // 获取当前程序路径
    static std::expected<Path, std::wstring> getCurrentExePath();

    // 生成新文件名
    static Path generateNewFileName(const Path &originalPath);

    // 读取文件内容
    static std::expected<ByteArray, std::wstring> readFileContent(const Path &filePath);

    // 移除已存在的附加内容
    static void removeExistingAttachment(ByteArray &srcData);

    // 写入附加文件
    static std::expected<void, std::wstring> writeAttachedFile(const Path &newExeFilePath, const ByteArray &srcData,
                                                               const ByteArray &attachData);
};
