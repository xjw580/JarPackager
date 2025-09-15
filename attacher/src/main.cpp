#define NOMINMAX
#define UNICODE
#include <corecrt_io.h>
#include <fcntl.h>

#include "attach.h"
#include "modify.h"
#include "strings.h"

import std;

int wmain(const int argc, wchar_t *argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);
    if (argc < 3) {
        std::wcout << L"用法: Attacher.exe src.exe launch.exe out.exe(可选,不填则代表附加到src.exe源文件中)" << std::endl;
        return 1;
    }

    const std::wstring srcExe = Strings::trimQuotes(argv[1]);
    const std::wstring launchExe = Strings::trimQuotes(argv[2]);
    const std::wstring outputExe = argc > 3 ? Strings::trimQuotes(argv[3]) : srcExe;
    std::wcout << L"原文件: " << srcExe << std::endl;
    std::wcout << L"启动器: " << launchExe << std::endl;
    std::wcout << L"输出文件: " << outputExe << std::endl;
    std::wcout << L"将启动器附加到原文件中..."<< std::endl;

    if (const auto attachRes = Attach::attachExe(srcExe, launchExe, outputExe); !attachRes) {
        std::wcout << attachRes.error() << std::endl;
        return 1;
    }

    std::wcout << L"附加完成, 输出到 [" << outputExe << L"]" << std::endl;
    return 0;
}
