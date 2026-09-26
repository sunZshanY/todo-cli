# todo-cli

面向 GNU/Linux（Debian）的终端待办工具。C 主程序使用 SQLite 保存任务，支持中文标题；依赖均可通过 apt 安装，无需 pip 或 npm。

## 特性

- 任务增删改查、完成状态、优先级（高/中/低）、截止日期
- SQLite 本地存储，零服务依赖
- 天气查询（wttr.in，支持中文输出）
- 新闻阅览（RSS/Atom，订阅源可配置）
- C 主程序 + Python 辅助脚本，Python 仅用标准库
- 支持 Linux / macOS / Windows

## Debian 安装与使用

取得与你的 Debian 版本和 CPU 架构匹配的 `.deb` 后，在包所在目录执行（以 amd64 为例）：

```bash
sudo apt install ./todo-cli_0.1.0-1_amd64.deb
todo add "复习一元一次方程"
todo list
todo done 1
todo delete 1
todo --help
```

`add` 返回任务 ID，后续命令使用这个 ID。`list` 默认显示未完成任务；`todo list --all` 查看全部，`todo list --done` 查看已完成任务。删除后的 ID 不会重新分配。

任务保存在 `~/.local/share/todo-cli/todo.db`，关闭终端后仍保留。设置了 `XDG_DATA_HOME` 时使用 `$XDG_DATA_HOME/todo-cli/todo.db`，也可用 `TODO_CLI_DATA_DIR` 指定数据目录。日常运行无需 sudo。

apt 会自动安装运行依赖。当前提供本地 `.deb` 打包方式，尚未发布到 Debian 官方仓库或独立 apt 仓库，因此安装时需要带 `./` 的包路径。

卸载：`sudo apt remove todo-cli`。用户目录中的任务数据库会保留。

## 在 Debian 上构建 .deb

建议使用 Debian 12 或更新版本。在项目根目录执行：

```bash
sudo apt update
sudo apt install --no-install-recommends build-essential cmake dpkg-dev file libsqlite3-dev python3
sh scripts/build-deb.sh
sudo apt install ./build-debian/packages/todo-cli_0.1.0-1_amd64.deb
```

脚本依次执行编译、核心测试、CLI 测试和 CPack 打包；任一步失败即停止。包位于 `build-debian/packages/`，架构由构建环境自动确定（如 `amd64`、`arm64`）。可传入构建目录：`sh scripts/build-deb.sh /tmp/todo-build`。

Linux 默认要求系统 SQLite，CMake 不会下载依赖。包内 libc 和 SQLite 的版本依赖由 `dpkg-shlibdeps` 自动生成。应在目标 Debian 版本或更早的兼容版本上构建，避免在较新 Ubuntu 上构建后直接给旧 Debian 使用。

## Windows / Docker 构建 Debian 包

仓库带有 Debian 12 构建镜像。Windows 上启动 Docker Desktop（Linux 容器），在 PowerShell 的项目目录执行：

```powershell
docker build -f packaging/Dockerfile.debian -t todo-cli-debian-build .
New-Item -ItemType Directory -Force dist | Out-Null
docker run --rm --mount "type=bind,source=$($PWD.Path),target=/src,readonly" --mount "type=bind,source=$($PWD.Path)/dist,target=/out" todo-cli-debian-build
```

生成的包位于 `dist/`。镜像中的工具均由 Debian apt 安装。

## 开发构建

依赖：CMake ≥ 3.20、C 编译器、SQLite 开发库、Python 3（测试及天气/新闻）。测试使用隔离的临时目录，不会修改个人任务。非 Linux 平台找不到系统 SQLite 时，可自动下载官方源码编译。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## 快速上手

```bash
todo add 写周报 -p 1 -d 2026-10-01
todo add 买牛奶
todo list
todo done 1
todo stats
todo weather 北京
todo news
```

完整帮助：`todo --help`，安装后也可运行 `man todo`。架构详见 [docs/DESIGN.md](docs/DESIGN.md)。

## 配置（可选）

Linux/macOS：`~/.config/todo-cli/config.ini`
Windows：`%APPDATA%\todo-cli\config.ini`

```ini
default_city = 北京
news_feeds = https://news.ycombinator.com/rss,http://feeds.bbci.co.uk/news/world/rss.xml
# python = python3
```

示例见 `docs/config.ini.example`。
