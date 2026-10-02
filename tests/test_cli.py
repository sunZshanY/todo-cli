"""Exercise the public CLI in separate processes with isolated, persistent data."""

import datetime
import os
import sqlite3
from contextlib import closing
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


TODO = str(Path(sys.argv.pop(1)).resolve())


class TodoCliTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="todo-cli-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.data = self.root / "data with spaces"
        self.env = os.environ.copy()
        self.env.update(
            HOME=str(self.root),
            APPDATA=str(self.root),
            XDG_CONFIG_HOME=str(self.root / "config"),
            XDG_DATA_HOME=str(self.root / "xdg-data"),
            TODO_CLI_DATA_DIR=str(self.data),
        )

    def run_todo(self, *args, code=0):
        result = subprocess.run(
            [TODO, *args], env=self.env, cwd=self.root,
            capture_output=True, text=True, encoding="utf-8", timeout=10,
        )
        self.assertEqual(result.returncode, code, result.stdout + result.stderr)
        return result

    def test_requested_workflow(self):
        title = "复习一元一次方程"
        self.assertIn("没有任务", self.run_todo("list").stdout)
        added = self.run_todo("add", title).stdout
        self.assertIn("#1", added)
        self.assertIn(title, added)
        listed = self.run_todo("list").stdout
        self.assertIn(title, listed)
        self.assertIn("[ ]", listed)
        self.assertTrue((self.data / "todo.db").is_file())
        self.run_todo("done", "1")
        self.assertNotIn(title, self.run_todo("list").stdout)
        completed = self.run_todo("list", "--all").stdout
        self.assertIn(title, completed)
        self.assertIn("[x]", completed)
        self.assertIn(title, self.run_todo("list", "--done").stdout)
        self.run_todo("delete", "1")
        self.assertNotIn(title, self.run_todo("list", "--all").stdout)
        self.assertIn("#2", self.run_todo("add", "下一个任务").stdout)

    def test_help_does_not_create_database(self):
        help_text = self.run_todo("--help").stdout
        for command in ("add", "list", "done", "delete", "trash", "restore",
                        "purge", "cal", "--help"):
            self.assertIn(command, help_text)
        self.assertFalse(self.data.exists())

    def test_subcommand_help_and_version(self):
        for args in (("project", "--help"), ("project", "show", "--help"),
                     ("add", "--help"), ("list", "--help"), ("done", "--help"),
                     ("trash", "--help"), ("restore", "--help"),
                     ("purge", "--help"), ("cal", "--help")):
            text = self.run_todo(*args).stdout
            for token in ("project add", "project show", "--priority", "P0", "P3"):
                self.assertIn(token, text)
        self.assertIn("0.2.1", self.run_todo("--version").stdout)
        self.assertFalse(self.data.exists())

    def test_projects_and_requested_board(self):
        for name in ("NanoX", "TODO_CLI", "Python学习", "蓝桥杯", "My Project"):
            self.run_todo("project", "add", name)
        names = self.run_todo("project", "list").stdout
        for name in ("TODO", "NanoX", "TODO_CLI", "Python学习", "蓝桥杯", "My Project"):
            self.assertIn(name, names)
        tasks = [
            ("修复文件树崩溃", "P0"), ("实现 Buffer", "P1"),
            ("增加 Vim Normal Mode", "P1"), ("Lua Plugin API", "P2"),
            ("主题系统", "P3"), ("创建项目", "P0"), ("实现基础 TUI", "P1"),
        ]
        for title, priority in tasks:
            self.run_todo("add", "NanoX", title, "--priority", priority)
        self.run_todo("done", "6", "7")
        board = self.run_todo("project", "show", "NanoX").stdout
        self.assertTrue(board.startswith("NanoX\n"))
        self.assertIn("Progress: 2/7", board)
        self.assertEqual(board.count("[ ]"), 5)
        self.assertEqual(board.count("[x]"), 2)
        self.assertLess(board.index("\nP0\n"), board.index("\nP1\n"))
        self.assertLess(board.index("\nP1\n"), board.index("\nP2\n"))
        self.assertLess(board.index("\nP2\n"), board.index("\nP3\n"))
        self.assertLess(board.index("\nP3\n"), board.index("\nCompleted\n"))
        for task_id, (title, _) in enumerate(tasks, 1):
            marker = "[x]" if task_id >= 6 else "[ ]"
            self.assertIn(f"{marker} {title} (#{task_id})", board)
        self.assertIn("NanoX | 5 | 2 | 2/7", self.run_todo("project").stdout)
        self.run_todo("undo", "6")
        self.assertIn("Progress: 1/7", self.run_todo("project", "show", "NanoX").stdout)
        self.run_todo("delete", "7")
        self.assertIn("Progress: 0/6", self.run_todo("project", "show", "NanoX").stdout)
        self.assertIn("#8", self.run_todo("add", "Python学习", "学习列表").stdout)
        self.assertNotIn("学习列表", self.run_todo("project", "show", "NanoX").stdout)
        self.assertIn("学习列表", self.run_todo("project", "show", "Python学习").stdout)
        self.assertIn("Progress: 0/0", self.run_todo("project", "show", "My Project").stdout)

    def test_combined_filters_and_priority_order(self):
        self.run_todo("project", "add", "NanoX")
        self.run_todo("project", "add", "蓝桥杯")
        for p in (3, 1, 0, 2):
            self.run_todo("add", "NanoX", f"task-P{p}", "-p", f"P{p}")
        self.run_todo("add", "蓝桥杯", "other-P0", "-p", "P0")
        self.run_todo("add", "NanoX", "completed-P0", "-p", "P0")
        self.run_todo("done", "6")
        pending = self.run_todo("list", "NanoX").stdout
        positions = [pending.index(f"task-P{p}") for p in range(4)]
        self.assertEqual(positions, sorted(positions))
        self.assertNotIn("other-P0", pending)
        self.assertNotIn("completed-P0", pending)
        for name_args in (("NanoX",), ("--project", "NanoX")):
            filtered = self.run_todo("list", *name_args, "--priority", "P0", "--todo").stdout
            self.assertIn("task-P0", filtered)
            for excluded in ("task-P1", "task-P2", "task-P3", "other-P0", "completed-P0"):
                self.assertNotIn(excluded, filtered)
        done = self.run_todo("list", "NanoX", "--done", "-p", "P0").stdout
        self.assertIn("completed-P0", done)
        self.assertNotIn("task-P0", done)
        self.assertIn("[x]", done)
        all_p0 = self.run_todo("list", "--all", "-p", "P0").stdout
        for title in ("task-P0", "other-P0", "completed-P0"):
            self.assertIn(title, all_p0)
        self.assertIn("没有任务", self.run_todo("list", "蓝桥杯", "-p", "P3").stdout)

    def test_priority_aliases_default_and_due(self):
        for p in range(4):
            self.run_todo("add", f"number{p}", "--priority", str(p))
        self.run_todo("add", "default", "--due", "2026-10-01")
        self.run_todo("add", "explicit project", "--project", "TODO", "-p", "p0")
        listing = self.run_todo("list").stdout
        for p in range(4):
            self.assertIn(f"[ ] | P{p} | #{p + 1} | TODO | number{p}", listing)
        self.assertIn("[ ] | P2 | #5 | TODO | default | 2026-10-01", listing)
        self.assertIn("[ ] | P0 | #6 | TODO | explicit project", listing)
        self.assertIn("due: 2026-10-01", self.run_todo("project", "show", "TODO").stdout)

    def test_trash_restore_purge_and_clear(self):
        self.run_todo("add", "回收测试", "-p", "P0", "-d", "2026-10-01")
        self.run_todo("add", "已完成任务")
        self.run_todo("done", "2")
        deleted = self.run_todo("delete", "1").stdout
        self.assertIn("回收站", deleted)
        self.assertNotIn("回收测试", self.run_todo("list", "--all").stdout)
        trash = self.run_todo("trash").stdout
        self.assertIn("回收测试", trash)
        self.assertIn("P0", trash)
        self.assertIn("2026-10-01", trash)
        self.assertIn("#1", trash)
        self.run_todo("restore", "1")
        listing = self.run_todo("list", "--all").stdout
        self.assertIn("回收测试", listing)
        self.assertIn("P0", listing)
        self.assertIn("回收站为空", self.run_todo("trash").stdout)
        self.run_todo("delete", "1")
        self.run_todo("clear")
        trash = self.run_todo("trash").stdout
        self.assertIn("已完成任务", trash)
        self.assertIn("[x]", trash)
        self.run_todo("purge", "2")
        self.assertNotIn("回收测试", self.run_todo("trash").stdout)
        self.run_todo("purge", "--all")
        self.assertIn("回收站为空", self.run_todo("trash").stdout)
        self.run_todo("restore", "99", code=1)
        self.run_todo("purge", "99", code=1)
        for args in (("trash", "x"), ("restore",), ("purge",),
                     ("purge", "--all", "1"), ("restore", "abc")):
            with self.subTest(args=args):
                self.run_todo(*args, code=2)

    def test_edit_priority_due_and_title(self):
        self.run_todo("add", "编辑任务")
        self.assertIn("[ ] | P2 | #1 | TODO | 编辑任务",
                      self.run_todo("list").stdout)
        self.run_todo("edit", "1", "-p", "P0")
        self.assertIn("[ ] | P0 | #1 | TODO | 编辑任务",
                      self.run_todo("list").stdout)
        self.run_todo("edit", "1", "新标题", "-p", "P1", "-d", "2026-10-02")
        self.assertIn("[ ] | P1 | #1 | TODO | 新标题 | 2026-10-02",
                      self.run_todo("list").stdout)
        self.run_todo("edit", "1", "-d", "2026-10-03")
        self.assertIn("新标题 | 2026-10-03", self.run_todo("list").stdout)
        self.run_todo("edit", "1", "改回标题")
        self.assertIn("改回标题", self.run_todo("list").stdout)
        for args in (("edit",), ("edit", "1"), ("edit", "abc", "x"),
                     ("edit", "1", "-p", "P9"), ("edit", "1", "-d", "bad"),
                     ("edit", "1", "-p", "P0", "-p", "P1")):
            with self.subTest(args=args):
                self.run_todo(*args, code=2)

    def test_calendar_and_reminders(self):
        self.run_todo("add", "日历提醒", "-d", "2026-10-05")
        self.run_todo("add", "另一提醒", "-p", "P0", "-d", "2026-10-20")
        out = self.run_todo("cal", "2026-10").stdout
        self.assertIn("2026年10月", out)
        self.assertIn("提醒事项（2026-10）", out)
        self.assertIn("日历提醒", out)
        self.assertIn("2026-10-05", out)
        self.assertIn("2026-10-20", out)
        self.assertIn("共 2 条提醒", out)
        today = datetime.date.today()
        for day in (5, 20):
            is_today = today.year == 2026 and today.month == 10 and today.day == day
            if not is_today:
                self.assertIn(f"{day}*", out)
        month11 = self.run_todo("-cal", "2026-11").stdout
        self.assertIn("2026年11月", month11)
        self.assertIn("本月没有提醒事项", month11)
        current = self.run_todo("cal").stdout
        self.assertIn("提醒事项（", current)
        for args in (("cal", "2026-13"), ("cal", "2026-1"), ("cal", "bad"),
                     ("cal", "2026-10", "extra")):
            with self.subTest(args=args):
                self.run_todo(*args, code=2)

    def test_project_names_and_titles_are_literal(self):
        project = "Python '学习'; DROP TABLE projects; --"
        title = "代码 '测试'; DROP TABLE todos; -- $HOME `id`"
        self.run_todo("project", "add", project)
        self.run_todo("add", project, title)
        self.assertIn(title, self.run_todo("project", "show", project).stdout)
        self.run_todo("add", "--project", project, "--", "--help")
        self.assertIn("[ ] --help", self.run_todo("project", "show", project).stdout)
        self.run_todo("add", "--", "--literal", "words")
        self.assertIn("--literal words", self.run_todo("project", "show", "TODO").stdout)

    def test_invalid_project_and_filter_arguments(self):
        self.run_todo("project", "add", "NanoX")
        self.run_todo("add", "NanoX", "keep")
        invalid = [
            ("project", "add"), ("project", "add", "   "),
            ("project", "show"), ("project", "show", "NanoX", "extra"),
            ("project", "remove", "NanoX"), ("project", "list", "extra"),
            ("add", "NanoX", "   "), ("add", "NanoX", "task", "--unknown"),
            ("add", "task", "--priority"), ("add", "task", "--project"),
            ("add", "task", "--due"), ("add", "task", "--due", "invalid"),
            ("list", "--priority"), ("list", "--project"),
            ("list", "NanoX", "--project", "NanoX"),
            ("list", "--all", "--done"), ("list", "--done", "--todo"),
            ("list", "-p", "P0", "-p", "P1"),
        ]
        for priority in ("P4", "P-1", "-1", "high", "P", "", "01", "9" * 40):
            invalid.extend([("add", "task", "-p", priority), ("list", "-p", priority)])
        for args in invalid:
            with self.subTest(args=args):
                self.run_todo(*args, code=2)
        for args in (("project", "add", "NanoX"), ("project", "show", "missing"),
                     ("list", "missing"), ("add", "missing", "task")):
            with self.subTest(args=args):
                self.run_todo(*args, code=1)
        self.assertIn("Progress: 0/1", self.run_todo("project", "show", "NanoX").stdout)
        self.assertNotIn("missing", self.run_todo("project", "list").stdout)

    def create_legacy_database(self):
        self.data.mkdir(parents=True)
        with closing(sqlite3.connect(self.data / "todo.db")) as db, db:
            db.executescript("""
                CREATE TABLE todos (
                    id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT NOT NULL,
                    priority INTEGER NOT NULL DEFAULT 2, done INTEGER NOT NULL DEFAULT 0,
                    created_at INTEGER NOT NULL, completed_at INTEGER, due TEXT
                );
                CREATE INDEX idx_todos_done ON todos(done);
                INSERT INTO todos VALUES(3, '旧待办', 1, 0, 100, NULL, '2026-10-01');
                INSERT INTO todos VALUES(8, '旧完成', 3, 1, 101, 200, NULL);
                INSERT INTO todos VALUES(20, '已删除', 2, 0, 102, NULL, NULL);
                DELETE FROM todos WHERE id = 20;
            """)

    def test_legacy_migration_preserves_data_and_deleted_ids(self):
        self.create_legacy_database()
        for _ in range(2):
            board = self.run_todo("project", "show", "TODO").stdout
            self.assertIn("[ ] 旧待办 (#3)", board)
            self.assertIn("[x] 旧完成 (#8)", board)
            self.assertIn("Progress: 1/2", board)
        with closing(sqlite3.connect(self.data / "todo.db")) as db, db:
            self.assertEqual(db.execute("PRAGMA user_version").fetchone()[0], 3)
            rows = db.execute("SELECT id, priority, created_at, completed_at, due, project_id FROM todos ORDER BY id").fetchall()
            self.assertEqual(rows, [(3, 1, 100, None, "2026-10-01", 1), (8, 3, 101, 200, None, 1)])
            self.assertEqual(db.execute("PRAGMA foreign_key_check").fetchall(), [])
            self.assertEqual(db.execute("PRAGMA integrity_check").fetchone()[0], "ok")
        self.run_todo("project", "add", "NanoX")
        self.assertIn("#21", self.run_todo("add", "NanoX", "new", "-p", "P0").stdout)
        self.run_todo("done", "3")
        self.assertIn("Progress: 2/2", self.run_todo("project", "show", "TODO").stdout)

    def test_newer_database_is_not_modified(self):
        self.create_legacy_database()
        with closing(sqlite3.connect(self.data / "todo.db")) as db, db:
            db.execute("PRAGMA user_version = 99")
        before = (self.data / "todo.db").read_bytes()
        self.assertIn("请升级", self.run_todo("list", code=1).stderr)
        self.assertEqual((self.data / "todo.db").read_bytes(), before)

    def test_failed_migration_rolls_back(self):
        self.create_legacy_database()
        with closing(sqlite3.connect(self.data / "todo.db")) as db, db:
            db.execute("CREATE INDEX idx_todos_project ON todos(title)")
        self.run_todo("list", code=1)
        with closing(sqlite3.connect(self.data / "todo.db")) as db, db:
            self.assertEqual(db.execute("PRAGMA user_version").fetchone()[0], 0)
            self.assertNotIn("project_id", [row[1] for row in db.execute("PRAGMA table_info(todos)")])
            self.assertEqual(db.execute("SELECT name FROM sqlite_master WHERE name = 'projects'").fetchall(), [])
            self.assertEqual(db.execute("SELECT COUNT(*) FROM todos").fetchone()[0], 2)
            db.execute("DROP INDEX idx_todos_project")
        self.assertIn("Progress: 1/2", self.run_todo("project", "show", "TODO").stdout)

    def test_invalid_input_does_not_modify_tasks(self):
        self.run_todo("add", "保留此任务")
        invalid = [(), ("unknown",), ("add",), ("add", "   "),
                   ("done",), ("delete",), ("list", "--unknown")]
        for command in ("done", "delete"):
            for task_id in ("0", "-1", "abc", "1x", " ", "9" * 40):
                invalid.append((command, task_id))
        for args in invalid:
            with self.subTest(args=args):
                self.run_todo(*args, code=2)
        for command in ("done", "delete"):
            self.assertIn("不存在", self.run_todo(command, "999", code=1).stderr)
        listing = self.run_todo("list").stdout
        self.assertIn("保留此任务", listing)
        self.assertIn("[ ]", listing)

    def test_title_is_stored_literally(self):
        title = "复习 '方程'; DROP TABLE todos; -- $HOME `id`"
        self.run_todo("add", title)
        self.assertIn(title, self.run_todo("list").stdout)

    @unittest.skipIf(os.name == "nt", "Linux XDG data directory")
    def test_xdg_data_directory(self):
        self.env.pop("TODO_CLI_DATA_DIR")
        self.run_todo("add", "XDG task")
        self.assertTrue((self.root / "xdg-data" / "todo-cli" / "todo.db").is_file())
        self.assertIn("XDG task", self.run_todo("list").stdout)


if __name__ == "__main__":
    unittest.main()
