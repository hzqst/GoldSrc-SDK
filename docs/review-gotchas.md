# GoldSrc-SDK 已修复缺陷模式速查（Agent Review）

> 基于 `hzqst/GoldSrc-SDK` 的 PR 历史整理（截至 2026-11-18 美西时间）；**这是已修复问题的回归审查索引，不代表当前代码仍有这些 bug**。每条只保留触发方式→后果→检查约束，按模式去重；`PR#N` 指 `https://github.com/hzqst/GoldSrc-SDK/pull/N`，可查看对应 issue、diff 和测试。
>
> **Agent 使用方法**：先按本次 diff 涉及的 API/模块查下面的关键词或分类；只加载相关小节，再点开 1–3 个最相似的 PR 看原始修复/测试。对新风险报告须给出具体调用路径、可触发条件和证据；不要把历史 bug 当成仍未修复的发现。

## OWN — 所有权、泄漏和释放（ownership / leak / double-free）

- **OWN-01** `COM_LoadFile` 的返回基址与 `COM_ParseFile` 的移动游标必须分开；释放 EOF 的 NULL 游标会泄漏，释放内部地址会非法 free。〔PR#94/95/394/395/396〕
- **OWN-02** `ListPanel::AddItem`、`ComboBox::AddItem/UpdateItem` 和菜单 `SetUserData` 会 `MakeCopy()` `KeyValues`；调用方仍须 `deleteThis()` 释放源对象。〔PR#389/391/392/393〕
- **OWN-03** `MenuItem::SetUserData(NULL)` 要将成员清空；传入自身旧数据时须先复制、再释放，否则 UAF/二次释放。〔PR#402〕
- **OWN-04** `StoreSettings()` 返回新建 `KeyValues`，`ApplySettings()` 只借用；成功应用后应释放临时树，不能假定调用会接管所有权。〔PR#204〕
- **OWN-05** 每个 early return 都要回收已取得的文件、模块、heap buffer/handle；尤其 WAV 加载失败、TGA/logo 读取失败和插件接口查找失败。〔PR#65/96/398/400〕
- **OWN-06** 分配/释放族要匹配：`new[]`↔`delete[]`、`strdup`↔`free`、`malloc`↔`free`；不可用 scalar `delete` 释放数组。〔PR#275/286/385〕
- **OWN-07** `ImageList` 自己生成的 `BlankImage` 是独立所有权，不可因 slot 0 被替换而丢失，也不可删除别名两次。〔PR#93〕
- **OWN-08** `SectionedListPanel::SetImageList(list, own)` 要兑现 `own`：重置引用旧图的 item 后才释放旧 list；再次设置同一指针只更新所有权。〔PR#436/442〕
- **OWN-09** `ListPanel::SetImageList` 收到与成员相同的指针不可先删除；换 list 时还要重映射/清除列头缓存的旧 `IImage*`。〔PR#441〕
- **OWN-10** Scoreboard 更换 `ImageList` 应先让使用者切换，再释放旧 list；保持销毁顺序以免 item/header 短暂持有悬空图。〔PR#443〕
- **OWN-11** `CAvatarImage` 拥有的 texture 必须在重建和析构时释放；`ImageList(false)` 不释放其 image，不能把它当 owning 容器。〔PR#446〕
- **OWN-12** `CAvatarImagePanel` 创建的 avatar 要由自己释放；外部 `SetImage`/scheme 可能换成其他 `IImage`，不能对 `GetImage()` 强转并修改。〔PR#449〕
- **OWN-13** `Label::ClearImages()` 不负责删除外部分配的 `TextImage`；`SectionedListPanel` 的列头、回收 item、重排列均须管理自身 image。〔PR#428/434〕
- **OWN-14** `PanelListPanel::RemoveAll` 与 `DeleteAllItems` 所有权不同：前者应清 `DATAITEM` 记录，但不能顺便删除由上层负责的 child Panel。〔PR#205〕
- **OWN-15** `ApplySettings`/`Init`/`SetName` 等可重复调用的覆盖操作，要先释放旧成员或 `Reset` 全部状态（含容器）；不能只在析构时清理。〔PR#203/307/386/388〕
- **OWN-16** VGUI1 `Dar`、`String` 等原始指针拥有者增加析构时必须同步补深拷贝/赋值和自赋值处理，否则由“泄漏”变“double-free”。〔PR#288/433〕
- **OWN-17** `SurfaceBase` 拷贝要独立拥有 mode-name 字符串；`Surface` 拷贝不可共享 window/DC、root panel 和 cursor，也不可冒充已注册实例做 App 清理。〔PR#435/444〕
- **OWN-18** `Scheme`、`BuildGroup`、`Bitmap`/`Cursor`、`Font`、`App` 中创建的子资源必须有明确释放方；VGUI1 的拥有权修复依赖 ABI 兼容开关。〔PR#336/359/362/366/368〕
- **OWN-19** Win32 GDI bitmap 被 DC 选中时 `DeleteObject` 可失败；先选走旧对象、再删除，替换 DC/窗口时还要正确 `DeleteDC`/`ReleaseDC`/`DestroyWindow`。〔PR#293/296/301〕
- **OWN-20** `fclose` 后即使返回错误也须立即将保存的 `FILE*` 失效；否则重复 close/后续 seek 可能操作已复用的文件槽。〔PR#276〕
- **OWN-21** VGUI1 `Panel` tooltip、共享 TextEntry、child、border 和 signal 需要逐个明确 owning/borrowed 关系，避免析构竞态和双删。〔PR#92/313/314/317〕

