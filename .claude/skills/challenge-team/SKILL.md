---
name: challenge-team
description: Design and analyse Fantasy Retold challenge teams (gym leaders and rematches, rivals, guest leaders, gauntlets, Elite Four / Champion) under the game's reworked battle rules, then write them into src/data/trainers.party once approved. Use when the user asks to build, rework, tune or review a boss / gauntlet / rival team, or gives a team brief (format, archetype, mon profile, player level).
---

# Challenge Team Builder

Build hard, fair boss teams for Fantasy Retold. This is about **gameplay under the new rules**: what the team does, which mechanics it uses, and what beats it. It is not a legality checker; pick legal sets as a matter of course, but spend the effort on the game plan.

## Read first (live, every time)

1. `combat.md`: the battle rules. It is the source of truth; never work from memory or from a copy. Sections you will lean on: §2 Stamina (incl. AI Stamina Management), §3 Dodge / Recuperate, §4 Trauma, §5 nature innates, §6 status rails, §7 archetype targets, §9 held items, §10 Signature / Itemized Moves, §11 Smart Trainer AI.
2. `design.md` §2: **Gym & Rival Lineups** and **Gym Leader Rematches** (level curve, EV budgets, party size, gimmick rules), **Gauntlets**, **Guest Gym Leaders** (tag team / doubles formats, the "strongest 3" pick), **Elite Four & Champion levels**.
3. [references/meta.md](references/meta.md): how the rules reshape roles, stats, types and support, and what the AI can pilot.
4. [references/output-template.md](references/output-template.md): the exact output layout.
5. The current party in `src/data/trainers.party`, if the battle already exists (the `=== TRAINER_X ===` block).

## Inputs

Ask only for what's missing and can't be inferred; take sensible defaults otherwise and state them.

| Input | Values | Default |
| --- | --- | --- |
| **Format** | single, double, tag team (two trainers back to back, no heal, or a two-trainer double) | the battle's existing format |
| **Limitation / archetype** | types, region, weather, status, physical / special, offense / defense, trick room, ... | none (or the gym type) |
| **Mon profile** | how many mons; how far each is pushed toward power, bulk or speed (shapes moves, items, EVs, natures) | 6, balanced |
| **Player level** | the level the player is expected to bring (or a placement: "after gym 5", "Winstrate") | from `design.md` §2 lineup tables |
| **Side notes** | anything else: a mon that must appear, a story beat, a mechanic to show off | none |

## Assumptions (always)

* **The player is hardcore.** By any challenge fight they have more options than they can use (Egg shop, gym rewards, lab trade, mart TMs, mints, Bottle Caps). Don't model what they "probably" have; model what a good player brings at that level.
* **The AI team is maxed out.** 31 IVs (`IVs: 0 Atk` on special attackers), the full EV budget for the slot, every gimmick the slot allows (Mega, Tera, Signature Move on each fully evolved member), natures chosen for their innate.
* **`AI: Smart Trainer` always**, plus extra flags a role needs (e.g. `/ Will Suicide` for a suicide lead; see meta.md).
* **Only builds the AI can pilot.** Drop any plan that needs judgement the AI doesn't have and that would be complex to add. If a plan's logic is **simple** but the current AI can't do it yet, you may keep it, and you must propose the AI fix as a separate task (see Workflow step 4).
* Respect the slot's rules in `design.md` §2 (party size, levels, EV caps, banned species, no bag items for big battles).

## Workflow

1. **Read** the files above.
2. **Proposal + analysis** (no file edits yet). Follow `references/output-template.md`:
   * the team, **short**: species, gimmick, role, and anything unusual that makes the pick good (a nature picked for its innate, a Signature Move choice, an item interaction);
   * the analysis against the player at the given level: per-mon threat profile, team pressure map and deliberate holes, type analysis, support coverage, 3–4 plausible player teams with a projection each, verdict and tuning knobs.
3. **Stop and wait for approval.** The user may redirect; iterate on the short form, which is cheap.
4. **AI gaps.** For every simple behaviour the plan relies on that the AI can't do yet, propose a separate task (a new session card, own initiative) with: the behaviour, where it would go (`src/battle_ai_fr_tactics.c`, `src/battle_ai_main.c` Stamina steps, `src/battle_ai_switch.c`), and a test idea. Never implement AI changes inside this skill's run.
5. **On approval, write it.** Edit the `=== TRAINER_X ===` block(s) in `src/data/trainers.party` directly: full sets (moves, nature, EVs, IVs, item, ability, `Tera Type:`, `Signature Move:`), keeping the file's conventions (see the Winona / Flannery blocks). Then:
   * re-run the analysis briefly on the final sets (did a move or EV choice change a matchup?), and report only what changed;
   * update `design.md` §2 (the lineup table / team bullets, or the gauntlet entry) and remove any `TODO` marker for that battle in `trainers.party`;
   * if the work changes a battle rule, that belongs in `combat.md`, not here.

## Quality bar

* **Hard but fair.** Every team has at least two clean lines of counterplay a hardcore player can find. A team with no fair answer is unfair, not hard; a team one profile walks through is easy.
* **One game plan, stated in one sentence**, with each mon's role serving it. Support roles count: a hazard lead, a status spreader, a speed setter or a redirector is often what makes the plan work.
* **Use the new rules, don't fight them.** No plan relies on OHKOs (damage is halved); Stamina, Trauma, the status rails and Dodge are where advantage comes from (meta.md).
* **Stay within §7's targets**: a setup sweeper trades 1-for-2–2.5 at best; never build an AI hyper-offense team that only works if the player misplays.
