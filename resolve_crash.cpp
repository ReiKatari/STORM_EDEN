#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <vector>
#include <string>

#pragma comment(lib, "dbghelp.lib")

int main() {
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, NULL, TRUE);

    const char* exe_path = "E:\\STORM EDEN\\build\\bin\\Release\\eden.exe";
    DWORD64 base = SymLoadModuleEx(process, NULL, exe_path, NULL, 0x140000000, 0, NULL, 0);
    std::cout << "Loaded base: 0x" << std::hex << base << std::endl;

    std::vector<DWORD64> offsets = {0x20b550b, 0x292efc, 0x1e859d, 0x1e8809};

    char buffer[sizeof(SYMBOL_INFO) + 256] = {};
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)buffer;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;

    for (DWORD64 off : offsets) {
        DWORD64 addr = 0x140000000 + off;
        DWORD64 disp = 0;
        std::string name = "???";
        if (SymFromAddr(process, addr, &disp, symbol)) {
            name = symbol->Name;
        }

        IMAGEHLP_LINE64 line_info = {};
        line_info.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD line_disp = 0;
        std::string file_name = "";
        DWORD line_number = 0;
        if (SymGetLineFromAddr64(process, addr, &line_disp, &line_info)) {
            file_name = line_info.FileName;
            line_number = line_info.LineNumber;
        }

        std::cout << "Offset 0x" << std::hex << off << " -> Symbol: " << name 
                  << " | File: " << file_name << ":" << std::dec << line_number << std::endl;
    }
    return 0;
}
