# todo-cli 设计文档

## 1. 项目定位

`todo-cli` 是一个终端待办事项管理工具，同时集成天气查询与新闻阅览功能。

- **技术栈**：C（主程序）+ Python 3（天气/新闻辅助脚本）
- **数据存储**：SQLite
- **支持平台**：Linux、macOS（Unix）、Windows
- **Python 依赖**：仅标准库（`urllib`、`xml.etree`、`json`），无需 pip 安装任何包

## 2. 总体架构

```
┌─────────────────────────────────────────────────────┐
│                    todo (C 主程序)                    │
│                                                     │
│  main.c ──► cli.c (命令解析)                         │
│              │                                      │
│              ├─► commands.c ──► todo.c ──► db.c     │
│              │       (业务命令)   (领域模型)  (SQLite)│
│              │                                      │
│              ├─► ext.c (管道调用 Python)             │
│              │       │                              │
│              │       ├──► python/weather.py (wttr.in)│
│              │       └──► python/news.py    (RSS/Atom)│
│              │                                      │
│              └─► config.c (配置/路径解析)             │
└─────────────────────────────────────────────────────┘
                          │
                    SQLite 数据库文件
              Linux: ~/.local/share/todo-cli/todo.db
              macOS: ~/.local/share/todo-cli/todo.db
              Windows: %APPDATA%\todo-cli\todo.db
```

## 3. 模块划分

### 3.1 C 模块（实现 `src/*.c`，公共头文件 `include/todo/*.h`）

| 模块 | 职责 |
|------|------|
| `main.c` | 入口；加载配置、打开数据库、分发命令 |
| `cli.c/h` | 子命令与参数解析、帮助文本 |
| `commands.c/h` | 各子命令的业务实现与输出 |
| `todo.c/h` | 任务领域模型，封装 SQL 操作 |
| `db.c/h` | SQLite 打开/关闭/建表 |
| `config.c/h` | 配置读取、跨平台路径解析 |
| `ext.c/h` | 通过管道调用 Python 脚本并捕获输出 |
| `util.c/h` | 字符串、目录、时间等工具函数 |

内部 API 统一由伞头 `include/todo.h` 聚合，配合 CMake 生成的
`todo/todo_version.h` 提供版本宏。CLI 运行包不安装开发头文件。

### 3.2 Python 模块（`python/`）

| 模块 | 职责 |
|------|------|
| `weather.py` | 调用 wttr.in API（`?format=j1`）解析 JSON 输出 |
| `news.py` | 抓取 RSS 2.0 / Atom 订阅源并解析输出 |

约定：Python 脚本只向 stdout 输出文本，由 C 主程序统一打印；错误写入 stderr（已被重定向进管道，由 C 捕获）。

## 4. 数据模型

SQLite 表结构（`todo.db`）：

```sql
CREATE TABLE IF NOT EXISTS todos (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  title        TEXT    NOT NULL,
  priority     INTEGER NOT NULL DEFAULT 2,   -- 1 高 / 2 中 / 3 低
  done         INTEGER NOT NULL DEFAULT 0,
  created_at   INTEGER NOT NULL,             -- Unix 时间戳
  completed_at INTEGER,                      -- 完成时间戳，未完成时 NULL
  due          TEXT                           -- 截止日期 YYYY-MM-DD，可空
);
CREATE INDEX IF NOT EXISTS idx_todos_done ON todos(done);
```

## 5. CLI 命令设计

```
todo add  <标题...> [-p 1|2|3] [-d YYYY-MM-DD]   添加任务
todo list | ls [-a|--all] [-d|--done] [-t|--todo] 列出任务（默认只显示待办）
todo done  <ID...>                                标记完成
todo undo  <ID...>                                取消完成
todo delete | del | rm <ID...>                    删除任务
todo edit  <ID> <新标题...>                       修改标题
todo clear                                        清除全部已完成任务
todo stats                                        统计
todo weather [城市]                               天气查询（缺省按 IP 定位）
todo news   [订阅源1,订阅源2,...]                  新闻阅览（缺省用配置的订阅源）
todo --version | --help                           版本 / 帮助（兼容 version、help）
```

退出码：`0` 成功，`1` 运行错误，`2` 用法错误。

## 6. 配置系统

配置文件（可选，INI 风格 `key = value`，`#`/`;` 注释）：

