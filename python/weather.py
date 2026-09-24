#!/usr/bin/env python3
import json
import sys
import urllib.parse
import urllib.request

TIMEOUT = 15
API = "https://wttr.in/{city}?format=j1"


def force_utf8_output():
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "todo-cli/0.1"})
    with urllib.request.urlopen(req, timeout=TIMEOUT) as resp:
        return resp.read().decode("utf-8")


def first(d, key):
    items = d.get(key) or [{}]
    if not items or not isinstance(items[0], dict):
        return {}
    return items[0]


def value(item, key):
    v = item.get(key, "")
    return v if v not in ("", None) else "-"


def main():
    force_utf8_output()
    city = " ".join(sys.argv[1:])
    url = API.format(city=urllib.parse.quote(city))
    try:
        raw = fetch(url)
    except Exception as exc:
        print(f"天气查询失败: {exc}", file=sys.stderr)
        return 1
    try:
        data = json.loads(raw)
    except ValueError:
        print("天气数据解析失败", file=sys.stderr)
        return 1

    cur = first(data, "current_condition")
    area = first(data, "nearest_area")
    area_name = value(first(area, "areaName"), "value")
    region = value(first(area, "region"), "value")
    country = value(first(area, "country"), "value")
    where = "、".join(x for x in (area_name, region, country) if x != "-")
    if not where:
        where = city or "未知位置"

    desc = value(first(cur, "lang_zh"), "value")
    if desc == "-":
        desc = value(first(cur, "weatherDesc"), "value")
    temp = value(cur, "temp_C")
    feels = value(cur, "FeelsLikeC")
    humidity = value(cur, "humidity")
    wind = value(cur, "windspeedKmph")
    wind_dir = value(cur, "winddir16Point")
    pressure = value(cur, "pressure")
    observed = value(cur, "observation_time")

    print(f"位置: {where}")
    print(f"天气: {desc}")
    print(f"温度: {temp} °C (体感 {feels} °C)")
    print(f"湿度: {humidity}%")
    print(f"风速: {wind} km/h {wind_dir}")
    print(f"气压: {pressure} hPa")
    print(f"观测时间: {observed}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
