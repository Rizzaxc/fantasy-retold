# Fantasy Retold: System Architecture & Design Document

**Target Engine:** `pokeemerald-expansion`

---

## 1. System Philosophy & Core Design Shift

*Fantasy Retold* transforms the traditional high-frequency switching, type-immunity-driven meta of vanilla Pokémon into an individual-focused, resource-managed tactical JRPG battle system. Battles are intentionally longer, more deliberate, and defined by individual matchup positioning and attrition management.

### Primary Engine Overhauls

* **Halved Damage Profile:** Final damage output is multiplied by **0.5 inside the damage formula**. Crucially, **move Base Power values remain vanilla** — the halving applies only to HP/Trauma output, *not* to the BP figures consumed by the Stamina formulas (action cost, attrition). This keeps the Stamina economy paced off real BP (~90 for a typical attacking move) while HP totals last twice as long.
* **Unified Defense:** The traditional **Defense** stat handles mitigation for both Physical and Special attack formulas. The **Special Defense** stat is no longer a mitigation stat.
* **Stat Repurposing:** The **Special Defense** stat block is stripped of its vanilla mitigation function and rebuilt as **Stamina** (its stat value becomes `BaseSta`). Its in-battle *stat stage* is repurposed as a Trauma-resistance modifier (see §4).
* **Type Effectiveness Caps:** Maximum type weakness or resistance multipliers are hard-capped at **2x** (4x weaknesses and 4x resistances are compressed down to 2x).

---

## 2. The Stamina Economy & Speed Mechanics

Stamina serves as the primary operational currency governing action speed, move execution, and defensive integrity.

### The Stat Block (`BaseSta` & `CurrentSta`)

* `BaseSta` is the repurposed Special Defense stat, used at its native scale (no rescaling — EV/IV expectations are preserved).
* **Stamina persists across switches.** A Pokémon's first send-out of the battle enters fresh at `CurrentSta = BaseSta`. On every **switch-out**, it banks its current Stamina plus a flat **+50 rest bonus** (capped at `BaseSta`); it returns to the field at that banked value. Speed decay is a function of `CurrentSta/BaseSta`, so it carries across switches too — an exhausted mon that pivots out comes back slow *and* near-empty.

#### Why a flat +50 (frail pivots, bulk plants)

The rest bonus is a flat amount, not a percentage, so it deliberately scales *against* pool size:

