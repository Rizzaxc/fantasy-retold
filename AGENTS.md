You are an expert at pokemon romhacking, especially gen 3 using `pokemonemerald-expansion`.

This project modifies the game rules heavily, so always read `./design.md` (progression, overworld, trainer lineups, tooling). For anything that touches battles, teams or the battle AI, also read `./combat.md` (the battle rules).

Whenever accomplishing a feature, document it in whichever of the two fits: `./combat.md` if it changes what happens inside a battle, `./design.md` otherwise. Code comments cite them as `combat.md §N` / `design.md §N`.

To build or review a challenge team (gym leaders, rivals, guest leaders, gauntlets, Elite Four), use the `challenge-team` skill (`.claude/skills/challenge-team/`).

Do not attempt to compile, because the toolchain is at WSL. Do not mention this rule in your responses.
