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
* **UI note:** Both healthboxes show Stamina as a **bar directly under the HP bar, the same length** (no number), so the two read side by side. It fills left to right as `CurrentSta/BaseSta`: **green** normally, **yellow** at or below the Recuperate threshold (`RECUPERATE_STAMINA_PERCENT`, 40%; a grey notch under the bar marks the threshold) so the player can see when R is live, and **empty with a red frame** at 0 Stamina (Exhaustion Zone). Any non-zero Stamina draws at least 1px. Exhaustion / auto-crit checks always use the exact live `CurrentSta`, not the drawn gauge. The bar **slides** to a new value at the HP bar's speed (a full bar in 24 frames, as `MoveBattleBar` does with `B_FAST_HP_DRAIN`): `battle_interface.c` keeps the shown value per battler (`sStaminaShown`) and the status-column sprite callback steps it every frame (`StepStaminaGauge`). A full healthbox redraw (a send-out) shows the live value at once.

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

## 3. Innate System Commands

Every Pokémon possesses two innate mechanical actions, reached from the **innate menu**. On the move selection screen, **L** (R in the L=A button mode, where L acts as A) swaps the four move slots for **Dodge**, **Recuperate** and the mon's **Signature Move** (if it has one); L or B swaps back. An entry that can't be used this turn is drawn faded (not enough Stamina for Dodge, above the 40% gate for Recuperate) and picking it shows why. The right-hand panel shows the highlighted entry's Stamina cost (`STA -n`) or gain (`STA +n`), or the Signature Move's Stamina cost and type (it has no PP). The innate menu has no description panel. The `L / MOVE INFO` tab above the menu is relabelled `L / INNATE` (upstream's move-description viewer is no longer reachable). Recuperate executes with high-priority sequencing, in the item usage window ahead of all moves, **unless the user is at 0 Stamina**: an exhausted Recuperate loses that priority and resolves in normal speed order (an exhausted mon is also slow, so a fast attacker gets its auto-crit in first). Dodge keeps its normal speed slot in the turn order, because its outcome (below) is decided by who acts first. **Selecting an Innate Command consumes the Pokémon's action for that turn** (it does not attack).

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
* **KO'd before it resolves:** An exhausted Recuperate acts in its speed slot, so a faster foe can KO the user first. The queued Recuperate (and likewise a queued Dodge) is then dropped: no refill, no heal, and the user faints normally.

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

* **Meaningful trade, not an upgrade:** Adamant (Sheer Force: power, no status) vs a +Atk Reckless nature (Super Luck: crit/Trauma, keeps status). Each wall ability likewise answers one threat while leaving the others open — Resilient blunts Trauma but not status; Natural Cure cleanses status but not a Trauma nuke; Battle Armor stops crits but not statuses.
* **Off-meta uplift:** handing Regenerator / Natural Cure to the off-meta walls (Relaxed, Lax, Sassy, Gentle) gives them a reason to see play.
* **[Alt — Jolly/Timid]** On-KO snowball (Moxie / Grim Neigh, +1 Atk/SpA per KO) remains an offense-flavored option, naturally capped by halved damage.
* **[Alt — Modest]** Tinted Lens (ignore one resistance step) if Adamant/Modest should differ.
* **[Tune]** Resilient's Trauma fraction and Regenerator's heal amount are designer-side; player text stays plain ("halves Trauma," "heals on switch"). [TODO: redesign; Trau resistence is too opaque behind Stamina]

---

## 6. Systemic Status Overhaul

### Core Model

* **Multi-stage rails.** A condition advances through levels. Applying the **same kind** again stacks it upward (Lv1 → Lv2 → Lv3). **Status moves apply directly at Level 2**; **attacking-move secondary effects, abilities and self-inflicted effects apply at Level 1**. (Exception: the Chill line always starts at Chilled.)
* **Lines coexist.** Unlike vanilla, a mon can carry several lines at once (only Burn excludes Soaked and the Chill line). Applying a line only fails when that line is already at its top level ("already burned / asleep / ...").
* **Persistence.** **Level 1 is volatile** (clears automatically on switch-out). **Level 2 and above are persistent** (require active curing or mechanical expiration; survive switches). This includes the Confusion line.
* **Proc rates.** Status effect triggers on all attacking moves have their baseline application probabilities **doubled**. This is applied when the chance is rolled (`CalcSecondaryEffectChance`), not in the move data, so move descriptions still show the vanilla number. It covers every status line, and random picks from a list of statuses (Tri Attack, Dire Claw).
* **Serene Grace and Rainbow** don't raise a status chance (they still give ×1.5 to other secondary effects, e.g. flinch). Instead each adds **one more roll**: the status only fails if every roll fails (`RollSecondaryEffect`). E.g. a doubled 20% becomes 1 − 0.8² = 36% with one of them, 1 − 0.8³ ≈ 49% with both.
* **Cleansing.**
  * Recuperate purges 1 random **Level 1** condition only.
  * **Full cures** (Full Heal-type items, Heal Bell / Aromatherapy, Natural Cure) clear every line **below Lv3**. A **Lv3 line drops to Lv1** instead (Paralyzed → Dazed, Nightmare → Drowsy, Frightened → Confused, Burn ×3 → Burn ×1). Clearing a Lv3 therefore takes two turns: the cure, then a switch-out (or Recuperate), since that leftover Lv1 is volatile as usual.
  * **Natural Cure** (ability or innate) fires on switch-out and cures like the above, but the Lv1 it makes from a Lv3 is **not cleared by that same switch-out** (otherwise it would clear a Lv3 in one free step). Any Lv1 the mon already had is cleared as usual. On the next switch-out the leftover is an ordinary Lv1 (and Natural Cure clears it anyway). Full cures also reach benched mons' rails (Heal Bell, a Full Heal used on a benched mon).
  * **Cleansing berries** (Cheri, Chesto, Pecha, Rawst, Aspear, Persim, Lum) **ignore Lv1**: they only trigger on a Lv2+ line, and then cure it like a move (Lv2 clears, Lv3 → a volatile Lv1).
  * **Other generic cures** — Shed Skin, Hydration, Healer, Purify, Jungle Healing, Pastel Veil — see every non-volatile line (Soaked included, Confusion not) and cure the same way: below Lv3 clears, Lv3 → a volatile Lv1.
  * **Refresh** wipes its own lines outright, at every level: Poison, Burn, Electric and Chill.
  * **Hit cures** — Smelling Salts, Wake-Up Slap, Sparkling Aria (and other "cure the target's X" moves) — cure their line like a full cure, at any level.
  * **Immunity abilities** (Limber, Insomnia / Vital Spirit, Immunity / Pastel Veil, Water Veil / Water Bubble / Thermal Exchange, Magma Armor, Own Tempo) wipe their whole line at every level when the mon has, or gains, the ability.
* **Hit-triggered advances respect immunities.** Shocked → Paralyzed on an Electric hit and the Ghost / Psychic crit upgrades are blocked by the same things as a normal application (abilities, types, Safeguard, Misty / Electric Terrain, ...).
* **"Has a status".** Effects that check for a status — Guts, Marvel Scale, Quick Feet, Facade, Hex / Infernal Parade, the catch-rate bonus — count **Lv2+ of any line, or Soaked**; Lv1 doesn't count. Line-specific ones read their own line at Lv2+: Toxic Boost, Venoshock, Barb Barrage, Merciless (Poison), Flare Boost (Burn), Smelling Salts (Electric), Wake-Up Slap and the Dream Ball (Sleep). Catch rate: Asleep+ or Frozen gives the big bonus, any other qualifying status the small one.
* **Moving lines.** **Psycho Shift** moves the user's first line the target can take (Poison, Burn, Electric, Sleep, Chill, in that order), at the same level, and cures the user of that line; other lines don't block it. **Baton Pass** hands the whole Confusion line (level and turns left) to the incoming mon, and the passer loses it.
* **Yawn** makes the target **Drowsy at once**; if it is still Drowsy at the end of the next turn it falls Asleep. Only an Asleep / Nightmare target makes Yawn fail (other lines don't).
* **After battle.** Lv1 is volatile, so the battle ending drops it like a switch-out; Lv2+ stays on the party (e.g. Toxic keeps poisoning in the overworld). Contact abilities (Static, Flame Body, Poison Point, Effect Spore) land at Lv1 at their normal chance; one layer of Toxic Spikes gives a volatile Poison ×1, two give Toxic.
* **Synchronize** has no exceptions: every line (Poison, Burn, Chill, Electric, Sleep, Confusion) is passed back, and the attacker's line jumps to the **synchronizer's level** (Frozen / Frostbitten keep their branch). If the attacker is already that deep, the line just advances one step.
* **Messages.** Every level and proc has its own line:
  * *Applied:* Drowsy / Dazed / Shocked / Chilled / Stupefied / Frightened / Nightmare have FR messages ("X grew drowsy!", "X is dazed!", ...), "X's burn got worse!" for extra stacks, vanilla messages where a vanilla status exists. Dazed + Soaked → "The water conducted the shock! X is paralyzed!".
  * *Procs on a hit:* "X got soaked!", "X's burn was doused!", "The shock left X paralyzed!" (Shocked), "The ghastly blow plunged X into a nightmare!" / "The psychic blow terrified X!" (crits), "X woke up!" (valve), "X warmed up!" / "X's burn eased!" / "X shook off some static!" (type interactions).
  * *Acting:* the confusion line opens with "X is confused! / stupefied! / frightened!" by level; a Stupefied proc adds "X turned on its ally!" or "It turned its own attack on itself!"; Frightened adds "X panicked and tried to flee!" or, from the bench, "X is too frightened to come out!". Nightmare: "X is trapped in a nightmare!".
  * *Blocked:* "X is too wet / too cold to be burned!", "X's burn keeps the cold away!", "X is already frozen solid! / frostbitten!", "It doesn't affect X" for a Psychic-type hit by a confusing status move, and "X's poison kept it from healing!" when Recuperate is poisoned.
  * *Ending / cures:* "X dried off!", "X warmed up!", "X woke up!", "X snapped out of it!"; a cure that only removed Soaked prints "X dried off!". Psycho Shift prints the message for the level it moved.
* **UI.** Every active line gets a cell in the healthbox **status column** (see "Healthbox layout" under §12, Battle UI graphics folder): its icon plus one pip per stage the line can reach, lit up to the current stage.

### The Condition Core

Numbers below are the current defaults (`include/constants/battle.h`, "status rail tuning"). Speed slows stack multiplicatively with each other and with Stamina decay.

**Soaked** *(Lv1, single-stack)*

* Speed ×0.85. Incoming Fire damage ×0.5; incoming Electric and Ice damage ×1.5.
* **Source:** every damaging Water move has a **20%** chance to Soak each target it hits. On a **burned** target the Soak **douses the Burn** (all stacks) instead of soaking it.
* **Duration:** 3 turns. A new Soak on a Soaked mon resets the count to 3.
* Water-type immune.

**Chilled → Frozen ‖ Frostbitten**

* **Progression:** Any Ice application (secondary or status move) lands **Chilled** (Lv1); the next one advances it into a **random** branch — either **Frozen** or **Frostbitten** (Lv2). There is no direct way to apply the Lv2 forms.
* **Chilled:** Speed ×0.75 and **+5 Stamina** on every move and Dodge. **Frozen and Frostbitten keep Chilled's effects.**
* **Frozen:** forces a complete loss of turn actions (no random thaw). **Frostbitten:** ticks **maxHP/8** each turn, all of it Trauma (it no longer weakens special attacks).
* **Removal:** Expires after **2 turns**. Every **Ice damage** taken resets it to 2, so a target that isn't hit by Ice again shakes it off. **Frozen is never renewed** (it always ends after 2 turns). **Instantly thaws if hit by, or executing, a Fire-type move** (the whole line, Chilled included).
* Ice-type immune. Mutually exclusive with Burn.

**Burned** *(stacks up to 3)*

* **Stacks:** a secondary adds 1 stack, a status move (Will-O-Wisp) lands at 2, each reapplication adds 1.
* **Modifiers:** End-of-turn tick of **maxHP/16 per stack**; physical damage **−10% per stack** (×0.90 / ×0.80 / ×0.70; Guts still ignores it).
* **Exclusions:** Can't apply to a Soaked or chilled target. **Loses a stack** whenever the burned mon uses, or is hit by, a Water or Ice move (and a Soak proc douses it entirely, above).
* Fire-type immune.

**Poison → Toxic**

* **Progression:** Baseline Poison rules govern Level 1; status-driven application upgrades to Toxic (Level 2).
* **Heal block (whole line):** a poisoned mon **can't heal from Recuperate, Leftovers, or any heal of maxHP/16 or less** (a weak drain, Aqua Ring, Rain Dish, ...). Recuperate still restores Stamina.
* **Modifiers:** Toxic ticks increase each consecutive turn and **bypass HP mitigation, carving directly into the Trauma pool** (capped at 8 turns of escalation). Toxic is the primary hard-counter to pure Recuperate-stalling walls (Recuperate cannot cleanse a Lv2).
* Poison-type immune.

**Dazed → Shocked → Paralyzed** *(Electric line)*

* **Dazed** (Lv1): accuracy ×0.9, and moves aimed at it get accuracy ×1.1. A mon that is **both Dazed and Soaked** skips Shocked: its next Electric application goes **straight to Paralyzed**.
* **Shocked** (Lv2): Dazed's effects, incoming Electric damage ×1.25, and **every Electric hit has a separate 25% chance to Paralyze** it. Thunder Wave and other paralysis status moves land here.
* **Paralyzed** (Lv3): Speed ÷4 and the vanilla 25% chance to lose the turn; keeps Shocked's penalties.
* **Downgrades** one level when the mon uses, or is hit by, a Ground move.
* Electric-type immune.

**Drowsy → Asleep → Nightmare** *(Sleep line)*

* **Drowsy** (Lv1): Speed ×0.75 and **+5 Stamina** on every move and Dodge.
* **Asleep** (Lv2): can't act (Snore / Sleep Talk still work).
* **Nightmare** (Lv3): can't act, and loses **maxHP/8** each turn, all of it Trauma. **Snore / Sleep Talk don't work.**
* **Duration & Valve:** Base duration **3 turns**, counted down at end of turn (Early Bird counts 2). Each direct damaging hit has a **50%** chance to take 1 turn off (valve applies across the whole rail), **except Ghost-type moves**, which never wake. Reaching 0 wakes it at once. A fresh Drowsy, or falling asleep, starts the 3 turns; advancing to Nightmare inherits the remaining count (no reset, no refresh).
* **Critical Ghost hit:** a crit from a Ghost-type move on a Drowsy / Asleep target sinks it into **Nightmare** (keeping the count).
* Ghost-type immune.

**Confused → Stupefied → Frightened** *(Confusion line)*

* **Confused** (Lv1): 33% chance to hit itself instead of acting (vanilla).
* **Stupefied** (Lv2): Confused's effects, plus a **25%** chance (rolled when the self-hit doesn't) that it turns on itself. It always takes the **vanilla self-hit**, and then:
  * a single-target damaging move in doubles, with a live ally, **goes on against the ally**;
  * with no ally (singles), the **move's own damage is added to the self-hit**;
  * any other action (status move, spread move) is lost to the self-hit alone.
  So a Stupefied proc is never milder than a Lv1 self-hit. (If the self-hit alone would KO the user, the move doesn't go on.)
* **Frightened** (Lv3): Stupefied's effects, plus a **25%** chance to panic and flee to a random party member (trainer battles; not while rooted, Dynamaxed or commanding). With nowhere to run it still loses the turn. **It can also panic on the bench:** a switch into a Frightened mon has a 25% chance to be refused ("X is too frightened to come out!"), and that switch action is lost.
* Effects are cumulative. Confuse Ray-type status moves land at Stupefied; secondary confusion, Outrage fatigue and confusing berries at Confused.
* Lasts **3 turns** (end-of-turn count). Advancing the line keeps the current count (turn 1 Confused, turn 2 Stupefied → the line still has 2 turns). **It doesn't count down while benched.**
* **Critical Psychic hit:** a crit from a Psychic-type move on a Confused / Stupefied target makes it **Frightened** (keeping the count).
* Psychic-type immune.

### Implementation

* **State.** `struct PartyState.rails[]` (`StatusRailEntry`: `level`, `counter` = Toxic escalation or turns left, `branch` = the Chill line's Lv2 form, `keepOnSwitch` = a Natural Cure leftover Lv1, kept through one switch-out) is the source of truth; `src/status_rail.c` owns it.
* **Mirror.** The vanilla `status1` bits and `volatiles.confusionTimer` are kept as a mirror (`SyncStatusRailBridge`): Poison/Toxic, Burn (any stacks), Frozen / Frostbitten, Paralyzed, Asleep / Nightmare (the sleep bits hold the turns left) and the whole Confusion line. Several bits can now be set together. Soaked, Chilled, Dazed, Shocked and Drowsy have no vanilla equivalent and exist only on the rail.
* **Reconcile.** Every rail change first reads the mirror back (`ReconcileStatusRailsFromBridge`), so anything vanilla writes straight to `status1` / `confusionTimer` (Toxic Orb, Rest, Yawn, Psycho Shift, Outrage fatigue, Uproar waking a sleeper, ...) is picked up. The healthbox only reads, since it polls every frame. Full cures go through `CureAllStatusRails` / `CurePartyStatusRails` **without** reconciling first (the caller may already have zeroed `status1`, which would wipe the Lv3s they keep), then re-sync the mirror; `BS_ItemCureStatus` puts the mirror back after `HealStatusConditions` clears it.
* **Hooks.** Application: `SetNonVolatileStatus` / `HandleSetEffectConfusion` through `ApplyStatusRailOrSynchronize` (new levels print through `BattleScript_FRRailEffectApplied` with `gBattleScripting.savedStringId`). Synchronize: `TrySynchronizeActivation` stores the line and level in `gBattleStruct->synchronize*`, `Cmd_trysynchronize` passes them back; `BattleScript_MoveEffectConfusion` now runs `trysynchronize` too. "Already" checks / immunities: `CanSetNonVolatileStatus` (Ghost vs sleep) / `CanBeConfused` (Psychic). Actions: `CancelerAsleepOrFrozen` (Nightmare blocks Snore / Sleep Talk), `CancelerConfused` (+ `BattleScript_FRStupefiedSelfHitRet` and the redirect in `CancelerSetTargets`), `HandleAction_Switch` (Frightened refusing, `BattleScript_FRFrightenedRefusedSwitch`). After a damaging move: `MOVEEND_STATUS_RAILS`, five steps per battler (type interactions; Chill renewal + Soak / douse; Shocked → Paralyzed; crit upgrades; Sleep valve). End of turn: Burn stacks, Frostbite and Nightmare Trauma ticks, and `ENDTURN_STATUS_RAILS` (timers, active battlers only). Effects: `GetStatusRailSpeedPercent`, `GetStatusRailStaminaSurcharge` (through `GetMoveStaminaCost` / `GetDodgeCost`), `GetStatusRailAccuracyPercent` + `GetStatusRailIncomingAccuracyPercent` (in `GetTotalAccuracy`), `GetStatusRailIncomingDamageModifier`, `GetBurnRailPhysicalModifier`. Heal block: `IsHealBlockedByStatusRails`, checked in `Cmd_healthbarupdate` (any heal), `TryLeftovers` (no message) and `HandleAction_Recuperate`. Berries: `CureStatusRailWithBerry` in the `TryCure*` helpers of `battle_hold_effects.c`. Other cures: `CureStatusRails` / `HasAnyStatusRail` / `GetStatusRailCureMessage` (Shed Skin / Hydration in `battle_util.c`, `BS_CureStatus`, `HandleSetEffectRemoveStatus`); Refresh is `Cmd_curestatuswithmove`; Jungle Healing and Purify gate on the `jumpifanystatusrail` script command; immunity abilities in `TryImmunityAbilityHealStatus`. Status checks: `HasStatusRailCondition` / `HasStatusRailConditionForStatus1`. Psycho Shift: `BS_TryPsychoShift` + `frpsychoshiftcureuser`. Baton Pass: `Cmd_getswitchedmondata` stashes the line in `gBattleStruct->batonPassConfusion*`, `SwitchInClearSetData` restores it. Yawn: `Cmd_setyawn` + the end-turn Yawn handler. Battle end: `HandleEndTurn_FinishBattle` writes `GetStatusRailPartyStatusAfterBattle` to each sent-out party mon.
* **Switching.** Switch-out drops Lv1 (`ClearVolatileStatusRails`); a `keepOnSwitch` Lv1 only loses the flag, so it lasts exactly one more switch-out; switch-in re-syncs the mirror after the volatiles reset, which restores a persisted Confusion line.

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

### Level-Band EXP

EXP rewards fighting opponents near your own level and discourages both over-levelling and power-levelling.

* **Rule.** Each EXP earner compares its level to the fainted Pokémon's. Within **±`EXP_LEVEL_BAND` (3) levels** (inclusive), its EXP is **doubled in wild battles**; trainer battles keep the default rate. Outside the band, its EXP is **halved** in every battle (an award of 1 stays 1).
* **Why wild-only.** Trainer EXP is fixed and can't be farmed, so the bonus would only inflate the planned level curve. Wild battles are the grindable source, and the bonus there pays for catching up to the route's level rather than grinding weak encounters.
* **Per earner.** The band is checked per Pokémon, so in one battle a Pokémon close in level can earn double while an over-levelled teammate or an Exp. Share holder earns half.
* **Engine.** This is the last step of `ApplyExperienceMultipliers` (`battle_script_commands.c`). It runs after the vanilla multipliers (traded, Lucky Egg, Exp. Charm, affection, Gen 5+ level scaling), so it stacks with them.

### Infinite Repel (start menu)

* The start menu has a **REPEL OFF / REPEL ON** entry (once the player has a Pokémon). Pressing A flips it and relabels the entry in place; the menu stays open.
* While ON it works like a Repel that never wears off: wild Pokémon below the lead's level don't appear (same rule as the item, `IsWildLevelAllowedByRepel`), including overworld-spawned encounters. Repel and Lure items keep working on top of it.
* **Engine.** `FLAG_INFINITE_REPEL` (was `FLAG_UNUSED_0x020`, saved). `IsRepelActive()` (`wild_encounter.c`) = Repel steps left **or** the flag; it replaces the bare `REPEL_STEP_COUNT` checks in `IsWildLevelAllowedByRepel` and `wild_encounter_ow.c`. The entry is two menu actions (`MENU_ACTION_REPEL_OFF` / `_ON`) sharing `StartMenuRepelCallback`.
* **Layout.** The start menu window is 1 tile wider (8 tiles, for "REPEL OFF"). A 9th entry doesn't fit with the vanilla 2-tile padding, so with more than 8 entries the window drops the padding and the text starts 1px from the top (`START_MENU_PADDING_TILES` / `START_MENU_TEXT_TOP` in `menu.h`).

### Battle Style locked to Set

* Battles always use **Set** (no "switch Pokémon?" prompt when a foe faints): `ClearSetBScriptingStruct` forces `OPTIONS_BATTLE_STYLE_SET`, new games default to Set, and the **BATTLE STYLE** row is removed from the Options menu (the save field is written as Set when the menu closes).

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
| Status     | Proc rate                       | ×2 baseline (incl. Tri Attack lists); Serene Grace/Rainbow: +1 roll |
| Status     | Sleep base duration             | 3 turns (50% −1 per non-Ghost damaging hit)                        |
| Status     | Soaked                          | Speed ×0.85; Fire ×0.5, Electric/Ice ×1.5 taken; 3 turns; 20% proc |
| Status     | Chill line / Drowsy             | Speed ×0.75 and +5 Stamina per action (each)                       |
| Status     | Chill duration                  | 2 turns, renewed by Ice damage (Frozen never)                      |
| Status     | Burn                            | maxHP/16 per stack per turn; physical −10% per stack               |
| Status     | Electric line accuracy          | own ×0.9, incoming ×1.1 (Shocked+: Electric ×1.25 taken)           |
| Status     | Shocked → Paralyzed             | 25% per Electric hit                                               |
| Status     | Paralyzed                       | Speed ÷4; 25% lose turn                                            |
| Status     | Frostbitten / Nightmare         | maxHP/8 / maxHP/8 per turn, all Trauma                             |
| Status     | Confusion line                  | 3 turns; Stupefied 25% ally/self, Frightened 25% flee / refuse     |
| Status     | Poison heal block               | Recuperate, Leftovers, any heal ≤ maxHP/16                         |
| Status     | Full cure / berry               | clears Lv2 (berry: not Lv1); Lv3 → Lv1 (kept on switch: Nat. Cure) |
| Setup      | Speed-boosting setup cost       | 0 Stamina                                                          |
| Nature     | Neutral                         | Advantage (guaranteed hit if faster)                               |
| Nature     | Reckless (under-used offense)   | Super Luck (+1 crit stage)                                         |
| Nature     | Adamant / Modest                | Sheer Force (+30% dmg, no Lv1 status)                              |
| Nature     | Jolly / Timid                   | Quick Start (+1 Speed on entry)                                    |
| Nature     | Bold / Impish                   | Battle Armor (crit immune)                                         |
| Nature     | Calm / Careful                  | Resilient (+1 Sp.Def / Trauma-resist stage on entry)               |
| Nature     | Relaxed / Lax                   | Regenerator (HP on switch-out)                                     |
| Nature     | Sassy / Gentle                  | Natural Cure (status cleared on switch)                            |
| EXP        | Level band                      | ×2 within ±3 levels (wild only; trainer ×1); ×0.5 outside          |
| Pacing     | Time-to-death (physical)        | frail 5–7 / mid 7–10 / giga 10+ turns                              |
| Sweep      | Optimal-case trade              | 1 for 2–2.5                                                        |

### Open Items

* Average move BP confirmation (drives the Stamina pacing — assumed ~90).
* Validate the Special/Trauma clock against a high-Def/high-Sta wall lands in the ~10–15 turn window.
* Body Tax % final value (1.5–2% range; currently 2%).
* Nature ability magnitudes (Crusher +50%, Disruptor `a→0.50`, Resilient −25%) — validate vs the wall-breaking triangle.
* Cap on Toxic Trauma escalation (§6).
* Playtest the §6 status defaults (Lv1 slows / surcharges, Electric-line accuracy, Shocked's 25% paralysis, Paralyzed ÷4, Frostbite / Nightmare maxHP/8, Stupefied 25%, Frightened 25%, Soak 20%, the 50% sleep valve). AI scoring doesn't value the new levels yet; it still reads the vanilla `status1` mirror.
* Bag / party-menu item checks still read the party `status1`, so a Full Heal can't be used on a mon whose only status has no vanilla bit (Shocked, Stupefied on the field, a leftover Lv1). Full cures still leave the party `status1` at 0 for an active mon until the rail mirror is next sent (the battle state is right).
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
* **Menu cursors** (action menu, move menu + move-swap cursors, Yes/No, catch-or-not) are `textbox.png` tiles 1–2 and 28–31, placed on top of those white menu windows. Vanilla draws them in BG palette 0, which is now the dark textbox palette, so the arrows came out navy/blue/red. Their pixels now use `text.pal` indices (14 white, 13 dark grey, 15 light grey), and the `*CreateCursorAt` functions set palette 5 in the tilemap entry (`0x5000 | tile`).

#### Healthbox layout

Drawn from scratch rather than adapted from the vanilla box. Each box is a **dark corner-cut plate** (the same navy as the text box) plus a **status column** beside it.

* **Plate.** Player: 104×32. Row 1 has the name + gender symbol and, right-aligned, the level. Below it, the **HP bar** (4px), then the **Stamina bar** (2px), both 94px. The bottom row has the 1px **EXP track** (singles only) and, right-aligned, `current / effective max` HP. Opponent: 100×24, with the same name row and the same bars at 90px. It has no numbers or EXP, and shows a 5px caught-ball icon before the name when the species is caught. Player doubles plates are the player plate without the EXP track. The HP numbers are always shown with the bars; the vanilla doubles bars/numbers toggle (`SwapHpBarsWithHpText`) is a no-op.
* **Status column.** A box as tall as the plate, with one 12×9 cell per active status line in a fixed order: Soaked, Chill line, Burn, Poison line, Electric line, Sleep line, Confusion line. Each cell holds a 7×7 icon in the line's colour (drop, snowflake, flame, skull, bolt, Z, swirl) and one pip per stage the line can reach, lit bottom-up to the current stage. Cells fill top-down, two per column. Extra columns grow away from the plate: left for the player, right toward the screen centre for the opponent. Up to 6 lines can be active at once (Burn excludes Soaked and the Chill line), which is 3 columns. With no status the column is hidden. `GetHealthboxStatusStages` maps lines to stages: each line shows its status rail's level (§6). It only reads (it runs every frame), so a status that vanilla code wrote straight to `status1` and that no rail has picked up yet shows at the stage its bit stands for (Burn 1, Frozen / Frostbitten → Chill 2, Paralysis → Electric 3, Sleep → Sleep 2, confusion timer → Confusion 1). The column is the old healthbar sprite, now 64×32 on the healthbox palette. Its callback polls the stages every frame and redraws on any change, because confusion and rails change without a healthbox update.
* **Art vs runtime.** `generate_healthbox_art.py` draws only the static parts: plates, bar frames, the 40% notch and the EXP track. Its layout numbers must match `sHealthboxLayouts` / `HB_*` in `battle_interface.c`. Text, bar fills, the exhausted red frame, the ball icon and the status column are drawn at runtime with `FillHealthboxRect` / `PrintOnHealthbox`. These treat the two healthbox sprites as one 128px canvas. The HP bar no longer uses the `hpbar` tiles.
* **Healthbox palette** (`ball_status_bar.png`): 1 plate, 2 empty gauge / cell, 3 ink, 4 slate, 5 white text, 6 grey, 7 red, 8 green, 9 purple, 10 cyan, 11 orange, 12 blue, 13 yellow, 14 sky, 15 pink (Confusion). Text is white with an ink shadow. The gender symbols use explicit colours (blue ♂ / red ♀) instead of the shared dynamic text colours.
* **Positions.** Every plate starts at the sprite's `(x − 32, y − 16)`. The 64×64 player singles box keeps the 64×32 template's centre offset. Gimmick indicators sit just left of the level digits (`sIndicatorPositions` in `battle_gimmick.c`).
* `B_HP_PERCENTAGE_DISPLAY` must stay **FALSE** (a `STATIC_ASSERT` enforces it); the plates have no room for it.
