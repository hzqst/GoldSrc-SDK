# GoldSrc-SDK 自动化缺陷审查实施指南

> 目标仓库：<https://github.com/hzqst/GoldSrc-SDK>；资料：仓库 README、CMake 配置及已合并修复 PR。基于 2026-11-18 美西时间可见记录；第 3、4 节的 Cppcheck、clang-tidy、Clang Static Analyzer 已在 Windows 上实际运行过；**其余章节仍是部署方案，而非实际运行过的扫描结果**。
>
> 配套规则索引：[review-gotchas.md](review-gotchas.md)。Cppcheck、clang-tidy、Clang Static Analyzer 由仓库根目录的 [`tools/static_analysis.py`](../tools/static_analysis.py) 统一驱动（见第 3、4 节）；CodeQL、Sanitizer 部分仍是部署方案。

## 1. 哪类工具能找出哪些旧缺陷？

| 旧缺陷类型 / PR 示例 | Cppcheck / Clang SA | CodeQL/自定义规则 | Sanitizer / 专项测试 | 原因 |
| --- | --- | --- | --- | --- |
| 空指针、未初始化、简单分支越界（#113/#267/#416） | 高（取决于可见路径） | 高 | ASan/UBSan 高 | 局部路径和范围比较明确 |
| 整数溢出、窄化、数组边界（#112/#271/#401） | 中至高 | 中至高 | UBSan/ASan 高 | 非执行路径与跨函数公式仍可能漏检 |
| `new[]`/`delete` 配对错误（#275/#385） | 中 | 中至高 | ASan 高 | 跨函数/成员存储可能让分析器追踪失败；#275 的 MSVC `/analyze` 曾漏报 |
| `COM_LoadFile` 基址被解析游标覆盖（#94/#95/#394–396） | 低至中 | **自定义 API/数据流查询**中至高 | ASan/LSan + 解析测试高 | 必须知道 `COM_ParseFile` 返回非拥有型内部指针 |
| `KeyValues` 被 `MakeCopy` 后源对象泄漏（#389/#391–393） | 低（无模型） | 自定义所有权模型中 | LSan + 计数对象测试高 | `AddItem` 不是所有权转移 |
| 多控件共享 `ImageList`/注册回调导致 UAF（#436/#441/#443/#448–452） | 低 | 定制专用查询中 | 生命周期测试 + ASan 高 | 容器缓存、别名和销毁顺序跨多个对象 |
| 缓存移动/淘汰使旧指针失效（#108） | 低 | 自定义失效 API 模型中 | 注入缓存搬移的测试 + ASan 高 | 没有显式 `free`，静态分析器通常不了解 `Hunk_TempAlloc` 语义 |
| UTF-8 bytes 与 `wchar_t` count 混淆（#49/#305/#310/#415/#456/#457） | 低至中 | 特定 API 规则中 | C/UTF-8 locale + 中日韩文字测试高 | 容量的“单位”和编码约定属于语义信息 |
| console 命令注入（#335/#373/#377–381） | 低 | **定制源→Cbuf sink 数据流**中至高 | 模糊/边界测试高 | GoldSrc `Cbuf` 解析规则不是 shell escaping |
| 初始化/刷新/线程/滑动条状态顺序（#72/#87/#102/#403/#423） | 低 | 多为低 | 状态机测试、故障注入高 | 逻辑不变量和异步生命周期需要业务语义 |
| VGUI1 ABI/跨 controls module 的 `VPANEL`（#313/#413/#427/#431） | 低 | 定制规则中 | Win32 x86 链接 + 双模式回归高 | 不同模块解析、导出符号、vtable 兼容性无法由一般分析器保证 |

**这些“高/中/低”是针对可见代码路径的经验覆盖倾向，不是实测命中率。** 静态分析并不需要触发运行路径，但也可能存在误报；Sanitizer 在执行到问题路径时通常能给出确切定位，未执行的路径则不会覆盖。

## 2. 项目已有的基础设施

