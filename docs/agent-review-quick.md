# GoldSrc-SDK Agent Review 入口（极简）

> 每次审查只加载本文件；**仅在 diff 命中相关模块/API 时**检索 [review-gotchas.md](review-gotchas.md) 的对应章节。历史 PR 是“已修复案例”，不得未经验证当作现存 bug。

| Diff 包含 | 重点查的历史坑 | 查阅规则 |
| --- | --- | --- |
| `COM_LoadFile` / `COM_ParseFile` / `COM_FreeFile` | 原始分配基址≠解析游标；early return 泄漏/内部指针非法 free | OWN-01, OWN-05 |
| `KeyValues` / `MakeCopy` / `SetUserData` / `AddItem` | 复制不接管、置 NULL 后旧指针未清、自传入 UAF | OWN-02/03/04 |
| `SetImageList` / `ImageList` / `SetImage` / `TextImage` | 重复设置、自赋值、图像别名、替换前未解除引用、texture 泄漏 | OWN-07..13 |
| `delete` / `free` / `Reset` / `Close` | 配对、每条退出路径、重复关闭；清 nullptr 不保护其它别名 | OWN-05/06/15, LIFE-12 |
| `Panel` / `FocusNavGroup` / `ButtonGroup` / `BuildGroup` | child/group 双向登记、析构/替换时取消 callback、拷贝后浅指针 | LIFE-01..05, OWN-16..18 |
| worker / Steam query / Cache / `Hunk_TempAlloc` | worker join 先于释放、取消异步回调、cache 搬移后旧地址失效 | LIFE-06..09 |
| `snprintf` / `strncpy` / `wchar_t` / UTF-8 | 是否保留 NUL、buffer size 是字节还是元素、截断是否在 UTF-8 边界 | BUF-01..06, UTF-01..12 |
| 文件/音频/图像 parser、array index、字体 | 截断数据、整数溢出、负 index、zero size、`[LIMIT]` 越界 | BUF-07..18, IDX-01..07 |
| `pfnClientCmd` / console / `Cbuf` | `;`、换行、引号可注入；加引号不等于安全逃逸 | CMD-01..09 |
| Menu / scroll / popup / Career reload | 行号≠ID、状态未 reset、依赖顺序、viewport 高度/缩放条件 | STATE-01..15 |
| `VPANEL` / VGUI / export / DLL / tests | 跨 controls-module NULL、Win32 ABI、测试没注册或 MSVC 链接没过 | ABI-01..08 |

**报告格式**：`[severity] path:line/function — 可触发条件 → 具体后果；对应历史规则/PR；最小复现或回归测试。` 只报告本次改动实际引入/重新暴露的问题；若旧问题已修复，标为“有相似历史但当前未见复现”。

**本地工具流程**：参见 [static-analysis-playbook.md](static-analysis-playbook.md)。
