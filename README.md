# mod-shaman-tweaks

AzerothCore module for a LAN WotLK server (including playerbots). Adds three
independent shaman changes. Each one is a `1` / `0` switch in
`shaman_tweaks.conf` — **no rebuild** after the first compile.

| Option | What it does |
| --- | --- |
| `ShamanTweaks.GhostWolfInstant` | Ghost Wolf is instant-cast |
| `ShamanTweaks.LavaBurstRefreshFlameShock` | Lava Burst refreshes **your** Flame Shock DoT on the target to full duration |
| `ShamanTweaks.LavaBurstExtraRanks` | Nine Lava Burst ranks from level 10, with the damage table below |

Set `ShamanTweaks.Enable = 0` to turn the whole module off.

## Lava Burst ranks

Vanilla rank 1 (51505) becomes **rank 8**. Vanilla rank 2 (60043) becomes **rank 9**.

| Rank | Spell ID | Level | Mana | Base damage |
| --- | --- | --- | --- | --- |
| 1 | 910001 | 10 | 10% base | 81–95 |
| 2 | 910002 | 20 | 10% base | 150–171 |
| 3 | 910003 | 30 | 10% base | 309–350 |
| 4 | 910004 | 40 | 10% base | 408–460 |
| 5 | 910005 | 50 | 10% base | 624–700 |
| 6 | 910006 | 60 | 10% base | 891–1017 |
| 7 | 910007 | 70 | 10% base | 1015–1160 |
| 8 | 51505 | 75 | 10% base | 1012–1290 |
| 9 | 60043 | 80 | 10% base | 1200–1550 |

Ranks are auto-taught on login and level-up (so playerbots pick them up without
visiting a trainer). The same ranks are also added to every shaman trainer that
already teaches vanilla Lava Burst.

If custom IDs `910001–910007` fail to load, the module falls back to teaching
vanilla 51505 at level 10 and **scaling its damage** to the table above. Bots
keep working in that mode because they already know spell 51505.

## Clean rebuild (server)

The module ships **everything the worldserver needs**: C++, config, world SQL,
and patched `Spell.dbc` / `SkillLineAbility.dbc`.

```bash
# 1. Place the folder in the core modules directory (keep this name)
cp -r mod-shaman-tweaks /path/to/azerothcore-wotlk/modules/

# 2. Reconfigure + rebuild the same way you already do
cd /path/to/azerothcore-wotlk
cmake --build build -j$(nproc)
# or: cd build && make -j$(nproc)
cmake --install build   # if you use an install prefix / env/dist

# 3. Install patched DBC into the server DataDir (required for named ranks)
bash modules/mod-shaman-tweaks/apps/install-server-data.sh
# If the script cannot find the core tree:
# bash modules/mod-shaman-tweaks/apps/install-server-data.sh /path/to/azerothcore-wotlk
# bash modules/mod-shaman-tweaks/apps/install-server-data.sh --datadir /path/to/server/data
```

Then start worldserver. On first boot it:

- copies `shaman_tweaks.conf.dist` → `shaman_tweaks.conf`
- applies `data/sql/db-world/base/2026_08_18_00_shaman_tweaks.sql`
  (`spell_dbc`, `spell_ranks`, `trainer_spell`, `spell_script_names`)

Confirm the log line:

```
Shaman Tweaks: loaded (GhostWolfInstant=1, RefreshFlameShock=1, ExtraRanks=1, CustomRankSpells=1)
```

`CustomRankSpells=1` means IDs 910001–910007 exist (DBC and/or SQL loaded).
If it prints `0`, the DBC copy did not land in `DataDir/dbc` and the SQL did
not apply — run the install script and/or import the SQL by hand.

After the first compile, toggles are `.conf` only (`.reload config` or restart).
Players / bots pick up rank changes on the next login or level-up.

If your core does not auto-import module SQL, run this against `acore_world`:

`data/sql/db-world/base/2026_08_18_00_shaman_tweaks.sql`

## Client display

The 3.3.5 **client** needs the same spell IDs in its own `Spell.dbc`. A ready
`patch-4.MPQ` was built for the Windows client (`tools/output/patch-4.MPQ`).
Put it in:

- `WoW/Data/patch-4.MPQ`
- `WoW/Data/enUS/patch-enUS-4.MPQ` (so locale patches do not override it)

Then delete the client `Cache/` folder and restart. Without the client patch,
bots still work server-side; your shaman falls back to scaled vanilla 51505.

## Uninstall

1. Set `ShamanTweaks.Enable = 0` and restart, **or** remove the module and rebuild.
2. Run `data/sql/uninstall/shaman_tweaks_uninstall.sql` against `acore_world`.
3. Relog shamans so leftover custom ranks are dropped.

## Playerbots

Bots learn ranks through the same login / level-up hook as players. If your
elemental bots already cast Lava Burst, they will cast the extra ranks as soon
as they know them (they look the spell up by name / family, and the module
copies the vanilla Lava Burst name and family flags onto the new IDs).

If a bot build hard-codes spell 51505 and ignores the new IDs, leave extra
ranks enabled anyway — the fallback path teaches 51505 at 10 and scales damage.

## Files

```
mod-shaman-tweaks/
  conf/shaman_tweaks.conf.dist
  src/shaman_tweaks.cpp
  src/shaman_tweaks_loader.cpp
  data/dbc/Spell.dbc                      # patched, for the server DataDir
  data/dbc/SkillLineAbility.dbc
  data/sql/db-world/base/2026_08_18_00_shaman_tweaks.sql
  data/sql/uninstall/shaman_tweaks_uninstall.sql
  apps/install-server-data.sh             # copies DBC after make install
  tools/output/patch-4.MPQ                # Windows client patch (already built)
```