- 源码根目录是仓库内的 `GoldSrc-SDK/`；支持 `windows-x86`（MSVC、Win32）与 `linux-x86-debug/release`（Ninja/GCC、i386）。CMake 开关：`GOLDSRC_BUILD_TESTS=ON`；Linux preset 已输出 `compile_commands.json`。
- 仓库根目录的 `tests/*.py` 会提取生产 C++ 函数，用 `tests/cxx_toolchain.py` 选择编译器；尽可能启用 ASan/UBSan。**这并不等于完整引擎或所有 DLL 都已被 sanitizer 插桩**。
- CTest 清单在 `GoldSrc-SDK/cmake/GoldSrcTests.cmake`；新增测试必须注册（#429/#432），并验证 MSVC 与 GCC、`VGUI_ABI_COMPAT_MODE` 两种模式（若适用）。

## 3. 统一入口与 Cppcheck

三个工具都通过 `tools/static_analysis.py` 运行，在仓库根目录执行：

```bash
python -I tools/static_analysis.py all --changed          # 相对 main 改动过（含未提交、未跟踪）的源文件
python -I tools/static_analysis.py all --files GoldSrc-SDK/GameUI/SaveGameDialog.cpp
python -I tools/static_analysis.py cppcheck               # 全部项目源文件；--exhaustive 对应 --check-level=exhaustive
python -I tools/static_analysis.py analyzer -j 8          # 子命令：cppcheck / clang-tidy / analyzer / all
```

- **编译数据库**：Windows 用 `windows-x86-analyze` preset（Ninja + MSVC x86，只 configure 不构建，`compile_commands.json` 在 configure 时生成），Linux 用 `linux-x86-debug`；`--preset` 可改。数据库不存在时脚本自动 configure；Windows 下通过 vswhere 进入 vcvarsall **x86** 环境（复用 `tests/cxx_toolchain.py` 的 `vcvars_environment`），不需要开发者命令行。
- **分析范围**：去掉 `external/`、`lib/`、构建目录内生成的源文件和 `.rc`，同一源文件被多个模块编译时只保留第一条（例如 `dlls/` 被多个 mod 共享，只按第一个模块的宏分析一次）；结果写到 `GoldSrc-SDK/bin/static-analysis/compile_commands.json`，三个工具共用。`--changed` 选中的头文件只提示、不展开成包含它的源文件。
- **只出报告**：报告位于 `GoldSrc-SDK/bin/static-analysis/`（`cppcheck.xml` / `cppcheck.txt`、`clang-tidy.txt`、`clang-analyzer.txt`）；退出码只在工具缺失或崩溃时非零，告警数量不影响退出码。尚无基线比较，判断「本次 PR 新增」时请对比改动前后的报告。
- **Cppcheck 参数**：`--enable=warning,performance,portability`、`--platform=win32A`（Windows）/ `unix32`（Linux）、`--library=windows` / `posix` 加 [`docs/cppcheck-goldsrc.cfg`](cppcheck-goldsrc.cfg)、`--inline-suppr`、`--cppcheck-build-dir` 增量缓存，第三方目录的告警被抑制。`--cppcheck <path>` 指定可执行文件。
- **全局抑制**（`CPPCHECK_SUPPRESSED_IDS`）：`uninitMemberVarNoCtor`（C 结构体没有构造函数）、`duplInheritedMember`（每个实体类都有自己的 `m_SaveData`）、`dangerousTypeCast`（C 风格转换）、`passedByValue`（按值传 `Vector` 是游戏导出/虚函数接口，改成引用会破坏 ABI）、`ctuOneDefinitionRuleViolation`（各游戏 DLL 各有自己的 `Vector` 等同名类，放在同一个编译数据库里会被误判为违反 ODR）。在 4 个代表性文件上，这几项占了 649 条告警中的 635 条。
- `docs/cppcheck-goldsrc.cfg` 是**实验性的直接 API 所有权模型**，不能假定它理解 `gEngfuncs.COM_LoadFile` 这样的函数指针成员，更不能误把 `COM_ParseFile` 的 cursor 当成新的分配。已知误报：`COM_LoadCacheFile` 中 `LOADFILE_CACHE` 模式的 `COM_LoadFile` 返回值由 `loadcache` 接管，会被报 `leakReturnValNotUsed`。
- Linux/Windows 的源文件集合不完全一致，不能只查一个平台。
- 对比历史回归：在单独工作树中分析代表性 PR 的合并前/合并后版本，看相同命令能否**准确**发现旧问题；不要根据工具宣传推测检测率。
- 版本：已在 Cppcheck 2.22.0 官方安装包上验证。Strawberry Perl 自带的 `cppcheck.exe` 2.14.0 找不到自己的 `std.cfg`，补齐后在本项目源文件上也会崩溃（0xC0000005），不要使用。

