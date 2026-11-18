#include <windows.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cwchar>
#include <vector>
#include <map>
#include <string>
struct HE { USHORT pid; USHORT creator; UCHAR type; UCHAR flags; USHORT handle; PVOID obj; ACCESS_MASK access; };
struct HI { ULONG n; HE h[1]; };
typedef NTSTATUS(NTAPI* QSI)(ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* QO)(HANDLE, ULONG, PVOID, ULONG, PULONG);
int wmain(int argc, wchar_t** argv) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  PROCESSENTRY32W pe{sizeof pe}; DWORD pid = 0;
  for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
    if (!_wcsicmp(pe.szExeFile, L"Cue.exe")) { pid = pe.th32ProcessID; break; }
  if (!pid) return 1;
  auto q = (QSI)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQuerySystemInformation");
  auto qo = (QO)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryObject");
  std::vector<char> buf(1 << 20); ULONG got = 0;
  while (q(16, buf.data(), (ULONG)buf.size(), &got) == (NTSTATUS)0xC0000004) buf.resize(buf.size() * 2);
  HI* hi = (HI*)buf.data();
  HANDLE proc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
  std::map<int, int> done;
  for (ULONG i = 0; i < hi->n; ++i) {
    HE& e = hi->h[i]; if (e.pid != (USHORT)pid) continue;
    HANDLE d = nullptr;
    if (!DuplicateHandle(proc, (HANDLE)(ULONG_PTR)e.handle, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS)) continue;
    char tb[1024]; ULONG l = 0; char nb[2048]; 
    std::wstring tn, nn;
    if (qo(d, 2, tb, sizeof tb, &l) == 0) { auto* u = (UNICODE_STRING*)tb; tn.assign(u->Buffer, u->Length / 2); }
    if (e.type == 8 || e.type == 17) { if (qo(d, 1, nb, sizeof nb, &l) == 0) { auto* u = (UNICODE_STRING*)nb; if (u->Buffer) nn.assign(u->Buffer, u->Length / 2); } }
    if (!done[e.type]++ || e.type == 8 || e.type == 17) std::wprintf(L"type %d %ls h=%x %ls\n", e.type, tn.c_str(), e.handle, nn.c_str());
    CloseHandle(d);
  }
  return 0;
}
