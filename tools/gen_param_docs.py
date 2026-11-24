#!/usr/bin/env python3
"""Generate docs/wiki/Parameters.md from the firmware's parameter table.

    python tools/gen_param_docs.py

The firmware table (src/config_store.cpp, kParams) is the single source of truth;
re-run this after adding or changing a parameter.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "config_store.cpp"
OUT = ROOT / "docs" / "wiki" / "Parameters.md"

ROW = re.compile(
    r'\{\s*"(?P<name>[\w.]+)"\s*,\s*PType::(?P<type>\w+)\s*,\s*"(?P<def>[^"]*)"\s*,\s*'
    r'(?P<min>[-\d.eE+]+)\s*,\s*(?P<max>[-\d.eE+]+)\s*,\s*(?P<reboot>true|false)\s*,\s*"(?P<desc>[^"]*)"\s*\}'
)

GROUPS = {
    "node": "Node identity",
    "sys": "System / status LED / USB stream",
    "mesh": "Meshtastic link (used by the mesh layer, not yet wired in)",
    "gps": "GPS",
    "env": "Environment sensor (BMP280 / BME280)",
    "imu": "IMU (BMI323)",
    "tamper": "Tamper detection",
    "mag": "Magnetometer (QMC5883L) and calibration",
    "radar": "Radar (LD2410C x3)",
}


def main() -> int:
    text = SRC.read_text(encoding="utf-8")
    block = text[text.index("kParams[]"): text.index("static constexpr size_t kCount")]
    rows = [m.groupdict() for m in ROW.finditer(block)]
    if not rows:
        print("no parameters parsed - table format changed?", file=sys.stderr)
        return 1

    out = [
        "# Parameter reference",
        "",
        "> **Generated file** - do not edit by hand. Run `python tools/gen_param_docs.py` after changing",
        "> the table in `src/config_store.cpp`.",
        "",
        f"{len(rows)} parameters. All of them can be changed live from the CLI (`set <name> <value>`) or the",
        "web app, and persist across power cycles once you `save`. Parameters marked **reboot** take",
        "effect only after `save` then `reboot`.",
        "",
    ]
    seen = []
    for r in rows:
        g = r["name"].split(".")[0]
        if g not in seen:
            seen.append(g)
    for g in seen:
        out += [f"## {GROUPS.get(g, g)}", "", "| Parameter | Type | Default | Range | Reboot | Description |", "|---|---|---|---|---|---|"]
        for r in (x for x in rows if x["name"].split(".")[0] == g):
            t = r["type"].lower()
            rng = "-" if t in ("bool", "str") else f'{r["min"]} to {r["max"]}'
            if t == "str":
                rng = f'up to {int(float(r["max"]))} chars'
            out.append(f'| `{r["name"]}` | {t} | `{r["def"]}` | {rng} | {"yes" if r["reboot"] == "true" else ""} | {r["desc"]} |')
        out.append("")
    OUT.write_text("\n".join(out), encoding="utf-8", newline="\n")
    print(f"wrote {OUT.relative_to(ROOT)} ({len(rows)} parameters)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
