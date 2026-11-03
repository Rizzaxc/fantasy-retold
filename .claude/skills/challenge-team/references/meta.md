# Fantasy Retold Meta for Teambuilders

`combat.md` holds the rules; this file explains what they mean when building a team. If the two disagree, `combat.md` wins: fix this file.

## 1. Stats, re-read

| Vanilla stat | What it does here | Building consequence |
| --- | --- | --- |
| HP | HP, minus Trauma, is all a mon can ever heal back to | Bulk against Trauma means HP + Def, not HP alone |
| Atk | Physical damage **and** Stamina attrition on the target (incoming BP/10, ×2 SE / ×0.5 NVE) | Physical attackers drain the target's resources and set up exhaustion |
| Def | Mitigation against **both** physical and special hits | The one defensive stat. Bulk Up, Iron Defense and Cotton Guard cover everything |
| SpA | Special damage; 25–65% of it is logged as Trauma (more on a drained target) | Special attackers are the wall-breakers and finishers |
| SpD | **Stamina pool** (`BaseSta` = the stat value). Its *stage* reduces Trauma taken (×0.85 per +1) | SpD EVs and +SpD natures buy pool size. Calm Mind is offence + Trauma guard, with no physical bulk |
| Spe | Turn order, times `0.15 + CurrentSta/BaseSta` | Speed decays as Stamina drains; a fast frail mon is fast for 2–3 turns |

**Quick maths** (the stat formula is vanilla):
* Stamina pool ≈ the SpD stat. At Lv50 with 31 IV / 252 EV / neutral: base 60 → ~112, base 100 → ~152, base 130 → ~182.
* Attack cost = BP/10 + 0.02×pool (a 90 BP hit from a 150 pool costs ~12). Status move = 10. Dodge = 5 + 0.02×pool (+20 per consecutive use). Speed-boosting setup = 0.
* Physical hit received drains BP/10 (×2 SE). A 150 pool trading 90 BP physical hits (~12 out, ~9 in) is exhausted in ~7 turns; against special attackers, in ~12.
* Rest on switch-out: 5 + Level/2 (Lv50 → 30). Recuperate: +40% of the pool, only at ≤ 40%.
* Effective Speed: full Stamina = 1.15×, 40% = 0.55×, empty = 0.15×. Two equal-speed mons flip order once one has spent noticeably more.
* Damage is halved: a neutral STAB hit from an even-level attacker is typically a 4–6HKO. A +2 boosted hit ≈ an unboosted vanilla hit (2HKO). There are no reliable OHKOs, so every plan has to survive return fire.

## 2. Mon profile → set

The brief's profile (power / bulk / speed) maps to these levers:

