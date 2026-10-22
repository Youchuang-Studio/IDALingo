#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifndef IDALINGO_DEFAULT_IDA_EXECUTABLE
#define IDALINGO_DEFAULT_IDA_EXECUTABLE "ida"
#endif

int main(int argc, char** argv) {
    std::error_code error;
    const std::filesystem::path executable = std::filesystem::canonical("/proc/self/exe", error);
    if (error) {
        std::fprintf(
            stderr, "IDALingo: failed to locate the launcher: %s\n", error.message().c_str());
        return 1;
    }

    const std::filesystem::path directory = executable.parent_path();
    const std::filesystem::path preload = directory / "libidalingo.so";
    const std::filesystem::path language = directory / "language.txt";
    if (!std::filesystem::is_regular_file(preload) || !std::filesystem::is_regular_file(language)) {
        std::fprintf(stderr, "IDALingo: missing %s or %s\n", preload.c_str(), language.c_str());
        return 1;
    }

    std::string preload_value = preload.string();
    if (const char* existing = std::getenv("LD_PRELOAD"); existing && *existing) {
        preload_value += ':';
        preload_value += existing;
    }
    if (setenv("LD_PRELOAD", preload_value.c_str(), 1) != 0 ||
        setenv("IDALINGO_LANGUAGE", language.c_str(), 1) != 0) {
        std::perror("IDALingo: setenv");
        return 1;
    }

    const char* override_path = std::getenv("IDALINGO_IDA_EXECUTABLE");
    const std::string target =
        override_path && *override_path ? override_path : IDALINGO_DEFAULT_IDA_EXECUTABLE;

    std::vector<char*> arguments;
    arguments.reserve(static_cast<std::size_t>(argc) + 1);
    arguments.push_back(const_cast<char*>(target.c_str()));
    for (int index = 1; index < argc; ++index) {
        arguments.push_back(argv[index]);
    }
    arguments.push_back(nullptr);

    if (target.find('/') == std::string::npos) {
        execvp(target.c_str(), arguments.data());
    } else {
        execv(target.c_str(), arguments.data());
    }
    std::fprintf(stderr, "IDALingo: failed to start %s (errno %d)\n", target.c_str(), errno);
    return 1;
}
