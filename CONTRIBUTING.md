# Contributing to IDALingo

Thanks for helping improve IDALingo. Keep changes focused and make them easy to verify on both supported platforms.

## Development workflow

1. Create a branch for one change.
2. Configure and build with CMake.
3. Run the CTest suite.
4. Keep generated files and local IDA databases out of the commit.
5. Explain the affected platform, IDA/Qt version, and verification steps in the pull request.

## Code changes

- Put portable translation logic in `src/common`.
- Keep Windows and Linux hook details in their platform directories.
- Preserve the collector's no-replacement behavior.
- Add a regression test for parsing, matching, encoding, or ABI changes: platform-independent tests go in `tests/unit`, Qt layout checks in `tests/abi`.
- Do not add build wrapper scripts; use the presets in `CMakePresets.json` or the root CMake project directly.

## Translation changes

- Edit `translations/zh-CN/language.txt` and keep it UTF-8.
- Prefer the complete source string captured from the current UI.
- Preserve placeholders, HTML, mnemonic markers, and line breaks.
- Do not translate shortcuts, action IDs, Qt object names, or plugin-owned identifiers.
- Check broad rules for accidental replacements such as `Data` inside `Database`.

## Style

Code style is defined by `.clang-format` and checked in CI, so formatting is not a matter for review. Format your changes with clang-format 20 before committing:

```sh
git ls-files -- '*.cpp' '*.hpp' ':!:third_party/**' | xargs clang-format -i
```

Static analysis uses the checks in `.clang-tidy`. The Linux preset exports `compile_commands.json`, so after `cmake --preset linux` you can run:

```sh
clang-tidy -p build/linux src/common/translation.cpp
```

Beyond the formatter:

- Use C++20 and keep compiler warnings enabled; CI builds with warnings as errors.
- Types and functions use `PascalCase`, variables `snake_case`, private class members a trailing `_`, mutable globals a `g_` prefix, and namespace-scope constants a `k` prefix.
- Classes that own resources or act as scope guards must delete or define their copy operations.
- Keep platform dependencies out of public headers unless the interface is explicitly platform-specific.
- Do not reformat `third_party/`.

## License and attribution

By contributing, you agree that your contribution is distributed under AGPL-3.0. Preserve existing copyright notices, the project attribution, and third-party license files.