| Push toward | EVs | Nature (= innate) | Items | Moves |
| --- | --- | --- | --- | --- |
| **Power** | Atk/SpA + Spe, or Atk/SpA + HP | Adamant / Modest (Sheer Force, but no Lv1 status procs), Reckless group (Super Luck, keeps procs) | Life Orb, Choice Band / Specs (no Dodge / Recuperate, §9), type boosters | High-BP STAB, coverage that removes the targets' answers |
| **Bulk** | HP + Def (all-round bulk), HP + SpD (pool: longer clock, more Dodges) | Bold / Impish (Battle Armor), Calm / Careful (Resilient), Relaxed / Lax (Regenerator), Sassy / Gentle (Natural Cure) | Leftovers, Rocky Helmet, Assault Vest (+50% pool), Eviolite | Recovery, status, phazing, Body Press |
| **Speed** | Spe + attack stat | Jolly / Timid (Quick Start: +1 Spe on entry) | Choice Scarf (never fully Dodged; Scarf Signature ignores Dodge), Focus Sash | Priority (beats Dodge's order: only halved), pivot moves |

A **Stamina-heavy spread** (HP / SpD) is a real third bulk axis: it sets the time-to-exhaustion against physical pressure and pays for Dodge. Physical walls want HP / Def; anti-Trauma walls want HP / Def plus a Trauma answer (Resilient, Calm Mind stages).

## 3. Natures are ability picks

Every nature carries an innate on top of the mon's own ability (`combat.md` §5). Pick the nature for the innate first, then check the stat spread still works:

* **Battle Armor (Bold, Impish):** crit-immune, so exhaustion auto-crits and crit-fishers do nothing. The safest wall innate. The Smart AI plays it well (no panic Recuperate; counts status / hazards / setup as progress).
* **Resilient (Calm, Careful):** no Special Trauma. Hard-stops Trauma breakers; still open to crits, Toxic, Frostbite and Nightmare.
* **Regenerator (Relaxed, Lax):** heal on switch-out. The Smart AI pivots on it (≤ 2/3 HP).
* **Natural Cure (Sassy, Gentle):** status cleanup on switch-out. The Smart AI pivots on Lv2+ lines. Strong into status teams.
* **Sheer Force (Adamant, Modest):** +30% damage, no Lv1 procs. Breakers.
* **Super Luck (Reckless group):** crit-fishers; crits log Trauma and ignore boosts.
* **Quick Start (Jolly, Timid):** +1 Speed on entry. The Smart AI re-enters to reset Speed drops.
* **Advantage (neutral group):** a guaranteed hit when acting first. Rarely worth it on a boss.

The AI's **innates are visible to the player only once they fire**, which helps the AI: a hidden Battle Armor or Resilient punishes a player who commits to the wrong breaker.

## 4. Roles under the new rules

### Offence

* **Physical exhauster.** Drains Stamina so later hits auto-crit; multi-hit and SE physical moves drain the most. It sets up its teammates more than it kills.
* **Trauma breaker (special).** Turns drained targets into unhealable damage; the answer to Recuperate walls. Pairs with an exhauster.
* **Crit-fisher.** Super Luck + high-crit moves / Scope Lens. Ignores Def boosts, hard-walled by Battle Armor.
* **Cleaner / revenge killer.** Priority or Scarf; finishes exhausted or low-HP targets. Priority is never fully Dodged, only halved.
* **Bulky setup.** Bulk Up, Calm Mind, Quiver Dance, Dragon Dance (Speed setup costs no Stamina). Target: 2–2.5 KOs, not a sweep.

### Defence

* **Recuperate wall.** High HP / Def / pool; out-sustains chip. Beaten by Trauma, Toxic and Lv2+ statuses (Recuperate cures Lv1 only).
* **Pivot.** U-turn / Volt Switch / Parting Shot / Regenerator. A frail pivot's rest bonus is most of its pool; a bulky pivot gains little from resting.
* **Status absorber.** Natural Cure, Guts / Poison Heal / Orb holders (they *want* a Lv2 status).

### Support

* **Hazard lead.** Stealth Rock / Spikes / Toxic Spikes on turn 1 (the Stamina AI keeps a +3-rated status move, so a fresh hazard goes up). Hazard chip has to stay scarce (§7's meta lever): one setter per team.
* **Suicide lead.** Explosion-type moves are halved like everything else and can be Dodged (a faster dodger makes them miss entirely). Only worth it as **hazards + a half-HP explode**. Needs `AI: Smart Trainer / Will Suicide`; without that flag the AI doesn't value self-KO moves.
* **Status spreader.** Status moves land at Lv2 and stack to Lv3. Toxic is the anti-wall clock; Will-O-Wisp (Burn ×2: −20% physical damage) cripples exhausters; Thunder Wave → Shocked, next Electric application → Paralyzed; Yawn → Drowsy at once. The Smart AI climbs rails and avoids self-sabotage (e.g. Ice into a burn).
* **Speed control setter.** Tailwind, Trick Room, Icy Wind / Electroweb, Paralysis, Chill. Smart AI scores speed control on *effective* speed (rails × Stamina decay) and goes for a decisive flip (+4).
* **Weather / terrain setter.** Drizzle, Drought, Psychic Surge, etc. Weather teams double as a type and rail plan (rain powers Water, which Soaks: Electric / Ice ×1.5 taken).
* **Healer.** Wish, Heal Pulse, Life Dew, Floral Healing. Every heal is capped at maxHP − Trauma, so healers stall poorly against Trauma teams. Poison blocks heals of maxHP/16 or less. `combat.md` §3 says heals scale with Stamina; check the code before relying on it for anything other than Recuperate.
* **Screens.** Reflect / Light Screen are both live (unified Defense still checks the category for screens). They buy setup turns.
* **Redirection (doubles).** Follow Me / Rage Powder; with a Trick Room setter or a setup mon.
* **Phazer / hazard control.** Roar, Whirlwind, Dragon Tail; Defog / Rapid Spin. They undo setup sweepers (the player's 1-for-2 plan).

## 5. Type analysis under the 2× cap

* Effectiveness is capped at **2× / 0.5×**; **immunities stay 0×**. A 4× weakness is only 2×, a double resistance only 0.5×.
* So **dual types are mostly upside**: a type can't be OHKO-exploited for 4×, and immunities are the only hard walls. Grade a team by **immunities, shared weaknesses (how many members are 2× to one type), and coverage holes**, not by 4× counts.
* SE physical hits also drain **double Stamina** (×2 attrition). A physical attacker with SE coverage is an exhaustion engine; a team sharing a physical weakness gets exhausted together.
* **Type ↔ rail interactions** (`combat.md` §6) belong in type analysis:
  * Water hits Soak (20%): Speed ×0.85, Electric / Ice ×1.5 taken, Fire ×0.5. A Soak on a burned target removes a Burn stack.
  * Fire thaws the whole Chill line, Ice removes Burn stacks, Ground downgrades the Electric line, and non-Ghost hits can wake a sleeper (Ghost never does).
  * Types are immune to their own lines (Water: Soak; Ice: Chill; Fire: Burn; Poison: Poison; Electric: Electric line; Ghost: Sleep line; Psychic: Confusion line).
  * Combos: Ghost crit on Drowsy / Asleep → Nightmare; Psychic crit on Confused / Stupefied → Frightened; Dazed + Soaked → straight to Paralyzed.

## 6. Gimmicks

* **Tera** (Smart Tera): a defensive Tera removes the team's shared weakness on the ace; an offensive Tera breaks the counter the player brought. Tera pairs with Dodge / Recuperate (combat.md §3).
* **Mega**: the AI Mega Evolves whenever it holds the stone. Budget it as the team's second ace.
* **Signature Move** (every fully evolved member, `Signature Move:` line; never one of its four moves): 8 PP, never fully Dodged (halved at worst), full damage through Dodge from a Scarf holder. Make it the move the plan most needs to land through a Dodge: the ace's best STAB, or a breaker's key coverage.
* **Itemized Move** (held TM): costs the item slot, 4 PP. Worth it only when the move covers a hole and the mon doesn't need its item.

## 7. Formats

* **Singles.** The Stamina clock decides it. Lead with a hazard setter or an exhauster; the AI's pivoting (Regenerator / Natural Cure / Quick Start) is a real asset.
* **Doubles.** Spread moves (×0.75) chip both foes' HP and, if physical, their Stamina. Speed control and redirection are worth more. A Stupefied mon can hit its own ally: give the team Own Tempo / Psychic types or avoid relying on single-target nukes next to a confusion-weak partner.
* **Tag team** (guest leaders, gauntlets): back to back with no heal. HP, PP, Signature PP and Lv2+ statuses carry over; Stamina is fresh each battle and Trauma resets per battle (its HP loss stays). Plan the first team to **leave lasting damage**: Toxic, Burn ×2, chip. In guest tag / doubles, each leader brings only its **3 strongest** (`design.md` §2, Guest Gym Leaders), so make the intended three actually rank highest.

## 8. What the AI can pilot

Use this list to filter plans. Where the right column applies, drop the plan, unless the gap is simple, in which case keep it and propose the AI fix as a separate task.

| AI-native (Smart Trainer does it right) | Not pilotable (don't build around it) |
| --- | --- |
| Taking KOs; punishing an exhausted target | Baton Pass chains (needs `Prefer Baton Pass`, and the AI passes poorly) |
| Dodge when a hit would KO or is a sure miss; reading the player's Dodge rate | Deliberate sacrifice to get a safe switch-in for the ace |
| Recuperate when bulkier; avoiding self-exhaustion; futile-Recuperate check | Sleep Talk / Rest loops, or Substitute + Pain Split timing |
| Hazards on turn 1; status climbing; rail combos; no self-sabotage | Choice-lock planning beyond scoring (combat.md §11, Not yet) |
| Speed control on effective speed, incl. decisive flips | Weather-timer management (re-setting weather on the last turn) |
| Natural Cure / Regenerator / Quick Start pivoting | Doubles combos needing both mons' turns planned together (beyond the scored Soak + Electric / Ice, Follow Me) |
| Signature / Itemized Move scoring as slots 4–5 | |
| Mega Evolution with the stone; Smart Tera | |
| Explosion with `/ Will Suicide` | |

**Simple gaps** (keep, propose a fix) are single-condition score tweaks of the kind already in `battle_ai_fr_tactics.c`: "prefer X when the target has rail Y", "switch out when Z", "value move M while field state F holds". Anything that needs look-ahead over several turns, or the partner's choice, is not simple.
