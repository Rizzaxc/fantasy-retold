#!/usr/bin/env python3
"""
Full move pool of a species for challenge-team building (design.md §2 "Engine").

Never read src/data/pokemon/all_learnables.json by hand: it is 2 MB, alphabetical,
uses its own species keys and pre-Gen-6 move names, and leaves out moves the game
adds elsewhere. This script merges every source:

  json      all_learnables.json entry (union of every official game's learnset)
  pre-evo   the json / level-up / egg pools of every earlier evolution stage
  level-up  src/data/pokemon/level_up_learnsets/gen_9.h
  egg       src/data/pokemon/egg_moves.h
  teach     src/data/pokemon/teachable_learnsets.h (generated TMs / tutors)
  universal special_movesets.json "universalMoves" (Return, Tera Blast, Substitute, ...)
            unless the species is TM_ILLITERATE
  event     EVENT_MOVES below (distribution-only moves; the json has none)
  form      FORM_MOVES below (Rotom's form-change moves)

Usage (run from the repo root):
  learnables.py Garchomp [Rotom-Wash ...]          print each pool
  learnables.py Garchomp --has "Stealth Rock" U-turn
  learnables.py --trainer TRAINER_WINONA_1 [...]   check every move + Signature Move
  learnables.py --trainer ALL                       check the whole trainers.party
"""

import argparse
import json
import pathlib
import re
import sys
import unicodedata
from collections import defaultdict

ROOT = pathlib.Path(__file__).resolve().parents[4]
DATA = ROOT / "src/data/pokemon"
SNAKIFY = re.compile(r"(?!^)([A-Z]+)")
# Moves a form gets on changing into it, which no learnset lists.
FORM_MOVES = {
    "ROTOM_HEAT": "MOVE_OVERHEAT",
    "ROTOM_WASH": "MOVE_HYDRO_PUMP",
    "ROTOM_FROST": "MOVE_BLIZZARD",
    "ROTOM_FAN": "MOVE_AIR_SLASH",
    "ROTOM_MOW": "MOVE_LEAF_STORM",
}
# Event-only moves (distributions), which the porymoves data behind all_learnables.json
# doesn't include. Later evolutions inherit them. Add entries as they come up.
EVENT_MOVES = {
    "PIKACHU": {"MOVE_EXTREME_SPEED"},
}


def norm(name: str) -> str:
    """'Rotom-Wash' / 'Farfetch'd' / 'Flabébé' / 'SPECIES_X' -> 'ROTOM_WASH' style key."""
    s = unicodedata.normalize("NFKD", name).encode("ascii", "ignore").decode()
    s = re.sub(r"^(SPECIES_|MOVE_)", "", s.strip(), flags=re.I)
    s = s.replace("'", "").replace(".", "")
    return re.sub(r"[^A-Za-z0-9]+", "_", s).strip("_").upper()


def load_aliases() -> dict[str, str]:
    txt = (ROOT / "include/constants/moves.h").read_text()
    return dict(re.findall(r"^\s*(MOVE_\w+)\s*=\s*(MOVE_\w+)\s*,", txt, re.M))


ALIASES = load_aliases()


def canon(move: str) -> str:
    m = move if move.startswith("MOVE_") else "MOVE_" + norm(move)
    while m in ALIASES:
        m = ALIASES[m]
    return m


def c_arrays(path: pathlib.Path, suffix: str) -> dict[str, set[str]]:
    out = {}
    for name, body in re.findall(r"static const \w+ s(\w+)" + suffix + r"\[\] = \{(.*?)\n\};", path.read_text(), re.S):
        key = SNAKIFY.sub(r"_\1", name).upper()
        out[key] = {canon(m) for m in re.findall(r"MOVE_\w+", body)} - {"MOVE_UNAVAILABLE", "MOVE_NONE"}
    return out


def evolutions() -> dict[str, set[str]]:
    """child -> set of direct pre-evolutions, from species_info `.evolutions = EVOLUTION(...)`."""
    parents = defaultdict(set)
    for f in (DATA / "species_info").glob("*.h"):
        txt = f.read_text()
        heads = [(m.start(), m.group(1)) for m in re.finditer(r"\[SPECIES_(\w+)\]\s*=", txt)]
        for i, (start, sp) in enumerate(heads):
            end = heads[i + 1][0] if i + 1 < len(heads) else len(txt)
            for m in re.finditer(r"\.evolutions\s*=\s*EVOLUTION\(", txt[start:end]):
                j, depth = start + m.end(), 1
                while depth and j < len(txt):
                    depth += {"(": 1, ")": -1}.get(txt[j], 0)
                    j += 1
                # each top-level {METHOD, PARAM, SPECIES_TARGET, CONDITIONS(...)} group: only
                # the first species is the target (later ones are party / trade conditions)
                body, depth, group = txt[start + m.end():j - 1], 0, ""
                for ch in body:
                    if ch == "{":
                        depth += 1
                    if depth:
                        group += ch
                    if ch == "}":
                        depth -= 1
                        if not depth:
                            child = re.search(r"SPECIES_(\w+)", group)
                            if child and child.group(1) != sp:
                                parents[child.group(1)].add(sp)
                            group = ""
    return parents