## LIFE — UAF、回调、注册关系和时序（lifetime / callback / async）

- **LIFE-01** `ScrollBar::SetSlider/SetButton` 替换 owned part 前要移除旧信号、解绑/延迟销毁；拒绝 NULL/越界 index/同一指针，复制对象不能误删原部件。〔PR#202/455〕
- **LIFE-02** `FocusNavGroup`/`ButtonGroup`/`BuildGroup` 的双向注册须在换组、析构、复制/赋值时同步；旧 list 残留已删对象会 UAF。〔PR#448/450/452〕
- **LIFE-03** `removeChild/removeAllChildren` 要把整棵子树的 surface 关系解绑；surface 与 root 任一先销毁时另一个不得再引用它。〔PR#325/331〕
- **LIFE-04** `App` 保存的 focus、mouse capture、arena、tick 和当前 surface 状态不得在相关 Panel/Surface 销毁后继续使用。〔PR#318/319/327〕
- **LIFE-05** `TablePanel` 向 header 注册的 handler，在换 header/销毁时必须反注册；不能让 header 回调已删除 TablePanel。〔PR#324〕
- **LIFE-06** Steam matchmaking 独立 ping/list 查询返回的 handle 要跟踪；callback 接收者析构前 cancel，失败回调也清 handle。〔PR#302/303〕
- **LIFE-07** 网络/CD 音频线程停止须“发布停止请求→唤醒→join→释放同步对象/缓存/设备”；不能在线程仍运行时先释放或在锁内 join。〔PR#72/102〕
- **LIFE-08** `Hunk_TempAlloc` 可能移动/淘汰 `sfxcache_t`；调用后重新 `Cache_Check` 并刷新所有持有者，缓存失效时结束声道。〔PR#108〕
- **LIFE-09** WAV 流换块分配失败后须清旧文件 handle/长度、停止使用空缓存、避免重复 close，失败状态不可伪装成功。〔PR#109〕
- **LIFE-10** `Sys_LoadModule` 成功不保证 `Sys_GetFactory`/所请求接口可用；先判空，失败立即 unload 模块。〔PR#398〕
- **LIFE-11** 已拥有的 GDI texture 重新上传或 DC 复用必须保证旧 bitmap 已取消选中，且新建失败时原可用资源不被提前销毁。〔PR#293/296〕
- **LIFE-12** `delete/free` 后清空成员指针可保证状态可重入/防止本成员重复释放，但不会清空其他别名；需先断开控件、回调和注册关系，必要时 `std::exchange` 后销毁。〔PR#276/402/441/443〕

