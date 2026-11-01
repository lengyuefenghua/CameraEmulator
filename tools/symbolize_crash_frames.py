import ctypes, sys
from ctypes import wintypes
exe=sys.argv[1]
base=int(sys.argv[2], 16)
addresses=[int(x,16) for x in sys.argv[3:]]
dbg=ctypes.WinDLL('dbghelp.dll')
hproc=ctypes.windll.kernel32.GetCurrentProcess()
dbg.SymInitializeW.argtypes=[wintypes.HANDLE,wintypes.LPCWSTR,wintypes.BOOL];dbg.SymInitializeW.restype=wintypes.BOOL
dbg.SymSetOptions.argtypes=[wintypes.DWORD];dbg.SymSetOptions.restype=wintypes.DWORD
dbg.SymSetOptions(0x10 | 0x2 | 0x80000000)
if not dbg.SymInitializeW(hproc, None, False): raise ctypes.WinError()
class IMAGEHLP_MODULE64(ctypes.Structure):
    _fields_=[('SizeOfStruct',wintypes.DWORD),('BaseOfImage',ctypes.c_ulonglong),('ImageSize',wintypes.DWORD),('TimeDateStamp',wintypes.DWORD),('CheckSum',wintypes.DWORD),('NumSyms',wintypes.DWORD),('SymType',ctypes.c_int),('ModuleName',ctypes.c_char*32),('ImageName',ctypes.c_char*256),('LoadedImageName',ctypes.c_char*256),('LoadedPdbName',ctypes.c_char*256),('CVSig',wintypes.DWORD),('CVData',ctypes.c_char*780),('Pdb未',wintypes.DWORD),('Dbg未',wintypes.DWORD),('Line未',wintypes.DWORD),('Global未',wintypes.DWORD),('Type未',wintypes.DWORD),('Source未',wintypes.DWORD),('Public未',wintypes.DWORD),('MachineType',wintypes.DWORD),('Reserved',wintypes.DWORD*3)]
dbg.SymLoadModuleExW.argtypes=[wintypes.HANDLE,wintypes.HANDLE,wintypes.LPCWSTR,wintypes.LPCWSTR,ctypes.c_ulonglong,wintypes.DWORD,wintypes.LPVOID,wintypes.DWORD];dbg.SymLoadModuleExW.restype=ctypes.c_ulonglong
loaded=dbg.SymLoadModuleExW(hproc,None,exe,None,base,0,None,0)
print('loaded base',hex(loaded))
class SYMBOL_INFO(ctypes.Structure):
    _fields_=[('SizeOfStruct',wintypes.ULONG),('TypeIndex',wintypes.ULONG),('Reserved',ctypes.c_ulonglong*2),('Index',wintypes.ULONG),('Size',wintypes.ULONG),('ModBase',ctypes.c_ulonglong),('Flags',wintypes.ULONG),('Value',ctypes.c_ulonglong),('Address',ctypes.c_ulonglong),('Register',wintypes.ULONG),('Scope',wintypes.ULONG),('Tag',wintypes.ULONG),('NameLen',wintypes.ULONG),('MaxNameLen',wintypes.ULONG),('Name',ctypes.c_char*1024)]
dbg.SymFromAddr.argtypes=[wintypes.HANDLE,ctypes.c_ulonglong,ctypes.POINTER(ctypes.c_ulonglong),ctypes.POINTER(SYMBOL_INFO)];dbg.SymFromAddr.restype=wintypes.BOOL
for a in addresses:
    info=SYMBOL_INFO(); info.SizeOfStruct=ctypes.sizeof(SYMBOL_INFO) - 1024;info.MaxNameLen=1024;disp=ctypes.c_ulonglong()
    ok=dbg.SymFromAddr(hproc,a,ctypes.byref(disp),ctypes.byref(info))
    if ok: print(hex(a),info.Name.decode('utf-8','replace'),'+',hex(disp.value))
    else: print(hex(a),'no symbol',ctypes.GetLastError())
dbg.SymCleanup.argtypes=[wintypes.HANDLE];dbg.SymCleanup(hproc)
