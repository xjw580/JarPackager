/**************************************************************************

Author:肖嘉威

Version:1.0.0

Date:2025/9/15 11:47

Description:

**************************************************************************/
#include "attach.h"
#include <shlwapi.h>

import std;

std::expected<Attach::Path, std::wstring> Attach::attachExe(const Path &srcEexPath, const Path &attachExePath,
                                                            const Path &outputPath) {
    if (attachExePath.empty() || srcEexPath.empty()) {
        return std::unexpected(L"附加EXE路径为空或源EXE路径为空");
    }

    auto exeFilePath = srcEexPath;

    // 确定输出路径
    Path newExeFilePath;
    bool needCleanupBackup = false;

    if (outputPath.empty()) {
        // 使用默认命名方案
        newExeFilePath = generateNewFileName(exeFilePath);
    } else {
        newExeFilePath = outputPath;
        // 如果输出路径与源exe路径一致，需要先备份源文件
        std::error_code ec;
        if (std::filesystem::equivalent(outputPath, exeFilePath, ec) && !ec) {
            auto tempPath = exeFilePath;
            tempPath += L".backup";

            std::filesystem::copy_file(exeFilePath, tempPath,
                                       std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                auto msg = ec.message();
                return std::unexpected(std::format(L"无法创建源文件备份: {}",
                                                   std::wstring(msg.begin(), msg.end())));
            }

            // 更新源文件路径为备份路径
            exeFilePath = tempPath;
            needCleanupBackup = true;
        }
    }

    // 读取当前程序内容
    auto srcDataResult = readFileContent(exeFilePath);
    if (!srcDataResult) {
        return std::unexpected(std::format(L"无法读取当前程序文件: {}", srcDataResult.error()));
    }
    auto srcData = std::move(srcDataResult.value());

    // 检查原程序是否已经附加
    removeExistingAttachment(srcData);

    // 读取新的附加 EXE
    auto attachDataResult = readFileContent(attachExePath);
    if (!attachDataResult) {
        return std::unexpected(std::format(L"无法读取附加 EXE 文件: {}", attachDataResult.error()));
    }
    const auto &attachData = attachDataResult.value();

    // 写入新文件
    auto writeResult = writeAttachedFile(newExeFilePath, srcData, attachData);
    if (!writeResult) {
        return std::unexpected(writeResult.error());
    }

    // 如果使用了备份文件，清理备份
    if (needCleanupBackup) {
        std::error_code ec;
        std::filesystem::remove(exeFilePath, ec);
    }

    return newExeFilePath;
}

std::expected<Attach::Path, std::wstring> Attach::attachExeToCurrent(const Path &attachExePath) {
    return attachExe(attachExePath, "");
}

std::expected<Attach::ByteArray, std::wstring> Attach::readAttachedExe(const Path &attachedExePath, const bool onlyVerify) {
    std::error_code ec;
    const auto fileSize = std::filesystem::file_size(attachedExePath, ec);
    if (ec) {
        return std::unexpected(std::format(L"无法获取文件大小: {}", attachedExePath.wstring()));
    }

    // 检查文件是否足够大
    if (fileSize < sizeof(ExeFooter)) {
        return std::unexpected(L"文件太小，没有 ExeFooter");
    }

    std::ifstream file(attachedExePath, std::ios::binary);
    if (!file) {
        return std::unexpected(std::format(L"无法打开文件: {}", attachedExePath.wstring()));
    }

    // 跳转到文件尾部读取 ExeFooter
    file.seekg(-static_cast<std::streamoff>(sizeof(ExeFooter)), std::ios::end);
    if (!file) {
        return std::unexpected(L"跳转到文件尾部失败");
    }

    // 读取 ExeFooter
    ExeFooter footer;
    file.read(reinterpret_cast<char *>(&footer), sizeof(footer));
    if (!file || file.gcount() != sizeof(footer)) {
        return std::unexpected(L"读取 ExeFooter 失败");
    }

    // 校验 magic
    if (footer.magic != EXE_MAGIC) {
        return std::unexpected(L"ExeFooter magic 不匹配");
    }

    if (onlyVerify) {
        return ByteArray{};
    }
    // 跳转到附加 EXE 偏移位置
    file.seekg(static_cast<std::streamoff>(footer.exeOffset));
    if (!file) {
        return std::unexpected(L"跳转到附加 EXE 偏移失败");
    }

    // 读取附加 EXE 内容
    ByteArray exeData(footer.exeSize);
    file.read(reinterpret_cast<char *>(exeData.data()), static_cast<std::streamsize>(footer.exeSize));
    if (!file || static_cast<std::uint64_t>(file.gcount()) != footer.exeSize) {
        return std::unexpected(L"读取附加 EXE 内容失败");
    }

    return exeData;
}