## BUF — 边界、长度、类型和内存安全（bounds / arithmetic）

- **BUF-01** `sprintf/vsprintf/strcat` 向固定数组写可变字符串要改为有界 API，并检查截断；输出不完整时应拒绝而非继续当完整命令。〔PR#265/266/335/360〕
- **BUF-02** MSVC 旧 `_snprintf/_vsnprintf` 在截断时可能不加 NUL；不能只看 `sizeof(buffer)`，还须检查负返回值/目标容量并显式终止。〔PR#363/376/418〕
- **BUF-03** `strncpy/wcsncpy(dst, src, capacity)` 恰好写满不会终止，随后 `%s`/Win32 API 会越界读；所有调用应保留终止符位。〔PR#373/411/412〕
- **BUF-04** 所有 `(dst, int capacity)` 字符串 API 要先检查 `dst != NULL && capacity > 0`；`capacity-1` 不能在 0/负数时写 `dst[-1]` 或变成巨大 `size_t`。〔PR#268/416/457〕
- **BUF-05** API 形参“buffer size”可能是 **字节数** 而非 `wchar_t` 元素数；传 `sizeof(wbuf)` 或 `ARRAYSIZE(wbuf)` 前先读接口契约。〔PR#415〕
- **BUF-06** `wcslen`/buffer length 不可窄化为 `short` 再参与 `+1`/分配；32767 边界可整数溢出、随后 `wcscpy` 覆盖堆。〔PR#401〕
- **BUF-07** 在 `width * height * bytes` 或 output length 乘法前先做除法式/宽整数上界检查；“乘完再比较”已经可能有符号溢出。〔PR#112/271〕
- **BUF-08** 带符号 `char` 传 glyph index 会负下标；256 个 glyph 的有效索引仅 0..255，clamp 到 256 同样越界。〔PR#279/280〕
- **BUF-09** 多维固定数组的每行有独立边界；不能把一行末端当作连续跨行内存访问（即使总对象足够大）。〔PR#137〕
- **BUF-10** 字体 atlas/page 的 slot 全部初始化，`pageCount == LIMIT` 前要退出，不能写 `bindIndex[LIMIT]` 污染相邻坐标。〔PR#281〕
- **BUF-11** 宽/高为 0、负值、反向矩形不得进入倒数/UV 插值计算；避免除零、NaN、越界纹理范围。〔PR#143〕
- **BUF-12** 文件读取/格式解析必须在做 `ptr+offset` 前检查完整 header/chunk/表范围，并处理截断、零大小和奇数 padding。〔PR#64/89/140/370〕
- **BUF-13** `GetText(offset, capacity)` 的循环应按**目标索引**计数、检查负 offset；分块读取推进量应等于实际复制字符数而非数组容量。〔PR#270/382〕
- **BUF-14** 重采样要以完整输入 sample 为边界（16-bit 不能读半个 sample），分配和填充共用同一 step/len/byte 公式。〔PR#98/112〕
- **BUF-15** `fgetc` 结果先存 `int` 再与 `EOF` 比较；先截成 `char` 会把 0xFF 与 EOF 混淆。〔PR#77〕
- **BUF-16** float→int 在 NaN/Inf/越界时不能无保护转换；跨 Win32/Linux 的 `Q_ftol` 应统一边界和回退语义。〔PR#285〕
- **BUF-17** `Hunk_TempAlloc` 会返回 NULL 而非必然 fatal；读入模型/音频和 `memcpy` 之前必须判空并走失败路径。〔PR#113〕
- **BUF-18** 即使下游 `snprintf` 保证 NUL，**截断本身**也会使两个不同 token 映射成同一个键，或切开 UTF-8；超长 `%...%` Label token 应拒绝注册。〔PR#418〕