官方资料：<https://cppcheck.sourceforge.io/manual.html>、<https://cppcheck.sourceforge.io/reference-cfg-format.pdf>。

## 4. clang-tidy / Clang Static Analyzer

- `clang-tidy` 子命令读取 [`GoldSrc-SDK/.clang-tidy`](../GoldSrc-SDK/.clang-tidy)：`bugprone-*`、`performance-*`、`portability-*`、`cert-err34-c`、`misc-redundant-expression`，并关闭在 C 风格老代码上几乎全是噪声的检查；`performance-enum-size` 也被关闭，因为它建议改枚举大小，会破坏导出结构体的 ABI。
- `analyzer` 子命令就是 Clang Static Analyzer：同一套 clang-tidy 调用，`--checks` 覆盖为 `clang-analyzer-*`（去掉 `osx` 和 `security.insecureAPI` 中针对 strcpy/sprintf 的告警）。这样 MSVC 与 GCC 两种编译数据库都能直接使用，不必把 `cl` 命令行翻译给 `clang --analyze` / `scan-build`。
- Windows 下脚本给 clang-tidy 加 `--target=i686-pc-windows-msvc`，否则 clang 按 64 位解析 MSVC 命令行，指针大小和 `sizeof` 断言都会出错；还会加 `-Wno-error`（命令行里的 `/WX`、`-Werror` 会把编译警告变成错误）和 `-Wno-overriding-option`（引擎在默认的 `/fp:precise` 后面又加了 `/fp:strict`）。
- clang-tidy 默认用 Visual Studio「C++ Clang tools」自带的版本（`VC/Tools/Llvm`），否则用 PATH 中的；`--clang-tidy <path>` 可指定。PATH 中的 llvm-msvc 分支版本在本项目上会崩溃。
- `HeaderFilterRegex` 是 `.*`（这个 clang-tidy 版本没有排除型过滤），第三方头文件里的诊断由脚本按路径丢弃。
- 已知盲区：Windows 全量 2156 个源文件中，有 91 个出现 `clang-diagnostic-error`（55 个在 `worldcraft/`，clang 不接受 MFC 消息映射里不带类名的成员函数写法；其余主要依赖 MSVC 允许的 `const char*` → `char*` 和临时对象绑定到非 const 引用）。这些文件的 clang-tidy / analyzer 结果不完整，需结合 Cppcheck 与 Linux 编译数据库的结果。
- 优先关注 `clang-analyzer-core`、`clang-analyzer-cplusplus.NewDelete` / `NewDeleteLeaks`、`clang-analyzer-unix.Malloc`、`bugprone-*`；脚本从不使用 `-fix`，不要手动对 VGUI1 ABI 相关代码使用 `-fix` 自动修改。对于能直接声明的自定义分配函数，可研究 Clang `ownership_returns/takes` 注解；对导出头/跨编译器接口，**不可在没有 ABI 评估时直接加不兼容语法**。
- 实测（Windows 全部 2156 个源文件，`-j` 默认取 CPU 核数）：Cppcheck 约 2.5 分钟、7219 条；clang-tidy 约 6 分钟、8481 条；analyzer 约 4 分钟、1970 条（多数为 `deadcode.DeadStores` 和 `optin.cplusplus.VirtualCall`，`core.NullDereference` 91 条）。尚无基线，这些数字只用来判断量级。

