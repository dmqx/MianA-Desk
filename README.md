# MianA Desk

<p align="center">
  <img src="assets/miana_desk.png" alt="MianA Desk" width="128">
</p>

<p align="center">
  <strong>MianA Desk - A 股桌面行情助手</strong>
</p>

<p align="center">
  「MianA Desk」是一款基于 C++20、Qt 6 与 QML 的轻量级 Windows 桌面行情组件。
</p>

> 本项目仅用于行情展示，不构成任何投资建议。行情可能存在延迟，请以交易所和券商数据为准。

## 功能

* 支持 A 股、港股、美股自选行情
* 完整、折叠、浮窗三种显示模式
* 自选管理、窗口置顶、锁定、托盘与开机启动
* 腾讯、新浪、东方财富、Yahoo 多行情源自动回退
* 半透明主题、桌面取色与 Windows 原生圆角阴影

## 系统要求

* Windows 10 / 11 x64
* Direct3D 11 图形环境

发布版本未包含 Qt 软件 OpenGL 后备库 `opengl32sw.dll`。在显卡驱动异常、精简系统或缺少图形加速的旧虚拟机中可能无法正常运行。

## 下载

前往 [MianA Desk Releases](https://github.com/dmqx/MianA-Desk/releases) 下载最新版本。

下载 `MianA Desk.exe` 后直接运行，无需安装。

## 从源码构建

### 环境要求

* Visual Studio 2022 Build Tools
* MSVC x64
* Qt 6.8+ MSVC 2022 x64
* CMake 3.21+

### 构建

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
cmake --install .build --config Release --prefix dist
```

如果 CMake 无法自动找到 Qt：

```powershell
cmake --preset windows-msvc-release `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
```

安装阶段会自动运行 `windeployqt`，并移除未使用的插件、主题、翻译、软件 OpenGL 及开发文件。

## 单文件打包

完成 `dist` 部署后：

```powershell
cmake -S packaging -B .package-build `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -DMIANA_DIST_DIR="$PWD/dist"

cmake --build .package-build `
  --config Release `
  --target MianADeskPackage
```

生成文件：

```text
.package-build/Release/MianA Desk.exe
```

打包器使用 Zstandard 压缩完整部署目录并嵌入可执行文件。


## 许可证

本项目基于 [GNU GPL v3](LICENSE) 开源。

你可以自由使用、修改和分发本项目；衍生版本须继续遵循 GPL-3.0 许可证。