* **Frail mon** (small `BaseSta`, e.g. 80): +50 is most of the tank — pivoting is a near-full reset, and a frail mon (which can't take hits) is *expected* to rotate out. Worth ~2 attacks + a Dodge on return.
* **Bulky mon** (large `BaseSta`, e.g. 250): +50 is a sliver. **Recuperate** (+50% of `BaseSta` = +125, in place) is the far better reset, so the wall has no resource reason to leave the field — it plants and grinds the attrition war, which is its job.

This is the lever that lets switching stay frail-favorable without the old full-refresh bribing bulky mons off the field. It also kills resource-refresh cycling outright: nobody gets a free top-to-full by tapping the bench.

### Stamina Costs

* **Action Drain:** Executing an attack costs Stamina equal to the move's $\frac{\text{Base Power (BP)}}{10}$ (vanilla BP). Status moves carry a flat execution cost of **10 Stamina**.
* **Body Tax:** Damaging attacks and Dodge additionally cost $0.02 \times \text{BaseSta}$ Stamina. This is a **gentle top-end compressor** that scales with pool size, preventing very-high-Stamina mons from acting indefinitely. *Status moves and Recuperate are exempt from the Body Tax.*
* **Physical Damage Attrition:** Absorbing a Physical attack chunks the target's `CurrentSta` by $\frac{\text{BP of incoming move * TypeEffectiveness}}{10}$. TypeEffectiveness is one of ${1, 2, 0.5}$. For multi-hit moves, the BP is the sum of total hits. Special attacks do **not** drain target Stamina — they route to Trauma instead; see §4.

### Stamina-Driven Speed Decay

Turn priority is dynamically re-calculated at the start of every combat round using an operational multiplier tied directly to the current state of the fuel tank:

$$\text{Effective Speed} = \text{Base Speed} \times \left(0.15 + \frac{\text{CurrentSta}}{\text{BaseSta}}\right)$$

* **The Exhaustion Cliff:** Fast but resource-inefficient attackers experience severe speed degradation across consecutive turns.
* **The Auto-Crit State:** If `CurrentSta` hits **0**, the Pokémon enters the *Exhaustion Zone*. Any incoming damaging move received while at 0 Stamina triggers an **automatic critical hit**.

### Pacing Targets (the Physical Clock)

Stamina depletion (own actions + incoming physical attrition) governs the **physical** time-to-death. With vanilla BP (~90 average), the design lands:

* **Frail mons:** ~5–7 turns to death.
* **Mid-bulk:** ~7–10 turns to exhaustion.
* **Giga-bulk:** ~10+ turns to exhaustion.

These bands assume physical pressure. Against a **special** attacker, Stamina drains roughly half as fast (no incoming attrition), and the time-to-death is instead governed by the **Trauma clock** (§4), which keys off the target's Defense, not its Stamina.

---

## 3. Innate System Commands (L/R Shoulder Buttons)

Every Pokémon possesses two innate mechanical actions mapped directly to the controller shoulder buttons. These commands execute with high-priority sequencing, functioning identically to item usage windows. **Selecting an Innate Command consumes the Pokémon's action for that turn** (it does not attack).

### Dodge (L Button)

An active maneuvering defense mechanism balanced by compounding resource taxes.

* **Stamina Cost:** $5 + (20 \times \text{Consecutive Uses}) + (0.02 \times \text{BaseSta})$. The consecutive-use term resets only after a turn passes without clicking Dodge (alternating Dodge with other actions keeps it cheap).
* **Speed Tiers & Outcomes** (compared on **round-start Effective Speed order**):
  * **Attacker is Slower:** The move completely misses.
  * **Attacker is Faster, or Tie:** The move lands, but damage is mitigated by **50%** and cannot proc status effects.
  * **Repeated Move Trapping:** If the user is dodging an identical move executed by the opponent on the immediate prior turn, the move completely misses, regardless of speed tiers.
* **Restrictions:** Status moves bypass Dodge checks entirely (but can still miss, via its Acc. check).

#### Dodge Layering vs. Traditional Accuracy/Evasion

Dodge is a mechanical command completely distinct from standard accuracy and evasion checks. Old accuracy mechanics remain fully functional:

1. When an attack is executed, the game first processes traditional parameters (the move's native accuracy pool, accuracy/evasion stage modifiers, and status-inflicted accuracy reductions). If a move fails this baseline accuracy check, it misses organically.
2. The custom Dodge resolution mechanics only evaluate *after* a move has successfully passed its native accuracy roll. It dictates whether a confirmed hit is cleanly evaded, mitigated by 50%, or hits completely based on speed priority and repetition rules.

### Recuperate (R Button)

The primary sustain mechanism used to stabilize an active Pokémon's resource depth. **It is the wincon engine for bulky mons** — its purpose is to let a tank out-sustain chip damage, with the Trauma pool (§4) acting as the inescapable counter-clock.

* **Mechanical Resolution:** Instantly restores `CurrentSta` by exactly $50\%$ of `BaseSta`. Exempt from the Body Tax.
* **Sustain Properties:** Grants a minor HP heal and purges **1 random Level 1 status condition** from the user's status rail. (It cannot touch Level 2+ statuses — persistent conditions and Trauma are how walls are beaten.)
* **Scaling Ceiling:** Any operational HP healing function in the engine scales dynamically using `CurrentSta` as its primary scaling vector ($\propto r = \frac{\text{CurrentSta}}{\text{BaseSta}}$), capping healing efficiency when exhausted.

---

## 4. Damage Modality: Health vs. Trauma

To prevent eternal recovery stalling in a format where damage is halved, health tracking is split into two independent bars.

### Health Pool (HP)

Standard health tracking that registers current match vitality. HP can be replenished using standard in-game items, abilities, or the Recuperate action.

### Trauma Pool

Permanent, unhealable physical degradation that shrinks the maximum structural capacity of the HP bar **for the duration of the engagement** (Trauma persists across switches — only Level 1 statuses clear on switch). When accumulated Trauma reaches a Pokémon's original max HP, it faints.

#### Special Move Translation (the Trauma split)

Every Special hit splits its output between the recoverable HP channel and the permanent Trauma channel, keyed to how drained the target is:

$$\text{Trauma} = D \times \left[a + b\,(1 - r)\right], \qquad \text{HP damage} = D \times \left(1 - [a + b\,(1-r)]\right)$$

where `D` is the (already halved) Special damage, $r = \frac{\text{CurrentSta}}{\text{BaseSta}}$, and the capped fraction $[a + b(1-r)] \le 1$.

* **Constants:** `a = 0.25` (floor — Trauma even versus a fresh target), `b = 0.40` (drain bonus). At full Stamina (`r=1`) **40%** of a Special hit becomes permanent Trauma; at 0 Stamina (`r=0`), **80%**.
* **Design role:** Physical attackers shred Stamina (lowering `r`); Special attackers punish the drained target by converting an escalating share to Trauma. This is the core offensive combo, and it is what lets a Special attacker beat a Recuperate-stalling wall — Recuperate heals HP but can never refund the shrinking ceiling.

#### Special Defense Stage = Trauma Resistance

The repurposed **Special Defense stat *stage*** (from Calm Mind, Quiver Dance, etc.) reduces **only the Trauma channel** of incoming Special hits:

* Each `+1` SpD stage multiplies incoming Special Trauma by **0.85** (bottoms at ~0.38 at `+6`).
* It affects the **Trauma channel only** — never the HP-damage channel (handled by unified Defense), never `BaseSta`/Stamina, never Speed.
* Consequence: Calm Mind is a *special-leaning* bulky setup that gives **zero** defensive value against Physical attacks, keeping it self-limiting. Quiver Dance (SpA + Speed + SpD-trauma-guard) is the build to watch for over-performance.

### Critical Hit Amplification

* Critical hits bypass standard stat-stage modifiers.
* The bonus damage portion generated by a critical strike is applied directly to the target's pool as **Trauma**, regardless of whether the move was classified as Physical or Special.
* In the Exhaustion Zone (`CurrentSta = 0`), every incoming damaging move auto-crits, so its bonus portion is dumped straight into Trauma.

---

## 5. Reworked Character Natures

Vanilla nature stat modifiers (+/- to standard stats) are entirely preserved to maintain core stat-tier progression and classic competitive identity. Every nature also grants an innate, (near-)vanilla ability. **Design rule:** an ability's impact must read the same in every game without simulating the board — flat / on-trigger effects only, no relative-comparison or stat-decay modifiers (a "resist speed-decay" ability was rejected as brittle).

### Neutral & Under-used Offense (fallback groups)

| Group                             | Natures                                                 | Innate Ability                                                                                                                                                            |
| --------------------------------- | ------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Neutral**                       | Hardy, Docile, Serious, Bashful, Quirky                 | **Advantage** — if acting ahead of the target, offensive moves with ≥70% native accuracy bypass accuracy/evasion for a guaranteed functional hit (still Dodge-checkable). |
| **Reckless** (under-used offense) | Lonely, Brave, Naughty, Mild, Quiet, Rash, Hasty, Naive | **Super Luck** *(vanilla)* — permanent +1 critical-hit stage.                                                                                                             |

### Meta Offense (bespoke)

| Nature      | Spread      | Innate Ability                                                                       |
| ----------- | ----------- | ------------------------------------------------------------------------------------ |
| **Adamant** | +Atk / −SpA | **Sheer Force** *(vanilla)* — +30% damage, but attacks no longer apply Lv1 statuses. |
| **Modest**  | +SpA / −Atk | **Sheer Force** *(vanilla)* — as above, the special wall-breaker.                    |
| **Jolly**   | +Spe / −SpA | **Quick Start** *(custom)* — +1 Speed stage on switch in..                           |
| **Timid**   | +Spe / −Atk | **Quick Start** *(custom)* — as above, for special attackers.                        |

### Defense (four wall abilities)

Walls get the same breadth as attackers: four abilities, each answering one of a wall's enemies. Pick the nature for the answer you want.

| Natures           | Spread   | Innate Ability                                                                                                         | Answers                    |
| ----------------- | -------- | ---------------------------------------------------------------------------------------------------------------------- | -------------------------- |
| **Bold, Impish**  | +Def     | **Battle Armor** *(vanilla)* — completely immune to critical hits (incl. the 0-Stamina auto-crit).                     | crit / exhaustion pressure |
| **Calm, Careful** | +SpD/Sta | **Resilient** *(custom)* — +1 Stamina stage on switch in.                                                              | the permanent-damage clock |
| **Relaxed, Lax**  | +Def     | **Regenerator** *(vanilla)* — restores some HP when it switches out.                                                   | HP attrition               |
| **Sassy, Gentle** | +SpD/Sta | **Natural Cure** *(vanilla+)* — cured of every status when it switches out & downgrades the first status on switch in. | the status / Toxic clock   |

### Notes

* **Meaningful trade, not an upgrade:** Adamant (Sheer Force: power, no status) vs a +Atk Reckless nature (Super Luck: crit/Trauma, keeps status). Each wall ability likewise answers one threat while leaving the others open — Resilient blunts Trauma but not status; Natural Cure cleanses status but not a Trauma nuke; Battle Armor stops crits but not Toxic.
* **Off-meta uplift:** handing Regenerator / Natural Cure to the off-meta walls (Relaxed, Lax, Sassy, Gentle) gives them a reason to see play, diversifying the wall pool.
* **[Alt — Jolly/Timid]** On-KO snowball (Moxie / Grim Neigh, +1 Atk/SpA per KO) remains an offense-flavored option, naturally capped by halved damage.
* **[Alt — Modest]** Tinted Lens (ignore one resistance step) if Adamant/Modest should differ.
* **[Tune]** Resilient's Trauma fraction and Regenerator's heal amount are designer-side; player text stays plain ("halves Trauma," "heals on switch").

---

## 6. Systemic Status Overhaul

### Core Model

* **Multi-stage rails.** A condition advances through levels. Applying the **same kind** again stacks it upward (Lv1 → Lv2 → Lv3). **Status moves apply directly at Level 2**; **attacking-move secondary effects apply at Level 1**.
* **Persistence.** **Level 1 is volatile** (clears automatically on switch-out). **Level 2 and above are persistent** (require active curing or mechanical expiration; survive switches).
* **Proc rates.** Status effect triggers on all attacking moves have their baseline application probabilities **doubled**.
* **Cleansing.** Recuperate purges 1 random **Level 1** condition only.

### The Condition Core

**Soaked** *(Lv1, single-stack)*

* Lowers effective action Speed. Reduces incoming Fire-type damage; scales up incoming Electric- and Ice-type damage.
* Water-type immuned

**Chilled → Frozen ‖ Frostbitten**

* **Progression:** Successive Ice applications / Status moves advance Chilled (Lv1) into a **random** branch — either **Frozen** or **Frostbitten** (Lv2). There is no direct way to apply the Lv2 forms.
* **Modifiers:** Chilled reduces Speed and adds a flat increase to all active Stamina action costs. **Frozen** forces a complete loss of turn actions. **Frostbitten** bypasses HP tracking to tick damage directly into the Trauma pool.
* **Removal:** Naturally expires after 2 active turns if un-renewed. **Instantly thaws if hit by, or executing, a Fire-type move.**
* Ice-type immuned

**Burned** *(stacks up to 3)*

* **Exclusions:** Mutually exclusive — cannot apply to a target afflicted by Soaked or any Ice-based status; downgrades when using or being hit by Water or Ice moves.
* **Modifiers:** Compounding damage ticks over time; degrades Physical damage output per stack.
* Fire-type immuned

**Poison → Toxic**

* **Progression:** Baseline Poison rules govern Level 1; status-driven application upgrades to Toxic (Level 2).
* **Modifiers:** Toxic ticks increase each consecutive turn and **bypass HP mitigation, carving directly into the Trauma pool.** *(Cap the escalation to prevent runaway lethality.)* Toxic is the primary hard-counter to pure Recuperate-stalling walls (Recuperate cannot cleanse a Lv2).
* Poison-type immuned

**Dazed → Shocked → Paralyzed** *(Electric line)*

* **Dazed** (Lv1): accuracy penalty. **Shocked** (Lv2): accuracy penalty, increased incoming Electric damage and Electric-status chance. **Paralyzed** (Lv3): massively lowers Speed + chance to skip turn.
* **Downgrades** when using, or being hit by, Ground moves.
* Electric-type immuned

**Drowsy → Asleep → Nightmare** *(Sleep line)*

* **Drowsy** (Lv1): scales down Speed, inflates Stamina costs. **Asleep** (Lv2): denies action execution. **Nightmare** (Lv3): denies action execution while draining health directly as permanent Trauma.
* **Duration & Valve:** Base duration **3 turns**. Reduced by **1 turn each time the target is struck by a direct damaging attack** (valve applies across the whole rail, including Nightmare). Cannot be refreshed or extended while an active sleep state is rolling; advancing to Nightmare inherits the remaining counter (no reset).

**Confused -> Stupefied-> Frightened** *(Confusion line)*

- Confused (Lv 1): Chance to miss/ hit itself

- Stupefied (Lv 2): Chance to target itself/ teammate with a move

- Frightened (Lv 3): Chance to randomly switch

- Effects in this chain are cummulative

- Lasts 3 turns if not reapplied. Each stack preserves the current count (ie turn 1 Confused, turn 2 Stupefied --> the chain still has 2 turns)



---

## 7. Archetype Balance & Setup

### Design Targets

* A setup sweeper should be **viable**, but its expected value in the **optimal case** (late game, favorable matchup) is a trade of **1 for 2–2.5**, not a clean sweep.
* A hyper-offense team of **3–4 setup sweepers must be entirely non-viable.**

### Self-Capping via Halved Damage

Halved damage means stat boosts buy back roughly what the reduction removes:

* A `+2` boosted hit ≈ an **unboosted vanilla hit** → a **2HKO** of a typical target. The sweeper takes return fire on every kill, so a frail body manages **~2–2.5 kills before dying** — exactly the target ceiling.
* Replicating a clean **OHKO** requires `+4` (two setup turns), which is unreachable against competent play. The 6-0 sweep is therefore gated off by the same mechanic.

### Setup Stamina Rule

* **Speed-boosting setup moves cost no Stamina** (Dragon Dance, Agility, Quiver Dance, etc.). This lets the sweeper enter its sweep at full Effective Speed, so the decay fuse begins when it starts attacking — improving the *consistency* of reaching ~2–2.5 kills without raising the ceiling.

### Why Hyper-Offense Stays Non-Viable

1. **No OHKOs (halved damage) = guaranteed interaction.** Every target gets to Dodge / Recuperate / hit back; HO's non-interactive-removal premise is gone.
2. **The 2–2.5 cap means no sweeper cleans a team**; subsequent sweepers face fresh bulk, universal Dodge, and Recuperate, trading toward 1-for-1.
3. **No defensive backbone = never reaches the late-game windows** where 2–2.5 is achievable. HO trades evenly early and is ground out.

### Viable Frail / Offensive Roles

* **Cleaner / revenge killer:** exploit the Exhaustion Zone and auto-crit to finish softened targets.
* **Crit-fisher:** Super Luck (any Reckless nature) + crit→Trauma ignores the defender's defensive boosts entirely — the system's native answer to "frail offense vs bulk." (Hard-walled by Battle Armor, which is crit-immune — break those walls with Special/Toxic Trauma instead.)
* **Bulky setup:** Bulk Up (boosts unified Defense) and Calm Mind / Quiver Dance (offense + Trauma-resistance) survive return fire and win by attrition.

### The Meta Lever

The honesty of the archetype rests on **setup-opportunity scarcity** — pivots, hazard chip, and forced switches must be kept scarce so that the "one free setup turn" remains a late-game payoff rather than a turn-1 default. This is a movepool/meta knob, not a formula.

---

## 8. Environmental Field Hazards

**[TBD - For Further Consideration]**

* *Placeholder for action-taxing hazards built around a Set-mode / trade-heavy gameplay loop. Intended as a secondary global clock alongside Trauma.*

---

## 9. Held Items & Accessories

**[TBD - For Further Consideration]**

* *Placeholder for Choice accessories, attrition recovery items, and system interaction tuning.*
* **Life Orb**
* **Leftover**
* **Choice Band, Specs**: vanilla, but choice-locked moves don't auto miss against Dodge. Disable L&R
* **Choice Scarf**: moves can't be Dodged, at worst still 50% damage. Signature moves deal 100%. Disable L&R

### Sp. Def items → Stamina (consequence of unified Defense)

Items whose only job was boosting the (now-removed) Sp. Def mitigation stat are redirected onto the **Stamina pool** (`BaseSta`), keeping their "bulky" identity. Implemented via `GetBaseStamina()`:

* **Assault Vest** → **+50% BaseSta** (still locked out of status moves). A bigger fuel tank / slower exhaustion.

* **Deep Sea Scale** (Clamperl) → ×2 BaseSta. **Soul Dew** (old-gen Lati@s) → +50% BaseSta.

* **Eviolite** already boosts the unified Defense (mitigation), so it is left as-is (not double-dipped into Stamina).

* *Still open:* sandstorm Sp.Def boost for Rock types is currently inert — redirect to BaseSta or drop (audit). ==> change to BaseSta

* **Choice accessories** are redesigned as high-variance offensive tools that offer immediate shield-breaking capability at the cost of complete defensive isolation.

---

## 10. Expansion Mechanics

### Signature Moves

Every Pokémon features an integrated Signature Move that sits outside the traditional 4-slot move framework. These actions are structurally impossible to dodge completely; at worst, they will always connect at a minimum of 50% damage against a faster dodging target. *(Signature moves are damaging attacks and pay the standard action cost + Body Tax; per-move BP/type TBD.)*

### Itemized Moves

Allows players to expand tactical flexibility by slotting an active, usable combat move directly into a Pokémon's held-item accessory slot. *(Mutually exclusive with other held items, including Choice accessories.)*

### Repurposed Moves (consequence of unified Defense)

* **Psyshock / Psystrike / Secret Sword** — their vanilla gimmick (special move that hits physical Def) is meaningless now that all special moves hit Def. Repurposed identity: **special damage that exhausts like a physical hit** — they apply the Physical Stamina attrition (`BP/10`, ×2 SE, per hit) and deal **no Trauma** (they opt out of the Special→Trauma split). The special-side stamina-shredders. *(Attrition implemented in Phase 2's hook; the no-Trauma opt-out lands with the Phase 3 split.)*

---

## 11. Tuning Constants (Quick Reference)

| System     | Constant                        | Value                                               |
| ---------- | ------------------------------- | --------------------------------------------------- |
| Damage     | Output multiplier               | ×0.5 (in damage formula; BP stays vanilla)          |
| Type chart | Effectiveness cap               | 2x max (weakness and resistance)                    |
| Stamina    | Action cost                     | BP/10                                               |
| Stamina    | Status move cost                | 10 (flat)                                           |
| Stamina    | Body Tax (attacks + Dodge)      | +0.02 × BaseSta                                     |
| Stamina    | Physical attrition              | incoming BP/10 (×2 if SE, x0.5 if NE)               |
| Stamina    | Speed decay                     | BaseSpeed × (0.15 + CurrentSta/BaseSta)             |
| Stamina    | Exhaustion (0 Sta)              | incoming damage auto-crits                          |
| Stamina    | Persistence                     | carries across switches (not refilled to full)      |
| Stamina    | Switch-out rest bonus           | +50 flat (capped at BaseSta); first send-out = full |
| Dodge      | Cost                            | 5 + 20×consecutive + 0.02×BaseSta                   |
| Recuperate | Stamina restore                 | +50% BaseSta (Body-Tax-exempt)                      |
| Trauma     | Special split (floor `a`)       | 0.25                                                |
| Trauma     | Special split (drain bonus `b`) | 0.40                                                |
| Trauma     | SpD stage resistance            | ×0.85 per +1 stage (Trauma channel only)            |
| Status     | Proc rate                       | ×2 baseline                                         |
| Status     | Sleep base duration             | 3 turns (−1 per damaging hit)                       |
| Setup      | Speed-boosting setup cost       | 0 Stamina                                           |
| Nature     | Neutral                         | Advantage (guaranteed hit if faster)                |
| Nature     | Reckless (under-used offense)   | Super Luck (+1 crit stage)                          |
| Nature     | Adamant / Modest                | Sheer Force (+30% dmg, no Lv1 status)               |
| Nature     | Jolly / Timid                   | Quick Start (+1 Speed on entry)                     |
| Nature     | Bold / Impish                   | Battle Armor (crit immune)                          |
| Nature     | Calm / Careful                  | Resilient (halves Trauma)                           |
| Nature     | Relaxed / Lax                   | Regenerator (HP on switch-out)                      |
| Nature     | Sassy / Gentle                  | Natural Cure (status cleared on switch)             |
| Pacing     | Time-to-death (physical)        | frail 5–7 / mid 7–10 / giga 10+ turns               |
| Sweep      | Optimal-case trade              | 1 for 2–2.5                                         |

### Open Items

* Average move BP confirmation (drives the Stamina pacing — assumed ~90).
* Validate the Special/Trauma clock against a high-Def/high-Sta wall lands in the ~10–15 turn window.
* Body Tax % final value (1.5–2% range; currently 2%).
* Nature ability magnitudes (Crusher +50%, Disruptor `a→0.50`, Resilient −25%) — validate vs the wall-breaking triangle.
* Cap on Toxic Trauma escalation (§6).
* Field Hazards and Held Item systems (§8, §9).
