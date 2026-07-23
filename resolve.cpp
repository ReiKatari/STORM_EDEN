#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#pragma comment(lib, "dbghelp.lib")

int main() {
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, FALSE);
    DWORD64 base_addr = 0x400000;
    DWORD64 mod = SymLoadModuleEx(process, 0, "E:\\STORM EDEN\\build\\bin\\Release\\eden.exe", NULL, base_addr, 0, NULL, 0);
    if (!mod) { std::cout << "failed load\n"; return 1; }
    
    DWORD64 addr = base_addr + 0x1e83b1;
    char buffer[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)buffer;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;
    DWORD64 displacement = 0;
    
    if (SymFromAddr(process, addr, &displacement, symbol)) {
        std::cout << "Symbol: " << symbol->Name << " + 0x" << std::hex << displacement << "\n";
    }
    
    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    DWORD disp_line = 0;
    if (SymGetLineFromAddr64(process, addr, &disp_line, &line)) {
        std::cout << "Line: " << line.FileName << ":" << std::dec << line.LineNumber << "\n";
    }
    
    return 0;
}
