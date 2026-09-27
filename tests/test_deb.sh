#!/bin/sh
# Run only in a disposable Debian container, as root:
# sh /src/tests/test_deb.sh /packages/new.deb [/packages/old.deb]
# TODO_DEB_REPOSITORY can point to a mounted directory with a Packages index.
set -eu
export DEBIAN_FRONTEND=noninteractive

package=${1:?Pass the new .deb path}
old_package=${2:-}
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

apt_install() {
    if [ -n "${TODO_DEB_REPOSITORY:-}" ]; then
        apt-get -o Dir::Etc::sourcelist=/tmp/todo-test-sources.list \
            -o Dir::Etc::sourceparts=- install -y "$1"
    else
        apt-get install -y "$1"
    fi
}

if [ -n "${TODO_DEB_REPOSITORY:-}" ]; then
    printf 'deb [trusted=yes] file:%s ./\n' "$TODO_DEB_REPOSITORY" > /tmp/todo-test-sources.list
    apt-get -o Dir::Etc::sourcelist=/tmp/todo-test-sources.list \
        -o Dir::Etc::sourceparts=- update
else
    apt-get update
fi

if [ -n "$old_package" ]; then
    apt_install "$old_package"
    export TODO_CLI_DATA_DIR=/tmp/todo-upgrade-data
    todo --version
    todo add "legacy pending" -p 1 -d 2026-10-01
    todo add "legacy completed" -p 3
    todo add "legacy deleted"
    todo done 2
    todo delete 3
fi

apt_install "$package"
dpkg-deb --info "$package"
todo --version
test "$(dpkg-query -W -f='${Version}' todo-cli)" = '0.2.0-1'
# Debian slim intentionally excludes man pages during installation.
package_dir=$(mktemp -d /tmp/todo-package.XXXXXX)
dpkg-deb --extract "$package" "$package_dir"
test -f "$package_dir/usr/share/man/man1/todo.1"
test -f "$package_dir/usr/share/doc/todo-cli/README.md"
test -f /usr/share/doc/todo-cli/copyright
test -f /usr/share/todo-cli/python/weather.py
test -f /usr/share/todo-cli/python/news.py

if [ -n "$old_package" ]; then
    todo project show TODO
    todo project add NanoX
    todo add NanoX "new task" --priority P0
    python3 - <<'PY'
import os
import sqlite3
from pathlib import Path

db = sqlite3.connect(Path(os.environ['TODO_CLI_DATA_DIR']) / 'todo.db')
rows = db.execute('SELECT id, title, priority, done, due, project_id FROM todos ORDER BY id').fetchall()
assert rows == [
    (1, 'legacy pending', 1, 0, '2026-10-01', 1),
    (2, 'legacy completed', 3, 1, None, 1),
    (4, 'new task', 0, 0, None, 2),
], rows
assert db.execute('PRAGMA integrity_check').fetchone()[0] == 'ok'
assert db.execute('PRAGMA foreign_key_check').fetchall() == []
db.close()
print('Installed-package upgrade preserved tasks and deleted IDs.')
PY
    unset TODO_CLI_DATA_DIR
fi

# Exercise the installed binary without root privileges, using temporary data.
su -s /bin/sh nobody -c 'python3 "$1/tests/test_cli.py" /usr/bin/todo' sh "$source_dir"
