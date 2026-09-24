#!/usr/bin/env python3
import email.utils
import sys
import urllib.request
import xml.etree.ElementTree as ET

DEFAULT_FEEDS = (
    "https://news.ycombinator.com/rss,"
    "http://feeds.bbci.co.uk/news/world/rss.xml"
)
TIMEOUT = 15
LIMIT = 10

ATOM = "{http://www.w3.org/2005/Atom}"


def force_utf8_output():
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "todo-cli/0.1"})
    with urllib.request.urlopen(req, timeout=TIMEOUT) as resp:
        return resp.read()


def format_time(s):
    if not s:
        return ""
    try:
        dt = email.utils.parsedate_to_datetime(s)
        return dt.strftime("%Y-%m-%d %H:%M")
    except Exception:
        return s


def parse_feed(raw):
    root = ET.fromstring(raw)
    channel = root.find(".//channel")
    if channel is not None or root.tag == "rss":
        items = root.findall(".//channel/item")
        return [
            (it.findtext("title") or "(无标题)",
             it.findtext("link") or "",
             format_time(it.findtext("pubDate") or ""))
            for it in items
        ]
    entries = root.findall(f"{ATOM}entry")
    if entries:
        out = []
        for e in entries:
            title = e.findtext(f"{ATOM}title") or "(无标题)"
            link_el = e.find(f"{ATOM}link")
            link = link_el.get("href", "") if link_el is not None else ""
            out.append((title, link, format_time(e.findtext(f"{ATOM}updated") or "")))
        return out
    return []


def show_feed(url):
    print(f"\n【来源】 {url}")
    try:
        items = parse_feed(fetch(url))
    except Exception as exc:
        print(f"  获取失败: {exc}", file=sys.stderr)
        return 1
    if not items:
        print("  (无条目)")
        return 0
    for idx, (title, link, when) in enumerate(items[:LIMIT], 1):
        print(f"{idx}. {title.strip()}")
        if when:
            print(f"   时间: {when}")
        if link:
            print(f"   链接: {link}")
    return 0


def main():
    force_utf8_output()
    feeds = sys.argv[1].split(",") if len(sys.argv) > 1 else DEFAULT_FEEDS.split(",")
    failed = 0
    for url in feeds:
        url = url.strip()
        if not url:
            continue
        failed += show_feed(url)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
