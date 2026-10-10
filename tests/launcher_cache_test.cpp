#include "../packaging/launcher.cpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <future>
#include <iterator>
#include <latch>

namespace {
template <typename Value>
void append(std::vector<char> &bytes, Value value) {
  const auto *data = reinterpret_cast<const char *>(&value);
  bytes.insert(bytes.end(), data, data + sizeof(value));
}

std::vector<char> testPayload() {
  std::vector<char> archive{'M', 'I', 'A', 'N', 'A', 'Z', 'S', '1'};
  append(archive, std::uint32_t{2});
  for (const std::string &path : {std::string("MianADesk.exe"),
                                  std::string("qml/test.txt")}) {
    const std::string contents = "fixture-" + std::to_string(GetCurrentProcessId());
    append(archive, static_cast<std::uint32_t>(path.size()));
    append(archive, static_cast<std::uint64_t>(contents.size()));
    archive.insert(archive.end(), path.begin(), path.end());
    archive.insert(archive.end(), contents.begin(), contents.end());
  }
  std::vector<char> payload(ZSTD_compressBound(archive.size()));
  const auto size = ZSTD_compress(payload.data(), payload.size(), archive.data(),
                                  archive.size(), 1);
  assert(!ZSTD_isError(size));
  payload.resize(size);
  return payload;
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
} // namespace

int main() {
  using namespace std::chrono_literals;
  const auto testRoot = executablePath().parent_path() /
      (L"MianADesk-cache-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
       std::to_wstring(GetTickCount64()));
  const auto payload = testPayload();
  const auto size = static_cast<DWORD>(payload.size());
  const auto identity = payloadIdentity(payload.data(), size);
  const auto cacheDirectory = testRoot / identity;
  std::array<std::future<const wchar_t *>, 2> workers;
  std::latch started(2);
  {
    const CacheLock held(identity);
    assert(held.acquired);
    for (auto &worker : workers)
      worker = std::async(std::launch::async, [&] {
        started.count_down();
        return prepareCache(payload.data(), size, cacheDirectory);
      });
    started.wait();
    for (auto &worker : workers)
      assert(worker.wait_for(100ms) == std::future_status::timeout);
    assert(!std::filesystem::exists(cacheDirectory));
  }
  for (auto &worker : workers) {
    assert(worker.wait_for(5s) == std::future_status::ready);
    assert(worker.get() == nullptr);
  }
  const std::string contents = "fixture-" + std::to_string(GetCurrentProcessId());
  assert(readFile(cacheDirectory / L"MianADesk.exe") == contents);
  assert(readFile(cacheDirectory / L"qml/test.txt") == contents);
  assert(std::filesystem::exists(cacheDirectory / L".complete"));
  assert(!std::filesystem::exists(
      testRoot / (L"staging-" + std::to_wstring(GetCurrentProcessId()))));

  const auto sentinel = cacheDirectory / L"reuse-sentinel";
  std::ofstream(sentinel) << "keep";
  assert(prepareCache(payload.data(), size, cacheDirectory) == nullptr);
  assert(readFile(sentinel) == "keep");

  const auto retryDirectory = testRoot / (identity + L"-retry");
  assert(prepareCache("bad", 3, retryDirectory) != nullptr);
  auto retry = std::async(std::launch::async, [&] {
    return prepareCache(payload.data(), size, retryDirectory);
  });
  assert(retry.wait_for(5s) == std::future_status::ready);
  assert(retry.get() == nullptr);
  assert(readFile(retryDirectory / L"qml/test.txt") == contents);
  std::filesystem::remove_all(testRoot);
}