## UTF — UTF-8 / Unicode / locale / 字符长度

- **UTF-01** `wcslen` 数的是宽字符而非 UTF-8 字节；ComboBox 宽字符 caption 的输出缓冲区应先查询编码后所需字节数。〔PR#49/456〕
- **UTF-02** `ConvertUnicodeToANSI` 写 UTF-8 时必须与“查询长度”使用**同一个转换器**，不能用 locale 相关 `wcstombs` 估算。〔PR#305/456〕
- **UTF-03** UTF-8→`wchar_t` 要检查非法序列、截断和 NUL 预算；Windows 与 POSIX 的转换返回值/`wchar_t` 宽度并不天然一致。〔PR#310〕
- **UTF-04** POSIX SDL clipboard 是 UTF-8，但 `mbstowcs/wcstombs` 取决于 `LC_CTYPE`；小 buffer/截断应按完整码点处理，不能写负索引。〔PR#457〕
- **UTF-05** UTF-8 字节预算包含后缀和 NUL；截断 hostname、路径、姓名时只能在完整字符边界结束，不能退化为“丢弃所有非 ASCII”。〔PR#52〕
- **UTF-06** Career UI 不可把 UTF-8 直接交给 locale 依赖的宽字符格式化，也不能把完整姓名压缩到固定 31-byte buffer。〔PR#50〕
- **UTF-07** Career profile 标识不能靠 31 字节 ASCII 归一化，易碰撞/覆盖别人的档案；校验完整 UTF-8 身份，编码成安全唯一存储键。〔PR#53/371〕
- **UTF-08** `Label::GetSettings` 回存文本时需按实际 UTF-8 最坏扩展留空间，截掉一个 `%` 或多字节字符会破坏往返和变量识别。〔PR#421〕
- **UTF-09** `TextImage::SetText` 从 `#token` 切到普通文本时要清旧 localization symbol；窄/宽重载默认是否清除须符合调用场景。〔PR#425〕
- **UTF-10** 分块宽字符→UTF-8 输出要以本次真实读取长度推进 offset，输出 buffer 按码点最坏长度规划，并处理分块 UTF-16 surrogate。〔PR#382〕
- **UTF-11** locale 日期输出如 `strftime` 不可假设为 UTF-8；使用 `wcsftime` + `SetWString` 等正确宽字符路径。〔PR#334〕
- **UTF-12** `toupper/tolower` 不可逐字节套在任意 UTF-8 上；只对 ASCII token 按 ASCII 规则处理，其他字节保留/走 Unicode 规则。〔PR#332/363〕

## CMD — 命令构造、外部输入和解析（injection / parsing）

- **CMD-01** GoldSrc `Cbuf` 会把 `;`（引号外）和 CR/LF 分成新命令；字符串可含 `"`，**单纯加双引号不能安全转义**。〔PR#335/373/378/379/380/381〕
- **CMD-02** 构造 console 命令时区分 quoted/unquoted 参数的禁用字符，拒绝危险/过长输入而不是静默截断；playlist 路径、bot/map/task/password 都要审。〔PR#377/378/379/380/381〕
- **CMD-03** `Career` profile 名作为 `writecfg` 参数时，还要验证完整 UTF-8、控制字符、反斜线、引号、长度和存储 key 可逆性。〔PR#53/371〕
- **CMD-04** `CheckParm/RemoveParm` 需匹配完整且可识别引号的 token；不能把 quoted path 的 `-`、负数或参数名的子串当新参数。〔PR#73/78〕
- **CMD-05** CommandLine `Create` 若输入引用当前 `GetCmdLine()` 或内部偏移，须先复制到独立持有的存储，再释放旧数据。〔PR#75〕
- **CMD-06** Query 参数的返回 `char**` 生命周期必须明确；不要用固定 128 byte 临时值截断用户输入，也不能返回已重建的旧指针。〔PR#74〕
- **CMD-07** 非 Windows argv 重建不能用固定栈 buffer 拼所有参数；需支持空参数、带空白参数、相邻引号和超长输入。〔PR#76/78〕
- **CMD-08** 从文件/配置读字符串供 parser 使用时须确认 NUL 或使用显式长度；`kb_def.lst`、WAV、SAVE token 和音频句子均有相关回归。〔PR#89/135/136/370/376〕
- **CMD-09** HPAK 操作必须使用调用者指定的 package，而不是误用上传资源名或全局默认；形参相似不代表同一资源。〔PR#147〕