官方资料：<https://clang.llvm.org/docs/analyzer/checkers.html>、<https://clang.llvm.org/extra/clang-tidy/>。

## 5. CodeQL：跨函数扫描与专用规则

```bash
# Linux x86 的 preset/依赖配置完成，并确保是真正重新编译后：
cd GoldSrc-SDK
codeql database create ../codeql-db-linux --language=cpp --source-root=. \
  --command 'cmake --build --preset linux-x86-debug --clean-first'
codeql database analyze ../codeql-db-linux \
  'codeql/cpp-queries:codeql-suites/cpp-security-and-quality.qls' \
  --format=sarif-latest --output=../codeql-linux.sarif
```

CodeQL C/C++ 支持无构建扫描，但这个项目**推荐 manual build** 以保留真实预处理宏、include、平台目标；Windows x86 需要单独构建/分析。CodeQL 内置 UAF/double-free/mismatched allocator/越界/字符串规则；自定义 query 应逐步建模：

1. **source/cursor/sink**：`COM_LoadFile`（原始持有）、`COM_ParseFile`（借用并前进）、`COM_FreeFile`（必须传原始基址）；查 cursor 被释放或原始基址丢失。
2. **copy-vs-transfer**：`ListPanel::AddItem`、`ComboBox::AddItem`、`MenuItem::SetUserData` 的 `KeyValues` 参数 **MakeCopy、不接管**；调用者若 `new KeyValues` 且没有释放源对象，标为待查。
3. **invalidation**：`Hunk_TempAlloc`、cache 淘汰、`SetImageList` 替换、`DeleteAllItems` 后，标出旧指针/旧 index 的后续使用。
4. **taint/sink**：来自配置、文件名、服务器、用户输入的字符串流向 `pfnClientCmd`/`Cbuf`；要求 quoted/unquoted 参数验证和**未截断**的安全格式化。
5. **size units**：`wchar_t` 数量传入字节容量、`wcslen` 用作 UTF-8 byte capacity、`wcstombs` 与项目特定 UTF-8 converter 混搭。

只有将真实的生产 API 契约建模后，这些规则才有意义。官方资料：<https://docs.github.com/en/code-security/how-tos/find-and-fix-code-vulnerabilities/manage-your-configuration/codeql-for-compiled-languages>、<https://docs.github.com/en/code-security/reference/code-scanning/codeql/codeql-queries/c-cpp-built-in-queries>。

## 6. 动态验证：已有 CTest、Sanitizer 与故障注入

```bash
# 在仓库根目录；保证完成测试目标构建并确认测试被 CTest 列出
cd GoldSrc-SDK
cmake --preset linux-x86-debug -DGOLDSRC_BUILD_TESTS=ON
cmake --build --preset linux-x86-debug
ctest --test-dir bin/obj/cmake/linux-x86-debug -N

# Linux/Clang 或 GCC 有合适的 sanitizer runtime 时
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
ctest --test-dir bin/obj/cmake/linux-x86-debug --output-on-failure
```

对这种旧引擎，优先新增**故障注入/边界回归**：失败的 `Hunk_TempAlloc`、`COM_ParseFile` 到 EOF、同一 `SetImageList` 再赋值、删除 child 后再次回调、`SetUserData(self)`、零大小 destination、32-bit `wchar_t` vs 16-bit、`LC_CTYPE=C` 和 `C.UTF-8`、UTF-8 非 ASCII、WAV/BSP/VOX 截断文件、重复 `Init/Reset/Shutdown`、`SetImage` 外部替换、线程退出交错。重点用 MSVC/Win32 + Linux/GCC 两种环境，检查真正的 ctest 注册、编译和运行；一部分历史测试只抽取生产函数，不是整游戏测试。

