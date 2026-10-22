#include <idalingo/text_exporter.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main() {
    const auto path = std::filesystem::temp_directory_path() / "idalingo_text_export_test.txt";
    std::filesystem::remove(path);

    {
        cn::TextExporter exporter;
        if (exporter.Open(std::filesystem::temp_directory_path())) {
            return 3;
        }
        if (!exporter.Open(path)) {
            return 1;
        }
        exporter.RecordUtf8("Open file");
        exporter.RecordUtf8("Open file");
        exporter.RecordUtf8("Line\n\"quoted\"");
        exporter.RecordUtf16(L"函数窗口");
    }

    std::string content;
    {
        std::ifstream input(path, std::ios::binary);
        content.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
    const std::string expected = "L\"Open file\",L\"Open file\",\r\n"
                                 "L\"Line\\n\\\"quoted\\\"\",L\"Line\\n\\\"quoted\\\"\",\r\n"
                                 "L\"函数窗口\",L\"函数窗口\",\r\n";
    if (content != expected) {
        std::cerr << "export mismatch\n";
        return 2;
    }

    std::filesystem::remove(path);
    return 0;
}