## STATE — 状态同步、顺序、布局和业务语义

- **STATE-01** `Menu::DeleteAllItems` 要同时重置 active/highlight ID；`ComboBox` 遍历须区分 menu ID 与 row，并以 item count 限制搜索。〔PR#403〕
- **STATE-02** TextEntry 多行高度小于一行时不能让 `displayLines=0` 产生 `lineBreaks[Count()]`；PageUp/Down 也应共用同一个“至少一行”定义。〔PR#426/451〕
- **STATE-03** RichText 在正高度但不足一行时，滚动窗口仍至少为 1；否则布局收缩/恢复可能把当前阅读位置跳到底部。〔PR#423〕
- **STATE-04** 仅在构造时缓存的 scrollbar/margin metrics，DPI/缩放/字体/屏幕尺寸变化后会过期；布局阶段应重新计算。〔PR#197/198/199/200/201〕
- **STATE-05** `EditablePanel` 缩放和裁剪普通 child 时，不应套用父坐标系去改 popup 的独立屏幕空间；modal 不能盖住自己的 descendant popup。〔PR#57/60〕
- **STATE-06** 应区分 UI 的 pending 值与已持久化/已提交值；写入失败、Cancel、重开或 Reset 不应错误地更新已保存状态。〔PR#83〕
- **STATE-07** Frame 默认 Escape/Close/Cancel 可能触发“完成操作”的回调；Career RoundEnd/MatchEnd 等业务模态框应显式区分取消与完成。〔PR#55/56〕
- **STATE-08** Career 普通 restart 前游戏规则已销毁；依赖生命周期的 task/bot/team 状态需在销毁前快照，并在正确初始化顺序恢复。〔PR#87〕
- **STATE-09** 初始化/设备 API 的成功条件要读准确：SDL 返回 0 为成功；声音设备重启失败应停止后续 painting/submit，不能继续用失效状态。〔PR#106/141/142〕
- **STATE-10** `Bitmap::setRGBA/setSize` 修改纹理像素后应使已上传缓存 dirty，保持 texture id 并在下一次 paint 重新上传。〔PR#273〕
- **STATE-11** 特殊边界 `duration==0` 不代表“效果不可见”；ScreenFade 仍可有 hold 时间，需分清立即变化与立即结束。〔PR#289〕
- **STATE-12** 外部 BSP / 当前 world 的模型身份和注册顺序不同；不能按含糊字段把独立 BSP 当 world inline 模型覆盖。〔PR#69/70/71〕
- **STATE-13** save/restore 字段需完整拷贝与终止、检查 index；资源数组的合法范围和业务计数不可混用。〔PR#67/68〕
- **STATE-14** 游戏浏览器刷新前应取消上轮 ping、清结果和 UI 选择；迟到 callback 不能用旧 row ID 索引新数据。〔PR#292〕
- **STATE-15** 搜索文件系统目录按 normalized physical directory 去重时，应尊重 path-ID 筛选，不能意外合并不同 PAK 结果。〔PR#81〕

## IDX — 索引、有效性和边界值（OOB / NULL）