- ASan：已执行路径的 heap/stack/global OOB、UAF、double/invalid free；LSan：可达执行路径的泄漏；UBSan：signed overflow、null/misaligned、部分边界。**单独 UBSan `undefined` 不覆盖全部 float-divide-by-zero 或所有隐式窄化**，必要时显式加检查项。
- 对 Windows，MSVC 支持 ASan，但不要把其功能等同于 Linux 下 ASan+LSan+UBSan；跨模块部分未插桩时仍可漏报。
- 不建议将 ThreadSanitizer 作为默认 Win32 x86 CI 必选项：先确认平台、编译器和依赖是否支持，再在可运行的目标上部署。

官方资料：<https://clang.llvm.org/docs/AddressSanitizer.html>、<https://clang.llvm.org/docs/LeakSanitizer.html>、<https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html>。

## 7. 你的新问题：释放后清 `nullptr` 的准则

| 场景 | 应做什么 |
| --- | --- |
| 可重复调用的 `Close/Reset/Shutdown`，成员 owning pointer | 清空成员，释放后保持一致的“无资源”状态；可按重入要求采用 `old = std::exchange(member, nullptr); delete old;` |
| 成员被别的 Panel、callback、GDI DC、worker、list item 观察 | 先解除/取消观察、注销回调、确保线程停止，之后释放；**仅将成员清空无法保护别名**（#302/#441/#443） |
| `SetX(new)` 允许 `new == old` 或 `new` 引用 `old` 内部数据 | 先判断同指针；有自引用可能时先复制新对象，再释放旧对象（#402/#441） |
| 局部变量 `delete p` 后立即 return / 离开作用域 | 通常没必要清空，优先 RAII |
| 析构函数释放成员，整对象马上结束生命周期 | 通常不必清空；但重入、回调或成员析构依赖时要正确排序 |
| `malloc` / `new[]` / 引擎自定义资源句柄 | 先用**正确的释放函数**（不能以 `nullptr` 掩盖 `delete`/`delete[]` 不匹配），句柄重置为对应的无效哨兵，而非一律 `nullptr` |

对新代码，优先 RAII（`unique_ptr`/自定义 deleter、`vector`、局部 guard）；但 VGUI1 的 ABI/export/vtable 可能不允许直接更改对象布局，先考虑**仅更改实现而不改公共 ABI**的方式。不要搞“全仓自动把每个 `delete` 后面加 `=nullptr`”的机械重构。

## 8. PR 审查工作流

1. **PR 触发**：Linux compile database 的 Cppcheck + clang-tidy 检查修改文件；另在 Win32 定期跑完整构建。先记录基线，只有新增且可复现/高置信告警才能阻挡合并。
2. **Nightly/定期**：两个平台的 CodeQL、Cppcheck exhaustive、sanitizer regression、`GOLDSRC_VGUI_ABI_COMPAT_MODE=ON/OFF` 的适用构建，以及已注册测试清单审计。
3. **Agent Review**：默认只读 [`agent-review-quick.md`](agent-review-quick.md)，根据文件和 API 选择性读取 [`review-gotchas.md`](review-gotchas.md) 对应小节；查看引用 PR 的原始 diff/测试，检查是否出现同源缺陷。
4. **报告最小化**：每个真实发现给 `文件:行/函数、触发条件、可执行路径、后果、证据、相似规则 ID+PR、推荐回归测试`；**没有证据的猜测仅列“需确认”**，不要重复报告已修复的历史问题。
5. **历史回放评估**：随机选 20–30 个已修复 PR，使用干净的 `parent`/`merge` 工作树分别运行工具；分别统计真实命中/误报与丢失的 macro/platform 配置，才决定是否将其设为 CI gate。

> 注：第 3、4 节已在 Windows（Visual Studio 2022 MSVC x86、Cppcheck 2.22.0、Visual Studio 自带 clang-tidy 19.1.5）上对全部项目源文件实际运行；Linux 编译数据库上的运行、CodeQL 与 sanitizer 部分尚未在本机验证，需在具备工具链/第三方依赖的工作机或 CI 中验证。
