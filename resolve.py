import ctypes
from ctypes import wintypes
import sys
import os

dbghelp = ctypes.windll.dbghelp
kernel32 = ctypes.windll.kernel32

class SYMBOL_INFO(ctypes.Structure):
    _fields_ = [
        ("SizeOfStruct", wintypes.ULONG),
        ("TypeIndex", wintypes.ULONG),
        ("Reserved", wintypes.ULONG * 2),
        ("Index", wintypes.ULONG),
        ("Size", wintypes.ULONG),
        ("ModBase", wintypes.ULONG64),
        ("Flags", wintypes.ULONG),
        ("Value", wintypes.ULONG64),
        ("Address", wintypes.ULONG64),
        ("Register", wintypes.ULONG),
        ("Scope", wintypes.ULONG),
        ("Tag", wintypes.ULONG),
        ("NameLen", wintypes.ULONG),
        ("MaxNameLen", wintypes.ULONG),
        ("Name", ctypes.c_char * 256),
    ]

process = kernel32.GetCurrentProcess()
dbghelp.SymInitialize(process, None, False)

base_addr = 0x400000
path = b"E:\\STORM EDEN\\build\\bin\\Release\\eden.exe"
mod = dbghelp.SymLoadModuleEx(process, 0, path, None, base_addr, 0, None, 0)
if mod == 0:
    print("Failed to load module")
    sys.exit(1)

offset = 0x1e83b1
addr = base_addr + offset

sym = SYMBOL_INFO()
sym.SizeOfStruct = 88 # sizeof(SYMBOL_INFO) - 256 + 1
sym.MaxNameLen = 255
displacement = wintypes.ULONG64()

res = dbghelp.SymFromAddr(process, addr, ctypes.byref(displacement), ctypes.byref(sym))
if res:
    print(f"Symbol: {sym.Name.decode('utf-8')} + 0x{displacement.value:x}")
else:
    print("SymFromAddr failed")

class IMAGEHLP_LINE64(ctypes.Structure):
    _fields_ = [
        ("SizeOfStruct", wintypes.DWORD),
        ("Key", ctypes.c_void_p),
        ("LineNumber", wintypes.DWORD),
        ("FileName", ctypes.c_char_p),
        ("Address", wintypes.ULONG64),
    ]

line = IMAGEHLP_LINE64()
line.SizeOfStruct = ctypes.sizeof(IMAGEHLP_LINE64)
disp_line = wintypes.DWORD()

res_line = dbghelp.SymGetLineFromAddr64(process, addr, ctypes.byref(disp_line), ctypes.byref(line))
if res_line:
    print(f"Line: {line.FileName.decode('utf-8')}:{line.LineNumber}")
else:
    print("SymGetLineFromAddr64 failed")

dbghelp.SymCleanup(process)
