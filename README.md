<div align="center">

# IDALingo

**Runtime localization for IDA Pro, without touching a single IDA binary.**

[![CI](https://github.com/Youchuang-Studio/IDALingo/actions/workflows/ci.yml/badge.svg)](https://github.com/Youchuang-Studio/IDALingo/actions/workflows/ci.yml)
[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-blue.svg)](LICENSE)
![Platforms](https://img.shields.io/badge/platform-Windows%20x64%20%7C%20Linux-lightgrey.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)

English | [简体中文](docs/zh-cn/README.md)

</div>

---

IDALingo translates the IDA Pro user interface at runtime. It intercepts a small set of Qt 6 text functions inside the IDA process and rewrites strings according to a plain-text rule file. IDA's executables, Qt libraries, and installation directory stay untouched, so removing IDALingo restores the original interface immediately.

The project ships with a Simplified Chinese translation for IDA 9.4 (about 2,700 rules) and a collector build that records untranslated text, which keeps the rule file in step with new IDA releases.

## Features

- **Non-invasive.** Loads through a `winmm.dll` proxy on Windows and `LD_PRELOAD` on Linux. Nothing in the IDA installation is patched.
- **Plain-text rules.** Translations live in a single UTF-8 file that can be reviewed, diffed, and edited without rebuilding.
- **Exact and substring matching.** Short labels are matched exactly. Labels that embed paths, counts, or addresses go through an Aho-Corasick matcher that prefers the longest rule, so `Data` never rewrites `Database`.
- **Collector build.** A separate binary uses the same hooks but only records strings, producing a `language_export.txt` in rule format, ready for translation.
- **Safe defaults.** Keyboard shortcut strings are never rewritten, and a built-in rule set keeps the main menus localized even if the rule file is missing.

## Compatibility

| Platform | Mechanism | Toolchain | Status |
| --- | --- | --- | --- |
| Windows x64 | `winmm.dll` proxy + MinHook | Visual Studio 2022 | Supported |
| Linux x86-64 | `LD_PRELOAD` + launcher | GCC or Clang (C++20) | Supported |
| macOS | — | — | Not supported |

IDALingo requires an IDA build linked against Qt 6. The bundled rules are maintained for **IDA Pro 9.4**. Hooked symbols are tied to IDA's Qt build, so verify them before using a new IDA release (see [Architecture](docs/architecture.md)).

## Installation

Prebuilt packages are produced by [CI](https://github.com/Youchuang-Studio/IDALingo/actions/workflows/ci.yml) as the `IDALingo-Windows` and `IDALingo-Linux` artifacts. You can also [build from source](#building-from-source).

### Windows

1. Copy `winmm.dll` and `language.txt` into the directory that contains `ida.exe`.
2. Start IDA as usual.

To uninstall, delete the two files.

### Linux

1. Put `idalingo`, `libidalingo.so`, and `language.txt` in the same directory.
2. Start IDA through the launcher:

   ```sh
   ./idalingo [ida arguments...]
   ```

The launcher runs `ida` from `PATH` by default. Set `IDALINGO_IDA_EXECUTABLE` to use a specific binary. `LD_PRELOAD` and `IDALINGO_LANGUAGE` are set for the IDA process only, and your shell environment is not modified.

## Translation rules

The source of the bundled translation is [`translations/zh-CN/language.txt`](translations/zh-CN/language.txt). The build copies it next to the binaries as `language.txt`. Each line holds one rule:

```text
# Lines starting with '#' or ';' are comments.
L"Source text",L"Translated text",
L"&New",L"新建(&N)",
```

- Files may be UTF-8 or BOM-marked UTF-16. `\"`, `\\`, `\n`, `\r`, and `\t` escapes are recognized.
- Rules in the file override the built-in defaults.
- Keep mnemonics (`&`), placeholders, HTML, and line breaks intact.
- Do not translate shortcuts, action IDs, Qt object names, or plugin-owned identifiers. Plugin text belongs in the plugin's own localization.
- Prefer full strings captured from the UI over broad substring rules.

### Collecting untranslated text

1. Install the collector build instead of the normal one:
   - Windows: `export-only/winmm.dll`
   - Linux: `LD_PRELOAD=/path/to/libidalingo_export.so ida`
2. Use the parts of IDA you want to translate.
3. Read `language_export.txt` next to the collector library. On Windows, if that directory is read-only, the file is written to `%LOCALAPPDATA%\IDALingo` instead.
4. Translate the new entries, merge them into `translations/zh-CN/language.txt`, and switch back to the normal build.

## Building from source

Requirements: CMake 3.24 or newer, plus Visual Studio 2022 (x64) on Windows or a C++20 compiler on Linux.

```sh
cmake --preset windows        # or: linux
cmake --build --preset windows
ctest --preset windows
cmake --install build/windows --config Release --prefix package
```

| Output | Windows | Linux |
| --- | --- | --- |
| Translation build | `build/windows/out/Release/winmm.dll` | `build/linux/out/libidalingo.so`, `build/linux/out/idalingo` |
| Collector build | `build/windows/export-only/Release/winmm.dll` | `build/linux/export-only/libidalingo_export.so` |
| Rules | `build/windows/out/Release/language.txt` | `build/linux/out/language.txt` |

<details>
<summary>CMake options</summary>

| Option | Default | Description |
| --- | --- | --- |
| `IDALINGO_BUILD_EXPORTER` | `ON` | Build the collector variant. |
| `IDALINGO_WARNINGS_AS_ERRORS` | `OFF` | Treat compiler warnings as errors (enabled in CI). |
| `IDALINGO_LANGUAGE_FILE` | `translations/zh-CN/language.txt` | Rule file shipped as `language.txt`. |
| `IDALINGO_IDA_EXECUTABLE` | `ida` | Default program started by the Linux launcher. |
| `IDALINGO_QT_DIR` | *(empty)* | Linux only: directory with IDA's `libQt6Core.so.6`, which enables the Qt ABI tests. |

</details>

## Project layout

```text
include/idalingo/      Public headers of the shared code
src/common/            Translation engine and text collector
src/windows/           winmm.dll proxy and MinHook integration
src/linux/             LD_PRELOAD library, launcher, and linker script
tests/unit/            Platform-independent unit tests
tests/abi/             Qt ABI checks
translations/zh-CN/    Simplified Chinese rules
third_party/           Vendored dependencies (MinHook)
docs/                  Architecture notes and translated documentation
```

## Contributing

Bug reports, translation fixes, and support for new IDA releases are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request. Changes to hook code should be verified with both the normal and the collector build.

## License

IDALingo is released under the [GNU Affero General Public License v3.0](LICENSE). Redistributions must keep the license, the copyright notices in [NOTICE](NOTICE), and attribution to the original author.

IDALingo bundles [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause). See [third_party/README.md](third_party/README.md).

IDA Pro is a trademark of Hex-Rays SA. IDALingo is an independent project and is not affiliated with or endorsed by Hex-Rays.
