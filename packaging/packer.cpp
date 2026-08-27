#include <zstd.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
template <typename Value>
void appendValue(std::vector<char> &output, Value value) {
  const auto *data = reinterpret_cast<const char *>(&value);
  output.insert(output.end(), data, data + sizeof(value));
}
} // namespace

int wmain(int argc, wchar_t **argv) {
  if (argc != 3) {
    std::wcerr << L"usage: miana-packer <input-directory> <output-file>\n";
    return 1;
  }

  const std::filesystem::path root =
      std::filesystem::weakly_canonical(argv[1]);
  if (!std::filesystem::is_directory(root)) {
    std::wcerr << L"input directory does not exist\n";
    return 2;
  }

  std::vector<std::filesystem::path> files;
  std::uintmax_t contentBytes = 0;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
    if (entry.is_regular_file()) {
      files.push_back(entry.path());
      contentBytes += entry.file_size();
    }
  }
  std::sort(files.begin(), files.end());

  std::vector<char> archive;
  archive.reserve(static_cast<std::size_t>(contentBytes) + files.size() * 96);
  constexpr std::array<char, 8> magic{'M', 'I', 'A', 'N', 'A', 'Z', 'S', '1'};
  archive.insert(archive.end(), magic.begin(), magic.end());
  appendValue(archive, static_cast<std::uint32_t>(files.size()));

  std::vector<char> buffer(1024 * 1024);
  for (const auto &file : files) {
    const auto relative = std::filesystem::relative(file, root).generic_u8string();
    const std::string path(reinterpret_cast<const char *>(relative.data()),
                           relative.size());
    const auto size = static_cast<std::uint64_t>(std::filesystem::file_size(file));
    appendValue(archive, static_cast<std::uint32_t>(path.size()));
    appendValue(archive, size);
    archive.insert(archive.end(), path.begin(), path.end());

    std::ifstream input(file, std::ios::binary);
    if (!input) {
      std::wcerr << L"cannot read " << file << L'\n';
      return 3;
    }
    while (input) {
      input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      archive.insert(archive.end(), buffer.data(), buffer.data() + input.gcount());
    }
  }

  std::vector<char> compressed(ZSTD_compressBound(archive.size()));
  const std::size_t compressedSize =
      ZSTD_compress(compressed.data(), compressed.size(), archive.data(),
                    archive.size(), 22);
  if (ZSTD_isError(compressedSize)) {
    std::cerr << ZSTD_getErrorName(compressedSize) << '\n';
    return 4;
  }

  std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
  output.write(compressed.data(), static_cast<std::streamsize>(compressedSize));
  if (!output)
    return 5;

  std::wcout << L"Packed " << files.size() << L" files: " << archive.size()
             << L" -> " << compressedSize << L" bytes\n";
  return 0;
}