- **IDX-01** 引擎 resource/precache 的 model、sound、event 和 decal index，在读写表前要判 `index >= 0 && index < LIMIT`；负数与刚好等于 LIMIT 同样非法。〔PR#88/91〕
- **IDX-02** `SV_CopyEdictToPhysent` 的实体 model index 有多条 SOLID 分支，不能只保护其中一种；模型可能不存在/不是 studio。〔PR#145/146〕
- **IDX-03** VGUI1 `CursorBitmap(dc_none)` 早返回前所有 hotspot 等字段仍须初始化；未初始化 stack/member 值不受普通功能测试保证。〔PR#267〕
- **IDX-04** `CServerBrowserDialog::JoinGame`/插件 factory 等“找不到”返回 NULL 时不能继续调用 `Connect`/函数指针。〔PR#330/398〕
- **IDX-05** 服务器列表处理不可信 name/map/player/bot/capacity：先归一化/夹取计数，再使用同一数据做显示、排序和过滤。〔PR#151〕
- **IDX-06** 1-based 玩家表（1..maxClients）不等于 0-based 容器；遍历不能漏最后一位，也不能使用比真实 player storage 更大的 max。〔PR#86〕
- **IDX-07** 空 VOX 句子、无有效词或空目录不能读字符串前一字节/使用未初始化 token；优先拒绝或进入安全 fallback。〔PR#135/136〕

## ABI — 跨模块、导出 ABI、平台与测试入口

- **ABI-01** `VGUI_ABI_COMPAT_MODE=1/0` 拥有权和析构语义不同；修正 virtual destructor/vtable、对象布局、`.def` 导出前必须验证兼容模式。〔PR#313/314/315/317/322〕
- **ABI-02** VGUI1 隐式 copy ctor/赋值也可能是公开导出：增加析构或深拷贝时同步 internal/public headers 和符号签名，不要误删历史 ABI。〔PR#288/433/435/444〕
- **ABI-03** `Panel::GetChild/GetParent` 会按 controls module 名解析，跨模块子面板可返回 NULL；遍历要跳过无法解析 child，不可直接 `->`。〔PR#413〕
- **ABI-04** FocusNav/EditablePanel/WizardPanel 跨模块导航要保留 `VPANEL` handle，借 `ipanel()` 操作；不可把跨模块 NULL 误判为不存在。〔PR#427/430/431〕
- **ABI-05** 不同游戏/平台曾各自编译 VGUI2 / VGUI1 代码副本；修复共享实现后，还要搜索历史 copy、对照模块链接入口。〔PR#251/252/253/261/262〕
- **ABI-06** 依赖 C++ 默认重载/符号及 Windows/MSVC 的编译测试，不可因 GCC/Clang 全绿就认定 Win32 链接没问题。〔PR#417/459〕
- **ABI-07** 测试文件存在不等于会执行：新增 `tests/*_tests.py` 必须加入 `GoldSrc-SDK/cmake/GoldSrcTests.cmake` 的 `foreach`/CTest 注册。〔PR#429/432〕
- **ABI-08** 测试桩抽取生产函数时，不能把“stub 编译失败/链接失败”误判成测试覆盖；同时覆盖 CXX/MSVC、实际发布用的 x86 ABI 和两种编译配置。〔PR#304/399/417/459〕

## Agent 报告输出格式

`[严重性] 文件:函数/行 — 触发路径与前置条件 → 后果；历史相似模式：OWN-01 (PR#395)；最小复现/建议测试：...`

> **优先搜索词**：`COM_LoadFile COM_ParseFile COM_FreeFile`、`KeyValues MakeCopy deleteThis`、`SetImageList SetSlider SetButton`、`OnDelete callback MarkForDeletion`、`wcsncpy _snprintf sizeof wchar_t UTF-8`、`VPANEL GetControlsModuleName`、`Cbuf_ExecuteCommandsFromBuffer`、`Hunk_TempAlloc Cache_Check`、`VGUI_ABI_COMPAT_MODE`。