std::expected<Attach::Path, std::wstring> Attach::attachExe(const Path &attachExePath,
                                                            const Path &outputPath) {
    // 获取当前程序路径
    auto exePathResult = getCurrentExePath();
    if (!exePathResult) {
        return std::unexpected(exePathResult.error());
    }
    return attachExe(exePathResult.value(), attachExePath, outputPath);
}

std::expected<Attach::Path, std::wstring> Attach::getCurrentExePath() {
    wchar_t path[MAX_PATH];
    if (const DWORD result = GetModuleFileNameW(nullptr, path, MAX_PATH); result == 0 || result == MAX_PATH) {
        return std::unexpected(L"无法获取当前程序路径");
    }
    return Path{path};
}

Attach::Path Attach::generateNewFileName(const Path &originalPath) {
    const auto stem = originalPath.stem().wstring();
    const auto extension = originalPath.extension().wstring();
    const auto parent = originalPath.parent_path();

    return parent / (stem + L"_attached" + extension);
}

std::expected<Attach::ByteArray, std::wstring> Attach::readFileContent(const Path &filePath) {
    std::error_code ec;
    auto fileSize = std::filesystem::file_size(filePath, ec);
    if (ec) {
        return std::unexpected(std::format(L"无法获取文件大小: {}", filePath.wstring()));
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return std::unexpected(std::format(L"无法打开文件: {}", filePath.wstring()));
    }

    ByteArray buffer(fileSize);
    file.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(fileSize));

    if (!file || static_cast<std::uintmax_t>(file.gcount()) != fileSize) {
        return std::unexpected(std::format(L"读取文件失败: {}", filePath.wstring()));
    }

    return buffer;
}

void Attach::removeExistingAttachment(ByteArray &srcData) {
    if (srcData.size() >= sizeof(ExeFooter)) {
        const auto *footerPtr =
                reinterpret_cast<const ExeFooter *>(srcData.data() + srcData.size() - sizeof(ExeFooter));

        if (footerPtr->magic == EXE_MAGIC) {
            srcData.resize(footerPtr->exeOffset);
        }
    }
}

std::expected<void, std::wstring> Attach::writeAttachedFile(const Path &newExeFilePath, const ByteArray &srcData,
                                                            const ByteArray &attachData) {
    std::ofstream file(newExeFilePath, std::ios::binary);
    if (!file) {
        return std::unexpected(std::format(L"无法创建输出文件: {}", newExeFilePath.wstring()));
    }

    // 写入原程序数据
    file.write(reinterpret_cast<const char *>(srcData.data()), static_cast<std::streamsize>(srcData.size()));
    if (!file) {
        return std::unexpected(L"写入当前程序失败");
    }

    // 记录 EXE 偏移
    auto exeOffset = static_cast<std::uint64_t>(file.tellp());

    // 写入附加数据
    file.write(reinterpret_cast<const char *>(attachData.data()), static_cast<std::streamsize>(attachData.size()));
    if (!file) {
        return std::unexpected(L"写入附加 EXE 失败");
    }

    // 写入 Footer
    ExeFooter footer{.magic = EXE_MAGIC, .exeOffset = exeOffset, .exeSize = attachData.size()};

    file.write(reinterpret_cast<const char *>(&footer), sizeof(footer));
    if (!file) {
        return std::unexpected(L"写入 ExeFooter 失败");
    }

    return {};
}
