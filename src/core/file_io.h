#pragma once
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string_view>

// Compare with bounded scratch storage. Never read an arbitrarily large existing
// file just to decide whether a small generated file needs replacing.
inline bool file_contents_equal(const std::filesystem::path& path, std::string_view desired) {
  std::ifstream current(path,std::ios::binary|std::ios::ate);
  if (!current || current.tellg()!=std::streampos(desired.size())) return false;
  current.seekg(0);
  std::array<char,4096> buffer{};
  for (size_t at=0;at<desired.size();) {
    size_t count=std::min(buffer.size(),desired.size()-at);
    if (!current.read(buffer.data(),(std::streamsize)count) || std::string_view(buffer.data(),count)!=desired.substr(at,count)) return false;
    at+=count;
  }
  return true;
}

// A small generated file, whole or not at all (18f, 17/11, varredura: on a full disk the old way
// truncated it first and left it empty, and the engine got an empty shader or no key bindings
// until a start with room). Written beside it and swapped in; on failure the old file stays as it
// was and false comes back.
inline bool write_whole_file(const std::filesystem::path& path, std::string_view contents) {
  std::filesystem::path tmp = path;
  tmp += ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(contents.data(), (std::streamsize)contents.size());
    f.flush();
    if (!f) { f.close(); std::error_code ec; std::filesystem::remove(tmp, ec); return false; }
  }
  std::error_code ec;
  if (!file_contents_equal(tmp, contents)) { std::filesystem::remove(tmp, ec); return false; }
  std::filesystem::rename(tmp, path, ec);  // (replaces: MoveFileExW with MOVEFILE_REPLACE_EXISTING on Windows)
  if (ec) { std::filesystem::remove(tmp, ec); return false; }
  return true;
}

inline constexpr size_t kMaxStateBytes = 4 * 1024 * 1024;
inline bool read_state_file(const std::filesystem::path& path, std::string& out) {
  out.clear();
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f || f.tellg() < 0 || f.tellg() > std::streampos(kMaxStateBytes)) return false;
  f.seekg(0);
  std::array<char,4096> buffer{};
  while (f) {
    f.read(buffer.data(), buffer.size());
    size_t count = static_cast<size_t>(f.gcount());
    if (count > kMaxStateBytes - out.size()) { out.clear(); return false; }
    out.append(buffer.data(), count);
  }
  return f.eof() && !f.bad();
}

// A file kept only in the cloud (OneDrive "online only" and the like), from its attributes:
// reading it, even its tags, brings the whole file down. Its name, size and the shell's own
// thumbnail cost nothing. (0xFFFFFFFF: no attributes, the file is not there.)
inline bool cloud_only_attributes(unsigned long attrs) {
  return attrs != 0xFFFFFFFFul && (attrs & (0x00001000ul /*OFFLINE*/ | 0x00040000ul /*RECALL_ON_OPEN*/ | 0x00400000ul /*RECALL_ON_DATA_ACCESS*/)) != 0;
}