class Pools:
    def __init__(self):
        self.json = {norm(k): {canon(m) for m in v} for k, v in json.loads((DATA / "all_learnables.json").read_text()).items()}
        self.lvl = c_arrays(DATA / "level_up_learnsets/gen_9.h", "LevelUpLearnset")
        self.egg = c_arrays(DATA / "egg_moves.h", "EggMoveLearnset")
        self.teach = c_arrays(DATA / "teachable_learnsets.h", "TeachableLearnset")
        special = json.loads((DATA / "special_movesets.json").read_text())
        self.universal = {canon(m) for m in special["universalMoves"]}
        self.illiterate = set()
        tt = ROOT / "tools/learnset_helpers/build/all_teaching_types.json"
        if tt.exists():
            for e in json.loads(tt.read_text()):
                if isinstance(e, dict) and e["teaching_type"] == "TM_ILLITERATE":
                    self.illiterate.add(SNAKIFY.sub(r"_\1", e["name"]).upper())
        self.parents = evolutions()
        self.known = set(self.json) | set(self.lvl) | set(self.teach)

    def resolve(self, name: str) -> str | None:
        """Exact key, else drop trailing form segments (AEGISLASH_SHIELD -> AEGISLASH)."""
        key = norm(name)
        parts = key.split("_")
        for n in range(len(parts), 0, -1):
            k = "_".join(parts[:n])
            if k in self.known:
                return k
        return None

    def ancestors(self, key: str) -> list[str]:
        out, todo = [], list(self.parents.get(key, ()))
        while todo:
            p = todo.pop()
            if p not in out:
                out.append(p)
                todo.extend(self.parents.get(p, ()))
        return out

    def pool(self, key: str) -> dict[str, set[str]]:
        """move -> set of source labels."""
        src = defaultdict(set)
        for label, table in (("json", self.json), ("level-up", self.lvl), ("egg", self.egg), ("teach", self.teach)):
            for m in table.get(key, ()):
                src[m].add(label)
        for m in EVENT_MOVES.get(key, ()):
            src[m].add("event")
        if key in FORM_MOVES:
            src[FORM_MOVES[key]].add("form change")
        if key not in self.illiterate:
            for m in self.universal:
                src[m].add("universal")
        for a in self.ancestors(key):
            ak = self.resolve(a) or a
            for table in (self.json, self.lvl, self.egg, EVENT_MOVES):
                for m in table.get(ak, ()):
                    src[m].add("pre-evo " + ak)
        return src


def pretty(move: str) -> str:
    return move[5:].replace("_", " ").title()


def check_trainers(pools: Pools, wanted: list[str]) -> int:
    txt = (ROOT / "src/data/trainers.party").read_text()
    blocks = re.split(r"^=== (TRAINER_\w+) ===\s*$", txt, flags=re.M)
    want_all = [w.upper() for w in wanted] == ["ALL"]
    bad = 0
    found = set()
    for i in range(1, len(blocks), 2):
        tr, body = blocks[i], blocks[i + 1]
        if not want_all and tr not in wanted:
            continue
        found.add(tr)
        for mon in re.split(r"\n\s*\n", body):
            lines = [l.strip() for l in mon.strip().splitlines()]
            lines = [l for l in lines if l and not l.startswith(("#", "/*", "//"))]
            if not lines or ":" in lines[0].split("@")[0]:
                continue  # trainer header block
            head = re.sub(r"\((M|F)\)", "", lines[0].split("@")[0]).strip()
            nick = re.search(r"\(([^()]+)\)\s*$", head)  # "Nickname (Species)"
            species = (nick.group(1) if nick else head).strip()
            moves = [l[1:].strip() for l in lines if l.startswith("-")]
            moves += [l.split(":", 1)[1].strip() for l in lines if l.lower().startswith("signature move:")]
            if not moves:
                continue
            key = pools.resolve(species)
            if key is None:
                print(f"{tr}: {species}: species not found")
                bad += 1
                continue
            pool = pools.pool(key)
            for mv in moves:
                if canon(mv) not in pool:
                    print(f"{tr}: {species}: {mv} not learnable")
                    bad += 1
    if not want_all:
        for w in set(wanted) - found:
            print(f"{w}: no such trainer block")
            bad += 1
    if not bad:
        print("all moves learnable")
    return 1 if bad else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("species", nargs="*")
    ap.add_argument("--has", nargs="+", metavar="MOVE", help="check these moves instead of printing the pool")
    ap.add_argument("--trainer", nargs="+", metavar="TRAINER_X", help="validate trainers.party blocks (or ALL)")
    ap.add_argument("--sources", action="store_true", help="show where each move comes from")
    a = ap.parse_args()
    pools = Pools()
    if a.trainer:
        return check_trainers(pools, a.trainer)
    if not a.species:
        ap.print_help()
        return 1
    rc = 0
    for sp in a.species:
        key = pools.resolve(sp)
        if key is None:
            print(f"{sp}: species not found")
            rc = 1
            continue
        pool = pools.pool(key)
        if a.has:
            for mv in a.has:
                c = canon(mv)
                hit = pool.get(c)
                print(f"{key}: {pretty(c)}: " + (f"yes ({', '.join(sorted(hit))})" if hit else "NO"))
                rc |= 0 if hit else 1
            continue
        print(f"{key} ({len(pool)} moves):")
        if a.sources:
            for m in sorted(pool):
                print(f"  {pretty(m)}  [{', '.join(sorted(pool[m]))}]")
        else:
            print("  " + ", ".join(pretty(m) for m in sorted(pool)))
    return rc


if __name__ == "__main__":
    sys.exit(main())
