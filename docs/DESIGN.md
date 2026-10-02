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
| `todo.c/h` | 项目与任务模型、组合筛选、项目统计，封装参数化 SQL |
| `db.c/h` | SQLite 打开/关闭、版本检查、事务化迁移 |
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
CREATE TABLE projects (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  name         TEXT NOT NULL UNIQUE CHECK(length(trim(name)) > 0),
  created_at   INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS todos (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  title        TEXT    NOT NULL,
  priority     INTEGER NOT NULL DEFAULT 2,   -- P0/P1/P2/P3，默认 P2
  done         INTEGER NOT NULL DEFAULT 0,
  created_at   INTEGER NOT NULL,             -- Unix 时间戳
  completed_at INTEGER,                      -- 完成时间戳，未完成时 NULL
  due          TEXT,                          -- 截止日期 YYYY-MM-DD，可空
  project_id   INTEGER REFERENCES projects(id)
);
CREATE INDEX IF NOT EXISTS idx_todos_done ON todos(done);
CREATE INDEX idx_todos_project ON todos(project_id, done, priority);

CREATE TABLE trash (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  original_id  INTEGER NOT NULL,              -- 任务原 ID，恢复时保留
  title        TEXT    NOT NULL,
  priority     INTEGER NOT NULL DEFAULT 2,
  done         INTEGER NOT NULL DEFAULT 0,
  created_at   INTEGER NOT NULL,
  completed_at INTEGER,
  due          TEXT,
  project_id   INTEGER REFERENCES projects(id),
  deleted_at   INTEGER NOT NULL               -- 移入回收站时间
);
CREATE INDEX idx_trash_deleted ON trash(deleted_at);
PRAGMA user_version = 3;
```

默认项目为 `TODO`（id=1）。项目名区分大小写且唯一，CLI 去除名称两端空白并拒绝空名称；任务 ID 跨项目唯一。任务只能添加到已存在的项目，创建项目与添加任务均采用参数化 SQL。

`todo delete` 与 `todo clear` 均为软删除：在事务中把任务原样复制到 `trash` 再移除。`todo trash` 列出回收站内容，`todo restore <回收ID>` 按原 ID 写回（AUTOINCREMENT 不会复用已删除 ID，因此不会冲突），`todo purge` 永久删除。

v0.1 数据库未设置 `user_version`，其值为 0。首次运行 v0.2 时开启外键检查，在 `BEGIN IMMEDIATE` 事务内读取版本、创建项目表、通过 `ALTER TABLE` 加入项目关联，再把旧任务归入 TODO。保留所有任务字段和 `sqlite_sequence`，包括已删除任务占用过的 ID；成功后版本设为 2，失败则整体回滚。v0.2.1 在版本低于 3 时新增 `trash` 表。版本高于 3 时拒绝修改。为兼容 SQLite 的 ALTER 限制，迁移列可空，但写入路径始终要求有效项目。

## 5. CLI 命令设计

```
todo project add <项目>                          创建项目
todo project [list]                              列出项目及 Todo/Done/Progress
todo project show <项目>                         按 P0-P3、Completed 展示，输出进度
todo add <项目> <标题...> [-p P0|P1|P2|P3] [-d YYYY-MM-DD]
todo add <标题> [--project <项目>]                单标题默认归入 TODO
todo list | ls [项目] [--project <项目>] [-p|--priority P0|P1|P2|P3]
             [-a|--all] [-d|--done] [-t|--todo|--pending]
todo done  <ID...>                                标记完成
todo undo  <ID...>                                取消完成
todo delete | del | rm <ID...>                    移入回收站
todo trash                                         查看回收站内容
todo restore <回收ID...>                           恢复任务（保留原 ID）
todo purge <回收ID...> | --all                     永久删除回收站任务
todo edit  <ID> [新标题...] [-p P0|P1|P2|P3] [-d YYYY-MM-DD]
                                                   修改标题/优先级/截止日期
todo cal | -cal [YYYY-MM]                          月历与提醒事项
todo clear                                        清除全部已完成任务（移入回收站）
todo stats                                        统计
todo weather [城市]                               天气查询（缺省按 IP 定位）
todo news   [订阅源1,订阅源2,...]                  新闻阅览（缺省用配置的订阅源）
todo --version | --help                           版本 / 帮助（兼容 version、help）
```

退出码：`0` 成功，`1` 运行错误，`2` 用法错误。

`list` 默认查询所有项目待办，项目、优先级、状态可以组合筛选。待办按优先级、截止日期（空日期最后）、ID 排序；Completed 按完成时间和 ID 倒序。看板固定显示 P0/P1/P2/P3 与 Completed，任务旁显示 ID，进度为完成数/总数；空项目为 0/0。列表用分隔符呈现，避免中文宽度造成表格对齐问题。

`edit` 动态拼接 `UPDATE` 语句，标题、优先级、截止日期三者至少提供一项。`cal` 按月查询 `due` 前缀为 `YYYY-MM` 的任务，月历用 `*` 标记有提醒的日期、`[...]` 标记今天（周一为一周起始），下方按日期列出提醒事项。所有子命令支持 `--help`；帮助和版本查询不会创建数据库。

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

- v0.1（已完成）：基础增删改查、SQLite 存储、天气/新闻、跨平台构建
- v0.2（已完成）：自定义项目、P0-P3、项目看板及进度、list 组合筛选、旧库迁移、Debian 包
- v0.2.1（已完成）：回收站（trash/restore/purge）、edit 修改优先级与截止日期、cal 月历提醒
- v0.3：标签、搜索、JSON 导入导出
- v0.4：同步/提醒（可选）

## 11. 已知限制

- 项目管理提供创建、列表和查看，暂不提供重命名、删除或跨项目移动任务
- 配置 `python` 仅支持单个可执行路径，不支持带参数
- 并发写入使用 SQLite 事务锁及 2 秒 busy timeout；超时返回错误
