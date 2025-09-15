#pragma once

import std;

namespace Strings {
    inline void trimQuotesInplace(std::wstring& s) {
        if (s.size() >= 2) {
            wchar_t first = s.front();
            wchar_t last  = s.back();
            if ((first == L'"' && last == L'"') || (first == L'\'' && last == L'\'')) {
                s.erase(s.size() - 1, 1); // 去掉末尾
                s.erase(0, 1);            // 去掉开头
            }
        }
    }

    inline std::wstring trimQuotes(const std::wstring& input) {
        if (input.size() >= 2) {
            wchar_t first = input.front();
            wchar_t last  = input.back();
            if ((first == L'"' && last == L'"') || (first == L'\'' && last == L'\'')) {
                return input.substr(1, input.size() - 2);
            }
        }
        return input;
    }
}
