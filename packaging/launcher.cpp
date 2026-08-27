#include "version.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <zstd.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr int PayloadResource = 101;

void showError(const wchar_t *message) {
  MessageBoxW(nullptr, message, L"MianA Desk", MB_OK | MB_ICONERROR);
}

std::wstring environmentValue(const wchar_t *name) {
  const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
  if (!required)
    return {};
  std::wstring value(required, L'\0');
  value.resize(GetEnvironmentVariableW(name, value.data(), required));
  return value;
}

std::filesystem::path executablePath() {
  std::wstring buffer(32768, L'\0');
  buffer.resize(GetModuleFileNameW(nullptr, buffer.data(),
                                  static_cast<DWORD>(buffer.size())));
  return buffer;
}

std::wstring payloadIdentity(const void *payload, DWORD size) {
  std::uint64_t hash = 14695981039346656037ULL;
  const auto *bytes = static_cast<const unsigned char *>(payload);
  for (DWORD index = 0; index < size; ++index) {
    hash ^= bytes[index];
    hash *= 1099511628211ULL;
  }
  std::wostringstream result;
  result << MIANA_PACKAGE_VERSION << L"-zstd-" << std::hex << std::setw(16)
         << std::setfill(L'0') << hash;
  return result.str();
}

template <typename Value>
bool readValue(const std::vector<char> &archive, std::size_t &offset,
               Value &value) {
  if (offset > archive.size() || archive.size() - offset < sizeof(value))
    return false;
  std::memcpy(&value, archive.data() + offset, sizeof(value));
  offset += sizeof(value);
  return true;
}

bool utf8Path(const char *data, int size, std::filesystem::path &result) {
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data,
                                        size, nullptr, 0);
  if (length <= 0)
    return false;
  std::wstring value(length, L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, size, value.data(),
                      length);
  result = std::filesystem::path(value).lexically_normal();
  if (result.empty() || result.is_absolute() || result.has_root_path())
    return false;
  for (const auto &part : result)
    if (part == L"..")
      return false;
  return true;
}

bool extractPayload(const void *payload, DWORD payloadSize,
                    const std::filesystem::path &destination) {
  const unsigned long long contentSize =
      ZSTD_getFrameContentSize(payload, payloadSize);
  if (contentSize == ZSTD_CONTENTSIZE_ERROR ||
      contentSize == ZSTD_CONTENTSIZE_UNKNOWN ||
      contentSize > 512ULL * 1024 * 1024)
    return false;

  std::vector<char> archive(static_cast<std::size_t>(contentSize));
  const std::size_t decoded =
      ZSTD_decompress(archive.data(), archive.size(), payload, payloadSize);
  if (ZSTD_isError(decoded) || decoded != archive.size())
    return false;

  constexpr std::array<char, 8> magic{'M', 'I', 'A', 'N', 'A', 'Z', 'S', '1'};
  if (archive.size() < magic.size() ||
      std::memcmp(archive.data(), magic.data(), magic.size()) != 0)
    return false;
  std::size_t offset = magic.size();
  std::uint32_t fileCount = 0;
  if (!readValue(archive, offset, fileCount) || fileCount > 100000)
    return false;

  std::error_code error;
  for (std::uint32_t index = 0; index < fileCount; ++index) {
    std::uint32_t pathSize = 0;
    std::uint64_t fileSize = 0;
    if (!readValue(archive, offset, pathSize) || pathSize == 0 ||
        pathSize > 32768 || !readValue(archive, offset, fileSize) ||
        offset > archive.size() || archive.size() - offset < pathSize)
      return false;

    std::filesystem::path relative;
    if (!utf8Path(archive.data() + offset, static_cast<int>(pathSize), relative))
      return false;
    offset += pathSize;
    if (fileSize > archive.size() - offset)
      return false;

    const auto target = destination / relative;
    std::filesystem::create_directories(target.parent_path(), error);
    if (error)
      return false;
    std::ofstream output(target, std::ios::binary | std::ios::trunc);
    output.write(archive.data() + offset,
                 static_cast<std::streamsize>(fileSize));
    if (!output)
      return false;
    offset += static_cast<std::size_t>(fileSize);
  }
  return offset == archive.size();
}

bool launch(const std::filesystem::path &application,
            const std::filesystem::path &workingDirectory) {
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  std::wstring commandLine = L"\"" + application.wstring() + L"\"";
  std::vector<wchar_t> command(commandLine.begin(), commandLine.end());
  command.push_back(L'\0');
  const std::wstring working = workingDirectory.wstring();
  if (!CreateProcessW(application.c_str(), command.data(), nullptr, nullptr,
                      FALSE, 0, nullptr, working.c_str(), &startup, &process))
    return false;
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return true;
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  const HRSRC resource =
      FindResourceW(instance, MAKEINTRESOURCEW(PayloadResource), RT_RCDATA);
  const HGLOBAL loaded = resource ? LoadResource(instance, resource) : nullptr;
  const DWORD payloadSize = resource ? SizeofResource(instance, resource) : 0;
  const void *payload = loaded ? LockResource(loaded) : nullptr;
  if (!payload || payloadSize == 0) {
    showError(L"程序运行包损坏。");
    return 1;
  }

  const std::wstring localAppData = environmentValue(L"LOCALAPPDATA");
  if (localAppData.empty()) {
    showError(L"无法获取用户缓存目录。");
    return 1;
  }
  const auto cacheRoot =
      std::filesystem::path(localAppData) / L"MianA/MianADesk/cache";
  const auto cacheDirectory = cacheRoot / payloadIdentity(payload, payloadSize);
  const auto application = cacheDirectory / L"MianADesk.exe";
  const auto completionMarker = cacheDirectory / L".complete";
  std::error_code error;

  if (!std::filesystem::exists(application) ||
      !std::filesystem::exists(completionMarker)) {
    std::filesystem::create_directories(cacheRoot, error);
    if (error) {
      showError(L"无法创建程序缓存目录。");
      return 1;
    }
    std::filesystem::remove_all(cacheDirectory, error);
    error.clear();
    const auto staging =
        cacheRoot / (L"staging-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::remove_all(staging, error);
    error.clear();
    std::filesystem::create_directories(staging, error);
    if (error || !extractPayload(payload, payloadSize, staging)) {
      showError(L"无法解压程序运行包。");
      std::filesystem::remove_all(staging, error);
      return 1;
    }
    {
      std::ofstream marker(staging / L".complete", std::ios::binary);
      marker << "MianA Desk Zstandard package";
    }
    std::filesystem::rename(staging, cacheDirectory, error);
    if (error && (!std::filesystem::exists(application) ||
                  !std::filesystem::exists(completionMarker))) {
      showError(L"无法完成程序缓存更新。");
      std::filesystem::remove_all(staging, error);
      return 1;
    }
    std::filesystem::remove_all(staging, error);
  }

  const auto self = executablePath();
  SetEnvironmentVariableW(L"MIANA_SINGLE_EXE", self.c_str());
  const bool started = launch(application, cacheDirectory);
  SetEnvironmentVariableW(L"MIANA_SINGLE_EXE", nullptr);
  if (!started) {
    showError(L"MianA Desk 启动失败。");
    return 1;
  }
  return 0;
}
