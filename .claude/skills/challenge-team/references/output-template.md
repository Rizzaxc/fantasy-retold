# Output Template

Use this layout for the proposal (workflow step 2). Keep the team section **short**; put the depth in the analysis. Use tables where shown, so teams can be compared side by side across sessions.

---

## <Battle name> — <format>, Lv <AI range> vs player Lv <n>

**Brief:** <archetype / limitation> · <mon profile> · <side notes>. State any defaults you assumed.

**Game plan:** one sentence. *E.g. "Rain-boosted Water hits Soak and drain the front line, then Electric / Ice special hits turn the drained, Soaked targets into Trauma."*

### Team

| # | Species | Gimmick | Role | Why this pick |
| --- | --- | --- | --- | --- |
| 1 | Pelipper | — | Weather setter, pivot | Drizzle; Bold (Battle Armor) so exhaustion doesn't punish it for staying in |
| … | | | | |

* **Gimmick:** Mega / Tera <type> / Sig. <move> / Itemized <move> / item (only if it defines the role).
* **Role:** from meta.md §4 (offence, defence or support roles), one or two per mon.
* **Why this pick:** only what's unusual or decisive: an innate chosen through the nature, a Signature Move that has to land through Dodge, an item interaction, a rail combo. No full movesets or EVs at this stage.
* AI flags beyond `Smart Trainer` (e.g. `/ Will Suicide`), and the lead / ace order if it matters.

### Analysis

**1. Threat profile per mon**

| Mon | Punishes (player profiles) | Loses to (player profiles) |
| --- | --- | --- |
| Pelipper | Fire attackers, frail physical leads | Electric special attackers, Natural Cure walls |

Profiles are role + mechanic, e.g. "frail Trauma breaker", "Battle Armor physical wall", "Toxic staller", "Scarf cleaner", "Trick Room setter", "hazard lead".

**2. Team pressure map**
* What the team covers **two or more times over** (the player can't remove it with one answer).
* What it covers **once** (one KO or one status cracks the plan), and which mon carries it.
* **Deliberate holes**: the intended ways to win, named.

**3. Type analysis**
* Immunities; shared weaknesses (count members 2× to each type); resistances that matter.
* The 2× cap's effect (no 4× liabilities; what dual typings buy).
* Type ↔ rail interactions the team uses or is exposed to (Soak, thaw, Burn removal, Ground vs Electric line, crit combos).
* What the team's coverage can't hit.

**4. Support coverage**
Hazards (set / removal), status spreading, speed control, weather / terrain, screens, healing, redirection, phazing, pivoting: which mon does each, and which the team lacks (and whether that's deliberate).

**5. Player teams at Lv <n>** (3–4 of them; each a 2–3 mon core plus flex picks, built around a preference)

| Player team | Core | Flex | How it goes | Likely result |
| --- | --- | --- | --- | --- |
| Trauma breakers + status | Modest special breaker, Sassy Toxic wall | Scarf cleaner, hazard lead | Toxic + Trauma beat the rain walls; the Electric threat forces Pelipper out early… | Player favoured if they keep the cleaner healthy |

Projections name the turning point (the mon or turn where the fight swings) and the lead matchup. At least one player team should be a reasonable "generalist" diversification, not just a hard counter.

**6. Verdict**
* Difficulty for a hardcore player at this level: one line, with the reason.
* **Tuning knobs:** one or two changes to make it harder and one or two to make it easier (a nature / innate swap, an EV shift, a gimmick move, one species swap).
* **AI gaps:** each simple behaviour the plan uses that the AI can't do yet (proposed as separate tasks), or "none".

---

After approval and writing `trainers.party`, report only: the battle(s) written, any matchup that changed in the re-check, the doc updates, and anything left open.
