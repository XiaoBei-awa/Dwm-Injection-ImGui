# Dwm-Injection-ImGui

基于 Windows 桌面窗口管理器（DWM）的注入式 ImGui 渲染框架，通过向 DWM 进程注入自定义 DLL，实现全局桌面级 ImGui UI 覆盖层；无需绑定特定应用窗口，即可在所有桌面元素上层绘制交互界面。

## 版本说明

项目包含两个版本，适配不同的系统与运行环境，请根据实际场景选择：

| 版本 | 支持环境 | 特性说明 |
| --- | --- | --- |
| **Dwm Injection ImGui** | Windows 10 22H2Windows 11 25H2（仅虚拟机） | 初代实现版本，仅兼容虚拟机环境与指定系统版本 |
| **Dwm Injection ImGui 2** | 全版本 Windows 系统物理机 + 虚拟机 通用 | 重构Hook函数，兼容性全面升级，支持物理机环境 |

## 📌 使用方法

### 操作步骤

1. **放置 DLL 文件**
将对应版本的 `DwmHook.dll` 文件复制到 **C 盘根目录**，确保最终文件完整路径为：

```
C:\DwmHook.dll
```
2. **执行注入 
右键点击注入器程序 → 选择「以管理员身份运行」**，等待注入完成后，即可在桌面看到 ImGui 覆盖层。

### 卸载与恢复

若需要移除注入效果，结束 DWM 进程或重启电脑即可；DWM 进程自动重启后会自动卸载注入的 DLL，不会残留系统修改。

## ⚠️ 禁用 Independent Flip 注册表（强制 Composed Flip）

### 为什么必须禁用 Independent Flip

在物理机环境中，当游戏进入全屏 / 无边框全屏模式时，Windows 系统会强制触发 **Independent Flip（独立翻转）** 机制：游戏画面直接输出到硬件显示平面，整个渲染过程**不再经过 DWM 进程合成**，完全绕过桌面窗口管理器。

因此必须通过修改注册表禁用 Independent Flip，强制系统使用 **Composed Flip（合成翻转）** 模式，让所有画面都经过 DWM 合成后再输出，保证 Overlay 覆盖层正常工作。

### 按系统版本选择对应方案

> 
> - Windows 11 24H2 及以上版本：使用 `GraphicsDrivers` 路径
> - Windows 10 / Windows 11 22H2 / 23H2 等旧版本：使用 `Dwm` 路径

#### 方案一：Windows 11 24H2 及以上版本（24H2 / 25H2）

在显卡驱动层面全局禁用硬件覆盖平面（MPO），彻底关闭 Independent Flip。

1. 按 `Win + R` 输入 `regedit`，打开注册表编辑器
2. 定位到以下路径：
```
HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\GraphicsDrivers
```
3. 右侧空白处右键 → 新建 → **DWORD (32 位) 值**，命名为 `DisableOverlays`
4. 双击该值，将「数值数据」改为 `1`，基数保持「十六进制」
5. **确认删除 `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\Dwm` 路径下的 `OverlayTestMode` 键值（如有）**
6. 重启电脑生效

#### 方案二：Windows 10 22H2 / Windows 11 22H2 / 23H2 旧版本

作用于 DWM 合成器层面，强制所有窗口走合成路径。

1. 按 `Win + R` 输入 `regedit`，打开注册表编辑器
2. 定位到以下路径：
```
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\Dwm
```
3. 右侧空白处右键 → 新建 → **DWORD (32 位) 值**，命名为 `OverlayTestMode`
4. 双击该值，将「数值数据」改为 `5`，基数保持「十六进制」
5. **确认删除 `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\GraphicsDrivers` 路径下的 `DisableOverlays` 键值（如有）**
6. 重启电脑生效

### 🚨 DWM进程崩溃警告

**Windows 11 24H2 及以上系统，绝对禁止同时设置以下两个注册表键值！**

- `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\GraphicsDrivers` → `DisableOverlays`
- `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\Dwm` → `OverlayTestMode`

同时配置驱动级与 DWM 合成器级的 MPO 禁用参数，会导致 DWM 渲染管道死锁，触发 `dwm.exe` 无限循环崩溃、桌面反复黑屏重启。

> 

### 禁用 Independent Flip 后的性能影响

强制切换为 Composed Flip 模式后，系统渲染路径发生变化，对性能的影响如下：

| 性能维度 | 变化情况 | 说明 |
| --- | --- | --- |
| GPU 占用 | 上升 2% ~ 5% | DWM 需要额外执行一次全屏画面合成，增加显卡负载 |
| 输入延迟 | 增加 1 ~ 3ms | 画面需经过 DWM 合成后输出，相比 Independent Flip 延迟小幅上升 |
| 帧率上限 | 受显示器刷新率限制 | 合成模式默认垂直同步，无法突破显示器刷新率上限 |
| 画面稳定性 | 显著提升 | 消除 Independent Flip 动态切换导致的闪烁、黑屏、层级异常 |

> 
> 整体性能损失远小于传统 BitBlt 拷贝模式，绝大多数游戏场景下体感差异极小，主要收益是 Overlay 覆盖层的兼容性与稳定性大幅提升。

### 还原方法

1. 打开注册表编辑器，定位到对应修改的路径
2. 删除创建的 `DisableOverlays` 或 `OverlayTestMode` 键值
3. 重启电脑即可恢复系统默认的 Independent Flip 策略

## 注意事项

- 注入操作必须拥有管理员权限，权限不足会直接导致注入失败
- 部分杀毒软件可能会对进程注入行为产生误报，属于正常技术特性，可添加信任后使用
- 建议优先在虚拟机环境中测试验证，确认兼容后再在物理机运行
- 若注入后出现 DWM 异常，强制重启电脑即可自动恢复系统正常状态

## 免责声明

本项目仅用于 Windows 系统底层技术学习与研究用途，禁止用于任何非法场景、违规软件、游戏外挂开发或商业用途。
进程注入属于系统底层操作，存在一定兼容性风险，因使用本项目造成的任何系统故障、数据丢失或其他后果，由使用者自行承担。
