"""Exercise the public CLI in separate processes with isolated, persistent data."""

import os
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
        for command in ("add", "list", "done", "delete", "--help"):
            self.assertIn(command, help_text)
        self.assertFalse(self.data.exists())

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
