#!/usr/bin/env python3
"""Clone vanilla Lava Burst (51505) into custom ranks 910001-910007.

Works on 3.3.5a Spell.dbc and SkillLineAbility.dbc.

Example:
    python3 patch_dbc.py --dbc-dir /home/you/azerothcore/env/dist/data/dbc
"""

from __future__ import annotations

import argparse
import shutil
import struct
import sys
from pathlib import Path

SPELL_TEMPLATE_ID = 51505
CUSTOM_RANKS = (
    # spell_id, level, min, max, rank_text
    (910001, 10, 81, 95, "Rank 1"),
    (910002, 20, 150, 171, "Rank 2"),
    (910003, 30, 309, 350, "Rank 3"),
    (910004, 40, 408, 460, "Rank 4"),
    (910005, 50, 624, 700, "Rank 5"),
    (910006, 60, 891, 1017, "Rank 6"),
    (910007, 70, 1015, 1160, "Rank 7"),
)

# 3.3.5 Spell.dbc field indexes (uint32 unless noted). See wowdev.wiki Spell
SPELL_F_ID = 0
SPELL_F_CAST_TIME = 28
SPELL_F_RECOVERY = 29
SPELL_F_BASE_LEVEL = 38
SPELL_F_SPELL_LEVEL = 39
SPELL_F_MANA_COST = 42
SPELL_F_RANGE = 46
SPELL_F_SPEED = 47  # float
SPELL_F_EFFECT1 = 71
SPELL_F_DIESIDES1 = 74
SPELL_F_BASEPOINTS1 = 80
SPELL_F_NAME = 136  # string offset, enUS is first of 16 + mask
SPELL_F_RANK = 153  # NameSubtext enUS
SPELL_F_DESC = 170  # Description enUS
SPELL_F_MANA_PCT = 204
SPELL_F_SPELL_CLASS_SET = 208
SPELL_F_CLASS_MASK2 = 210
SPELL_F_SCHOOL = 226


class DBC:
    def __init__(self, path: Path):
        data = path.read_bytes()
        if data[:4] != b"WDBC":
            raise ValueError(f"{path} is not a WDBC file")
        self.record_count, self.field_count, self.record_size, self.string_size = struct.unpack_from(
            "<IIII", data, 4
        )
        rec_bytes = self.record_count * self.record_size
        header = 20
        self.records = [
            bytearray(data[header + i * self.record_size : header + (i + 1) * self.record_size])
            for i in range(self.record_count)
        ]
        self.strings = bytearray(data[header + rec_bytes :])
        if len(self.strings) < self.string_size:
            self.strings.extend(b"\x00" * (self.string_size - len(self.strings)))

    def get_u32(self, rec: bytearray, field: int) -> int:
        return struct.unpack_from("<I", rec, field * 4)[0]

    def set_u32(self, rec: bytearray, field: int, value: int) -> None:
        struct.pack_into("<I", rec, field * 4, value & 0xFFFFFFFF)

    def set_i32(self, rec: bytearray, field: int, value: int) -> None:
        struct.pack_into("<i", rec, field * 4, value)

    def add_string(self, text: str) -> int:
        raw = text.encode("utf-8") + b"\x00"
        offset = len(self.strings)
        self.strings.extend(raw)
        return offset

    def find_id(self, spell_id: int) -> bytearray | None:
        for rec in self.records:
            if self.get_u32(rec, 0) == spell_id:
                return rec
        return None

    def write(self, path: Path) -> None:
        header = struct.pack(
            "<4sIIII",
            b"WDBC",
            len(self.records),
            self.field_count,
            self.record_size,
            len(self.strings),
        )
        path.write_bytes(header + b"".join(self.records) + bytes(self.strings))


def patch_spell(dbc_dir: Path) -> None:
    src = dbc_dir / "Spell.dbc"
    if not src.is_file():
        raise SystemExit(f"Missing {src}")

    backup = src.with_suffix(".dbc.bak")
    if not backup.exists():
        shutil.copy2(src, backup)
        print(f"Backed up {src} -> {backup}")

    dbc = DBC(src)
    template = dbc.find_id(SPELL_TEMPLATE_ID)
    if template is None:
        raise SystemExit(f"Spell.dbc has no id {SPELL_TEMPLATE_ID}")

    for spell_id, level, min_dmg, max_dmg, rank_text in CUSTOM_RANKS:
        existing = dbc.find_id(spell_id)
        rec = bytearray(existing if existing is not None else template)
        dbc.set_u32(rec, SPELL_F_ID, spell_id)
        dbc.set_u32(rec, SPELL_F_BASE_LEVEL, level)
        dbc.set_u32(rec, SPELL_F_SPELL_LEVEL, level)
        dbc.set_i32(rec, SPELL_F_BASEPOINTS1, min_dmg - 1)
        dbc.set_i32(rec, SPELL_F_DIESIDES1, max_dmg - min_dmg + 1)
        dbc.set_u32(rec, SPELL_F_MANA_PCT, 10)
        dbc.set_u32(rec, SPELL_F_RANK, dbc.add_string(rank_text))
        if existing is None:
            dbc.records.append(rec)
            print(f"  added Spell {spell_id} ({rank_text}, level {level}, {min_dmg}-{max_dmg})")
        else:
            idx = next(i for i, r in enumerate(dbc.records) if dbc.get_u32(r, 0) == spell_id)
            dbc.records[idx] = rec
            print(f"  updated Spell {spell_id} ({rank_text})")

    dbc.write(src)
    print(f"Wrote {src} ({len(dbc.records)} records)")


def patch_skill_line(dbc_dir: Path) -> None:
    src = dbc_dir / "SkillLineAbility.dbc"
    if not src.is_file():
        print(f"Skip SkillLineAbility.dbc (not found at {src})")
        return

    backup = src.with_suffix(".dbc.bak")
    if not backup.exists():
        shutil.copy2(src, backup)
        print(f"Backed up {src} -> {backup}")

    dbc = DBC(src)
    # Field 1 is Spell in 3.3.5 SkillLineAbility
    template = None
    for rec in dbc.records:
        if dbc.get_u32(rec, 1) == SPELL_TEMPLATE_ID:
            template = rec
            break
    if template is None:
        print("SkillLineAbility.dbc has no Lava Burst (51505) row; skip")
        return

    for spell_id, *_rest in CUSTOM_RANKS:
        already = any(dbc.get_u32(r, 1) == spell_id for r in dbc.records)
        if already:
            print(f"  SkillLineAbility already has {spell_id}")
            continue
        rec = bytearray(template)
        # Field 0 is ID. Assign a high unused id: 900000 + (spell_id - 910000)
        new_row_id = 900000 + (spell_id - 910000)
        dbc.set_u32(rec, 0, new_row_id)
        dbc.set_u32(rec, 1, spell_id)
        dbc.records.append(rec)
        print(f"  added SkillLineAbility row {new_row_id} -> spell {spell_id}")

    dbc.write(src)
    print(f"Wrote {src} ({len(dbc.records)} records)")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dbc-dir", required=True, type=Path, help="Directory containing Spell.dbc")
    args = parser.parse_args()
    dbc_dir = args.dbc_dir.expanduser().resolve()
    if not dbc_dir.is_dir():
        raise SystemExit(f"Not a directory: {dbc_dir}")

    patch_spell(dbc_dir)
    patch_skill_line(dbc_dir)
    print("Done. Copy the patched DBC files to the server DataDir/dbc and into a client patch MPQ.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
