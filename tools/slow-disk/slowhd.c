// LD_PRELOAD shim: everything under $SLOWHD_PREFIX behaves like an external USB hard drive.
// One queue for all processes (flock), a seek per non-sequential read and per metadata lookup,
// a transfer rate, and a spin-up after the drive has been idle (it went to sleep).
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

static const char* prefix; static size_t prefix_n; static char pre[4][512]; static int npre;
static double seek_ms = 14, mbps = 25, spinup_ms = 2500, sleep_s = 30;
static int lockfd = -1, active = 0;
struct shared { double last; long long fd_key; long long end; long long ops; double waited_ms; };
static struct shared* sh;
static unsigned char slowfd[65536];

static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
static void nap(double ms) { if (ms <= 0) return; struct timespec t = {(time_t)(ms / 1000), (long)((ms - (long)(ms / 1000) * 1000) * 1e6)}; while (nanosleep(&t, &t) && errno == EINTR) {} }

__attribute__((constructor)) static void init(void) {
  prefix = getenv("SLOWHD_PREFIX");
  if (!prefix || !*prefix || strstr(program_invocation_short_name, "wineserver")) return;
  prefix_n = strlen(prefix);
  { char tmp[2048]; snprintf(tmp, sizeof tmp, "%s", prefix); for (char* t = strtok(tmp, "|"); t && npre < 4; t = strtok(0, "|")) snprintf(pre[npre++], 512, "%s", t); }
  if (getenv("SLOWHD_SEEK_MS")) seek_ms = atof(getenv("SLOWHD_SEEK_MS"));
  if (getenv("SLOWHD_MBPS")) mbps = atof(getenv("SLOWHD_MBPS"));
  if (getenv("SLOWHD_SPINUP_MS")) spinup_ms = atof(getenv("SLOWHD_SPINUP_MS"));
  if (getenv("SLOWHD_SLEEP_S")) sleep_s = atof(getenv("SLOWHD_SLEEP_S"));
  int (*real_open)(const char*, int, ...) = dlsym(RTLD_NEXT, "open");
  int f = real_open("/tmp/slowhd.state", O_RDWR | O_CREAT | O_CLOEXEC, 0666);
  if (f < 0) return;
  if (ftruncate(f, sizeof(struct shared)) != 0) return;
  sh = mmap(0, sizeof(struct shared), PROT_READ | PROT_WRITE, MAP_SHARED, f, 0);
  lockfd = f;
  active = sh != MAP_FAILED;
}
static int under(const char* p) { if (!active || !p) return 0; for (int i = 0; i < npre; ++i) if ((strncmp(p, pre[i], strlen(pre[i])) == 0 && p[strlen(pre[i])] != 0) || (getenv("SLOWHD_LOG") && 0)) return 1; if (getenv("SLOWHD_LOG") && strstr(p, "Serie")) fprintf(stderr, "slowhd: unmatched %s\n", p); return 0; }
// one operation on the drive: queue, maybe wake it, seek, transfer
// (the drive's own root is not a file: Windows answers "which kind of drive" without the disk)
// SLOWHD "unplugged": while /tmp/slowhd.gone exists, the disk is gone (a pendrive pulled out, a
// share that dropped): looking a file up says it is not there, reading an open file fails
#include <sys/syscall.h>
static int gone(void) { static double at = -1; static int g = 0; double t = now_ms();
  if (t - at > 50) { at = t; g = syscall(SYS_faccessat, AT_FDCWD, "/tmp/slowhd.gone", F_OK, 0) == 0; } return g; }
// a share that stops answering mid-way: while /tmp/slowhd.lag holds a number, every operation
// waits that many more milliseconds
static double lag(void) { static double at = -1, v = 0; double t = now_ms();
  if (t - at > 100) { at = t; v = 0; int f = (int)syscall(SYS_openat, AT_FDCWD, "/tmp/slowhd.lag", O_RDONLY | O_CLOEXEC);
    if (f >= 0) { char b[32] = {0}; if (syscall(SYS_read, f, b, sizeof b - 1) > 0) v = atof(b); syscall(SYS_close, f); } } return v; }
static void disk(int seek, size_t bytes, long long key, long long off) {
  flock(lockfd, LOCK_EX);
  double t0 = now_ms(), cost = 0;
  if (sh->last > 0 && t0 - sh->last > sleep_s * 1000) cost += spinup_ms;
  if (sh->last == 0) cost += spinup_ms;
  if (seek || sh->fd_key != key || sh->end != off) cost += seek_ms;
  cost += bytes / (mbps * 1e6) * 1e3;
  cost += lag();
  nap(cost);
  sh->last = now_ms(); sh->fd_key = key; sh->end = off + (long long)bytes; sh->ops++; sh->waited_ms += cost;
  flock(lockfd, LOCK_UN);
}
static int under_at(int d, const char* p) {
  if (!active || !p) return 0;
  if (p[0] == '/' || d == AT_FDCWD) return under(p);
  char link[64], dir[1024], full[2048]; snprintf(link, sizeof link, "/proc/self/fd/%d", d);
  ssize_t n = readlink(link, dir, sizeof dir - 1); if (n <= 0) return 0; dir[n] = 0;
  snprintf(full, sizeof full, "%s/%s", dir, p); return under(full);
}
static long long bytes_total;
static long long key_of(int fd) { return ((long long)getpid() << 20) | fd; }
static void mark(int fd, int on) { if (fd >= 0 && fd < 65536) slowfd[fd] = on ? 1 : 0; }
// (Wine's server opens files and hands the descriptor over: known by its path at the first read)
static int slow(int fd) {
  if (!active || fd < 0 || fd >= 65536) return 0;
  if (!slowfd[fd]) { char link[64], path[1024]; snprintf(link, sizeof link, "/proc/self/fd/%d", fd);
    ssize_t n = readlink(link, path, sizeof path - 1); path[n > 0 ? n : 0] = 0; slowfd[fd] = n > 0 && under(path) ? 1 : 2; }
  return slowfd[fd] == 1;
}

