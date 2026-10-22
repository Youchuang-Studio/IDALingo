<div align="center">

# IDALingo

**IDA Pro 运行时汉化方案，不修改任何 IDA 文件。**

[![CI](https://github.com/Youchuang-Studio/IDALingo/actions/workflows/ci.yml/badge.svg)](https://github.com/Youchuang-Studio/IDALingo/actions/workflows/ci.yml)
[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-blue.svg)](../../LICENSE)
![Platforms](https://img.shields.io/badge/platform-Windows%20x64%20%7C%20Linux-lightgrey.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)

[English](../../README.md) | 简体中文

</div>

---

IDALingo 在运行时翻译 IDA Pro 的界面：它在 IDA 进程内拦截少量 Qt 6 文本函数，按照纯文本规则文件替换界面字符串。IDA 的可执行文件、Qt 库和安装目录都保持原样，删除 IDALingo 即可恢复英文界面。

项目自带面向 IDA 9.4 的简体中文翻译（约 2700 条规则），并提供只采集、不替换的版本，用于在 IDA 更新后快速找出未翻译的文本。

## 特性

- **无侵入**：Windows 通过 `winmm.dll` 代理加载，Linux 通过 `LD_PRELOAD` 加载，不改动 IDA 安装目录中的任何文件。
- **规则即文本**：所有译文集中在一个 UTF-8 文件中，可以直接审阅、比对和修改，无需重新编译。
- **精确匹配与子串匹配**：普通标签按整句匹配；带路径、数量或地址的长文本由 Aho-Corasick 匹配器处理，并优先采用最长规则，避免 `Data` 误改 `Database` 这类问题。
- **采集版本**：使用与汉化版本相同的 Hook 点，只记录不替换，输出的 `language_export.txt` 与规则文件格式一致，可直接用于翻译。
- **稳妥的默认行为**：快捷键文本不会被改写；即使缺少规则文件，内置规则也能保证主菜单为中文。

## 兼容性

| 平台 | 加载方式 | 工具链 | 状态 |
| --- | --- | --- | --- |
| Windows x64 | `winmm.dll` 代理 + MinHook | Visual Studio 2022 | 支持 |
| Linux x86-64 | `LD_PRELOAD` + 启动器 | GCC 或 Clang（C++20） | 支持 |
| macOS | — | — | 暂不支持 |

IDALingo 要求 IDA 使用 Qt 6 构建。自带规则针对 **IDA Pro 9.4** 维护。Hook 的符号与 IDA 所用的 Qt 构建相关，升级到新版本 IDA 前请先确认这些符号仍然有效，详见[架构说明](../architecture.md)。

## 安装

[CI](https://github.com/Youchuang-Studio/IDALingo/actions/workflows/ci.yml) 会生成 `IDALingo-Windows` 和 `IDALingo-Linux` 两个预编译包，也可以[从源码编译](#从源码编译)。

### Windows

1. 将 `winmm.dll` 和 `language.txt` 复制到 `ida.exe` 所在目录。
2. 照常启动 IDA。

卸载时删除这两个文件即可。

### Linux

1. 将 `idalingo`、`libidalingo.so` 和 `language.txt` 放在同一目录。
2. 通过启动器运行 IDA：

   ```sh
   ./idalingo [IDA 参数...]
   ```

启动器默认从 `PATH` 中查找 `ida`，可以用 `IDALINGO_IDA_EXECUTABLE` 指定其他路径。`LD_PRELOAD` 和 `IDALINGO_LANGUAGE` 只对本次启动的 IDA 进程生效，不会影响当前 Shell 环境。

## 翻译规则

自带译文的源文件是 [`translations/zh-CN/language.txt`](../../translations/zh-CN/language.txt)，编译时会以 `language.txt` 的文件名复制到输出目录。每行一条规则：

```text
# 以 '#' 或 ';' 开头的行是注释
L"Source text",L"Translated text",
L"&New",L"新建(&N)",
```

- 文件可以是 UTF-8，也可以是带 BOM 的 UTF-16；支持 `\"`、`\\`、`\n`、`\r`、`\t` 转义。
- 文件中的规则优先于内置规则。
- 保留助记符（`&`）、占位符、HTML 标签和换行。
- 不要翻译快捷键、动作 ID、Qt 对象名或插件内部标识符；插件文本应由插件自己的本地化机制处理。
- 尽量使用从界面采集到的完整字符串，少用范围宽泛的子串规则。

### 采集未翻译文本

1. 用采集版本替换汉化版本：
   - Windows：`export-only/winmm.dll`
   - Linux：`LD_PRELOAD=/path/to/libidalingo_export.so ida`
2. 在 IDA 中打开需要翻译的界面和功能。
3. 在采集库所在目录查看 `language_export.txt`。Windows 下如果该目录不可写，文件会改为写入 `%LOCALAPPDATA%\IDALingo`。
4. 翻译新增条目，合并到 `translations/zh-CN/language.txt`，再换回汉化版本。

## 从源码编译

环境要求：CMake 3.24 或更高版本；Windows 需要 Visual Studio 2022（x64），Linux 需要支持 C++20 的编译器。

```sh
cmake --preset windows        # Linux 使用 linux
cmake --build --preset windows
ctest --preset windows
cmake --install build/windows --config Release --prefix package
```

| 产物 | Windows | Linux |
| --- | --- | --- |
| 汉化版本 | `build/windows/out/Release/winmm.dll` | `build/linux/out/libidalingo.so`、`build/linux/out/idalingo` |
| 采集版本 | `build/windows/export-only/Release/winmm.dll` | `build/linux/export-only/libidalingo_export.so` |
| 规则文件 | `build/windows/out/Release/language.txt` | `build/linux/out/language.txt` |

<details>
<summary>CMake 选项</summary>

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `IDALINGO_BUILD_EXPORTER` | `ON` | 同时编译采集版本。 |
| `IDALINGO_WARNINGS_AS_ERRORS` | `OFF` | 将编译警告视为错误（CI 中开启）。 |
| `IDALINGO_LANGUAGE_FILE` | `translations/zh-CN/language.txt` | 作为 `language.txt` 发布的规则文件。 |
| `IDALINGO_IDA_EXECUTABLE` | `ida` | Linux 启动器默认启动的程序。 |
| `IDALINGO_QT_DIR` | *（空）* | 仅 Linux：IDA 自带 `libQt6Core.so.6` 所在目录，设置后启用 Qt ABI 测试。 |

</details>

## 目录结构

```text
include/idalingo/      共享代码的公共头文件
src/common/            翻译引擎与文本采集器
src/windows/           winmm.dll 代理与 MinHook 集成
src/linux/             LD_PRELOAD 库、启动器与链接脚本
tests/unit/            与平台无关的单元测试
tests/abi/             Qt ABI 检查
translations/zh-CN/    简体中文翻译规则
third_party/           第三方依赖（MinHook）
docs/                  架构说明与中文文档
```

## 参与贡献

欢迎提交问题反馈、译文修正以及对新版本 IDA 的适配。提交拉取请求前请阅读[贡献指南](contributing.md)。修改 Hook 相关代码时，请同时验证汉化版本和采集版本。

## 许可证

IDALingo 基于 [GNU Affero General Public License v3.0](../../LICENSE) 发布。再分发时必须保留许可证、[NOTICE](../../NOTICE) 中的版权声明以及对原作者的署名。

项目内置 [MinHook](https://github.com/TsudaKageyu/minhook)（BSD 2-Clause），详见 [third_party/README.md](../../third_party/README.md)。

IDA Pro 是 Hex-Rays SA 的商标。IDALingo 是独立项目，与 Hex-Rays 没有关联，也未获得其认可。
