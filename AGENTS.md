# GoldSrc-SDK Agent 规则

## Code Review

- 执行任何代码审查（含 PR review、diff review、`/code-review`）前，**先阅读 [docs/agent-review-quick.md](docs/agent-review-quick.md)**。
- 仅当 diff 命中该文件表格中的模块/API 时，再按需检索 [docs/review-gotchas.md](docs/review-gotchas.md) 对应章节；本地静态分析流程见 [docs/static-analysis-playbook.md](docs/static-analysis-playbook.md)。

## 构建

所有构建命令都在 `GoldSrc-SDK/` 子目录下执行，均为 32 位 x86，详细依赖见 [README.md](README.md#building)。不需要 SDK 工具时可在配置时加 `-DGOLDSRC_BUILD_UTILS=OFF`。

- **Windows**（CMake 4.2+、Visual Studio 2026 + MSVC v145、Windows SDK 10.0.26100.0）：

  ```bat
  cd GoldSrc-SDK
  cmake --preset windows-x86
  cmake --build --preset windows-x86-release      :: 或 windows-x86-debug；加 --target <name> 只构建单个目标
  ```

- **Linux**（CMake、Ninja、带 32 位 multilib 的 GCC；引擎和 `mdlviewer` 需要设置 `VCPKG_ROOT`）：

  ```bash
  cd GoldSrc-SDK
  export VCPKG_ROOT=~/vcpkg
  cmake --preset linux-x86-release      # 或 linux-x86-debug
  cmake --build --preset linux-x86-release
  ```

产物位于 `GoldSrc-SDK/bin/Win32/<config>/`、`GoldSrc-SDK/bin/Linux32/<config>/`，中间文件位于 `GoldSrc-SDK/bin/obj/cmake/<preset>/`。

## 单元测试

测试位于 [tests/](tests/)，由 [GoldSrc-SDK/cmake/GoldSrcTests.cmake](GoldSrc-SDK/cmake/GoldSrcTests.cmake) 注册到 CTest。Python 测试从生产源码中抽取函数，用宿主编译器单独编译运行，不依赖构建树；编译器由 [tests/cxx_toolchain.py](tests/cxx_toolchain.py) 选择：优先 `CXX`，否则 Windows 上通过 `vswhere` 找 MSVC，其它平台用 `c++`。

- **单独运行某个 Python 测试**（Windows / Linux 通用，在仓库根目录）：

  ```bash
  python -I tests/<name>_tests.py
  ```

- **Windows 通过 CTest 运行全部测试**（多配置生成器，需要 `-C`）：

  ```bat
  cd GoldSrc-SDK
  cmake --preset windows-x86 -DGOLDSRC_BUILD_TESTS=ON
  cmake --build --preset windows-x86-release
  ctest --test-dir bin/obj/cmake/windows-x86 -C Release --output-on-failure
  ```

- **Linux 通过 CTest 运行全部测试**：

  ```bash
  cd GoldSrc-SDK
  cmake --preset linux-x86-release -DGOLDSRC_BUILD_TESTS=ON
  cmake --build --preset linux-x86-release
  ctest --test-dir bin/obj/cmake/linux-x86-release --output-on-failure
  ```

用 `ctest ... -R <regex>` 只运行匹配的测试。新增 Python 测试时，在 `GoldSrcTests.cmake` 的列表中按字母序单独占一行登记。
