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
* **Accelerated Overworld Cadence:** Outside of battle, link / wireless, and test-runner sensitive paths, the main callback loop advances `OVERWORLD_LOGIC_UPDATES_PER_FRAME` times per rendered frame (currently **4x**). Music mixing and map-music cadence remain on the original VBlank schedule, so BGM tempo stays vanilla; **sound effects stay tied to game logic** and therefore accelerate with the faster simulation. The play-time counter also remains VBlank-based so the save clock tracks real time rather than simulation ticks.
* **Battle Pace Compression:** Battles keep their controller/menu loop at **1x** for stability, but combat presentation is accelerated in two safe ways: (1) stable in-battle visual/task updates run `BATTLE_VISUAL_UPDATES_PER_FRAME` times per rendered frame (currently **3x**) **only while `gMain.callback2 == BattleMainCB2`**, so party/bag/menu handoffs remain untouched; and (2) `BATTLE_SPEED_MULTIPLIER` (currently **2x**) shortens battle-script pauses, battle text delay, and battle animation-script frame waits. This speeds up combat flow without reintroducing the menu/switch hangs caused by multi-stepping the full battle loop.

---

## 2. The Stamina Economy & Speed Mechanics

Stamina serves as the primary operational currency governing action speed, move execution, and defensive integrity.

### The Stat Block (`BaseSta` & `CurrentSta`)

* `BaseSta` is the repurposed Special Defense stat, used at its native scale (no rescaling — EV/IV expectations are preserved).
* **Stamina persists across switches.** A Pokémon's first send-out of the battle enters fresh at `CurrentSta = BaseSta`. On every **switch-out**, it banks its current Stamina plus a level-scaled rest bonus of **5 + Level / 2** (capped at `BaseSta`); it returns to the field at that banked value. Speed decay is a function of `CurrentSta/BaseSta`, so it carries across switches too — an exhausted mon that pivots out comes back slow *and* near-empty.
* **UI note:** Both healthboxes show Stamina as a **bar directly under the HP bar, the same length** (no number), so the two read side by side. It fills left to right as `CurrentSta/BaseSta`: **green** normally, **yellow** at or below the Recuperate threshold (`RECUPERATE_STAMINA_PERCENT`, 40%; a grey notch under the bar marks the threshold) so the player can see when R is live, and **empty with a red frame** at 0 Stamina (Exhaustion Zone). Any non-zero Stamina draws at least 1px. Exhaustion / auto-crit checks always use the exact live `CurrentSta`, not the drawn gauge.

#### Why a level-scaled rest bonus (frail pivots, bulk plants)

The rest bonus is a flat formula ($5 + \text{Level}/2$), not a percentage, so it deliberately scales *against* pool size:

