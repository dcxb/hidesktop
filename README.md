# hidesktop

> 双击桌面空白处 → 隐藏所有桌面图标；再双击 → 原样显示回来。

一个常驻托盘的小工具（Windows 10/11，C++17 + Win32，无第三方运行时依赖）。
它用全局低级鼠标钩子识别"桌面空白处的双击"，然后直接控制 Explorer 的桌面图标窗口，
所以既没有焦点要求，也不会和双击图标打开程序的习惯冲突。

**[⬇ 下载 hidesktop.exe](https://github.com/dcxb/hidesktop/releases/latest)** ·
Windows 10/11 x64 · 单文件免安装 · 约 300 KB · [更新记录](https://github.com/dcxb/hidesktop/releases)

---

## 1. 功能

| 能力 | 说明 |
| --- | --- |
| 双击桌面空白处 | 隐藏 / 显示全部桌面图标（幂等切换） |
| 托盘左键 | 切换图标显示状态 |
| 托盘右键 | 菜单：切换、开关双击功能、开机自启、通知、关于、退出 |
| 图标状态可视化 | 托盘图标随状态变色（蓝=显示，灰=隐藏），并带气泡提示 |
| Explorer 重启自愈 | 监听 `TaskbarCreated`，任务栏重建后自动重新挂托盘图标 |
| 单实例 | 重复启动会唤起已运行实例的菜单，不会出现两个托盘图标 |
| 命令行 | `--toggle/--hide/--show/--status/--probe/--selftest`，方便做快捷键或脚本 |
| 退出时还原 | 默认恢复到程序启动时的图标状态 |

---

## 2. 快速开始

**不想自己编译**：直接下载发布好的 [hidesktop.exe](https://github.com/dcxb/hidesktop/releases/latest)
（单文件，双击即用，见 2.1）。

自己构建：前置 MinGW-w64（`g++`、`windres` 在 `PATH` 中，实测 g++ 15.2.0 / x86_64-w64-mingw32）。

```bat
build.bat
build\hidesktop.exe
```

产物：`build\hidesktop.exe`（静态链接，单文件，无需 DLL）。

自检（会真实隐藏/显示一次桌面图标，然后恢复）：

```bat
build\hidesktop.exe --selftest
```

打开"开机自动启动"：托盘右键 → 开机自动启动（写入
`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`，值为 `"<exe 路径>" --startup`）。

重新生成托盘图标（可选，需要 Python + Pillow）：

```bat
python tools\make_icons.py
```

### 2.1 单文件分发

`build\hidesktop.exe` **本身就是单个可执行文件**，直接拷走即可用，不需要安装、不需要附带
DLL、图标/manifest/版本信息都已经嵌进 exe：

```
DLL Name: ADVAPI32.dll   KERNEL32.dll   msvcrt.dll   SHELL32.dll   USER32.dll
```

全部是 Windows 自带的系统库（`-static -static-libgcc -static-libstdc++` 已把 MinGW 运行时
静态链进去），所以在一台干净的 Windows 10/11 上双击就能跑。

默认不写任何文件；只有你改过托盘菜单里的开关才会生成
`%APPDATA%\hidesktop\hidesktop.ini`。

**便携模式**：如果 exe 同目录存在 `hidesktop.ini`，程序就只读写这一份（日志也放在旁边），
完全不碰 `%APPDATA%`。做"U 盘随身带"的单文件版本时，把 exe 放到空文件夹、
需要预置设置时在旁边写一个 ini 即可。

---

## 3. 使用

### 3.1 交互

1. 启动后托盘出现 hidesktop 图标。
2. 在桌面**空白处**双击 → 图标全部隐藏（托盘图标变灰）。
3. 再在桌面空白处双击 → 图标恢复（托盘图标变蓝）。

双击判定遵循系统设置：时间窗口取 `GetDoubleClickTime()`，位移窗口取
`SM_CXDOUBLECLK` / `SM_CYDOUBLECLK`，与资源管理器的双击判定一致。

### 3.2 命令行

```
hidesktop                  启动托盘程序（已在运行时唤起其菜单）
hidesktop --startup        开机自启动入口，行为同上
hidesktop --toggle         切换一次后退出（有常驻实例时由它执行）
hidesktop --hide           隐藏桌面图标
hidesktop --show           显示桌面图标
hidesktop --status         打印 visible/hidden；退出码 0=显示 1=隐藏 2=找不到桌面
hidesktop --probe [x y]    诊断：判断某屏幕坐标属于桌面空白/桌面图标/其他窗口
hidesktop --selftest       自检：定位桌面窗口并往返切换一次
hidesktop --help           帮助
```

`--probe` 的输出形如：

```
desktop-background (1900, 1000) window=0x00000000000101D2 class=SysListView32
desktop-icon (60, 60) window=0x00000000000101D2 class=SysListView32
other-window (1225, 890) window=0x0000000000030914 class=Chrome_RenderWidgetHostHWND
```

排查"为什么这里双击没反应"时非常有用：只有 `desktop-background` 才会触发切换。

---

## 4. 设计

### 4.1 模块与数据流

```
       鼠标双击
          │
          ▼
  DoubleClickHook (WH_MOUSE_LL)          ← 只做两件事：判双击、问一句"这是桌面空白吗？"
          │  DesktopIcons::ProbePoint()
          ▼
   ┌──────────────────┐   Background  → 吞掉第 2 次按下/抬起 + PostMessage(kMsgToggleIcons)
   │ 桌面命中分类      │   Icon        → 放行（双击图标照常打开程序）
   │ WindowFromPoint  │   NotDesktop  → 放行（任务栏、其他窗口）
   └──────────────────┘
          │
          ▼
  App::SetIconsVisible()                 ← 消息循环里执行，钩子回调保持轻量
          │
          ▼
  DesktopIcons::SetVisible()  →  ShowWindow(SysListView32, SW_HIDE / SW_SHOWNA)
          │                        （失败时兜底：向 SHELLDLL_DefView 发 0x7402）
          ▼
  TrayIcon::Update + 气泡提示            ← 状态一律以 IsWindowVisible 读回为准
```

| 文件 | 职责 |
| --- | --- |
| `src/main.cpp` | 入口、命令行解析、单实例互斥、`--selftest`/`--probe`/`--status` |
| `src/App.*` | 控制器：隐藏窗口、消息路由、托盘菜单、设置应用、退出还原 |
| `src/DesktopIcons.*` | 定位桌面图标窗口、显示/隐藏、屏幕坐标分类（含跨进程命中测试） |
| `src/DoubleClickHook.*` | `WH_MOUSE_LL` 钩子与双击状态机 |
| `src/TrayIcon.*` | `Shell_NotifyIcon` 封装（图标、tooltip、气泡） |
| `src/Settings.*` | INI 读写 + 开机自启动注册表 |
| `src/util.*` | 路径、格式化、控制台输出（GUI 程序输出到父控制台/重定向） |
| `src/Log.*` | 可选文件日志，默认关闭 |
| `src/res/*` | 图标、manifest（asInvoker + per-monitor v2 DPI）、版本信息 |

### 4.2 关键决策

**桌面图标窗口的定位。** 桌面图标不是"画在桌面上"，而是
`Progman → SHELLDLL_DefView → SysListView32` 这一串普通子窗口（壁纸幻灯片或部分
Windows 11 配置下宿主会变成 `WorkerW`）。因此按"类名 + 父子关系"三级搜索，
并把句柄缓存起来（`IsWindow` + 类名校验失效后重新解析），避免每次点击枚举窗口。

**为什么用 `ShowWindow` 而不是直接发 `0x7402`。** `0x7402` 是资源管理器"显示桌面图标"
菜单命令，本质是 toggle，读不到状态就容易漂移；`ShowWindow` 幂等且可以立刻用
`IsWindowVisible` 读回校验。二者结合：先 `ShowWindow`，校验失败再退回 `0x7402`。

**为什么要跨进程 `LVM_HITTEST`。** 桌面图标本身就在那个 list view 里，如果只在窗口
层面判断，就无法区分"点在空白处"和"点在某个图标上"，双击图标会被误吞。`LVM_HITTEST`
需要把 `LVHITTESTINFO` 放进 shell 进程（`VirtualAllocEx` + `WriteProcessMemory` +
`SendMessageTimeout(LVM_HITTEST)` + `ReadProcessMemory`）。分不清时（打不开进程、超时）
一律按"点在图标上"处理，宁可不动，也不抢用户的点击。

**为什么要吞事件。** 低级别鼠标钩子返回 1 可以丢弃该消息。识别为"桌面空白双击"后，
吞掉第 2 次按下与随后的抬起，Explorer 完全收不到这次点击，桌面不会出现选中框、
也不会有第二套双击逻辑。代价是钩子回调里绝不能做重活：真正的切换动作通过
`PostMessage` 回到消息循环执行。

**状态读取。** 所有对外状态（托盘图标、菜单文案、气泡）都在操作后重新读
`IsWindowVisible`，不维护"我以为是隐藏"的镜像变量，因此 Explorer 刷新、用户从右键
菜单改设置之后也不会显示错乱。

### 4.3 配置

设置文件的位置（按优先级）：

1. **exe 同目录的 `hidesktop.ini`**（存在则用它，便携模式）；
2. 否则 `%APPDATA%\hidesktop\hidesktop.ini`（首次修改设置时才创建）。

```ini
[general]
doubleClick=1     ; 是否启用"双击桌面切换"
notifications=1   ; 气泡提示
restoreOnExit=1   ; 退出时恢复启动时的图标状态
logging=0         ; 日志写入与 ini 同目录的 hidesktop.log
```

### 4.4 目录结构

```
hidesktop/
├─ build.bat                 一键构建
├─ src/
│  ├─ main.cpp  App.*  DesktopIcons.*  DoubleClickHook.*
│  ├─ TrayIcon.*  Settings.*  util.*  Log.*
│  └─ res/  resource.h  hidesktop.rc  app.manifest  *.ico
├─ tools/make_icons.py       托盘图标生成
└─ README.md
```

---

## 5. 已知限制

- 只对 Windows 自带 Explorer 桌面有效；替换 shell（如第三方桌面）不保证可用。
- 双击必须落在**桌面空白处**。这是刻意设计：落在图标上时放行原始双击，避免抢掉
  "双击图标打开程序"。
- Explorer 重启、桌面按 <kbd>F5</kbd>、切换显示设置等情况可能让图标自行恢复显示，
  此时重新双击一次即可（程序会重新解析窗口句柄）。
- 因为走的是 `ShowWindow`，桌面右键菜单里"查看 → 显示桌面图标"的勾选状态不会同步；
  但下一次通过菜单或双击操作仍以真实可见性为准，不会错乱。
- 使用全局低级鼠标钩子（`WH_MOUSE_LL`）：不轮询、不注入、只在点击时被唤起；
  若被安全软件拦截，托盘会自动关闭双击功能并提示，仍可用托盘左键或命令行。
- 如果 Explorer 以更高权限运行而本程序不是，跨进程命中测试会失败，此时程序选择
  "不抢这次点击"，表现为双击无效（不会误吞）。
- DPI：程序声明 per-monitor v2，钩子拿到的是物理像素，命中测试同样使用物理像素，
  多显示器/高 DPI 下无需换算。

## 6. 可以继续做的

- 全局热键（`RegisterHotKey`）与双击功能并存；
- 隐藏图标时顺便隐藏/恢复 `Progman` 的壁纸？不需要，但可加"隐藏任务栏"选项；
- 用 `WM_DISPLAYCHANGE` / `WM_SETTINGCHANGE` 监听分辨率、主题变化并主动校验状态；
- 更细的触发策略：仅中键双击、仅在主显示器、双击间隔自定义。

## 7. 许可

[MIT](LICENSE) © 2026 dcxb

构建出的 `hidesktop.exe` 完全自包含（静态链接 MinGW 运行时），可自由分发。

