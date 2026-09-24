# todo-cli

终端待办事项管理工具，附带天气查询与新闻阅览。

## 特性

- 任务增删改查、完成状态、优先级（高/中/低）、截止日期
- SQLite 本地存储，零服务依赖
- 天气查询（wttr.in，支持中文输出）
- 新闻阅览（RSS/Atom，订阅源可配置）
- C 主程序 + Python 辅助脚本，Python 仅用标准库
- 支持 Linux / macOS / Windows

## 构建

依赖：CMake ≥ 3.20、C 编译器（GCC/Clang/MinGW/MSVC）、Python 3（运行时）。

SQLite 优先使用系统库；找不到时 CMake 会自动下载官方 amalgamation 静态编译。

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build          # 可选，运行测试
cmake --install build           # 可选，安装
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

完整帮助：`todo help`。架构详见 [docs/DESIGN.md](docs/DESIGN.md)。

## 配置（可选）

Linux/macOS：`~/.config/todo-cli/config.ini`
Windows：`%APPDATA%\todo-cli\config.ini`

```ini
default_city = 北京
news_feeds = https://news.ycombinator.com/rss,http://feeds.bbci.co.uk/news/world/rss.xml
# python = python3
```

示例见 `docs/config.ini.example`。
