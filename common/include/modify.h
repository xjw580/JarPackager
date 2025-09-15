#pragma once
#include <expected>
#include <string>
#include <vector>
#include <windows.h>

enum class ExecutionLevel {
    AsInvoker, // 普通权限
    RequireAdmin // 需要管理员权限
};


class PEModifier {
private:
    std::wstring filePath;
    std::vector<uint8_t> fileData;

    DWORD peHeaderOffset = 0;
    DWORD optionalHeaderOffset = 0;

public:
    explicit PEModifier(const std::wstring &path);

    std::expected<bool, std::wstring> loadFile();

    std::expected<bool, std::wstring> validatePE();

    std::expected<WORD, std::wstring> getCurrentSubsystem();

    std::expected<bool, std::wstring> setSubsystem(WORD subsystem);

    [[nodiscard]] std::expected<bool, std::wstring> setExecutionLevel(ExecutionLevel level) const;

    std::expected<ExecutionLevel, std::wstring> getExecutionLevel() const;

    std::expected<bool, std::wstring> setIcon(const wchar_t *icoFile) const;

    void showPEInfo();
};