- Linux/macOS：`$XDG_CONFIG_HOME/todo-cli/config.ini`（缺省 `~/.config/todo-cli/config.ini`）
- Windows：`%APPDATA%\todo-cli\config.ini`

支持键：

| 键 | 说明 | 默认 |
|----|------|------|
| `data_dir` | 数据目录（存放 todo.db） | 见 §2 |
| `python` | Python 解释器（单个可执行文件名或完整路径） | Unix `python3` / Windows `python` |
| `script_dir` | Python 脚本目录 | 自动探测（见 §7） |
| `default_city` | 默认城市，留空则按 IP 定位 | 空 |
| `news_feeds` | 默认新闻订阅源，逗号分隔 | Hacker News + BBC World |

环境变量覆盖（优先级最高）：`TODO_CLI_DATA_DIR`、`TODO_CLI_PYTHON`、`TODO_CLI_SCRIPT_DIR`。

## 7. 跨平台策略

| 关注点 | Unix/Linux/macOS | Windows |
|--------|------------------|---------|
| 数据目录 | `$XDG_DATA_HOME/todo-cli` 或 `~/.local/share/todo-cli` | `%APPDATA%\todo-cli` |
| 配置目录 | `$XDG_CONFIG_HOME/todo-cli` 或 `~/.config/todo-cli` | `%APPDATA%\todo-cli` |
| 目录创建 | `mkdir(2)` 递归 | `CreateDirectoryA` 递归 |
| 进程调用 | `fork + execvp + pipe`（不经 shell，无注入风险） | `CreateProcessA + CreatePipe`（不经 cmd.exe） |
| 可执行路径探测 | `readlink("/proc/self/exe")`，macOS 用 `_NSGetExecutablePath` | `GetModuleFileNameA` |
| 控制台输出 | UTF-8 原生 | `SetConsoleOutputCP(CP_UTF8)` |

所有差异集中在 `util.c` / `ext.c`，以 `#ifdef _WIN32` / `#ifdef __APPLE__` 隔离，业务代码保持平台无关。

**Python 脚本目录探测顺序**：
1. `TODO_CLI_SCRIPT_DIR` 环境变量
2. 配置 `script_dir`
3. 可执行文件相对路径 `<exe_dir>/../share/todo-cli/python`（安装布局）
4. `<exe_dir>/python`（开发构建）
5. 编译期默认值（CMake 源目录下的 `python/`）

**Python 解释器探测顺序**：`TODO_CLI_PYTHON` 环境变量 → 配置 `python` → 平台默认。

## 8. 构建系统（CMake）

- 标准 `CMakeLists.txt`，C11，无第三方构建依赖
- SQLite 获取策略：Linux 默认要求系统库（Debian 安装 `libsqlite3-dev`）；其他平台优先系统库，未找到时 `FetchContent` 下载官方 amalgamation（3.53.4，SHA3-256 校验，静态编译）。`TODO_USE_SYSTEM_SQLITE=ON` 可禁止下载。
- Windows 上 MinGW 与 MSVC 均可构建（MinGW 需 `-municode` 以支持 `wmain`）
- 安装规则：`bin/todo`、`share/todo-cli/python/`（运行时可自动定位脚本）、`share/man/man1/todo.1`（man 页）、`share/doc/todo-cli/`（文档与许可证）
- Debian 打包：`sh scripts/build-deb.sh` 运行编译与测试，然后由 CPack 生成 `.deb`；系统动态库依赖由 `dpkg-shlibdeps` 探测，安装前缀为 `/usr`。
- 测试：CTest + 核心断言测试（`tests/test_core.c`）+ 隔离数据目录中的 CLI 集成测试（`tests/test_cli.py`）

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## 9. 数据源

- **天气**：wttr.in（免费、无需 API key，JSON 接口含中文描述字段 `lang_zh`）
- **新闻**：通用 RSS 2.0 / Atom 解析，订阅源可配置；默认 Hacker News + BBC World

## 10. 路线图

- v0.1（当前设计）：基础增删改查、SQLite 存储、天气/新闻、跨平台构建
- v0.2：任务优先级/截止日期排序与高亮、ANSI 颜色
- v0.3：标签、搜索、JSON 导入导出
- v0.4：同步/提醒（可选）

## 11. 已知限制

- `list` 输出按字符宽度对齐，CJK 宽字符可能引起轻微错位（v0.2 修复）
- 配置 `python` 仅支持单个可执行路径，不支持带参数
- 未做数据库并发访问锁（单用户 CLI 场景）