* **Frail mon** (small `BaseSta`, e.g. 80): the rest bonus is most of the tank — pivoting is a near-full reset, and a frail mon (which can't take hits) is *expected* to rotate out. Worth ~2 attacks + a Dodge on return.
* **Bulky mon** (large `BaseSta`, e.g. 250): the rest bonus is a sliver. **Recuperate** (+40% of `BaseSta` = +100, in place, but only once the user is at 40% max Stamina or lower) is the far better reset, so the wall has no resource reason to leave the field — it plants and grinds the attrition war, which is its job.

This is the lever that lets switching stay frail-favorable without the old full-refresh bribing bulky mons off the field. It also kills resource-refresh cycling outright: nobody gets a free top-to-full by tapping the bench.

### Stamina Costs

* **Action Drain:** Executing an attack costs Stamina equal to the move's $\frac{\text{Base Power (BP)}}{10}$ (vanilla BP). Status moves carry a flat execution cost of **10 Stamina**.
* **Body Tax:** Damaging attacks and Dodge additionally cost $0.02 \times \text{BaseSta}$ Stamina. This is a **gentle top-end compressor** that scales with pool size, preventing very-high-Stamina mons from acting indefinitely. *Status moves and Recuperate are exempt from the Body Tax.*
* **Physical Damage Attrition:** Absorbing a Physical attack chunks the target's `CurrentSta` by $\frac{\text{BP of incoming move * TypeEffectiveness}}{10}$. TypeEffectiveness is one of ${1, 2, 0.5}$. For multi-hit moves, the BP is the sum of total hits. Special attacks do **not** drain target Stamina — they route to Trauma instead; see §4.

### Stamina-Driven Speed Decay

Turn priority is dynamically re-calculated at the start of every combat round using an operational multiplier tied directly to the current state of the fuel tank:

$$\text{Effective Speed} = \text{Base Speed} \times \left(0.15 + \frac{\text{CurrentSta}}{\text{BaseSta}}\right)$$

* **The Exhaustion Cliff:** Fast but resource-inefficient attackers experience severe speed degradation across consecutive turns.
* **The Auto-Crit State:** If `CurrentSta` hits **0**, the Pokémon enters the *Exhaustion Zone*. Any incoming damaging move received while at 0 Stamina triggers an **automatic critical hit**. The check uses the target's **live pre-hit Stamina**, not a turn-start snapshot: if a mon exhausts itself earlier in the round, later incoming hits auto-crit; if a physical hit is the thing that drains Stamina to 0, that same hit does **not** retroactively become a crit. If Battle Armor / Shell Armor blocks that exhaustion auto-crit, the battle log explicitly announces that the target's armor blocked the critical hit; if the Bold / Impish innate armor effect is the blocker, it instead says the target is immune to critical hits.

### AI Stamina Management

`AI_FLAG_STAMINA_MANAGEMENT` is part of `AI_FLAG_BASIC_TRAINER`, and `GetAiFlags` also adds it to every trainer whose flags include Check Bad Move (most of `trainers.party` lists only that), so every scoring trainer manages Stamina. Battle tests opt in explicitly. It decides after normal move scoring, in this order:

0. **Exhausted (0 Stamina): Recuperate**, unless a scored attack KOs the target before the target acts. An exhausted Recuperate has no priority, so a faster target's hit still auto-crits that turn, but staying at 0 means every later hit does too.
1. **Take the KO** if any scored attack faints the target.
2. **Punish exhaustion:** a target at 0 Stamina is attacked with the best attack that won't exhaust the AI.
3. **Dodge** (never twice in a row) when it has the Stamina for it and either: the incoming hit would KO it and Dodging saves it (a full miss, or half damage it survives); or Dodge is a guaranteed miss (it acts before the target) on a physical hit or one taking ≥ 1/3 of its HP — the latter only half the time, so it isn't a fixed pattern.
4. **Recuperate** at or below the threshold if it is bulkier than the target (more max HP *and* BaseSta), or if it isn't frailer (less of both) and the target can't KO it this turn. A frailer battler keeps attacking instead.
5. **Avoid self-exhaustion:** if the chosen attack plus the incoming physical attrition would empty it, it switches to a safe alternate attack (half the time Recuperating instead when it is repeating the same move), else Recuperates, else switches out (the switch is sent in the action phase).

A slower battler at full Stamina against a full-Stamina target just attacks when the exchange doesn't exhaust it. The "incoming hit" is the predicted move when `AI_FLAG_PREDICT_MOVE` rolls a prediction; otherwise it is the target's strongest damaging move the AI knows about (revealed moves, or all with move omniscience), so the flag works without prediction flags.

### Pacing Targets (the Physical Clock)

Stamina depletion (own actions + incoming physical attrition) governs the **physical** time-to-death. With vanilla BP (~90 average), the design lands:

* **Frail mons:** ~5–7 turns to death.
* **Mid-bulk:** ~7–10 turns to exhaustion.
* **Giga-bulk:** ~10+ turns to exhaustion.

These bands assume physical pressure. Against a **special** attacker, Stamina drains roughly half as fast (no incoming attrition), and the time-to-death is instead governed by the **Trauma clock** (§4), which keys off the target's Defense, not its Stamina.

---

## 3. Innate System Commands (L/R Shoulder Buttons)

Every Pokémon possesses two innate mechanical actions, reached from the **innate menu**. On the move selection screen, **L** (R in the L=A button mode, where L acts as A) swaps the four move slots for **Dodge**, **Recuperate** and the mon's **Signature Move** (if it has one); L or B swaps back. An entry that can't be used this turn is drawn faded (not enough Stamina for Dodge, above the 40% gate for Recuperate) and picking it shows why. The right-hand panel shows the highlighted entry's Stamina cost (`STA -n`) or gain (`STA +n`), or the Signature Move's Stamina cost and type (it has no PP). While the menu is open, the move-description window above it (`B_WIN_MOVE_DESCRIPTION`) describes the highlighted entry and updates as the cursor moves. Dodge and Recuperate use their own three-line text (`sText_InnateDodgeDesc` / `sText_InnateRecuperateDesc` in `battle_controller_player.c`), in plain words with no numbers, because the panel already shows the Stamina cost or gain. The Signature Move uses the normal move description (category icon, PWR, ACC and text). The window closes when the menu closes or an entry is picked. The `L / MOVE INFO` tab above the menu is relabelled `L / INNATE` (upstream's move-description viewer is no longer reachable). Recuperate executes with high-priority sequencing, in the item usage window ahead of all moves, **unless the user is at 0 Stamina**: an exhausted Recuperate loses that priority and resolves in normal speed order (an exhausted mon is also slow, so a fast attacker gets its auto-crit in first). Dodge keeps its normal speed slot in the turn order, because its outcome (below) is decided by who acts first. **Selecting an Innate Command consumes the Pokémon's action for that turn** (it does not attack).

**Battle text.** When the action runs, Dodge prints *"X is dodging!"* and Recuperate prints *"X is recuperating!"*, each before its animation (`BattleScript_FRDodge` / `BattleScript_FRRecuperate`). If Recuperate also purged a Level 1 status, it uses `BattleScript_FRRecuperateCured`, which adds *"X shook off a minor ailment!"* after the HP heal. Dodge keeps its normal speed slot, so a faster attacker's hit can land before the Dodge line prints; the wording is present-tense so it reads correctly in either order.

When an attack meets a Dodge, the result says which outcome happened:

* **Full dodge** (the dodger acted first): *"X dodged the attack!"* instead of the usual *"X avoided the attack!"* (`Cmd_resultmessage`).
* **Half damage** (the attacker acted first): *"X partly dodged the attack!"* before the effectiveness message (`BattleScript_FRDodgeHalvedMsg`). The damage modifier marks a real hit in `gBattleStruct->dodgeHalvedHit` (never during AI estimates); the result message clears it, and so does move end. For a multi-hit move it prints only once, the same way effectiveness does.
* Both outcomes share one rule in `battle_util.c`: `IsMoveFullyDodged` / `IsMoveHalvedByDodge`.

**Battle gimmicks and the innate menu.** The *transformation* gimmicks — Mega Evolution, Ultra Burst, Dynamax and Terastallization — can be paired with **Dodge or Recuperate**. Nothing else pairs with an innate entry: Z-Moves need a move, and the Signature Move never takes a gimmick.

* **Player.** The gimmick trigger stays on screen while Dodge or Recuperate is highlighted and hides on the Signature Move (or if the gimmick is a Z-Move). START toggles it there just like on the move screen; the toggle is shared with the move screen, so it carries over in both directions. Confirming Dodge / Recuperate with the trigger on sends `RET_GIMMICK` with the action.
* **Engine.** On a Dodge / Recuperate action carrying `RET_GIMMICK`, `battle_main.c` queues the gimmick (`gimmick.toActivate`), and it activates in the usual before-moves step. Recuperate itself resolves earlier, in the item window, so the mon recuperates first and transforms after.
* **AI.** Choosing Dodge / Recuperate no longer cancels a planned transformation gimmick (`BattleAI_ChooseMoveIndex`); the opponent controller sends `RET_GIMMICK` with the action.
* `CanUseGimmickWithInnateCommand` (`battle_gimmick.c`) is the single rule both sides check: the usable gimmick is one of the four above and the trainer hasn't used it yet.

### Dodge

An active maneuvering defense mechanism balanced by compounding resource taxes.

* **Stamina Cost & Legality:** $5 + (20 \times \text{Consecutive Uses}) + (0.02 \times \text{BaseSta})$. Here, `Consecutive Uses` means the count of immediately prior turns in which the user also clicked Dodge, so the first Dodge pays only the base `5 + 0.02×BaseSta`; the consecutive-use term resets only after a turn passes without clicking Dodge (alternating Dodge with other actions keeps it cheap). **Dodge selection is gated; it can only be selected/executed if the user's current Stamina is greater than or equal to this calculated cost.**
* **Speed Tiers & Outcomes** (compared on **round-start Effective Speed order**):
  * **Attacker is Slower:** The move completely misses.
  * **Attacker is Faster, or Tie:** The move lands, but damage is mitigated by **50%** and cannot proc status effects.
  * **Repeated Move Trapping:** If the user is dodging an identical move executed by the opponent on the immediate prior turn, the move completely misses, regardless of speed tiers. *(Not implemented yet; the AI does not count on it.)*
* **Restrictions:** Status moves bypass Dodge checks entirely (but can still miss, via its Acc. check). Selection is blocked in the action selection UI if the Pokémon does not have enough Stamina to cover the cost.

#### Dodge Layering vs. Traditional Accuracy/Evasion

Dodge is a mechanical command completely distinct from standard accuracy and evasion checks. Old accuracy mechanics remain fully functional:

1. When an attack is executed, the game first processes traditional parameters (the move's native accuracy pool, accuracy/evasion stage modifiers, and status-inflicted accuracy reductions). If a move fails this baseline accuracy check, it misses organically.
2. The custom Dodge resolution mechanics only evaluate *after* a move has successfully passed its native accuracy roll. It dictates whether a confirmed hit is cleanly evaded, mitigated by 50%, or hits completely based on speed priority and repetition rules.

### Recuperate

The primary sustain mechanism used to stabilize an active Pokémon's resource depth. **It is the wincon engine for bulky mons** — its purpose is to let a tank out-sustain chip damage, with the Trauma pool (§4) acting as the inescapable counter-clock.

* **Mechanical Resolution:** Can only be selected if the user is at **40% of `BaseSta` or lower**. When legal, it instantly restores `CurrentSta` by exactly $40\%$ of `BaseSta`. Exempt from the Body Tax.
* **Sustain Properties:** Grants a minor HP heal of `RECUPERATE_HP_HEAL_PERCENT` (12%) of max HP × `CurrentSta/BaseSta` after the refill — about 5% from empty, about 10% from the 40% gate — and purges **1 random Level 1 status condition** from the user's status rail. (It cannot touch Level 2+ statuses — persistent conditions and Trauma are how walls are beaten.)
* **Scaling Ceiling:** Any operational HP healing function in the engine scales dynamically using `CurrentSta` as its primary scaling vector ($\propto r = \frac{\text{CurrentSta}}{\text{BaseSta}}$), capping healing efficiency when exhausted.

---

## 4. Damage Modality: Health vs. Trauma

To prevent eternal recovery stalling in a format where damage is halved, the **single HP bar** is tracked with **two numbers**: current HP and accumulated **Trauma**. Trauma is *not* a second bar.

### The Core Model (one bar, two numbers)

* **All damage is HP damage.** Every hit — physical or special, Trauma-causing or not — subtracts its full amount from current HP exactly as normal. Lethality is unchanged: a hit that would bring HP to 0 faints the target.
* **Trauma is a running tally of *unhealable* damage.** A portion of certain hits (the Special fraction, and crit bonuses) is added to a per-Pokémon `Trauma` counter. This does **not** reduce the hit's HP damage — it only records how much of the bar can never be restored.
* **Heals are capped at `maxHP − Trauma`.** Any HP recovery (items, abilities, Recuperate, end-of-turn, drain) may not raise current HP above this **effective max**. So Trauma is felt only when the target tries to recover.
* **Persistence & death.** Trauma persists across switches for the whole engagement (only Level 1 statuses clear on switch). When Trauma reaches the original max HP, the effective max is 0 and the Pokémon can hold no HP — it faints.
* **Display.** The HP numbers show `current / (maxHP − Trauma)` — the effective max, since the rest can never come back. The HP bar still spans the original max HP and reads, left to right: **purple** (Trauma, permanently lost) → **red** (current HP) → **slate grey** (recoverable loss). Because current HP is capped at the effective max, the red fill always starts where the purple ends. Any non-zero Trauma draws at least 1px.

#### Special Move Translation (the Trauma fraction)

A Special hit deals its full damage to HP like anything else; additionally, a stamina-keyed fraction of that damage is logged as Trauma (unhealable):

$$\text{Trauma added} = D \times \left[a + b\,(1 - r)\right], \qquad \text{HP damage} = D \text{ (full, unchanged)}$$

where `D` is the (already halved) Special damage, $r = \frac{\text{CurrentSta}}{\text{BaseSta}}$, and the fraction $[a + b(1-r)] \le 1$.

* **Constants:** `a = 0.25` (floor — Trauma even versus a fresh target), `b = 0.40` (drain bonus). At full Stamina (`r=1`) **25%** of a Special hit is logged as Trauma; at 0 Stamina (`r=0`), **65%**.
* **Design role:** Physical attackers shred Stamina (lowering `r`); Special attackers punish the drained target by making an escalating share of the (full) damage permanent. This is the core offensive combo, and it is what lets a Special attacker beat a Recuperate-stalling wall — Recuperate heals HP but can never refund the shrinking ceiling.

#### Special Defense Stage = Trauma Resistance

The repurposed **Special Defense stat *stage*** (from Calm Mind, Quiver Dance, etc.) reduces **only the Trauma logged** by incoming Special hits (never the HP damage):

* Each `+1` SpD stage multiplies the Trauma logged by **0.85** (bottoms at ~0.38 at `+6`).
* It affects **only the Trauma tally** — never the HP damage dealt (handled by unified Defense), never `BaseSta`/Stamina, never Speed.
* Consequence: Calm Mind is a *special-leaning* bulky setup that gives **zero** defensive value against Physical attacks, keeping it self-limiting. Quiver Dance (SpA + Speed + SpD-trauma-guard) is the build to watch for over-performance.

### Critical Hit Amplification

* Critical hits bypass standard stat-stage modifiers.
* The crit still deals its full (boosted) damage to HP; additionally, the bonus portion (~1/3 of the dealt damage) is logged as **Trauma** (unhealable), regardless of whether the move was Physical or Special.
* In the Exhaustion Zone (`CurrentSta = 0` at the start of the turn), every incoming damaging move auto-crits, so its bonus portion is logged as Trauma.

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
* **UI.** Every active line gets a cell in the healthbox **status column** (see "Healthbox layout" under §12, Battle UI graphics folder): its icon plus one pip per stage the line can reach, lit up to the current stage.

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
* **Choice Scarf**: vanilla, moves can't be Dodged, at worst still 50% damage. Signature moves deal 100%. Disable L&R

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

*Current implementation:*

* **Earning it.** A fully evolved mon's most-used move (≥ `SIGNATURE_MOVE_USE_THRESHOLD` = 100 uses, tracked per party slot) becomes its Signature Move. The move then **leaves the four-slot moveset**, freeing that slot (later moves, their PP, PP Ups and use counts shift up). This is settled **after battle** (and after evolution / Rare Candy), never mid-battle, so a battler's moveset can't change under it. Saves from before the slot was freed are fixed up the same way.
* **Knowing it.** The Signature Move counts as known: level-up learning, TMs and tutors treat it as already learned, so it can't be re-added to a slot.
* **Using it.** It is picked from the innate menu (L on the move screen). It has **exactly 8 PP** — it only pays the normal Stamina action cost (BP/10 + Body Tax), shown in the menu as `STA -n` with its type. Disable, Taunt, Encore, Choice lock etc. still apply through the engine's normal checks. In doubles it currently uses the move's default target (no manual target pick yet).
* **Engine.** It is chosen with an extra move position, `SIGNATURE_MOVE_POSITION` (= `MAX_MON_MOVES`); `GetBattlerMoveAtPosition` resolves it. It is never stored in `gBattleMons[].moves / .pp`, so code that indexes those by a chosen/current position checks `position < MAX_MON_MOVES` (PP checks and deduction, Grudge, Last Resort's used-move bits, Mimic and Sketch fail when used as the signature).
* **Summary screen.** The BW summary's moves page shows it in the row under the four moves (where a move being learned or "Cancel" appears): type icon, name, and `Signature` in place of PP. That row goes back to "Cancel" / the new move while it's needed (move details, forget-a-move) and the signature returns afterwards. It isn't selectable for details yet.
* **Announcement.** Using it prints *"X is using its Signature Move!"* before the usual *"X used Y!"* (`BattleScript_SignatureAttackstring`, picked in `CancelerAttackstring` when the move position is `SIGNATURE_MOVE_POSITION`). It is skipped on a charge move's second turn and on a Dancer copy.
* **No gimmicks.** A Signature Move never carries a gimmick (see §3).
* **Not yet:** the anti-Dodge property above.

### Itemized Moves

Allows players to expand tactical flexibility by slotting an active, usable combat move directly into a Pokémon's held-item accessory slot. *(Mutually exclusive with other held items, including Choice accessories.)*

### Repurposed Moves (consequence of unified Defense)

* **Psyshock / Psystrike / Secret Sword** — their vanilla gimmick (special move that hits physical Def) is meaningless now that all special moves hit Def. Repurposed identity: **special damage that exhausts like a physical hit** — they apply the Physical Stamina attrition (`BP/10`, ×2 SE, per hit) and deal **no Trauma** (they opt out of the Special→Trauma split). The special-side stamina-shredders. *(Attrition implemented in Phase 2's hook; the no-Trauma opt-out lands with the Phase 3 split.)*

---

## 11. Tuning Constants (Quick Reference)

| System     | Constant                        | Value                                                              |
| ---------- | ------------------------------- | ------------------------------------------------------------------ |
| Damage     | Output multiplier               | ×0.5 (in damage formula; BP stays vanilla)                         |
| Type chart | Effectiveness cap               | 2x max (weakness and resistance)                                   |
| Stamina    | Action cost                     | BP/10                                                              |
| Stamina    | Status move cost                | 10 (flat)                                                          |
| Stamina    | Body Tax (attacks + Dodge)      | +0.02 × BaseSta                                                    |
| Stamina    | Physical attrition              | incoming BP/10 (×2 if SE, x0.5 if NE)                              |
| Stamina    | Speed decay                     | BaseSpeed × (0.15 + CurrentSta/BaseSta)                            |
| Stamina    | Exhaustion (0 Sta)              | incoming damage auto-crits                                         |
| Stamina    | Persistence                     | carries across switches (not refilled to full)                     |
| Stamina    | Switch-out rest bonus           | 5 + Level / 2 (capped at BaseSta); first send-out = full           |
| Dodge      | Cost                            | 5 + 20×consecutive + 0.02×BaseSta; gated behind CurrentSta >= cost |
| Recuperate | Stamina restore / gate          | +40% BaseSta; usable only at ≤40% BaseSta                          |
| Recuperate | HP heal                         | 12% maxHP × CurrentSta/BaseSta after refill (~5–10%)               |
| Recuperate | Priority                        | item window (before moves); none at 0 Stamina                      |
| Trauma     | Special split (floor `a`)       | 0.25                                                               |
| Trauma     | Special split (drain bonus `b`) | 0.40                                                               |
| Trauma     | SpD stage resistance            | ×0.85 per +1 stage (Trauma channel only)                           |
| Status     | Proc rate                       | ×2 baseline                                                        |
| Status     | Sleep base duration             | 3 turns (−1 per damaging hit)                                      |
| Setup      | Speed-boosting setup cost       | 0 Stamina                                                          |
| Nature     | Neutral                         | Advantage (guaranteed hit if faster)                               |
| Nature     | Reckless (under-used offense)   | Super Luck (+1 crit stage)                                         |
| Nature     | Adamant / Modest                | Sheer Force (+30% dmg, no Lv1 status)                              |
| Nature     | Jolly / Timid                   | Quick Start (+1 Speed on entry)                                    |
| Nature     | Bold / Impish                   | Battle Armor (crit immune)                                         |
| Nature     | Calm / Careful                  | Resilient (+1 Sp.Def / Trauma-resist stage on entry)               |
| Nature     | Relaxed / Lax                   | Regenerator (HP on switch-out)                                     |
| Nature     | Sassy / Gentle                  | Natural Cure (status cleared on switch)                            |
| Pacing     | Time-to-death (physical)        | frail 5–7 / mid 7–10 / giga 10+ turns                              |
| Sweep      | Optimal-case trade              | 1 for 2–2.5                                                        |

### Open Items

* Average move BP confirmation (drives the Stamina pacing — assumed ~90).
* Validate the Special/Trauma clock against a high-Def/high-Sta wall lands in the ~10–15 turn window.
* Body Tax % final value (1.5–2% range; currently 2%).
* Nature ability magnitudes (Crusher +50%, Disruptor `a→0.50`, Resilient −25%) — validate vs the wall-breaking triangle.
* Cap on Toxic Trauma escalation (§6).
* Field Hazards and Held Item systems (§8, §9).

---

## 12. Development Tooling

### clangd / Zed include-path fallback

This repo now includes a root `.clangd` file so editor tooling can resolve the main game headers even though `bear`'s generated `compile_commands.json` only captures helper-tool builds under `tools/` and not the ROM `src/*.c` pipeline.

The fallback flags mirror the minimum project assumptions needed for code navigation and diagnostics in the gameplay codebase:

* language mode: `gnu17`
* macros: `MODERN=1`, `TESTING=0`, `EMERALD`, `__INTELLISENSE__=1`
* header roots: `include`, project root, and `tools/agbcc/include`

Primary goal: make quoted project includes such as `#include "global.h"` resolve reliably in the editor without changing the actual build pipeline.

### Upstream base

The branch tracks `rh-hideout/pokeemerald-expansion` **`master`** (not `upcoming`). Last merged: **expansion 1.17.1 + 20** (`origin/master` @ 2026-10-22). Where upstream relocated code that carries Fantasy Retold hooks, the hooks moved with it:

| Hook                                                                                                                           | Lives in                                                                       |
| ------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------ |
| Trauma tally per hit (`TrackTraumaFromHit`), Trauma heal cap on move-damage HP gain, Physical/Psyshock Stamina attrition       | `battle_move_resolution.c` — `CancelerHealthBarUpdate` / `TryMoveDamageUpdate` |
| Status-rail level on move-applied status (`SetNonVolatileStatus(..., railMinLevel)`), innate Sheer Force secondary suppression | `battle_set_effect.c` (upstream's `SetMoveEffect` handler table)               |
| Crit-blocked messages, innate Natural Cure / Regenerator, EV-gain disable, Stamina banking on switch-out                       | `battle_script_commands.c`                                                     |

### Battle UI graphics folder

Fantasy Retold battle-interface art lives in `graphics/battle_interface_fantasy_retold/` (a fork of upstream's `graphics/battle_interface/`); `src/graphics.c`, `gimmicks.h`, `battle_interface.c` and the level-up banner point there. Concatenated outputs (`textbox.gbapal`, `battle_bar.4bpp`) have their own rules in `graphics_file_rules.mk` under `FRBATINTGFXDIR`.

* **Palettes.** `textbox.gbapal` is built as `textbox_0.gbapal` + `textbox_1.gbapal`, which come from the JASC `textbox_0.pal` / `textbox_1.pal`. Those `.pal` files are written by `generate_jasc_pals.py`; `generate_dark_palettes.py` holds the same colour lists for the raw `.gbapal` outputs. Edit a palette in **both** scripts and rerun `generate_jasc_pals.py` — the build only reads the `.pal` files.
* **Battle menu text** (move names, PP / type, FIGHT / BAG menu) uses the **vanilla** `graphics/battle_interface/text.pal` / `text_pp.pal` (dark grey on white). The dark `text.pal` in the Fantasy Retold folder is no longer used.

#### Healthbox layout

Drawn from scratch rather than adapted from the vanilla box. Each box is a **dark corner-cut plate** (the same navy as the text box) plus a **status column** beside it.

* **Plate.** Player: 104×32. Row 1 has the name + gender symbol and, right-aligned, the level. Below it, the **HP bar** (4px), then the **Stamina bar** (2px), both 94px. The bottom row has the 1px **EXP track** (singles only) and, right-aligned, `current / effective max` HP. Opponent: 100×24, with the same name row and the same bars at 90px. It has no numbers or EXP, and shows a 5px caught-ball icon before the name when the species is caught. Player doubles plates are the player plate without the EXP track. The HP numbers are always shown with the bars; the vanilla doubles bars/numbers toggle (`SwapHpBarsWithHpText`) is a no-op.
* **Status column.** A box as tall as the plate, with one 12×9 cell per active status line in a fixed order: Soaked, Chill line, Burn, Poison line, Electric line, Sleep line, Confusion line. Each cell holds a 7×7 icon in the line's colour (drop, snowflake, flame, skull, bolt, Z, swirl) and one pip per stage the line can reach, lit bottom-up to the current stage. Cells fill top-down, two per column. Extra columns grow away from the plate: left for the player, right toward the screen centre for the opponent. Up to 6 lines can be active at once (Burn excludes Soaked and the Chill line), which is 3 columns. With no status the column is hidden. `GetHealthboxStatusStages` maps lines to stages. It reads `RAIL_POISON`, and until the other rails exist it uses vanilla `status1`: Burn → 1, Frozen / Frostbitten → Chill 2, Paralysis → Electric 3, Sleep → Sleep 2, and the confusion timer → Confusion 1. **New rails plug in there.** The column is the old healthbar sprite, now 64×32 on the healthbox palette. Its callback polls the stages every frame and redraws on any change, because confusion and rails change without a healthbox update.
* **Art vs runtime.** `generate_healthbox_art.py` draws only the static parts: plates, bar frames, the 40% notch and the EXP track. Its layout numbers must match `sHealthboxLayouts` / `HB_*` in `battle_interface.c`. Text, bar fills, the exhausted red frame, the ball icon and the status column are drawn at runtime with `FillHealthboxRect` / `PrintOnHealthbox`. These treat the two healthbox sprites as one 128px canvas. The HP bar no longer uses the `hpbar` tiles.
* **Healthbox palette** (`ball_status_bar.png`): 1 plate, 2 empty gauge / cell, 3 ink, 4 slate, 5 white text, 6 grey, 7 red, 8 green, 9 purple, 10 cyan, 11 orange, 12 blue, 13 yellow, 14 sky, 15 pink (Confusion). Text is white with an ink shadow. The gender symbols use explicit colours (blue ♂ / red ♀) instead of the shared dynamic text colours.
* **Positions.** Every plate starts at the sprite's `(x − 32, y − 16)`. The 64×64 player singles box keeps the 64×32 template's centre offset. Gimmick indicators sit just left of the level digits (`sIndicatorPositions` in `battle_gimmick.c`).
* `B_HP_PERCENTAGE_DISPLAY` must stay **FALSE** (a `STATIC_ASSERT` enforces it); the plates have no room for it.
