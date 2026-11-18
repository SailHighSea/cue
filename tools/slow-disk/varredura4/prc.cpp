#include <windows.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cwchar>
#include <vector>
#include <map>
struct HE { USHORT pid; USHORT creator; UCHAR type; UCHAR flags; USHORT handle; PVOID obj; ACCESS_MASK access; };
struct HI { ULONG n; HE h[1]; };
typedef NTSTATUS(NTAPI* QSI)(ULONG, PVOID, ULONG, PULONG);
int wmain(int argc, wchar_t** argv) {
  const wchar_t* name = argc > 1 ? argv[1] : L"Cue.exe";
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  PROCESSENTRY32W pe{sizeof pe}; DWORD pid = 0, threads = 0;
  for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
    if (!_wcsicmp(pe.szExeFile, name)) { pid = pe.th32ProcessID; threads = pe.cntThreads; break; }
  CloseHandle(snap);
  if (!pid) { std::printf("none\n"); return 1; }
  auto q = (QSI)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQuerySystemInformation");
  std::vector<char> buf(1 << 20); ULONG got = 0; NTSTATUS st;
  while ((st = q(16, buf.data(), (ULONG)buf.size(), &got)) == (NTSTATUS)0xC0000004) buf.resize(buf.size() * 2);
  HI* hi = (HI*)buf.data(); int n = 0; std::map<int,int> bytype;
  for (ULONG i = 0; i < hi->n; ++i) if (hi->h[i].pid == (USHORT)pid) { ++n; bytype[hi->h[i].type]++; }
  std::printf("handles %d threads %lu types", n, threads);
  for (auto& [t, c] : bytype) std::printf(" %d:%d", t, c);
  std::printf("\n");
  return 0;
}
