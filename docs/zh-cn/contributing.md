# 参与 IDALingo 开发

感谢参与改进 IDALingo。每次改动应保持单一目标，并确保两个受支持平台都容易验证。

## 开发流程

1. 为一项改动创建独立分支。
2. 使用 CMake 配置并编译。
3. 运行 CTest 测试。
4. 不提交生成文件和本地 IDA 数据库。
5. 在拉取请求中说明受影响的平台、IDA/Qt 版本和验证步骤。

## 代码改动

- 可移植的翻译逻辑放在 `src/common`。
- Windows 与 Linux 的 Hook 细节放在各自平台目录。
- 保持只采集版本不替换文本的行为。
- 修改解析、匹配、编码或 ABI 时添加回归测试：与平台无关的测试放在 `tests/unit`，Qt 内存布局检查放在 `tests/abi`。
- 不添加编译包装脚本，使用 `CMakePresets.json` 中的预设或直接使用根目录 CMake 工程。

## 翻译改动

- 修改 `translations/zh-CN/language.txt`，并保持 UTF-8 编码。
- 优先使用从当前界面采集到的完整原文。
- 保留占位符、HTML、助记键标记和换行。
- 不翻译快捷键、动作 ID、Qt 对象名或插件内部标识符。
- 检查宽泛规则是否会产生类似 `Data` 误替换 `Database` 的问题。

## 代码风格

代码格式由 `.clang-format` 统一定义，并在 CI 中检查，代码审查时不再讨论格式问题。提交前请使用 clang-format 20 格式化改动：

```sh
git ls-files -- '*.cpp' '*.hpp' ':!:third_party/**' | xargs clang-format -i
```

静态检查使用 `.clang-tidy` 中配置的规则。Linux 预设会导出 `compile_commands.json`，执行 `cmake --preset linux` 后即可运行：

```sh
clang-tidy -p build/linux src/common/translation.cpp
```

格式之外的约定：

- 使用 C++20，保持编译警告开启；CI 会将警告视为错误。
- 类型和函数使用 `PascalCase`，变量使用 `snake_case`，类的私有成员以 `_` 结尾，可变全局变量使用 `g_` 前缀，命名空间作用域的常量使用 `k` 前缀。
- 持有资源或作为作用域守卫的类，必须显式删除或定义拷贝操作。
- 除非接口本身与平台相关，公共头文件中不要引入平台依赖。
- 不要格式化 `third_party/` 中的代码。

## 许可证与署名

提交贡献即表示同意按 AGPL-3.0 发布该贡献。请保留现有版权声明、项目署名和第三方许可证文件。