#define REAL(ret, name, ...) static ret (*real_##name)(__VA_ARGS__); if (!real_##name) real_##name = dlsym(RTLD_NEXT, #name)
int open(const char* p, int fl, ...) { va_list a; va_start(a, fl); int m = va_arg(a, int); va_end(a); REAL(int, open, const char*, int, ...);
  int s = under(p); if (s && gone()) { errno = ENOENT; return -1; } if (s) disk(1, 0, -1, 0); int fd = real_open(p, fl, m); mark(fd, s); return fd; }
int open64(const char* p, int fl, ...) { va_list a; va_start(a, fl); int m = va_arg(a, int); va_end(a); REAL(int, open64, const char*, int, ...);
  int s = under(p); if (s && gone()) { errno = ENOENT; return -1; } if (s) disk(1, 0, -1, 0); int fd = real_open64(p, fl, m); mark(fd, s); return fd; }
int openat(int d, const char* p, int fl, ...) { va_list a; va_start(a, fl); int m = va_arg(a, int); va_end(a); REAL(int, openat, int, const char*, int, ...);
  int s = under_at(d, p); if (s && gone()) { errno = ENOENT; return -1; } if (s) disk(1, 0, -1, 0); int fd = real_openat(d, p, fl, m); mark(fd, s); return fd; }
int openat64(int d, const char* p, int fl, ...) { va_list a; va_start(a, fl); int m = va_arg(a, int); va_end(a); REAL(int, openat64, int, const char*, int, ...);
  int s = under_at(d, p); if (s && gone()) { errno = ENOENT; return -1; } if (s) disk(1, 0, -1, 0); int fd = real_openat64(d, p, fl, m); mark(fd, s); return fd; }
int close(int fd) { REAL(int, close, int); mark(fd, 0); return real_close(fd); }
int stat(const char* p, struct stat* b) { REAL(int, stat, const char*, struct stat*); if (under(p)) { if (gone()) { errno = ENOENT; return -1; } disk(1, 0, -1, 0); } return real_stat(p, b); }
int lstat(const char* p, struct stat* b) { REAL(int, lstat, const char*, struct stat*); if (under(p)) { if (gone()) { errno = ENOENT; return -1; } disk(1, 0, -1, 0); } return real_lstat(p, b); }
int fstatat(int d, const char* p, struct stat* b, int f) { REAL(int, fstatat, int, const char*, struct stat*, int); if (under_at(d, p)) { if (gone()) { errno = ENOENT; return -1; } disk(1, 0, -1, 0); } return real_fstatat(d, p, b, f); }
int stat64(const char* p, struct stat64* b) { REAL(int, stat64, const char*, struct stat64*); if (under(p)) { if (gone()) { errno = ENOENT; return -1; } disk(1, 0, -1, 0); } return real_stat64(p, b); }
int lstat64(const char* p, struct stat64* b) { REAL(int, lstat64, const char*, struct stat64*); if (under(p)) { if (gone()) { errno = ENOENT; return -1; } disk(1, 0, -1, 0); } return real_lstat64(p, b); }
int fstatat64(int d, const char* p, struct stat64* b, int f) { REAL(int, fstatat64, int, const char*, struct stat64*, int); if (under_at(d, p)) { if (gone()) { errno = ENOENT; return -1; } disk(1, 0, -1, 0); } return real_fstatat64(d, p, b, f); }
int statx(int d, const char* p, int f, unsigned m, struct statx* b) { REAL(int, statx, int, const char*, int, unsigned, struct statx*); if (under_at(d, p)) { if (gone()) { errno = ENOENT; return -1; } disk(1, 0, -1, 0); } return real_statx(d, p, f, m, b); }
ssize_t read(int fd, void* b, size_t n) { REAL(ssize_t, read, int, void*, size_t); if (slow(fd) && gone()) { errno = EIO; return -1; } ssize_t r = real_read(fd, b, n);
  if (slow(fd) && r > 0) { off_t o = lseek(fd, 0, SEEK_CUR) - r; disk(0, (size_t)r, key_of(fd), o); } return r; }
ssize_t pread(int fd, void* b, size_t n, off_t o) { REAL(ssize_t, pread, int, void*, size_t, off_t); if (slow(fd) && gone()) { errno = EIO; return -1; } ssize_t r = real_pread(fd, b, n, o);
  if (slow(fd) && r > 0) disk(0, (size_t)r, key_of(fd), o); return r; }
ssize_t pread64(int fd, void* b, size_t n, off_t o) { REAL(ssize_t, pread64, int, void*, size_t, off_t); if (slow(fd) && gone()) { errno = EIO; return -1; } ssize_t r = real_pread64(fd, b, n, o);
  if (slow(fd) && r > 0) disk(0, (size_t)r, key_of(fd), o); return r; }
DIR* opendir(const char* p) { REAL(DIR*, opendir, const char*); if (under(p)) disk(1, 4096, -1, 0); return real_opendir(p); }
