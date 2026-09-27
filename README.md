# todo-cli v0.2

面向 GNU/Linux（Debian）的终端待办工具。C 主程序使用 SQLite 保存任务，支持中文标题；依赖均可通过 apt 安装，无需 pip 或 npm。
> 跟todo-cli v0.1相比，增加了许多特性 ......

内容问题：<a href="error_project/ERROR_2026_9_27.md">todo问题反馈</a>

## 特性

- 自定义项目，支持中文名称（NanoX、TODO_CLI、Python学习、蓝桥杯等）
- 任务增删改查、完成状态、P0/P1/P2/P3 优先级、截止日期
- 项目看板：按优先级分组、Completed 列表、Progress 完成进度
- list 按项目、优先级、待办/已完成状态组合筛选
- SQLite 本地存储，零服务依赖
- 天气查询（wttr.in，支持中文输出）
- 新闻阅览（RSS/Atom，订阅源可配置）
- C 主程序 + Python 辅助脚本，Python 仅用标准库
- 支持 Linux / macOS / Windows

## Debian 安装与使用

取得与你的 Debian 版本和 CPU 架构匹配的 `.deb` 后，在包所在目录执行（以 amd64 为例）：

```bash
sudo apt install ./todo-cli_0.2.0-1_amd64.deb
todo --version
todo project add NanoX
todo project add TODO_CLI
todo project add Python学习
todo project add 蓝桥杯
todo add NanoX "修复崩溃" --priority P0
todo add NanoX "优化UI" --priority P1
todo add NanoX "增加主题" --priority P2
todo add NanoX "添加彩蛋" --priority P3
todo project show NanoX
todo project list
todo list NanoX --priority P0 --todo
todo done 1
todo delete 1
todo --help
```

`add` 返回任务 ID，后续命令使用这个 ID。`list` 默认显示未完成任务；`todo list --all` 查看全部，`todo list --done` 查看已完成任务。删除后的 ID 不会重新分配。

ID 在所有项目中唯一。示例中的 `1` 适用于全新数据库；已有数据时请使用实际输出的 ID。项目名包含空格时使用引号，例如 `todo project add "My Project"`。项目名称区分大小写，必须先创建项目再添加任务。

项目看板示例：

```text
NanoX
────────────────────────────

P0
  [ ] 修复文件树崩溃 (#1)

P1
  [ ] 实现 Buffer (#2)
  [ ] 增加 Vim Normal Mode (#3)

P2
  [ ] Lua Plugin API (#4)

P3
  [ ] 主题系统 (#5)

Completed
  [x] 实现基础 TUI (#7)
  [x] 创建项目 (#6)

Progress: 2/7
```

完成任务会移入 Completed，`todo undo <ID>` 可恢复到原优先级分组。空项目显示 `Progress: 0/0`；删除任务后进度按剩余任务重新计算。

常用筛选：

```bash
todo list                                      # 所有项目的待办
todo list NanoX                                 # NanoX 的待办
todo list --project NanoX --priority P1 --all    # NanoX 的全部 P1 任务
todo list NanoX --done                          # NanoX 的已完成任务
todo list --priority P0 --todo                  # 所有项目的 P0 待办
```

`--all`、`--done`、`--todo` 互斥；`--pending` 等同于 `--todo`。优先级从 P0（最高）到 P3（最低），默认 P2；数字 `0/1/2/3` 和小写 `p0/p1/p2/p3` 也可使用。

### 从 v0.1 升级

直接安装新版 `.deb` 即可升级。首次运行数据命令时，旧数据库在事务中自动迁移，旧任务归入默认 `TODO` 项目，保留 ID、标题、完成状态、时间、截止日期和优先级数值（旧 1/2/3 对应 P1/P2/P3）。迁移失败时回滚；遇到高于当前支持版本的数据库时拒绝修改。

`todo add "复习一元一次方程"` 仍可使用，任务进入 `TODO`。新语法中，两个或更多位置参数的第一个是项目名，因此含空格的单任务标题应加引号；也可显式使用 `todo add --project TODO write weekly report`。以 `-` 开头的标题用 `todo add NanoX -- "--标题"`，或 `todo add -- "--标题"` 添加到默认项目。迁移后的数据库应继续使用 v0.2 或更新版本。

任务保存在 `~/.local/share/todo-cli/todo.db`，关闭终端后仍保留。设置了 `XDG_DATA_HOME` 时使用 `$XDG_DATA_HOME/todo-cli/todo.db`，也可用 `TODO_CLI_DATA_DIR` 指定数据目录。日常运行无需 sudo。

apt 会自动安装运行依赖。当前提供本地 `.deb` 打包方式，尚未发布到 Debian 官方仓库或独立 apt 仓库，因此安装时需要带 `./` 的包路径。

卸载：`sudo apt remove todo-cli`。用户目录中的任务数据库会保留。

## 在 Debian 上构建 .deb

建议使用 Debian 12 或更新版本。在项目根目录执行：

```bash
sudo apt update
sudo apt install --no-install-recommends build-essential cmake dpkg-dev file libsqlite3-dev python3
sh scripts/build-deb.sh
sudo apt install ./build-debian/packages/todo-cli_0.2.0-1_amd64.deb
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

安装后可对实际安装的程序运行测试（使用临时目录，不修改个人任务）：

```bash
python3 tests/test_cli.py /usr/bin/todo
```

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
