# Architecture

IDALingo is split into portable translation code and small platform-specific hook layers.

## Shared components

`TranslationTable` loads UTF-8 or BOM-marked UTF-16 rules. Exact lookups handle Qt translation calls; an Aho-Corasick matcher handles the longer labels that contain paths, counts, or other runtime data. Longest matches win so short rules do not corrupt more specific strings.

`TextExporter` writes each observed UTF-8 or UTF-16 string once in the same syntax accepted by `language.txt`. The collector build calls it from the hook layer and always passes the original text to Qt.

## Windows

The Windows target is a 64-bit `winmm.dll` proxy. Its assembly stubs forward the exported multimedia functions to the system library. After Qt 6 is loaded, MinHook redirects selected text construction, translation, widget, and painter functions. The normal build performs replacement; the collector build only records inputs.

## Linux

The Linux target exports symbols matching selected Qt 6 functions through a linker version script. Each wrapper resolves the next implementation with `dlsym`, applies the shared translation or collection logic, and preserves the Qt return-value ABI. The launcher scopes `LD_PRELOAD` and `IDALINGO_LANGUAGE` to the child IDA process.

## Boundaries

- The patch does not modify IDA executables or Qt libraries.
- Hook symbol names and object layouts are version-sensitive and must be verified when IDA changes its Qt build.
- Plugin-owned text is intentionally outside the translation file unless the plugin explicitly chooses the same Qt path.
- The collector and translator use the same hook surface so collected samples correspond to strings the normal build can replace.
