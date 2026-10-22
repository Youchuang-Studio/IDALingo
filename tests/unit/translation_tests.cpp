#include <idalingo/translation.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int failures = 0;

template <typename Actual, typename Expected>
void ExpectEqual(const Actual& actual, const Expected& expected, const char* label) {
    if (actual != expected) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

} // namespace

int main(int argc, char** argv) {
    const auto path = std::filesystem::temp_directory_path() / "idalingo_translation_test.txt";
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "# test rules\n"
               << "; another comment\n"
               << "L\"Open file\",L\"打开文件\",\n"
               << "L\"Data\",L\"数据\",\n"
               << "L\"Database\",L\"数据库\",\n"
               << "L\" already exists.\",L\" 已存在。\",\n"
               << "L\"Do you want to overwrite it?\",L\"是否覆盖？\",\n"
               << "\"IDA View\",\"自定义视图\"\n"
               << "\"Line\\nBreak\",\"行\\n换行\"\n";
    }

    cn::TranslationTable table;
    std::wstring status;
    ExpectEqual(table.LoadFile(path, &status), true, "file loads");
    ExpectEqual(
        table.TranslateUtf8("Open file", false), std::string("打开文件"), "L prefix syntax");
    ExpectEqual(table.TranslateUtf8("IDA View", false), std::string("自定义视图"),
        "file overrides builtin");
    ExpectEqual(table.TranslateUtf8("Action: Open file", true), std::string("Action: 打开文件"),
        "substring replacement");
    ExpectEqual(
        table.TranslateUtf16(
            L"Database D:\\IDA\\winmm.dll.i64 already exists.\nDo you want to overwrite it?", true),
        std::wstring(L"数据库 D:\\IDA\\winmm.dll.i64 已存在。\n是否覆盖？"),
        "longest match prevents Data from corrupting Database");
    ExpectEqual(
        table.TranslateUtf8("Line\nBreak", false), std::string("行\n换行"), "escape decoding");
    ExpectEqual(
        table.TranslateUtf16(L"Open file", false), std::wstring(L"打开文件"), "UTF-16 table");
    ExpectEqual(cn::IsShortcutText("Ctrl-PgDown"), true, "shortcut with modifier");
    ExpectEqual(cn::IsShortcutText("Shift-Ctrl-PgDown"), true, "shortcut with multiple modifiers");
    ExpectEqual(cn::IsShortcutText("PgDown"), true, "named shortcut key");
    ExpectEqual(cn::IsShortcutText("sys(MoveToNextPage)"), true, "system shortcut");
    ExpectEqual(cn::IsShortcutText("Scroll down"), false, "ordinary label");

    std::filesystem::remove(path);

    if (argc > 1) {
        cn::TranslationTable full_table;
        std::wstring full_status;
        if (!full_table.LoadFile(argv[1], &full_status)) {
            std::wcerr << L"FAIL: " << full_status << L'\n';
            ++failures;
        } else {
            ExpectEqual(full_table.TranslateUtf16(L"Clone Repository", true),
                std::wstring(L"克隆仓库"), "clone repository dialog title");
            ExpectEqual(full_table.TranslateUtf16(L"Create Repository", true),
                std::wstring(L"创建仓库"), "create repository dialog title");
            ExpectEqual(
                full_table.TranslateUtf16(
                    L"You're creating a git repository starting from this database.\nOptionally, create an empty repository on your server and paste the URL below.",
                    false),
                std::wstring(
                    L"正在以当前数据库创建 Git 仓库。\n您也可以在服务器上创建一个空仓库，并在下方粘贴其 URL。"),
                "create repository combined description");
            ExpectEqual(
                full_table.TranslateUtf16(
                    L"You need to enter the repository URL that was shared with you.\nAfter cloning, you can open an existing database from the repository or add new ones.",
                    false),
                std::wstring(
                    L"请输入与您共享的仓库 URL。\n克隆后，您可以从仓库打开现有数据库或添加新数据库。"),
                "clone repository combined description");
            ExpectEqual(full_table.TranslateUtf16(L"Destination folder", true),
                std::wstring(L"目标文件夹"), "destination folder label");
            ExpectEqual(full_table.TranslateUtf16(
                            L"URL of a new, empty remote repository (e.g., GitHub/GitLab)", false),
                std::wstring(L"新的空远程仓库 URL（例如 GitHub/GitLab）"),
                "new remote repository tooltip");
            ExpectEqual(full_table.TranslateUtf16(L"URL of the shared repository", false),
                std::wstring(L"共享仓库的 URL"), "shared repository tooltip");
            ExpectEqual(
                full_table.TranslateUtf16(
                    L"Folder the repository will be cloned into. Pre-filled with the repository name; edit it if you prefer.",
                    false),
                std::wstring(L"仓库将克隆到此文件夹。默认填入仓库名称，您可以根据需要修改。"),
                "clone folder tooltip");
            ExpectEqual(full_table.TranslateUtf16(L"Enable Git LFS", true),
                std::wstring(L"启用 Git LFS"), "Git LFS checkbox");
            ExpectEqual(full_table.TranslateUtf16(L"Xrefs graph", false),
                std::wstring(L"交叉引用图"), "xrefs graph section");
            ExpectEqual(full_table.TranslateUtf16(L"Max no&de name length", false),
                std::wstring(L"最大节点名称长度(&D)"), "xref node name label");
            ExpectEqual(full_table.TranslateUtf16(L"&Nodes count threshold", false),
                std::wstring(L"节点数量阈值(&N)"), "xref node threshold label");
            ExpectEqual(full_table.TranslateUtf16(L"Few missing xrefs threshold", false),
                std::wstring(L"少量缺失交叉引用阈值"), "few missing xrefs label");
            ExpectEqual(full_table.TranslateUtf16(L"Many missing xrefs threshold", false),
                std::wstring(L"大量缺失交叉引用阈值"), "many missing xrefs label");
            ExpectEqual(full_table.TranslateUtf16(L"Teams &backend", false),
                std::wstring(L"Teams 后端(&B)"), "Teams backend label");
            ExpectEqual(
                full_table.TranslateUtf16(L"&Show Teams information in the status bar", false),
                std::wstring(L"在状态栏中显示 Teams 信息(&S)"), "Teams status bar option");
            ExpectEqual(
                full_table.TranslateUtf16(
                    L"Git is the recommended collaboration backend for IDA Teams.<br/>It uses standard Git repositories with native IDB diff and merge support.<br/><br/><a href=\"https://hex-rays.com/r/ida/teams/about\">Learn more about Teams ↗</a>",
                    false),
                std::wstring(
                    L"Git 是 IDA Teams 推荐的协作后端。<br/>它使用标准 Git 仓库，并原生支持 IDB 差异比较与合并。<br/><br/><a href=\"https://hex-rays.com/r/ida/teams/about\">进一步了解 Teams ↗</a>"),
                "Teams backend rich text");
            std::cout << "full language file effective rules=" << full_table.RuleCount() << '\n';
        }
    }
    if (failures != 0) {
        return 1;
    }
    std::cout << "translation tests passed; rules=" << table.RuleCount() << '\n';
    return 0;
}
