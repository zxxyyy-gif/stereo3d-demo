# stereo3d-demo

3DS 裸眼立体（分层视差）的最小可运行示例。零外部素材，纯矩形/三角形画出完整场景，
滑块一推就能看到"人物浮在砖块前面、前景草贴脸"的效果。

> 说明：本工程是在**自己的程序里**实现立体分层，和修改商业游戏是两件不同的事（见文末）。

> 编译状态：云端 CI（`devkitpro/devkitarm` 容器）已跑通，产出 `stereodemo.3dsx` 约 44KB。

## 需要什么

devkitPro（含 devkitARM）+ 3ds-dev 包（提供 libctru / citro2d / citro3d）。

### 方案 A：本地安装（需要能访问 pkg.devkitpro.org）

双击 `install-devkitpro.bat`，按提示点 UAC 的"是"，它会自动下载官方安装器、
装好 MSYS2 环境，再执行 `pacman -S 3ds-dev`。

手动等价步骤：

1. 装 [官方图形安装器](https://github.com/devkitPro/installer/releases/latest)，默认路径 `C:\devkitPro`
2. 打开 `C:\devkitPro\msys2\msys2_shell.bat`
3. 执行 `pacman -S 3ds-dev`

> **已知网络问题**：`pkg.devkitpro.org` 在部分网络环境下会被 Cloudflare 整站拦截，
> 表现为所有请求返回 `403 Forbidden`，页面标题是 "Attention Required"。
> 此时安装器会在下载阶段直接失败。换网络或挂代理后重试，或直接用方案 B。

### 方案 B：云端编译（不需要本机装任何东西）

仓库里已带好 `.github/workflows/build.yml`，用的是 devkitPro 官方容器镜像：

1. 把本工程推到 GitHub 仓库（工程根目录就是仓库根目录）
2. 进入 Actions 页签，跑 `build-3dsx`
3. 跑完在 Artifacts 里下载 `stereodemo-3dsx`，解压得到 `stereodemo.3dsx`

这条路不受本地网络影响——runner 访问 devkitPro 源是通的，而且工具链在镜像里已经预装好。

## 编译

在工程根目录：

```sh
make
```

产出 `stereodemo.3dsx`。

## 部署到机器

1. 把 `stereodemo.3dsx` 拷到 SD 卡的 `/3ds/` 目录下
2. 3DS 上打开 Homebrew Launcher，选中 `stereodemo` 运行
3. 开始前记得把 3D 滑块推到中间位置

## 操作

| 按键 | 作用 |
|---|---|
| 圆摇杆 / 十字键左右 | 移动角色（相机平滑跟随） |
| START | 退出 |
| 3D 滑块 | 实时调节立体强度，推到 0 就是纯 2D |

下屏显示当前滑块值，以及五个图层左右眼各自的分离量示意。

## 效果是怎么来的

核心只有两件事：

```c
gfxSet3D(true);                    // 上屏进入视差屏障工作状态

// 上屏左右眼各画一遍，每层按深度给不同的水平偏移
C2D_SceneBegin(topLeft);  draw_scene(+1.0f, level);
C2D_SceneBegin(topRight); draw_scene(-1.0f, level);

// 每层偏移
ox = eye * depth * STEREO_MAX_PX * level;
```

图层深度表（`source/main.c` 顶部的 `D_*` 宏）：

| 图层 | depth | 视差滚动 | 滑块推满时单眼偏移 |
|---|---|---|---|
| 天空 | 0.00 | — | 0 px（无限远） |
| 云 | 0.10 | 0.18 | 1.2 px |
| 远山 | 0.22 | 0.42 | 2.6 px |
| 树 | 0.40 | 0.70 | 4.8 px |
| 地形 | 0.60 | 1.00 | 7.2 px |
| 角色 | 0.90 | 1.00 | 10.8 px |
| 前景草 | 1.00 | 1.28 | 12.0 px |

调参入口：

- `STEREO_MAX_PX` —— 全局强度。晕就调到 8，没感觉就调到 16。
- 各层 `depth` —— 决定远近关系。相邻层差 0.15 以上才有明显层次。
- 各层 `scroll` —— 非立体的"横向视差滚动"，和立体视差是两个独立维度，配合起来纵深才自然。

## 想换成真素材 / 更像 Terraria

工程里所有绘制都是 `C2D_DrawRectSolid` / `C2D_DrawTriangle`，换成 `C2D_Sprite` 即可，
`draw_scene(eye, level)` 的调用结构不用改 —— 每层只需要在画 sprite 时加上同一个 `ox`。
要注意的两点：

1. **HUD、下屏 UI 绝对不能加视差**，否则 UI 会飘出屏幕。真实游戏里所有 UI 都要单独走"深度 0"的绘制通道。
2. **相机移动时，各层滚动速度不同**。如果只做立体不做滚动视差，立体感会很假。

## 为什么这个不能直接套到 Terraria 上

Terraria 3DS 的开发方 Codeglue 在 2015 年首发 FAQ 里写过：游戏"plays in 2D ONLY"，
原因是 3DS 的图层结构不适合做立体效果，且开 3D 的性能开销会迫使他们在世界大小等方面做妥协。

要做同样的事，需要改的是它闭源 ARM11 二进制里的渲染管线：给上屏分配双 framebuffer、
拦截每个 sprite batch、按层注入投影偏移、处理地图边缘剔除和 HUD 隔离。这不是"加一个功能"，
是重写渲染层，属于引擎级逆向工程的工作量。

社区里流行的 3D 补丁（CTRPF / Luma 金手指改内存里的立体参数，如 `0x41200000` 这类 float）
只对**本身已走通官方立体渲染路径**的游戏有效。对纯 2D 游戏强行打开视差屏障，右眼图像会是乱的，
因为右眼那一帧从来没被渲染出来。

## 注意

自己改着玩没问题；不要把改过的完整商业游戏分发包出去。
