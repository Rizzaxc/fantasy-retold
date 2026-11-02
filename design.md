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
* **Accelerated Overworld Cadence:** Outside of battle, link / wireless, and test-runner sensitive paths, the main callback loop advances several logic updates per rendered frame, chosen by the **SPEED UP** row in the Option menu: **OFF** (1x), **2X**, or **4X** (new-game default 4X). It is stored in `gSaveBlock2Ptr->optionsSpeedUp` (2 spare option bits, `OPTIONS_SPEED_UP_*`) and mapped to updates per frame by `sOverworldLogicUpdatesPerFrame` in `main.c`. Before a save is loaded (title / intro) the field reads 0, so those screens run at 1x; saves made before this option existed also read 0 (OFF) until changed. Battle pace is not affected by this option. Music mixing and map-music cadence remain on the original VBlank schedule, so BGM tempo stays vanilla; **sound effects stay tied to game logic** and therefore accelerate with the faster simulation. The play-time counter also remains VBlank-based so the save clock tracks real time rather than simulation ticks. The Pokémon Center 1F's passive Union Room scan (`InitUnionRoom`, run from `CableClub_OnResume`) sets `gWirelessCommType` without any partner, so it does **not** count as a wireless session (`IsUnionRoomPassiveScanActive`): the overworld keeps the chosen speed inside Centers, while the extra ticks call the callbacks directly so RFU link servicing stays at 1x. Vanilla never shuts that scan down (map loads and menus wipe its task with `ResetTasks`), which left `gWirelessCommType` stuck at 1 after leaving a Center and pinned the overworld to 1x; `ResumeMap` now calls `StopUnionRoomPassiveScan` first, closing the scan's link, and the Center's OnResume script restarts it if the player is still inside. Menus also run at the chosen speed, so a load step can run again before VBlank has done the previous step's queued copy. Vanilla `LoadRegionMapGfx` (`region_map.c`) skipped the PokéNav map's tilemap and palette when that copy was still pending, which left the PokéNav map garbled; those steps now wait for the copy instead (the field map and Fly map load in one blocking loop, so only the palette step there goes ahead without waiting).
* **Battle Pace Compression:** Battles keep their controller/menu loop at **1x** for stability, but combat presentation is accelerated in two safe ways: (1) stable in-battle visual/task updates run `BATTLE_VISUAL_UPDATES_PER_FRAME` times per rendered frame (currently **3x**) **only while `gMain.callback2 == BattleMainCB2`**, so party/bag/menu handoffs remain untouched; and (2) `BATTLE_SPEED_MULTIPLIER` (currently **2x**) shortens battle-script pauses, battle text delay, and battle animation-script frame waits. This speeds up combat flow without reintroducing the menu/switch hangs caused by multi-stepping the full battle loop.

---

## 2. The Stamina Economy & Speed Mechanics

Stamina serves as the primary operational currency governing action speed, move execution, and defensive integrity.

### The Stat Block (`BaseSta` & `CurrentSta`)

* `BaseSta` is the repurposed Special Defense stat, used at its native scale (no rescaling — EV/IV expectations are preserved).
* **Stamina persists across switches.** A Pokémon's first send-out of the battle enters fresh at `CurrentSta = BaseSta`. On every **switch-out**, it banks its current Stamina plus a level-scaled rest bonus of **5 + Level / 2** (capped at `BaseSta`); it returns to the field at that banked value. Speed decay is a function of `CurrentSta/BaseSta`, so it carries across switches too — an exhausted mon that pivots out comes back slow *and* near-empty.
* **UI note:** Both healthboxes show Stamina as a **bar directly under the HP bar, the same length** (no number), so the two read side by side. It fills left to right as `CurrentSta/BaseSta`: **green** normally, **yellow** at or below the Recuperate threshold (`RECUPERATE_STAMINA_PERCENT`, 40%; a white tick through the bar's frame marks the threshold) so the player can see when R is live, and **empty with a red frame** at 0 Stamina (Exhaustion Zone); otherwise the frame matches the fill colour. Any non-zero Stamina draws at least 1px. Exhaustion / auto-crit checks always use the exact live `CurrentSta`, not the drawn gauge. The bar **slides** to a new value at the HP bar's speed (a full bar in 24 frames, as `MoveBattleBar` does with `B_FAST_HP_DRAIN`): `battle_interface.c` keeps the shown value per battler (`sStaminaShown`) and the status-column sprite callback steps it every frame (`StepStaminaGauge`). A full healthbox redraw (a send-out) shows the live value at once.

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

* **Futile Recuperate:** Recuperate is skipped whenever it can't buy a safe attack: if, after the refill (+40%) and the predicted incoming attrition, its cheapest attack would still empty it next turn, it attacks now with its best attack instead, exhausting or not (`AIRecuperateIsFutile`). This is the small-pool case: at Lv5 a pool is ~10, and a 40 BP hit, the attack cost and the refill are all ~4, so without this check the AI Recuperates every turn and never attacks (issue #1, rival Torchic vs Pound-spamming Treecko).

0. **Exhausted (0 Stamina): Recuperate** (unless futile, above), unless a scored attack KOs the target before the target acts. An exhausted Recuperate has no priority, so a faster target's hit still auto-crits that turn, but staying at 0 means every later hit does too.
1. **Take the KO** if any scored attack faints the target.
2. **Punish exhaustion:** a target at 0 Stamina is attacked with the best attack that won't exhaust the AI.
3. **Dodge** (never twice in a row) when it has the Stamina for it and either: the incoming hit would KO it and Dodging saves it (a full miss, or half damage it survives); or Dodge is a guaranteed miss (it acts before the target) on a physical hit or one taking ≥ 1/3 of its HP — the latter only half the time, so it isn't a fixed pattern.
4. **Progress over refill:** at or below the threshold, Recuperating costs a turn, so it first looks for a move that still moves the game forward and leaves it above 0 Stamina after the incoming attrition. In order: a physical hit whose attrition **exhausts the target** (its later hits taken auto-crit); a move that **lands or advances a status** the target can still take (a status move, or an attack with at least a 30% status chance after doubling), when the target needs **3+ hits** to KO it (`AI_STATUS_PRESSURE_MIN_HITS_TO_KO`), picking the best-scored one; and, unless it is the bulkier battler, its best safe attack when the target is **also drained** (at or below its own Recuperate threshold, with no larger share of its pool left) — both are low, so trading hits beats giving the target a free turn.
5. **Recuperate** at or below the threshold if it is bulkier than the target (more max HP *and* BaseSta), or if it isn't frailer (less of both) and the target can't KO it this turn. A frailer battler keeps attacking instead.
6. **Avoid self-exhaustion:** if the chosen attack plus the incoming physical attrition would empty it, it switches to a safe alternate attack (half the time Recuperating instead when it is repeating the same move), else Recuperates, else switches out (the switch is sent in the action phase). **Exception:** a status move the scoring rates at least +3 (`AI_STAMINA_STATUS_WORTH`; e.g. a hazard on its first turn, +4) is kept when the AI can pay its cost and the target can't KO it next turn. Status moves cost a flat 10, so without this a small early pool (a Lv10 Aron has ~15) never set Stealth Rock against any physical attacker.

**Type-immune moves are never picked by these steps.** A damaging move into a type immunity (Electric into Ground) still scores ~80 after the -20 penalty, so the Stamina pickers (`FindBestAIDamagingMove`, the progress-over-Recuperate pick, `AIRecuperateIsFutile`) skip it explicitly (`AIMoveIsTypeImmune`); previously the cheap immune move was the only "safe" one left once pricier attacks were ruled out.

**Smart trainers (`AI_FLAG_FR_TACTICS`, §10) refine this:** every step also weighs the Signature / Itemized Move (AI move slots 4 and 5); a crit-immune AI (Battle Armor / Shell Armor ability, or the Bold / Impish innate) skips step 0 (exhaustion's auto-crits can't hit it) and, at step 4, also counts any status, hazard or setup move the scoring rates at least neutral as progress; step 2 and the Stamina score bonus skip a target the AI knows is crit-immune; and the Dodge step uses the attacker's Dodge bypass (a Signature Move or Choice Scarf hit is never a full miss, so it Dodges only to halve it) and counts on Repeated Move Trapping (the player repeating last turn's move is a sure miss). A Choice item holder never Dodges or Recuperates (§9), for every AI.

A slower battler at full Stamina against a full-Stamina target just attacks when the exchange doesn't exhaust it. The "incoming hit" is the predicted move when `AI_FLAG_PREDICT_MOVE` rolls a prediction; otherwise it is the target's strongest damaging move the AI knows about (revealed moves, or all with move omniscience), so the flag works without prediction flags.

### Pacing Targets (the Physical Clock)

Stamina depletion (own actions + incoming physical attrition) governs the **physical** time-to-death. With vanilla BP (~90 average), the design lands:

* **Frail mons:** ~5–7 turns to death.
* **Mid-bulk:** ~7–10 turns to exhaustion.
* **Giga-bulk:** ~10+ turns to exhaustion.

These bands assume physical pressure. Against a **special** attacker, Stamina drains roughly half as fast (no incoming attrition), and the time-to-death is instead governed by the **Trauma clock** (§4), which keys off the target's Defense, not its Stamina.

---

## 3. Innate System Commands

Every Pokémon possesses two innate mechanical actions, reached from the **innate menu**. On the move selection screen, **L** (R in the L=A button mode, where L acts as A) swaps the four move slots for **Dodge**, **Recuperate**, the mon's **Signature Move** and its **Itemized Move** (each if it has one, §10); L or B swaps back. An entry that can't be used this turn is drawn faded (not enough Stamina for Dodge, above the 40% gate for Recuperate, no PP left on a move) and picking it shows why. The right-hand panel shows the highlighted entry's Stamina cost (`STA -n`) or gain (`STA +n`), or for the Signature / Itemized Move its PP (`PP n/max`) and type, as on the move screen. The innate menu has no description panel. The `L / MOVE INFO` tab above the menu is relabelled `L / INNATE` (upstream's move-description viewer is no longer reachable). Recuperate executes with high-priority sequencing, in the item usage window ahead of all moves, **unless the user is at 0 Stamina**: an exhausted Recuperate loses that priority and resolves in normal speed order (an exhausted mon is also slow, so a fast attacker gets its auto-crit in first). Dodge keeps its normal speed slot in the turn order for resolution, because its outcome (below) is decided by who acts first, but it is *announced and animated* first in the turn (see Battle text). **Selecting an Innate Command consumes the Pokémon's action for that turn** (it does not attack).

**Battle text.** When the action runs, Dodge prints *"X is dodging!"* and Recuperate prints *"X is recuperating!"*, each before its animation (`BattleScript_FRDodge` / `BattleScript_FRRecuperate`). If Recuperate also purged a Level 1 status, it uses `BattleScript_FRRecuperateCured`, which adds *"X shook off a minor ailment!"* after the HP heal. **Dodge is presented first in the turn:** before any action runs (even a switch or item), each dodger in turn order prints *"X is dodging!"* and plays its animation (`TryShowDodgesBeforeActions` → `BattleScript_FRDodgeIntro`, once per turn via `dodgeIntroDone`), so a later attack that is nullified or halved never appears to beat a Dodge the player hasn't seen yet. This is presentation only: the Dodge action still resolves in its own speed slot, paying its Stamina cost silently (`BattleScript_FRDodgeShown`, `dodgeAnimShown`) and deciding full miss vs half damage exactly as below. A dodger KO'd before its slot has still announced. The intro runs through `BattleScriptExecute`, which zeroes `gCurrentActionFuncId` (= use move), so `TryShowDodgesBeforeActions` restores the first action's real type afterwards; otherwise whoever acts first (often the dodger) also used its last chosen move.

When an attack meets a Dodge, the result says which outcome happened:

* **Full dodge** (the dodger acted first, or the attacker repeated its move): *"X dodged the attack!"* instead of the usual *"X avoided the attack!"* (`Cmd_resultmessage`).
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
* **Priority:** Dodge is a priority-0 action in the speed slot (only `B_ACTION_USE_MOVE` reads a move's priority in `GetWhichBattlerFasterOrTies`). So a **+priority** attack (Quick Attack, Aqua Jet...) or Quick Claw / Custap / Quick Draw acts before it and is only *halved*, whatever the speeds; a **-priority** attack acts after it and is a *full miss*, even from a faster mon. Equal priority falls back to Speed (reversed under Trick Room). Dodge never gains priority itself, unlike Recuperate, which runs in the item window.
* **Speed Tiers & Outcomes** (compared on **turn order**, i.e. priority first, then Effective Speed):
  * **Attacker is Slower:** The move completely misses.
  * **Attacker is Faster, or Tie:** The move lands, but damage is mitigated by **50%** and cannot proc status effects.
  * **Repeated Move Trapping:** If the user is dodging an identical move executed by the opponent on the immediate prior turn, the move completely misses, regardless of speed tiers — **status moves included** (using Stun Spore two turns running into a Dodge fails). Each battler records the move it used this turn (`moveUsedThisTurn`, set in `HandleAction_UseMove`); `BattleTurnPassed` rotates it into `moveUsedLastTurn`, so a turn without a move (Dodge, switch, item) breaks the chain, and leaving the field clears it. `IsMoveFullyDodged` / `IsMoveHalvedByDodge` apply it, and it only applies across sides (never to self/ally-targeted moves). Smart trainers count on it (§10).
* **Dodge bypass** (`GetDodgeBypass`, by the attacker's held item and move position): a **Signature Move** is never a full miss: whatever would have been one (the dodger acting first, or a repeated move) is the halved hit instead. **Choice Band / Specs**: repeating the move isn't a full miss (the speed rule still applies). **Choice Scarf**: never a full miss, halved at worst; its holder's **Signature Move ignores Dodge** entirely (full damage). Choice items are read through `GetBattlerHoldEffect`, so Klutz / Embargo / Magic Room switch this off.
* **Restrictions:** Apart from Repeated Move Trapping, status moves bypass Dodge checks entirely (but can still miss, via its Acc. check). Selection is blocked in the action selection UI if the Pokémon does not have enough Stamina to cover the cost. A Pokémon **holding a Choice item can't Dodge or Recuperate** (faded in the innate menu, *"A Choice item rules out Dodge and Recuperate!"*; `IsInnateCommandBlockedByChoice`); its Signature / Itemized Move stay usable and choice-lock it as usual.

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
* **Display.** The HP numbers show `current / (maxHP − Trauma)` — the effective max, since the rest can never come back. The HP bar still spans the original max HP and reads, left to right: **purple** (Trauma, permanently lost) → **red** (current HP) → **slate grey** (recoverable loss). Because current HP is capped at the effective max, the red fill always starts where the purple ends. Any non-zero Trauma draws at least 1px. The purple **slides** to a new Trauma value at the HP bar's speed, like the Stamina bar: `battle_interface.c` keeps the shown value per battler (`sTraumaShown`), and the status-column sprite callback steps it every frame (`StepTraumaGauge`), redrawing the HP bar with the last HP fill it drew. A full healthbox redraw (a send-out) shows the live value at once.

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
| **Calm, Careful** | +SpD/Sta | **Resilient** *(custom)* — immune to Special Trauma; the hit is dealt as normal HP instead.                                | the permanent-damage clock |
| **Relaxed, Lax**  | +Def     | **Regenerator** *(vanilla)* — restores some HP when it switches out.                                                   | HP attrition               |
| **Sassy, Gentle** | +SpD/Sta | **Natural Cure** *(vanilla+)* — cured of every status when it switches out & downgrades the first status on switch in. | the status / Toxic clock   |

### Notes

* **Meaningful trade, not an upgrade:** Adamant (Sheer Force: power, no status) vs a +Atk Reckless nature (Super Luck: crit/Trauma, keeps status). Each wall ability likewise answers one threat while leaving the others open — Resilient shrugs off Special Trauma but not crit Trauma, Toxic or status; Natural Cure cleanses status but not a Trauma nuke; Battle Armor stops crits but not statuses.
* **Off-meta uplift:** handing Regenerator / Natural Cure to the off-meta walls (Relaxed, Lax, Sassy, Gentle) gives them a reason to see play.
* **[Alt — Jolly/Timid]** On-KO snowball (Moxie / Grim Neigh, +1 Atk/SpA per KO) remains an offense-flavored option, naturally capped by halved damage.
* **[Alt — Modest]** Tinted Lens (ignore one resistance step) if Adamant/Modest should differ.
* **[Tune]** Regenerator's heal amount is designer-side; player text stays plain ("heals on switch"). **Resilient** is a flat immunity to the *Special* Trauma fraction (§4) — the hit still deals its full damage to HP, just none of it is logged as unhealable. Crit-bonus Trauma and Frostbite / Nightmare / Toxic ticks still apply. Implemented as a skip of the Special block in `TrackTraumaFromHit` (`battle_move_resolution.c`); the former +1 SpD-stage-on-entry is removed.

---

## 6. Systemic Status Overhaul

### Core Model

* **Multi-stage rails.** A condition advances through levels. Applying the **same kind** again stacks it upward (Lv1 → Lv2 → Lv3). **Status moves apply directly at Level 2, and stack**: a direct Lv2 application on a mon that already has Lv1 of that line takes it to **Lv3** (Lv2 → Lv3 as before); **attacking-move secondary effects, abilities and self-inflicted effects apply at Level 1**. (Exception: the Chill line always starts at Chilled, and never stacks past it.) **Secondary chance above 100%:** attacking moves double their status chance, so a base chance over 50% exceeds 100%; the excess is a separate roll to apply that secondary at **Lv2** instead of Lv1 (Zap Cannon, 100 → 200%, always lands Shocked). It is only a floor, so if the normal Lv1 stacking would give Lv3 anyway, that wins (`STATUS_RAIL_APPLY_FLOOR`, `SetEffect.lv2Bonus`).
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
* **Hit-triggered advances respect immunities.** Shocked → Paralyzed on an Electric hit and the Ghost / Psychic crit upgrades are blocked by the same things as a normal application (abilities, types, Safeguard, Misty / Electric Terrain, ...). A hit that applies Drowsy or Confused cannot also promote that newly applied line straight to Lv3 with its critical hit; a critical upgrade only applies to a line that was already present before that move.
* **"Has a status".** Effects that check for a status — Guts, Marvel Scale, Quick Feet, Facade, Hex / Infernal Parade, the catch-rate bonus — count **Lv2+ of any line, or Soaked**; Lv1 doesn't count. Line-specific ones read their own line at Lv2+: Toxic Boost, Venoshock, Barb Barrage, Merciless (Poison), Flare Boost (Burn), Smelling Salts (Electric), Wake-Up Slap and the Dream Ball (Sleep). Catch rate: Asleep+ or Frozen gives the big bonus, any other qualifying status the small one.
* **Moving lines.** **Psycho Shift** moves the user's first line the target can take (Poison, Burn, Electric, Sleep, Chill, in that order), at the same level, and cures the user of that line; other lines don't block it. **Baton Pass** hands the whole Confusion line (level and turns left) to the incoming mon, and the passer loses it.
* **Yawn** makes the target **Drowsy at once**; if it is still Drowsy at the end of the next turn it falls Asleep. Only an Asleep / Nightmare target makes Yawn fail (other lines don't).
* **Status Orbs.** Flame Orb and Toxic Orb are the exception to "self-inflicted lands at Lv1": they land at **Lv2** (Burn ×2 / Toxic), so Guts, Facade, Toxic Boost, Flare Boost, Poison Heal and Marvel Scale count them. Each fires only while its line is below Lv2 (once per send-out in practice, not every turn) and goes through the rail (`RaiseStatusRailTo` in `TryFlameOrb` / `TryToxicOrb`), so the holder's other lines are kept (the old direct `status1 =` write wiped their mirror bits, and the next reconcile cured them).
* **After battle.** Lv1 is volatile, so the battle ending drops it like a switch-out; Lv2+ stays on the party (e.g. Toxic keeps poisoning in the overworld) with its level intact: Toxic doesn't revert to Poison (`DowngradeBadPoison` is off), and the lines with no vanilla `status1` bit at that level (Shocked, Nightmare, Stupefied / Frightened, Burn stacks) are kept in party-only `STATUS1_SAVED_*` bits (2-bit level each) of the mon's status, written by `GetStatusRailPartyStatusAfterBattle`, read on the mon's first send-out (`SeedStatusRailsFromSavedStatus`), and stripped from the battle copy of `status1`. A Burn / Sleep / Paralysis cure from the party menu clears the matching saved bits (`GetStatusRailSavedBitsToCure`); a Pokémon Center clears everything. A restored Confusion line restarts its 3-turn timer. Contact abilities (Static, Flame Body, Poison Point, Effect Spore) land at Lv1 at their normal chance; one layer of Toxic Spikes gives a volatile Poison ×1, two give Toxic.
* **Synchronize** has no exceptions: every line (Poison, Burn, Chill, Electric, Sleep, Confusion) is passed back, and the attacker's line jumps to the **synchronizer's level** (Frozen / Frostbitten keep their branch). If the attacker is already that deep, the line just advances one step.
* **Messages.** Every level and proc has its own line:
  * *Applied:* Drowsy / Dazed / Shocked / Chilled / Stupefied / Frightened / Nightmare have FR messages ("X grew drowsy!", "X is dazed!", ...), "X's burn got worse!" for extra stacks, vanilla messages where a vanilla status exists. Dazed + Soaked → "The water conducted the shock! X is paralyzed!".
  * *Procs on a hit:* "X got soaked!", "X's burn eased!" / "X's burn was doused!" (a Soak on a burned target), "The shock left X paralyzed!" (Shocked), "The ghastly blow plunged X into a nightmare!" / "The psychic blow terrified X!" (crits), "X woke up!" (valve), "X warmed up!" / "X's burn eased!" / "X shook off some static!" (type interactions).
  * *Acting:* the confusion line opens with "X is confused! / stupefied! / frightened!" by level; a Stupefied proc adds "X turned on its ally!" or "It turned its own attack on itself!"; Frightened adds "X panicked and tried to flee!" or, from the bench, "X is too frightened to come out!". Nightmare: "X is trapped in a nightmare!".
  * *Blocked:* "X is too wet / too cold to be burned!", "X's burn keeps the cold away!", "X is already frozen solid! / frostbitten!", "It doesn't affect X" for a Psychic-type hit by a confusing status move, and "X's poison kept it from healing!" when Recuperate is poisoned.
  * *Ending / cures:* "X dried off!", "X warmed up!", "X woke up!", "X snapped out of it!"; a cure that only removed Soaked prints "X dried off!". Psycho Shift prints the message for the level it moved.
* **UI.** Every active line gets a cell in the healthbox **status column** (see "Healthbox layout" under §12, Battle UI graphics folder): its icon plus one pip per stage the line can reach, lit up to the current stage.

### The Condition Core

Numbers below are the current defaults (`include/constants/battle.h`, "status rail tuning"). Speed slows stack multiplicatively with each other and with Stamina decay.

**Soaked** *(Lv1, single-stack)*

* Speed ×0.85. Incoming Fire damage ×0.5; incoming Electric and Ice damage ×1.5.
* **Source:** every damaging Water move has a **20%** chance to Soak each target it hits. On a **burned** target the Soak instead **takes one Burn stack off** ("X's burn eased!", or "X's burn was doused!" when that was the last one). This is the only way Water counters a Burn: a Water hit that doesn't Soak leaves the Burn alone.
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
* **Exclusions:** Can't apply to a Soaked or chilled target. **Loses a stack** whenever the burned mon uses, or is hit by, an Ice move, or a Soak proc lands on it (above). Plain Water damage doesn't touch it.
* Fire-type immune.

**Poison → Toxic**

* **Progression:** Baseline Poison rules govern Level 1; status-driven application upgrades to Toxic (Level 2).
* **Heal block (whole line):** a poisoned mon **can't heal from Recuperate, Leftovers, or any heal of maxHP/16 or less** (a weak drain, Aqua Ring, Rain Dish, ...). The shared poison check reconciles the status mirror before reading the Poison rail, so Leftovers and queued heals cannot miss a vanilla-written Toxic status. Every blocked heal says *"X's poison kept it from healing!"* A poisoned Leech Seed user cannot receive the drain's healing, and its seed deals no damage to the target. Recuperate still restores Stamina.
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
* **Critical Ghost hit:** a crit from a Ghost-type move on a Drowsy / Asleep target sinks it into **Nightmare** (keeping the count). If that move also inflicts Drowsy, the same hit does not immediately promote the new condition to Nightmare.
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
* **Critical Psychic hit:** a crit from a Psychic-type move on a Confused / Stupefied target makes it **Frightened** (keeping the count). If that move also inflicts Confused, the same hit does not immediately promote the new condition to Frightened.
* Psychic-type immune.

### Implementation

* **State.** `struct PartyState.rails[]` (`StatusRailEntry`: `level`, `counter` = Toxic escalation or turns left, `branch` = the Chill line's Lv2 form, `keepOnSwitch` = a Natural Cure leftover Lv1, kept through one switch-out) is the source of truth; `src/status_rail.c` owns it.
* **Mirror.** The vanilla `status1` bits and `volatiles.confusionTimer` are kept as a mirror (`SyncStatusRailBridge`): Poison/Toxic, Burn (any stacks), Frozen / Frostbitten, Paralyzed, Asleep / Nightmare (the sleep bits hold the turns left) and the whole Confusion line. Several bits can now be set together. Soaked, Chilled, Dazed, Shocked and Drowsy have no vanilla equivalent and exist only on the rail.
* **Reconcile.** Every rail change first reads the mirror back (`ReconcileStatusRailsFromBridge`), so anything vanilla writes straight to `status1` / `confusionTimer` (Toxic Orb, Rest, Yawn, Psycho Shift, Outrage fatigue, Uproar waking a sleeper, ...) is picked up. The healthbox only reads, since it polls every frame. Full cures go through `CureAllStatusRails` / `CurePartyStatusRails` **without** reconciling first (the caller may already have zeroed `status1`, which would wipe the Lv3s they keep), then re-sync the mirror; `BS_ItemCureStatus` puts the mirror back after `HealStatusConditions` clears it.
* **Hooks.** Application: `SetNonVolatileStatus` / `HandleSetEffectConfusion` through `ApplyStatusRailOrSynchronize` (new levels print through `BattleScript_FRRailEffectApplied` with `gBattleScripting.savedStringId`). Synchronize: `TrySynchronizeActivation` stores the line and level in `gBattleStruct->synchronize*`, `Cmd_trysynchronize` passes them back; `BattleScript_MoveEffectConfusion` now runs `trysynchronize` too. "Already" checks / immunities: `CanSetNonVolatileStatus` (Ghost vs sleep) / `CanBeConfused` (Psychic). Actions: `CancelerAsleepOrFrozen` (Nightmare blocks Snore / Sleep Talk), `CancelerConfused` (+ `BattleScript_FRStupefiedSelfHitRet` and the redirect in `CancelerSetTargets`), `HandleAction_Switch` (Frightened refusing, `BattleScript_FRFrightenedRefusedSwitch`). After a damaging move: `MOVEEND_STATUS_RAILS`, five steps per battler (type interactions; Chill renewal + Soak / douse; Shocked → Paralyzed; crit upgrades; Sleep valve). End of turn: Burn stacks, Frostbite and Nightmare Trauma ticks, and `ENDTURN_STATUS_RAILS` (timers, active battlers only). Effects: `GetStatusRailSpeedPercent`, `GetStatusRailStaminaSurcharge` (through `GetMoveStaminaCost` / `GetDodgeCost`), `GetStatusRailAccuracyPercent` + `GetStatusRailIncomingAccuracyPercent` (in `GetTotalAccuracy`), `GetStatusRailIncomingDamageModifier`, `GetBurnRailPhysicalModifier`. Heal block: `IsHealBlockedByStatusRails`, checked in `Cmd_healthbarupdate` (any queued heal), `TryLeftovers` and `HandleAction_Recuperate`; blocked heals print the poison message. `HandleEndTurnLeechSeed` also checks the poisoned seed user and prevents its target from taking drain damage. Berries: `CureStatusRailWithBerry` in the `TryCure*` helpers of `battle_hold_effects.c`. Other cures: `CureStatusRails` / `HasAnyStatusRail` / `GetStatusRailCureMessage` (Shed Skin / Hydration in `battle_util.c`, `BS_CureStatus`, `HandleSetEffectRemoveStatus`); Refresh is `Cmd_curestatuswithmove`; Jungle Healing and Purify gate on the `jumpifanystatusrail` script command; immunity abilities in `TryImmunityAbilityHealStatus`. Status checks: `HasStatusRailCondition` / `HasStatusRailConditionForStatus1`. Psycho Shift: `BS_TryPsychoShift` + `frpsychoshiftcureuser`. Baton Pass: `Cmd_getswitchedmondata` stashes the line in `gBattleStruct->batonPassConfusion*`, `SwitchInClearSetData` restores it. Yawn: `Cmd_setyawn` + the end-turn Yawn handler. Battle end: `HandleEndTurn_FinishBattle` writes `GetStatusRailPartyStatusAfterBattle` to each sent-out party mon.
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

* **Speed-boosting setup moves cost no Stamina** (Dragon Dance, Agility, Quiver Dance, etc.; any status move with a Speed raise, `IsSpeedBoostingSetupMove` in `GetMoveStaminaCost`; the Chill / Drowsy surcharge still applies). This lets the sweeper enter its sweep at full Effective Speed, so the decay fuse begins when it starts attacking — improving the *consistency* of reaching ~2–2.5 kills without raising the ceiling.

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
* **Choice Band, Specs**: vanilla, but repeating the locked move into a Dodge isn't an automatic miss (Repeated Move Trapping doesn't apply; the speed rule does). No Dodge / Recuperate while held (§3).
* **Choice Scarf**: vanilla, but its moves are never fully Dodged (halved at worst), and its holder's Signature Move ignores Dodge (full damage). No Dodge / Recuperate while held (§3).

### No bag items in trainer battles

The player can't use the bag in **any trainer battle** (`IsAllowedToUseBag` returns FALSE for `BATTLE_TYPE_TRAINER`; picking BAG prints *"Items can't be used now."*). Wild battles follow `B_VAR_NO_BAG_USE` as before (Poké Balls still work). Battle tests keep the bag (`gTestRunnerEnabled`), so the upstream item tests still run. On the AI side, every big battle carries no items: the `Items:` lines were removed from all gym leaders (and rematches), rivals (Wally, Steven), Magma / Aqua leaders and admins, the Elite Four, the Champion and every Smart trainer, and `ShouldUseItem` never uses one for `AI_FLAG_FR_TACTICS`. Other trainers keep theirs.

### Competitive berries restore after battle

Held items used up in battle come back when it ends (`B_RESTORE_HELD_BATTLE_ITEMS`, Gen 9). Gen 9 leaves out berries; here **every berry with a battle hold effect** (Sitrus, Lum, the cleansing / pinch / resist berries, ...) comes back too, so a berry is a reusable battle item, not a consumable. Berries with no hold effect (Razz, Pomeg, ...) are never eaten in battle anyway.

* **Engine.** `IsItemRestoredAfterBattle` (`battle_util.c`) is the one rule, used by `TryRestoreHeldItems` (party after battle) and `RestoreCaughtWildMonHeldItem` (a caught wild mon gets back the berry it ate).

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

* **Mastering a move.** Every use of a slot move in battle counts toward mastering it, **per Pokémon and per move**, never per party slot or move slot: reordering moves, the party or the PC changes nothing. A move used `SIGNATURE_MOVE_USE_THRESHOLD` = 100 times is **mastered**. Only the player's own Pokémon count (not a multi-battle partner's), and only a move the Pokémon really has in that slot (not a Mimic / Transform copy). The Itemized Move and the Signature Move itself don't count.
* **Storage.** `SaveBlock1.moveUseRecords[PARTY_SIZE]` (`struct MoveUseRecord`): one record per Pokémon, keyed by personality, holding up to 4 move IDs with their counts (bits 0–6, saturating at 127; bit 7 = mastery announced). A Pokémon that needs a record takes a free one, else one of a Pokémon no longer in the party, else the least-used one, so a deposited Pokémon keeps its progress until its record is reused. A move it no longer knows gives its entry to a new move unless it is mastered. These records replaced the old per-slot counters (which leaked counts from one Pokémon to whoever took its party slot); every Pokémon started again from 0.
* **"Mastered" prompt.** Before a battle closes (not after a loss, nor in link / Frontier / e-Reader battles), each newly mastered move prints *"X mastered Y!"*, plus *"Once fully evolved, it can make a mastered move its Signature Move!"* if X isn't fully evolved (`TryAnnounceMasteredMove` in `HandleEndTurn_FinishBattle`, `BattleScript_FRMoveMastered` / `...NotFinal`). Once per move.
* **Committing it.** A Signature Move is **never assigned automatically**. When a Pokémon **evolves into its final stage** (evolution scene: level up, Rare Candy, items) with at least one mastered move it still knows and no Signature Move, the scene says *"X can commit a mastered move as its Signature Move!"* and opens the move selection screen (`ShowSelectSignatureMoveSummaryScreen_BW`; the row below the moves is "Cancel"). Only a mastered move can be picked (others play the error sound), then *"Make Y its Signature Move?"*: Yes commits it (*"Y became X's Signature Move!"*), No goes back to the selection. Cancel / B gives *"X didn't commit a Signature Move."* This runs before the evolution's new moves are learned. The move then **leaves the four-slot moveset**, freeing that slot (later moves, their PP and PP Ups shift up), and the Pokémon's move records start over (`CommitSignatureMove`). Trade evolutions don't offer it (a traded-in Pokémon has no mastered moves). Saves that still hold the signature in a slot are fixed up after battle and after evolution (`FixUpSignatureMove(s)`).
* **TODO: earning one later.** A Pokémon that refused, mastered a move only after reaching its final stage (or never evolves), or forgot its Signature Move at the Move Relearner has no way to commit one yet; this path is still to be designed.
* **Forgetting it.** The Move Relearner lists the Signature Move first on the level-up list as *"Y (Sig.)"* (so `START RELEARN` / the NPC relearner show up for it). Picking it asks *"Make X forget its Signature Move, Y?"*; Yes clears it and the Pokémon's move records (*"Its uses count from 0 again."*). It is not put back into a slot; since it no longer counts as known, it can be relearned like any move. (`ForgetSignatureMove`, `move_relearner.c`.)
* **Knowing it.** The Signature Move counts as known: level-up learning, TMs and tutors treat it as already learned, so it can't be re-added to a slot.
* **Using it.** It is picked from the innate menu (L on the move screen). It has **exactly 8 PP** (`SIGNATURE_MOVE_PP`, no PP Ups) that **persist between battles**, like a slot move's PP: healing (Pokémon Center, whiting out, `HealPlayerParty`, depositing in the PC) refills them, and a newly earned Signature Move starts full. Using it pays the normal Stamina action cost (BP/10 + Body Tax) as well. The menu shows `PP n/8` and its type. Disable, Taunt, Encore, Choice lock etc. still apply through the engine's normal checks. In doubles it currently uses the move's default target (no manual target pick yet).
* **Engine.** It is chosen with an extra move position, `SIGNATURE_MOVE_POSITION` (= `MAX_MON_MOVES`); `GetBattlerMoveAtPosition` resolves it. It is never stored in `gBattleMons[].moves / .pp`, so code that indexes those by a chosen/current position checks `position < MAX_MON_MOVES` (Last Resort's used-move bits; Mimic and Sketch fail when used as the signature). Its PP is `MON_DATA_SIGNATURE_PP`, stored as PP **used** in 4 spare bits of substruct 1 (`signaturePPUsed`, was `unused_04`), so 0 = full and saves from before this start full; `BoxMonRestorePP` refills it. In battle it is read and taken on the party mon directly through `GetBattlerExtraMovePP` / `DeductBattlerExtraMovePP` (selection and canceler PP checks, PP deduction incl. Pressure, Grudge).
* **Summary screen.** The BW summary's battle moves page is vanilla (four moves, `START RELEARN` at the bottom). **SELECT** swaps the four rows for the **innate moveset**, in the battle innate menu's order: Dodge, Recuperate (shown as *Innate*), then the Signature Move (type icon, name, PP `n/8` coloured like slot PP) and the Itemized Move (type icon, name, `4/4`), whichever it has; SELECT swaps back, and leaving the page resets to the four moves (changing Pokémon keeps the view). `A INFO` inspects them: the Signature / Itemized Move show the normal move details, Dodge / Recuperate a short description; the innate rows can't be reordered. SELECT does nothing on the contest page or in the forget-a-move / commit screens. A `SELECT INNATE` prompt sits at the bottom of the page, right of `START RELEARN` (or in its place when there's nothing to relearn); it's a sprite drawn in the relearn prompt's font and palette (`graphics/summary_screen/bw/innate_prompt.png`, `UpdateInnatePrompt`) and hides with it during move selection. (`showInnateMoves`, `GetBattleMoveRowMove` / `PrintBattleMoveRowDetails` in `bw_summary_screen.c`.)
* **Announcement.** Using it prints *"X is using its Signature Move!"* before the usual *"X used Y!"* (`BattleScript_SignatureAttackstring`, picked in `CancelerAttackstring` when the move position is `SIGNATURE_MOVE_POSITION`). It is skipped on a charge move's second turn and on a Dancer copy.
* **No gimmicks.** A Signature Move never carries a gimmick (see §3).
* **Trainer parties.** A trainer's Pokémon can be given one in `src/data/trainers.party` with a `Signature Move: <Move>` line among its attributes (before its moves). `trainerproc` writes it to `TrainerMon.signatureMove` and refuses a Signature Move that is also one of that Pokémon's four moves (it counts as known). `GenerateMonFromTrainerMon` (`trainer_util.c`) sets it on the mon with full PP, so it covers normal trainers, partners and guest leaders. Smart trainers use it (§10, Smart Trainer AI); other AI trainers never pick it.
* **Dodge.** Never fully Dodged: the halved hit at worst, full damage from a Choice Scarf holder (§3, `GetDodgeBypass`).

### Itemized Moves

Allows players to expand tactical flexibility by slotting an active, usable combat move directly into a Pokémon's held-item accessory slot. *(Mutually exclusive with other held items, including Choice accessories.)*

*Current implementation:*

* **Slotting it.** Give a Pokémon a **TM** (held item) whose move it can learn (its teachable learnset). That move is its Itemized Move. The TM is **not consumed**: it is an ordinary held item, taken back like any other. HMs are key items and can't be held.
* **Who can hold it.** The party menu refuses a TM the Pokémon can't learn: giving from the bag or from the party menu prints *"X can't learn Y, so it can't hold this TM."*; swapping held items between party members fails with the error sound. Other routes (PC item mode, Trick, Thief, ...) aren't blocked, but a TM the holder can't learn simply gives no move.
* **Using it.** It appears in the L menu (the innate menu), after the Signature Move: Dodge / Recuperate fill the top row, then the Signature Move and the Itemized Move fill the bottom row in that order, whichever the Pokémon has (an Itemized Move with no Signature Move takes the third cell). The panel shows `PP n/max` and its type, like the move screen.
* **Everything else is a normal move:** the usual Stamina cost (BP/10 + Body Tax), accuracy, Dodge interaction, no announcement. It has **4 PP per battle** (`ITEMIZED_MOVE_PP`, whatever the move), counted per party Pokémon (switching keeps it); it is full again at the start of every battle, or when the Pokémon starts holding a different TM. Pressure, Grudge, Disable, Taunt, Encore, Torment etc. go through the engine's normal checks. A transformation gimmick (Mega, Ultra Burst, Dynamax, Tera) can be paired with it, as with Dodge / Recuperate (unlike the Signature Move). In doubles it uses the move's default target (no manual pick yet, as for the Signature Move).
* **Losing it.** The move is read from the held item live, so Knock Off, Trick, Thief, etc. take it away (and Trick can hand a learnable TM to the foe). If the TM is lost after the move was chosen but before the Pokémon acts, it loses its turn: *"X no longer has its TM!"*. Held items come back after battle (§9).
* **Doesn't count as known.** Unlike the Signature Move, it is not "known": it never blocks learning the same move into a slot, and its uses don't count toward mastering a move.
* **Engine.** `GetItemizedMove(species, item)` (`signature_move.c`) is the one rule (TM / HM pocket, `CanLearnTeachableMove`). In battle it is chosen with `ITEMIZED_MOVE_POSITION` (= `MAX_MON_MOVES + 1`), resolved by `GetBattlerMoveAtPosition` → `GetBattlerItemizedMove`. Its PP lives in `PartyState.itemizedMove` / `.itemizedMovePP` (set to 4 the first time a TM's move is seen this battle), read and taken through the same `GetBattlerExtraMovePP` / `DeductBattlerExtraMovePP` as the Signature Move. The lost-TM case is caught in `HandleAction_UseMove` (`BattleScript_FRItemizedMoveLost`). Menu: `GetInnateMenuEntry` maps grid cells to entries in `battle_controller_player.c`. Party menu: `CanMonHoldTM` in `party_menu.c`.
* **Not yet:** only Smart trainers use Itemized Moves (§10). Ethers / Elixirs / Leppa Berry don't restore the Signature Move's PP. If all four slot moves are out of PP, the game goes straight to Struggle and the L menu can't be reached (same for Dodge / Recuperate / Signature).

### Smart Trainer AI (`AI_FLAG_FR_TACTICS`)

Part of `AI_FLAG_SMART_TRAINER`, so every `AI: Smart Trainer` party gets it (Roxanne / Brawly so far). It **adds** to the upstream AI and never replaces it: KO logic, the type chart (already in the damage numbers), hazards and the damage comparison run as before. Its per-move score (`AI_FrTactics`, `src/battle_ai_fr_tactics.c`) is capped at +3 and never lowers a move that KOs the target, so a fast / slow kill (+6 / +4, plus the best-damage +1) still wins. The only exception is decisive speed control (+4), and only when the AI has no KO on that target.

* **Signature and Itemized Move.** Scored as AI move slots 4 and 5 (`AI_MOVE_SLOTS`, positions `SIGNATURE_` / `ITEMIZED_MOVE_POSITION`) through the whole pipeline: damage simulation, bad-move / KO / viability scoring, the damage comparison, and the Stamina logic. Their selection limits (PP, Disable, Taunt, Choice lock, ...) come from `CheckExtraMoveLimitations`. With 8 / 4 PP they give way to an equally scored slot move unless they KO (`AI_FrTacticsPostScore`). No gimmick goes with the Signature Move; the Itemized Move takes transformations only. The AI knows the player's Signature / Itemized Move with move omniscience, or once it has seen it used (`extraMovesSeen`), and braces for it like any other move.
* **Reading the player's Dodge.** A *Dodge spot* is a turn the player can Dodge (enough Stamina, not Dodged last turn, no Choice item) and has a reason to (the AI threatens a KO, or is about to repeat last turn's best hit). In a spot the AI expects a Dodge at this player's own rate this battle, (Dodges in spots + 1) / (spots + 2), rolled once per turn; spots and Dodges are counted at the end of every turn (`AI_RecordDodgeSpots`). Expecting a Dodge: a move that would fully miss loses 3, a halved one 1 (never a KO move), a halved hit that still KOs gains 3, a hit that lands in full (Scarf Signature) gains 2, and setup / hazards / field moves gain 2 (the Dodger wastes its turn). The Dodge outcome uses the engine's rules (speed, Repeated Move Trapping, `GetDodgeBypass`).
* **Status rails.** Climbing a line the target already has gains 2 to its top level (Paralyzed, Nightmare, Frightened, Burn ×3, Toxic) or 1 from Lv1 to a lasting Lv2; Dazed + Soaked (straight to Paralyzed) gains 2. A target that wants the status (an Orb with Guts / Poison Heal / Quick Feet / Marvel Scale, Toxic / Flare Boost) loses 2. Cleansing berries ignore Lv1: a Lv2 status move into a fresh line loses 2, Lv1 procs gain 1. A known Natural Cure innate makes a sub-Lv3 status (but Toxic) lose 1. Poison into Leftovers gains 1. Burn into a physical attacker gains 2, 3 if it 2HKOs the AI. Confusion moves are no longer ruled out on a Confused / Stupefied target (`AI_CanBeConfused` now only stops at Frightened, for every AI).
* **No self-sabotage.** Ice into a burned target (Water only eases a Burn through a Soak proc, which isn't scored), Fire into a Chilled / Frostbitten one, Ground into the Electric line lose 2 (gain 1 if the target wants that status); a non-Ghost hit into a sleeper loses 1 (the valve), a Ghost hit gains 1. The same types on the AI's own lines gain 1 (they cure it).
* **Combos.** A Ghost hit on Drowsy / Asleep and a Psychic hit on Confused / Stupefied gain 1–3 by crit stage (the move, Focus Energy, Super Luck ability or innate, Scope Lens; 3 for a sure crit), for the Nightmare / Frightened upgrades. Doubles: a Water hit that can Soak a foe gains 1 (2 if spread) when the partner has an Electric or Ice attack.
* **Doubles status hazards.** A Stupefied AI loses 2 on single-target hits that could KO its own ally. A confusion move gains 2 on a foe whose own single-target hit can KO its ally (1 if that ally is under half HP).
* **Speed control.** Paralysis (when it reaches Paralyzed), Chill, Yawn, Speed drops from hits or status moves (Icy Wind, Electroweb, String Shot, Scary Face, ...), Tailwind and Trick Room: +2 if it flips the order for the AI or its partner (real Effective Speed: rails × Stamina decay), +1 if it only closes the gap. **Decisive (+4)** when the flipped battler then OHKOs the target and the AI can't KO it this turn.
* **Own innates.** Battle Armor: see §2 (no panic Recuperate, status / hazards / setup count as progress). Natural Cure / Regenerator: pivot out with the matching ability's switch chances (Natural Cure on a Lv2+ line, more eagerly for Toxic / Asleep / Frozen / Paralyzed; Regenerator at ≤ 2/3 HP below the Trauma cap); Quick Start: switch out when its Speed was dropped below neutral and it is outsped (`ShouldSwitchForNatureInnate`). Never while the AI has a KO. Super Luck: the crit combos above.
* **Switching.** The switch checks (bad odds, 1v1s, threat to the incoming mon, picking the switch-in, revives) and the shared KO / hits-to-KO / best-damage helpers count the Signature / Itemized Move on both sides, the candidate's own included. In doubles, a Stupefied / Frightened AI with 2+ turns of the line left switches out half the time when one of its single-target hits could KO its ally (`ShouldSwitchStupefiedFromAlly`); the line keeps on the bench, so this only shields the ally for now.
* **The player's innates** are read only once they have visibly fired (`RecordNatureInnate` → `AiPartyMon.innateRevealed`): Battle Armor when it stops a sure crit or the 0-Stamina auto-crit, Natural Cure when it cures, Regenerator when it heals, Resilient when a special hit leaves no Trauma, Quick Start on its switch-in boost. Super Luck, Sheer Force and Advantage have no clean visible moment and are never read (the shared damage code still applies them).
* **Held items.** Contact into Rocky Helmet that would KO the AI loses 3 (not a KO move). Assault Vest's bigger Stamina pool is already in `GetBaseStamina`. No bag items (§9).
* **Not yet:** Choice-locked AI doesn't plan around the Scarf Signature rule beyond scoring.

### Level-Band EXP

EXP rewards fighting opponents near your own level and discourages both over-levelling and power-levelling.

* **Rule.** Each EXP earner compares its level to the fainted Pokémon's. Within **±`EXP_LEVEL_BAND` (4) levels** (inclusive), its EXP is **doubled**, in wild **and trainer** battles. Outside the band, its EXP is **halved** in every battle (an award of 1 stays 1).
* **Why trainers too.** The doubling makes grinding less necessary: following the planned route, trainer fights alone keep the party in step with the level curve, so the player doesn't have to farm wild encounters to keep up.
* **Per earner.** The band is checked per Pokémon, so in one battle a Pokémon close in level can earn double while an over-levelled teammate or an Exp. Share holder earns half.
* **Engine.** This is the last step of `ApplyExperienceMultipliers` (`battle_script_commands.c`). It runs after the vanilla multipliers (traded, Lucky Egg, Exp. Charm, affection, Gen 5+ level scaling), so it stacks with them.

### Gym Leader Rewards

After the badge and TM, each Hoenn gym leader offers **one extra reward, picked by the player**:

* **A random competitive held item**, drawn evenly from one pool shared by every gym (`sGymRewardItems`, `src/gym_reward.c`), rolled when the player picks it (so backing out and talking again can't fish for a specific item):
  * *Power:* Choice Band / Specs / Scarf, Life Orb, Expert Belt, Muscle Band, Wise Glasses, Scope Lens, Razor Claw, King's Rock, Wide Lens, Zoom Lens, Metronome, Loaded Dice, Punching Glove, Throat Spray, Weakness Policy, Blunder Policy, Power Herb.
  * *Bulk / sustain:* Leftovers, Black Sludge, Shell Bell, Big Root, Assault Vest, Eviolite, Rocky Helmet, Focus Sash, Focus Band, Sitrus Berry, Lum Berry.
  * *Utility:* Quick Claw, Bright Powder, Air Balloon, Heavy-Duty Boots, Safety Goggles, Protective Pads, Covert Cloak, Clear Amulet, Ability Shield, Utility Umbrella, Mental / White / Mirror Herb, Red Card, Eject Button, Eject Pack, Room Service, Adrenaline Orb, Shed Shell, Toxic Orb, Flame Orb, Iron Ball.
  * *Field:* Light Clay, Terrain Extender, Binding Band, Grip Claw, Damp / Heat / Smooth / Icy Rock.

  Type-boosting items, Plates, Mega Stones, Z-Crystals and species-only items (Light Ball, Thick Club, ...) are left out.

* **The gym's type** (used by the Egg): Roxanne Rock, Brawly Fighting, Wattson Electric, Flannery Fire, Norman Normal, Winona Flying, Tate & Liza Psychic, Juan Water.

* **Or an Egg** of a random species that shares the gym's type, rolled when the player picks it. The pool is every first-stage species (default form only, so no regional / Mega / battle forms) that:
  * has the gym's type itself (the hatchling, not a later evolution);
  * is not restricted / sub-legendary, mythical, an Ultra Beast or Paradox;
  * is **not from Hoenn**: neither it nor anything it evolves into has a Hoenn Pokédex number (so Pichu is out, Pikachu being in the Hoenn dex);
  * isn't an Undiscovered-group species that never evolves (Unown and the like; babies are allowed).

  The species is hidden until it hatches. With a full party the Egg goes to the PC (and says so); only when the PC is full too is it refused.

**Flow.** The offer is a two-option menu (`HELD ITEM` / `EGG`). A full bag, or a full party *and* PC, says so and returns to the menu; **B** puts it off, and the leader offers it again whenever talked to until it is claimed. It is offered once per gym (rematches don't repeat it).

* **Engine.** `Common_EventScript_GymReward` (`data/scripts/gym_reward.inc`), called with `VAR_0x8008` = gym number at the end of each gym's give-TM scripts and in the leader's post-battle talk once the TM is owned. Specials: `GymReward_IsClaimed` / `GymReward_SetClaimed` (`FLAG_GYM_REWARD_1`–`_8`, the old unused flags 0x21–0x28), `GymReward_BufferType` (type name for the offer), `GymReward_RollItem`, `GymReward_GiveEgg` (`PickEggRewardSpecies`, then `ScriptGiveEgg`). Kanto (FRLG) gyms are not hooked.

### Tera Orb

* **Roxanne's gift.** After her gym reward (§ Gym Leader Rewards), Roxanne gives the **TERA ORB** (Key Items). With it, one of the player's Pokémon can Terastallize **once per battle** into its Tera Type, as upstream (`CanTerastallize` looks for the orb in the bag). A full Key Items pocket says so; she offers it again whenever talked to (after the post-battle reward flow) until it is in the bag.
* **No recharging.** The orb is always charged (`B_TERA_ORB_ALWAYS_CHARGED` = `TRUE` in `include/config/battle.h`): no Pokémon Center visit is needed, the once-per-battle limit is the only one, as for Mega Evolution and Z-Moves.
* **Engine.** `RustboroCity_Gym_EventScript_GiveTeraOrb` (`data/scripts/gimmick_reward.inc`, texts there too), called after `Common_EventScript_GymReward` in Roxanne's give-TM script and her post-battle talk. No flag: owning `ITEM_TERA_ORB` (a key item, never lost) closes the offer.

### Mega Ring / Z-Ring

The player gets **one** of the two gimmick rings, for good, and with it the items to use it.

* **Wattson's choice.** After his gym reward (§ Gym Leader Rewards), Wattson offers **both** rings and the player keeps **one**: `MEGA RING` / `Z-RING` (a two-option menu, then a YES/NO "you can't change your mind"). The ring lets the player's Pokémon Mega Evolve (holding a Mega Stone) or use Z-Moves / Ultra Burst (holding a Z-Crystal), as upstream (`CanMegaEvolve` / the Z-Move checks look for the ring in the bag). **B** puts it off; he offers it again whenever talked to (after the post-battle reward flow) until one is taken. A full Key Items pocket says so and leaves it open. The item `ITEM_Z_POWER_RING` is renamed **Z-Ring**.
* **Birch's call.** Once a ring is received, Prof. Birch **calls once** by PokéNav (after 10 steps in a town, city or route) and asks the player to come to the lab. His daily lab / Route 101 / Route 103 rotation is unchanged: talking to him **wherever he is** with the gift pending gives it, before the call too (then no call follows). While it is pending he is **also in the lab** whenever the player enters it, even on a route day.
* **Birch's gift** for the chosen starter (`VAR_STARTER_MON`): with the Mega Ring, the Mega Stone of its line (Sceptilite / Blazikenite / Swampertite; random if the line has several, e.g. Charizardite X / Y); with the Z-Ring, the type Z-Crystal of its first type (Grassium / Firium / Waterium Z). Bag full: come back later. A line with no Mega Stone gets an apology and closes the gift.
* **Game Corner.** The Mauville prize counter that sells the dolls also has **MEGA STONE (4,000 coins)** and **Z-CRYSTAL (3,000 coins)** rows, sold whichever ring the player holds. Each opens that category's **stock of 3**, shown by name (a bought one shows `SOLD OUT`); picking one asks to confirm, then checks coins and bag space.
  * *Stock:* 3 distinct items per category, uniform among the ones **not in the player's bag** when it is rolled (a stone held by a Pokémon doesn't count as owned). With fewer than 3 left, the rest of the slots are `SOLD OUT`. Mega Stones only count if some enabled species Mega Evolves with them (`FORM_CHANGE_BATTLE_MEGA_EVOLUTION_ITEM`); every Z-Crystal (type and species) counts.
  * *Refresh:* both categories reroll after every **gym badge**, **rival battle** and **Elite Four clear**, the egg shop's milestones (`GetEggShopMilestones`, now shared through `include/egg_shop.h`). Lazily, like the egg shop: checked when the prize lady is talked to, so several milestones between visits give one fresh stock; the first visit always stocks.
* **Engine.** `src/gimmick_reward.c` (+ `include/gimmick_reward.h`, `include/constants/gimmick_reward.h`) and `data/scripts/gimmick_reward.inc`. `VAR_BIRCH_GIMMICK_STATE` (the old unused 0x40FE): `BIRCH_GIMMICK_NONE` → `_RING` (ring taken) → `_CALLED` → `_GIFT_DONE`. Wattson: `MauvilleCity_Gym_EventScript_GiveGimmickRing`, called after each `Common_EventScript_GymReward` in the gym script. Call: `ShouldDoBirchGimmickCall`, checked in `TryStartStepCountScript` after the Aide's call, runs `Common_EventScript_BirchGimmickCall`. Gift: `ProfBirch_EventScript_GimmickGift`, reached from `LittlerootTown_ProfessorBirchsLab_EventScript_Birch` and `ProfBirch_EventScript_RatePokedexOrRegister` (route Birch) in state `_RING` or `_CALLED`; in those states the lab's OnTransition also clears `FLAG_HIDE_LITTLEROOT_TOWN_BIRCHS_LAB_BIRCH` after `ProfBirch_EventScript_UpdateLocation` (`..._ShowBirchForGift`), without hiding his route self. Special `BirchGimmick_GetGift` (walks the starter's evolutions for Mega Stones; type crystals are `ITEM_NORMALIUM_Z`–`ITEM_FAIRIUM_Z`, matched by `secondaryId`). Game Corner specials (`VAR_0x8004` = `GAME_CORNER_STOCK_MEGA_STONE` / `_Z_CRYSTAL`, `VAR_0x8005` = row): `GameCorner_Restock`, `GameCorner_BufferStock` (rows in `STR_VAR_1`–`3`), `GameCorner_BufferOffer` (item, or `ITEM_NONE` if sold; name in `STR_VAR_1`), `GameCorner_SetSold`. Stock vars: `VAR_GAME_CORNER_MEGA_STONE_1`–`_3`, `VAR_GAME_CORNER_Z_CRYSTAL_1`–`_3` (item id, 0 = sold) and `VAR_GAME_CORNER_STOCK_KEY` (milestones + 1), the old unused 0x40A8, 0x40B8, 0x40BB, 0x40DB, 0x40DC, 0x40E5, 0x40FF. Prices `GAME_CORNER_MEGA_STONE_COINS` / `_Z_CRYSTAL_COINS`; rows added to `MULTI_GAME_CORNER_DOLLS`, sub-menu in `MauvilleCity_GameCorner_EventScript_ChooseGimmickPrize`.

### Guest Gym Leaders

From the **4th gym** on (Flannery, Norman, Winona, Tate & Liza, Juan), the **first** fight with the leader may host a **guest leader from another region**: Kanto for now (the 8 FRLG leaders). Johto joins once its leader sprites are in.

* **Appearance.** Each gym gets one roll: a **20%** chance of a guest, and which guest. It is rolled the first time that leader is engaged (in practice, after the previous badge) and **rerolled on every whiteout**, anywhere, until the gym is beaten. The guest is picked evenly among Kanto leaders whose **type differs from the home gym's** (no Blaine at Lavaridge, Sabrina at Mossdeep or Misty at Sootopolis) and who **haven't been beaten yet**: each guest can be beaten once per save. Nothing in the gym hints at it; the leader reveals the guest when talked to.
* **Format.** Rolled when the player engages, evenly: **tag team** (the guest, then the home leader, back to back with **no heal**; losing either is a whiteout), **doubles** (the guest and the home leader together, as a two-trainer double battle), or **guest only** (the guest stands in for the leader). The format stays **locked** until the player wins or whites out, so backing out and talking again can't fish for another. Mossdeep has no doubles (Tate & Liza already fight a double battle), so it rolls tag team or guest only, 50/50.
* **Two Pokémon.** Doubles, or a tag team that ends in Tate & Liza's double battle, needs two usable Pokémon when engaging; otherwise the leader says so and the player has to come back (the format stays locked). A double battle reached mid-tag with one Pokémon left still runs.
* **Guest teams.** Each guest has one party, authored at its Kanto levels with fully evolved Pokémon. At battle time every level is **shifted** by (home leader's highest level − guest's highest level), so the guest's ace matches the home ace and the spread is kept; moves are left as authored (an empty move list uses the learnset at the new level). Authored **EVs scale with the level** (each × new level / authored level); if a stat goes over 252 or the total over 510, all six are scaled down together so their ratio holds.
* **Strongest three.** In **tag team and doubles**, each leader (guest and home, Tate & Liza included) brings only their **3 strongest** Pokémon, in their party order. Strongest = highest level, then has a gimmick (Tera Type set, Dynamax / Gigantamax, or holding a Mega Stone / Z-Crystal / Primal orb), then has a held item, then most total EVs, then most total IVs, then random. Guest only uses the guest's full party.
* **Rewards.** Winning in any format gives the normal **badge, TM and gym reward**, plus the guest's **prize money** (paid at its shifted level; both leaders pay in tag team and doubles) and the **guest's TM** (`tm` in `sGuestLeaders`, set per guest; `ITEM_NONE` gives none). Rematches never have guests.
* **Engine.** `src/guest_leader.c` (+ `include/guest_leader.h`, `include/constants/guest_leader.h`) and `Common_EventScript_GuestLeader` (`data/scripts/guest_leader.inc`, texts there too; per-guest lines are in `sGuestLeaders`). Each gym 4–8 leader script calls it first with `VAR_0x8008` = gym and branches on `VAR_RESULT` (`GUEST_LEADER_RESULT_NONE` → vanilla battle, `_WON` → the gym's badge script, `_BLOCKED` → release). Norman's inline badge flow got a `PetalburgCity_Gym_EventScript_NormanDefeated` label for this. State: `VAR_GUEST_LEADER` (the old unused 0x4083) = guest index + 1 in bits 0–3 (0 = none), locked format in bits 4–5 (`GUEST_FORMAT_*`), the gym the roll is for in bits 8–11; `FLAG_GUEST_BEATEN_*` (the old unused flags 0x2A–0x31). Trainers `TRAINER_GUEST_BROCK`–`_GIOVANNI` (855–862, `src/data/trainers.party`, class Leader with the FRLG pics). Battles are set up in C (`GuestLeader_ConfigureBattle`, `VAR_0x8005` = `GUEST_BATTLE_*`) and run with `dotrainerbattle`, which continues the script on a win; doubles set `gNoOfApproachingTrainers = 2`. Hooks: `CreateNPCTrainerParty` (`battle_setup.c`) → `GuestLeader_TryCreateParty` (level shift, EV scaling, trim), `GetTrainerMoneyToGive` → `GuestLeader_GetPrizeLevel`, `DoWhiteOut` → `GuestLeader_OnWhiteOut`. On a win, `GuestLeader_OnWin` sets the guest's beaten flag and the home leader's trainer flag (guest only never fights them, but rematches and `goto_if_defeated` need it), then clears the roll.

### Gym Leader Rematches

The rematches (`TRAINER_<LEADER>_2`–`_5`) are built from the leader's reworked first-fight lineup, not vanilla's rematch teams: the same core Pokémon, evolved, with the same items, abilities, Tera Types and move ideas, plus extra members as the tiers go up. They keep vanilla's level ranges, party sizes and double battles, carry no bag items (§9), use the Smart Trainer AI, have 31 IVs and EVs that grow by tier (~252 total at tier 2, ~340, ~420, then 510). Aces and other fully evolved members carry **Signature Moves** (§10), one at tier 2 and more on later tiers.

* **Roxanne** (core Aron, Geodude, Corsola, Nosepass; adds Hoenn fossils). Tier 2 (Lv32–37): Lairon, Graveler, Corsola, Probopass (sig. Power Gem). Tier 3 (37–42): + Armaldo, Golem (sig. Stone Edge). Tier 4 (42–47): Aggron (Rock Head), Corsola gets sig. Wave Crash. Tier 5 (47–52): + Cradily, Aggron gets sig. Head Smash.
* **Brawly** (core Croagunk, Meditite, Timburr, Makuhita; adds Water / Hoenn fighters). Tier 2 (Lv33–37): Toxicroak, Medicham, Gurdurr, Hariyama (sig. Close Combat). Tier 3 (38–42): Medicham gets sig. High Jump Kick. Tier 4 (40–47): + Poliwrath, Conkeldurr replaces Gurdurr, Toxicroak gets sig. Poison Jab. Tier 5 (46–52): + Breloom, Conkeldurr gets sig. Drain Punch.
* **Gyms 3–8** follow the Gym & Rival Lineups rules instead of the tier EVs above. Every tier is a **6-Pokémon double battle**: tier 2 is the first-fight team retuned for doubles (Protect, spread moves, Tailwind / Trick Room / Follow Me), and tiers 3, 4 and 5 each swap one member for a new one. EVs are the first fight's total **+80 per tier** (cap 510), the gimmicks stay the first fight's Mega + Tera (Wattson adds Tera Electric Manectric), and every fully evolved member has a Signature Move. Levels keep vanilla's tier aces (Juan +4, so tier 2 is above his first fight); the others sit 2–5 below the ace, never under their evolution level.
  * **Wattson** (ace 40 / 45 / 50 / 55): Galvantula, Rotom-Wash, Ampharos, Vikavolt, Mega Raichu X, Manectric (Specs). Swaps: Galvantula → Magnezone, Vikavolt → Electivire, Ampharos → Eelektross.
  * **Flannery** (40–55): Ninetales, Scovillain, Centiskorch, Arcanine, Mega Houndoom, Torkoal (sig. Eruption). Swaps: Scovillain → Camerupt, Centiskorch → Magcargo, Ninetales → Talonflame.
  * **Norman** (45–60): Ambipom, Porygon2 (Trick Room), Bewear, Staraptor, Mega Kangaskhan, Slaking. Swaps: Porygon2 → Blissey, Bewear → Snorlax, Staraptor → Porygon-Z.
  * **Winona** (45–60): Pelipper, Skarmory, Togekiss (Follow Me), Hawlucha, Gliscor, Mega Altaria; Tailwind team. Swaps: Gliscor → Corviknight, Hawlucha → Dragonite (Lv55, takes the Tera Flying), Pelipper → Gyarados.
  * **Tate & Liza** (50–65): the first-fight Trick Room team. Swaps: Claydol → Slowbro, Hatterene → Reuniclus, Indeedee-F → Farigiraf (Armor Tail).
  * **Juan** (55–70): the first-fight rain team. Swaps: Ludicolo → Crawdaunt, Walrein → Lapras, Crawdaunt → Barbaracle.
* **Known exception:** Wattson's Raichu keeps Extreme Speed from his first fight, which Raichu can't legally learn.

### Elite Four & Champion levels

Every level of Sidney, Phoebe, Glacia, Drake and Wallace is **+6** over vanilla (aces 55 / 57 / 59 / 61 / 64), so the League sits above Juan (50) and Wally (52). Their teams are still vanilla; a redesign under the lineup rules is planned.

### Gym & Rival Lineups

From the **4th gym** on, every gym leader's first fight follows one set of rules; the rivals follow them from their **first battle after Roxanne** (Rustboro). Roxanne, Brawly and Wattson were tuned by hand before these rules and stay as they are.

* **Rules (gyms).** 6 Pokémon, all of the gym's type; no restricted / sub-legendary, mythical, Paradox or Ultra Beast. Every evolution stage must be reachable at its level under vanilla rules of any generation (Slaking needs Lv36, so Norman's ace is 36). 31 IVs (the trainerproc default; special attackers get `IVs: 0 Atk`). EVs: each Pokémon's total is **+80 over the last gym** (Wattson 240, Flannery 320, Norman 400, Winona 480), capped at the vanilla **510** (Tate & Liza, Juan), never over 252 in a stat. Natures are picked for their innate (§5) and the spread for the role. At least one vanilla gimmick (Mega, Z-Move, Tera; Dynamax is off), never all of them: each of gyms 4–8 has one Mega and one Tera. The vanilla ace stays on the team, not necessarily as the highest level. Every **fully evolved** member has a **Signature Move** (§10; damaging, never one of its four); unevolved members (Eviolite users, Porygon2, rival stage-2 starters) have none, since a Signature Move is committed on the final evolution. All use `AI: Smart Trainer` (the only AI that uses Signature Moves) and carry no bag items (§9).
* **Rules (rivals).** The same, except: no shared type, the rival always has their starter, and before gym 4 the party is as big as the last gym's (Roxanne 4, Brawly 5). Their EVs match the last gym beaten, counting back from Wattson (gym 1 → 80, gym 2 → 160). May and Brendan have the same team for the same player starter. Wally's Mauville battle stays a single Ralts (he just caught it). Route 103 is untouched.
* **Levels.** The curve is above vanilla, since players reach Wattson at ~22–24.

| Battle | After | Levels | EVs | Party | Gimmicks |
| --- | --- | --- | --- | --- | --- |
| Rival, Rustboro | gym 1 | 13–16 | 80 | 4 | Tera (starter) |
| Rival, Route 110 | gym 2 | 19–22 | 160 | 5 | Tera (starter) |
| Wally, Mauville | gym 2 | 18 | 160 | 1 | Tera (Ralts) |
| Flannery | — | 28–31 | 320 | 6 | Mega Houndoom, Tera Grass Torkoal |
| Norman | — | 33–36 | 400 | 6 | Mega Kangaskhan, Tera Normal Slaking |
| Rival, Route 119 | gym 5 | 33–37 | 400 | 6 | Tera (starter) |
| Winona | — | 36–40 | 480 | 6 | Mega Altaria, Tera Flying Hawlucha |
| Rival, Lilycove | gym 6 | 37–41 | 480 | 6 | Mega starter, Tera Flying Tropius |
| Tate & Liza (doubles) | — | 42–45 | 510 | 6 | Mega Medicham, Tera Fire Solrock |
| Juan | — | 46–50 | 510 | 6 | Mega Sharpedo, Tera Dragon Kingdra |
| Wally, Victory Road | gym 8 | 47–52 (+3 per rematch) | 510 | 6 | Mega Gardevoir, Tera Fairy Altaria |

* **Teams.**
  * *Flannery* (sun): Ninetales (Drought), Scovillain (Chlorophyll), Centiskorch, Arcanine (Impish), Mega Houndoom, ace **Torkoal** (Drought, Bold, Body Press).
  * *Norman*: Ambipom (Fake Out), Porygon2 (Eviolite, Calm), Bewear, Staraptor (Reckless), Mega Kangaskhan, ace **Slaking** (Giga Impact as its Signature Move, so the recharge falls on its Truant turn).
  * *Winona* (rain): Pelipper (Drizzle), Skarmory (hazards), Togekiss (Serene Grace), Hawlucha (Unburden, sig. Flying Press), Gliscor (Poison Heal), ace **Mega Altaria** (Dragon Dance, Pixilate Double-Edge).
  * *Tate & Liza* (Trick Room doubles): Indeedee-F (Psychic Surge, Follow Me), Hatterene, Claydol (screens), Mega Medicham, aces **Lunatone** (Trick Room) and **Solrock** (Brave, slow attackers for Trick Room).
  * *Juan* (rain): Politoed (Drizzle), Milotic, Walrein, Ludicolo (Swift Swim), Mega Sharpedo, ace **Kingdra** (Swift Swim, sig. Draco Meteor).
  * *Rival* (suffix = player's starter): the counter-starter, the two other starter types (Lotad line for Grass, Wingull line for Water, Numel line for Fire, which replaces vanilla's Slugma because Magcargo can't be legal at Route 119), the Electrike line, Absol (Super Luck crit-fisher, from Route 110) and Tropius (from Route 119). The starter is stage 2 from Rustboro, fully evolved from Route 119 and Mega Evolves at Lilycove.
  * *Wally, Victory Road*: Delcatty, Roserade, Magnezone, Flygon, Altaria (Calm, Cotton Guard), ace **Mega Gardevoir**. Rematches 2–5 are the same team, 3 levels higher per tier.
* **Engine.** Data only: `src/data/trainers.party` (`TRAINER_FLANNERY_1`, `_NORMAN_1`, `_WINONA_1`, `_TATE_AND_LIZA_1`, `_JUAN_1`, `TRAINER_{BRENDAN,MAY}_{RUSTBORO,ROUTE_110,ROUTE_119,LILYCOVE}_*`, `TRAINER_WALLY_MAUVILLE`, `TRAINER_WALLY_VR_1`–`_5`). A `Tera Type:` line is what lets the AI Terastallize (`ShouldTrainerBattlerUseGimmick`); a held Mega Stone is enough for Mega Evolution. Every move and Signature Move is in the species' `all_learnables.json` entry; levels below a move's level-up level rely on egg / TM / tutor access, which the evolution-stage rule doesn't restrict. Guest leaders (§10) still match their ace to the home ace's level.

### Daycare Egg Shop

The Route 117 **Day-Care Lady** also sells Eggs. Talking to her opens `DAY-CARE` / `BUY AN EGG` (B leaves); `DAY-CARE` is the vanilla flow. The shop is a **money sink**: prices are deliberately steep.

* **Stock.** Three Eggs, **one per tier** (Starter, Strong, Powerhouse, in that order), each sold as **`<REGION> <TIER> EGG`** (e.g. `KANTO POWERHOUSE EGG`). Only the region is rolled, **evenly** among the regions that have at least one species in the tier. A bought slot shows `SOLD OUT`. With all three sold she says she has none left.
* **Species is a surprise.** Like the gym Egg, the species is rolled **when the Egg is bought**, evenly among that tier's species of that region, and stays hidden until it hatches.
* **Region** of a species: a regional form belongs to its form's region (Galarian Slowpoke → Galar, Paldean Wooper → Paldea, Hisuian Sneasel and White-Striped Basculin → Hisui); otherwise it's the region where its National Dex number debuted.
* **Prices.** Starter **¥30,000**, Strong **¥60,000**, Powerhouse **¥120,000**. The money is taken only once the Egg is given (party, or PC when the party is full; refused when both are full).
* **Refresh.** The stock rolls again after every **gym badge**, **rival battle** (May / Brendan on Route 103, Rustboro / Route 104, Route 110, Route 119, Lilycove; Wally in Mauville and at Victory Road) and **Elite Four clear** (Hall of Fame entry). It is refreshed lazily: the shop counts those milestones when opened and rerolls if the count changed since the last stock, so several milestones between visits give one fresh stock. The first visit always stocks.
* **Pools.** Every species is the Egg that breeding gives (first stage, no incense babies), and only species **obtainable from an Egg in vanilla**: one in a real Egg Group, or a baby that evolves into one (`CanHatchFromEgg`), so fossil chimeras (Dracozolt, Dracovish) and Gimmighoul are out. All pools drop anything **from Hoenn** (the same rule as the gym Egg: neither it nor anything it evolves into has a Hoenn Pokédex number) and anything disabled in the build.
  * *Starter:* every starter (Bulbasaur … Quaxly), plus **Eevee**.
  * *Strong:* lines where some member, in any form, has been Smogon **OU or Ubers in any generation**, excluding Starter / Powerhouse lines, legendaries, mythicals, Ultra Beasts and Paradoxes: Bellsprout, Shellder, Gastly, Drowzee, Cubone, Exeggcute, Chansey, Kangaskhan, Smoochum, Scyther, Tauros, Lapras, Porygon, Aerodactyl, Snorlax, Slowpoke, Galarian Slowpoke, Meowth, Nidoran♂, Diglett, Cleffa, Poliwag, Tangela, Onix, Togepi, Pineco, Gligar, Sneasel, Swinub, Houndour, Miltank, Smeargle, Murkrow, Yanma, Aipom, Paldean Wooper, Teddiursa, Riolu, Bronzor, Hippopotas, Snover, Croagunk, Shellos, Rotom, Buneary, Ferroseed, Drilbur, Frillish, Timburr, Solosis, Gothita, Litwick, Axew, Mienfoo, Pawniard, Foongus, Darumaka, Galarian Darumaka, Vullaby, Tympole, Fletchling, Honedge, Klefki, Hawlucha, Mareanie, Mimikyu, Rookidee, Arrokuda, Impidimp, Hatenna, Nacli, Glimmet, Dondozo, Tinkatink, Tandemaus, Duraludon, Applin. (Hoenn-dex lines such as Gyarados, Skarmory, Heracross or Magnezone would qualify but are dropped by the Hoenn rule.)
  * *Powerhouse:* the pseudo-legendaries (Dratini, Larvitar, Gible, Deino, Goomy, Jangmo-o, Dreepy, Frigibax; Bagon and Beldum are Hoenn), plus Larvesta, Greavard, White-Striped Basculin, Flittle, Mankey, Finizen and Hisuian Sneasel.
* **Engine.** `src/egg_shop.c` + `Common_EventScript_EggShop` (`data/scripts/egg_shop.inc`), reached from `Route117_PokemonDayCare_EventScript_DaycareWoman`. Each slot is one var (`VAR_EGG_SHOP_SLOT_1`–`_3`, the old unused 0x40F8–0x40FA): tier in bits 0–3, `enum Region` in bits 4–11, bit 15 = in stock (0 = sold). `VAR_EGG_SHOP_STOCK_KEY` (0x40F7) holds milestones + 1 for the current stock (`GetEggShopMilestones`: badge flags, `HasTrainerBeenFought` on the rival trainer IDs, `GAME_STAT_ENTERED_HOF`). Specials: `EggShop_Restock` (restocks if needed, returns Eggs left), `EggShop_BufferMenu` (rows in `STR_VAR_1`–`3`), `EggShop_BufferOffer` / `EggShop_CanAfford` / `EggShop_Buy` (slot in `VAR_0x8004`; `Buy` rolls the species with `PickEggShopSpecies(tier, region)`). Region: `GetEggShopRegion`. The Hoenn rule is `IsInHoennFamily` (`gym_reward.c`, now shared through `gym_reward.h`). Prices are `sEggShopPrices`; slot *n* is tier *n*.

### Mart TMs

Every Hoenn **town mart** (the clerk in each `*_Mart`: Oldale, Petalburg, Rustboro, Slateport, Mauville, Verdanturf, Fallarbor, Lavaridge, Fortree, Mossdeep, Sootopolis) sells **5 TMs** after its normal stock, at a flat **¥5,000** each.

* **Fixed per save.** The TMs are rolled once per playthrough and never refresh: they are derived from the **trainer ID**, so nothing is stored and a new game rolls new ones. They show from the first visit, whatever the mart's basic / expanded stock.
* **Pool.** The 50 TMs minus the **8 gym TMs** (Rock Tomb, Bulk Up, Shock Wave, Overheat, Facade, Aerial Ace, Calm Mind, Water Pulse), which stay gym-only: 42 TMs. HMs are never sold.
* **Spread.** The pool is shuffled once (seeded by the trainer ID) and each mart takes the next 5 in **story order** (Oldale first, Sootopolis last), wrapping around. So a mart never lists the same TM twice, the first 8 marts share none, and only the last 3 (Fortree, Mossdeep, Sootopolis) repeat TMs from earlier marts (55 slots for 42 TMs).
* **Price.** ¥5,000 in these marts only (Slateport's PokéNews sale halves it, as any mart price); the TM's own price is unchanged elsewhere (Lilycove, selling).
* **Names.** Every mart's buy list shows a TM with its move, **`TM08 Bulk Up`** (the list font narrows to fit), not just `TM08`. This also applies to the Lilycove Department Store TM counters.
* **Engine.** `src/shop.c` + `include/constants/mart_tms.h`. Each clerk script runs `setvar VAR_0x8004, MART_TMS_<TOWN>` + `special PokeMart_SetTMStock` before its `pokemart`; the special leaves the id pending and `CreateShopMenu` moves it into `sMartInfo.tmStockId` (cleared for every other shop). `CB2_InitBuyMenu` runs `BuyMenuAppendMartTMs` after `TryBuildDynamicShopItemList`, replacing that allocated list with one holding the 5 TMs (`GetMartTMs`: pool = `FOREACH_TM` minus `sMartTMExcluded`, Fisher-Yates with `LocalRandomSeed(OTID ^ "MART")`, slice `(id - 1) * 5`). Every buy-menu price goes through `BuyMenuGetItemPrice` (`MART_TM_PRICE` for a TM while `tmStockId` is set). `BuyMenuSetListEntry` appends the move name to TM / HM names (`sItemNames` widened by `MOVE_NAME_LENGTH`). No vars or flags.

### Lab Research Trade

Prof. Birch's **Aide** in the Littleroot lab runs a 2-for-1 trade: the player lends him **two party Pokémon** for research and gets **one** stronger, foreign Pokémon back.

* **When.** The first request opens with the Route 103 rival win: at the end of the Pokédex scene the Aide walks up to the player, explains the trade and says he'll have a new request after every gym badge (he doesn't offer it there; the player only has the starter). After that, one more request opens after every **gym badge** and every **Elite Four clear** (Hall of Fame entry). Missed ones don't stack: several milestones between visits give one request. Later rival battles and gym rematches don't count. With no request open he only chats; later requests start by talking to him, or by **PokéNav call**: once the player has the PokéNav (`FLAG_SYS_POKENAV_GET`), the Select menu gets a **LAB RESEARCH** row that calls him and runs the same trade from anywhere. To make the player aware of it, he **calls once** after the PokéNav is received (after 10 steps in a town, city or route) to point them to that row; he never calls about individual requests.
* **Inputs.** Picked one at a time from the **party** (B on either pick backs out). Not an Egg, not the same Pokémon twice; fainted Pokémon and any level are fine, and a 2-Pokémon party can trade (it ends with 1). Held items go back to the bag (with the Pokémon if the bag is full).
* **Type request.** From the **4th** request on, he names a type: at least one input must have it (its current types). Rolled once when the request opens (talking again doesn't change it), uniform over the 18 types, never the same as the last request. It doesn't touch the reward roll.
* **Reward type.** A type is rolled from the inputs' current types, duplicates kept (Pidgey + Zubat = Normal, Flying, Poison, Flying: Flying 50%). The reward's line must have that type in **some member**. A type with no eligible species is dropped and the roll repeats.
* **Reward species.** Uniform among the eligible species of that type:
  * **first stage** (babies count), in its default form or a **regional form** (Galarian Meowth, Hisuian Growlithe, Paldean Wooper...), and the default form must be a first stage too (no battle forms);
  * **not from Hoenn** (the gym Egg rule, `IsInHoennFamily`), not restricted / sub-legendary, mythical, Ultra Beast, Paradox or Totem; Undiscovered-group lines (Unown...) are allowed;
  * **line BST** above both inputs' line BSTs. A species' line BST is the best BST among the final evolutions it can reach (default forms, no Megas), its own if it doesn't evolve. Strictly higher, with no relaxation at the top: two 600-class Pokémon can never be traded.
* **The reward** is a Lv5 gift (player OT, Poké Ball, met in Littleroot, normal shiny odds, registered in the Pokédex): random nature and non-hidden ability, and **each IV is rolled twice, keeping the higher**. It is rolled at confirmation, with no preview, then offered a nickname.
* **Order.** Pick both, then he checks the pair before anything is taken (type request, then whether any reward exists), then confirms (*"I'll take X and Y..."*), then swaps them.
* **No reward possible.** He offers a **Rare Candy** instead (the player may decline and keep the request open). Taking it uses up the request and counts as a completed one (toward the 4th-request type rule); with a full bag the request stays open. Missing the requested type has no such skip.
* **Engine.** `src/lab_research.c` + `Common_EventScript_LabResearch` (`data/scripts/lab_research.inc`), reached from `LittlerootTown_ProfessorBirchsLab_EventScript_Aide` once `VAR_BIRCH_LAB_STATE` ≥ 5 (Pokédex received); the walk-up is `LittlerootTown_ProfessorBirchsLab_EventScript_AideExplainResearch`, at the end of `..._GivePokedex` (the Aide is held still at (9,8) for that scene by `..._SetAidePosForPokedex` on transition). Vars (the old unused 0x40FB–0x40FD): `VAR_LAB_RESEARCH_KEY` = milestones (badges + `GAME_STAT_ENTERED_HOF`) at the last completed request, `VAR_LAB_RESEARCH_COUNT` = completed requests (the first is open while it is 0), `VAR_LAB_RESEARCH_TYPE` = requested type in the low byte, bit 15 while the open request has rolled it (cleared when a request completes; the low byte stays as the last type), bit 14 once the Aide has made his one PokéNav call (never cleared). Specials: `LabResearch_GetState` (`LAB_RESEARCH_CLOSED` / `_OPEN` / `_OPEN_TYPED`, type name in `STR_VAR_1`), `LabResearch_CheckPair` (`LAB_RESEARCH_PAIR_OK` / `_NEEDS_TYPE` / `_NO_REWARD`; slots in `VAR_0x8005` / `VAR_0x8006`), `LabResearch_Trade` (rolls, swaps, returns the reward's slot, also in `VAR_0x8004`, species in `STR_VAR_2`), `LabResearch_Skip`. Constants in `include/constants/lab_research.h`. The PokéNav call is `Common_EventScript_LabResearchCall` (from `Common_EventScript_SelectMenu`); it sets `VAR_0x8009` so the script re-locks with `lockall` after each party pick instead of `lock` / `faceplayer`. The incoming call is `ShouldDoLabResearchCall` (`include/lab_research.h`, step counter in EWRAM), checked with the vanilla scripted calls (Scott, Roxanne...) in `TryStartStepCountScript` (`field_control_avatar.c`), which runs `Common_EventScript_LabResearchIncomingCall` (`pokenavcall`, then `LabResearch_SetCalled`).

### Select Menu (Infinite Repel, Exp. Share)

Pressing **SELECT** in the overworld opens a small menu (it replaces vanilla's direct use of the registered key item, which is now its third entry). Rows: **REPEL: OFF/ON**, **EXP. SHARE: OFF/ON**, **KEY ITEM**, **LAB RESEARCH** (only once the player has the PokéNav: calls Birch's Aide for the research trade, see Lab Research Trade), **CANCEL**. A toggle relabels the row and the menu stays open with that row selected; **B** closes it. The Mom in Littleroot explains it when she hands over the Running Shoes (`LittlerootTown_Text_ExplainSelectMenu`). The start menu no longer has a Repel entry.

* **Infinite Repel.** While ON it works like a Repel that never wears off: wild Pokémon below the lead's level don't appear (same rule as the item, `IsWildLevelAllowedByRepel`), including overworld-spawned encounters. Repel and Lure items keep working on top of it.
* **Exp. Share.** While ON, every healthy party member that did not fight earns **half** the EXP of the fainted foe; the ones that fought keep their full share. This is upstream's Gen 6+ Exp. Share (`IsGen6ExpShareEnabled`, `expShareExpValue` = half), driven by `I_EXP_SHARE_FLAG` = `FLAG_EXP_SHARE`, so the other EXP rules still apply on top (the level band, §10). The held Exp. Share item (Gen 5 style) still works too.
* **KEY ITEM.** Runs the registered key item as Select used to. The entry queues it (`SelectMenu_QueueRegisteredItem`) and the field input handler runs `UseRegisteredKeyItemOnField` once the menu script has ended; with nothing registered, the usual "no registered item" message shows. Nothing opens in the Union Room or a multi-partner room (as vanilla).
* **Engine.** `Common_EventScript_SelectMenu` (`data/scripts/select_menu.inc`, a `dynmultichoice` whose labels come from `SelectMenu_BufferLabels`), opened from `ProcessPlayerFieldInput` (`field_control_avatar.c`); specials in `src/select_menu.c`. `FLAG_INFINITE_REPEL` (was `FLAG_UNUSED_0x020`) and `FLAG_EXP_SHARE` (was `FLAG_UNUSED_0x029`) are saved flags. `IsRepelActive()` (`wild_encounter.c`) = Repel steps left **or** the flag.

### Battle Style locked to Set

* Battles always use **Set** (no "switch Pokémon?" prompt when a foe faints): `ClearSetBScriptingStruct` forces `OPTIONS_BATTLE_STYLE_SET`, new games default to Set, and the **BATTLE STYLE** row is removed from the Options menu (the save field is written as Set when the menu closes).

### HMs Without Teaching

* **Rule.** The badge-gated HM field moves (Cut, Flash, Rock Smash, Strength, Surf, Fly, Dive, Waterfall) can be used with **no Pokémon knowing the move**. The matching **badge is still required**. A Pokémon that does know the move is still preferred as the user. Teleport, Dig, Sweet Scent, Soft Boiled and the other non-badge field moves are unchanged.
* **Flag.** `OW_HM_WITHOUT_TEACHING` (`include/config/overworld.h`), currently **TRUE**. Setting it to FALSE restores the vanilla "a party member must know it" rule.
* **User.** With nobody knowing the move, the first non-egg party member performs it (its name and sprite appear in the "X used CUT!" text and animation).
* **Engine.** `IsFieldMoveFreeToUse` (`field_move.h`) = flag on, `BADGE_UNLOCK` move, badge owned. `ScrCmd_checkfieldmove` falls back to the first non-egg mon when no one knows the move, which covers every script-driven use (Cut tree, Rock Smash, Strength, Waterfall, Dive). `PartyHasMonWithSurf` returns TRUE for Surf. Cut (grass), Flash and Fly have no overworld trigger, so `SetPartyMonFieldSelectionActions` adds them to **every** party member's menu while the badge is owned and there is room (the menu fits Summary, Stat Edit, 4 field moves, Switch, Item, Cancel; moves the Pokémon really knows are listed first).
* **Itemized and Signature field moves.** Independent of the flag, a field move a Pokémon has **outside its four slots** counts for the overworld: its **Signature Move**, or its **Itemized Move** (a held TM it can learn, e.g. a Dig TM gives Dig). `MonCanUseFieldMove` (`party_menu.c`) = `MonKnowsMove` (slots + Signature) or `GetItemizedMove`; `ScrCmd_checkfieldmove` and `PartyHasMonWithSurf` use it, and the party menu lists such moves after the slot moves (same 4-entry cap, shared with the free HMs above). `MonKnowsMove` itself is unchanged, so teaching a TM the Pokémon already holds is still allowed.

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
| Nature     | Calm / Careful                  | Resilient (immune to Special Trauma)                                   |
| Nature     | Relaxed / Lax                   | Regenerator (HP on switch-out)                                     |
| Nature     | Sassy / Gentle                  | Natural Cure (status cleared on switch)                            |
| EXP        | Exp. Share (Select menu)        | non-participants get ×0.5 of the foe's EXP while ON                |
| EXP        | Level band                      | ×2 within ±4 levels (wild and trainer); ×0.5 outside                |
| Items      | Restored after battle           | all held items, incl. berries with a battle hold effect            |
| Items      | Itemized Move (held TM)         | learnable TM's move in the L menu; 4 PP per battle, TM not used up |
| Moves      | Signature Move                  | mastered at 100 uses (per mon, per move); committed on final evo   |
| Moves      | Signature Move PP               | 8, persists between battles, refilled by healing                   |
| Moves      | Signature vs Dodge              | never a full miss (halved); full damage from a Choice Scarf holder |
| Items      | Choice items                    | no Dodge / Recuperate; Band/Specs skip repeat-trap; Scarf never fully Dodged |
| Items      | Bag in battle                   | none in trainer battles; big-battle trainers carry no items        |
| AI         | Smart Trainer extras            | bonus cap +3, never on a KO move; decisive speed control +4        |
| Gym reward | Choice per gym                  | random held item (shared pool), or Egg: random non-Hoenn, non-legendary, gym type |
| Tera Orb   | Roxanne, after her gym reward  | always charged; Terastallize once per battle |
| Gimmick ring | Wattson, pick one (permanent) | Mega Ring or Z-Ring; Birch then gives the starter's Mega Stone / type Z-Crystal; Game Corner: stock of 3 unowned Mega Stones (4,000 coins) + 3 Z-Crystals (3,000), restocked per badge / rival / E4 clear |
| Guest leader | Gyms 4–8, first fight      | 20% per roll (rerolled on whiteout); Kanto guest of another type, once per save; tag team / doubles / guest only, 1/3 each, locked till win or whiteout; ace = home ace, EVs × level; top 3 each in tag / doubles |
| Lineups    | Gyms 4–8, rivals from Rustboro | 6 mons (gym type), 31 IVs, EVs +80 per gym (Wattson 240 → cap 510), 1 Mega + 1 Tera, Signature Move on every fully evolved member, Smart Trainer AI; rematches (gyms 3–8) 6-mon doubles, +80 EVs per tier, one swap per tier; E4 / Champion +6 Lv |
| Egg shop   | Prices (Starter / Strong / Powerhouse) | ¥30,000 / ¥60,000 / ¥120,000; one `<REGION> <TIER> EGG` per tier, region and species uniform (species hidden), restocked per badge / rival / E4 clear |
| Mart TMs   | 5 per Hoenn town mart, ¥5,000 | fixed per save (trainer ID); 42-TM pool (no HMs, no gym TMs), no repeats within a mart, first 8 marts disjoint; listed as `TM08 Bulk Up` |
| Lab trade  | 2 party Pokémon → 1 reward       | Lv5 first-stage, non-Hoenn, non-legendary; line shares a rolled input type; line BST > both inputs; IVs best of 2 per stat; 1 per badge / E4 clear; typed from the 4th; Rare Candy if no reward |
| Pacing     | Time-to-death (physical)        | frail 5–7 / mid 7–10 / giga 10+ turns                              |
| Sweep      | Optimal-case trade              | 1 for 2–2.5                                                        |

### Open Items

* Average move BP confirmation (drives the Stamina pacing — assumed ~90).
* Validate the Special/Trauma clock against a high-Def/high-Sta wall lands in the ~10–15 turn window.
* Body Tax % final value (1.5–2% range; currently 2%).
* Nature ability magnitudes (Crusher +50%, Disruptor `a→0.50`) — validate vs the wall-breaking triangle.
* Cap on Toxic Trauma escalation (§6).
* Playtest the §6 status defaults (Lv1 slows / surcharges, Electric-line accuracy, Shocked's 25% paralysis, Paralyzed ÷4, Frostbite / Nightmare maxHP/8, Stupefied 25%, Frightened 25%, Soak 20%, the 50% sleep valve). Only Smart trainers value the rail levels (§10); other AI still reads the vanilla `status1` mirror.
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

* **`L / INNATE` tab** (`move_info_window_l/r.png`). It's drawn with the ability popup's palette (`TAG_ABILITY_POP_UP`), not the PNG's own, so the label uses index 3 (near-black) over index D (yellow). Index 5 is white in that palette and was unreadable there.
* **Party status balls** (`ball_display.png`: healthy, empty, fainted, status). They share the healthbar palette (`TAG_HEALTHBAR_PAL`), so they're drawn on its indices: healthy is a red/black/white Poké Ball, status is the same ball with a purple top, fainted is a filled dark ball, and empty is an outline. Don't make them by swapping the palette onto the vanilla pixels: vanilla's index meanings don't match, and that's how they came out as dark, scrambled blobs.

* **Palettes.** `textbox.gbapal` is built as `textbox_0.gbapal` + `textbox_1.gbapal`, which come from the JASC `textbox_0.pal` / `textbox_1.pal`. Those `.pal` files are written by `generate_jasc_pals.py`; `generate_dark_palettes.py` holds the same colour lists for the raw `.gbapal` outputs. Edit a palette in **both** scripts and rerun `generate_jasc_pals.py` — the build only reads the `.pal` files.
* **Battle menu text** (move names, PP / type, FIGHT / BAG menu) uses the **vanilla** `graphics/battle_interface/text.pal` / `text_pp.pal` (dark grey on white). The dark `text.pal` in the Fantasy Retold folder is no longer used.
* **Menu cursors** (action menu, move menu + move-swap cursors, Yes/No, catch-or-not) are `textbox.png` tiles 1–2 and 28–31, placed on top of those white menu windows. Vanilla draws them in BG palette 0, which is now the dark textbox palette, so the arrows came out navy/blue/red. Their pixels now use `text.pal` indices (14 white, 13 dark grey, 15 light grey), and the `*CreateCursorAt` functions set palette 5 in the tilemap entry (`0x5000 | tile`).

#### Healthbox layout

Drawn from scratch rather than adapted from the vanilla box. Each box is a **dark corner-cut plate** (the same navy as the text box) plus a **status column** beside it.

* **Plate.** Player: 104×32. Row 1 has the name + gender symbol and, right-aligned, the level. Below it, the **HP bar** (4px), a 1px gap, then the **Stamina bar** (2px), both 94px. Each bar's 1px frame is drawn in its fill colour (HP red; Stamina green / yellow, red when exhausted) so both read at their true size on the dark plate. The bottom row has the 1px **EXP track** (singles only) and, right-aligned, `current / effective max` HP, 1px below the Stamina frame (`HB_HP_TEXT_Y`; the digits' ink shadow sits on the plate's ink bottom edge, since the 64×32 doubles sprite leaves no room to make the plate taller). Opponent: 100×25, with the same name row and the same bars at 90px. It has no numbers or EXP, and shows a 5px caught-ball icon before the name when the species is caught. Player doubles plates are the player plate without the EXP track. The HP numbers are always shown with the bars; the vanilla doubles bars/numbers toggle (`SwapHpBarsWithHpText`) is a no-op.
* **Status column.** A box as tall as the plate, with one 12×9 cell per active status line in a fixed order: Soaked, Chill line, Burn, Poison line, Electric line, Sleep line, Confusion line. Each cell holds a 7×7 icon in the line's colour (drop, snowflake, flame, skull, bolt, Z, swirl) and one pip per stage the line can reach, lit bottom-up to the current stage. Cells fill top-down, two per column. Extra columns grow away from the plate: left for the player, right toward the screen centre for the opponent. Up to 6 lines can be active at once (Burn excludes Soaked and the Chill line), which is 3 columns. With no status the column is hidden. `GetHealthboxStatusStages` maps lines to stages: each line shows its status rail's level (§6). It only reads (it runs every frame), so a status that vanilla code wrote straight to `status1` and that no rail has picked up yet shows at the stage its bit stands for (Burn 1, Frozen / Frostbitten → Chill 2, Paralysis → Electric 3, Sleep → Sleep 2, confusion timer → Confusion 1). The column is the old healthbar sprite, now 64×32 on the healthbox palette. Its callback polls the stages every frame and redraws on any change, because confusion and rails change without a healthbox update. It draws with the healthbox palette, so the level-up flash (`FreeHealthboxPalsForLevelUp`) must hand it back that palette, not the stock healthbar one (which re-colored the column after every level-up).
* **Art vs runtime.** `generate_healthbox_art.py` draws only the static parts: plates, bar frames, the 40% tick and the EXP track. Its layout numbers must match `sHealthboxLayouts` / `HB_*` in `battle_interface.c`. Text, bar fills, both bar frames and the 40% tick (also in the art), the exhausted red frame, the ball icon and the status column are drawn at runtime with `FillHealthboxRect` / `PrintOnHealthbox`. These treat the two healthbox sprites as one 128px canvas. The HP bar no longer uses the `hpbar` tiles.
* **Healthbox palette** (`ball_status_bar.png`): 1 plate, 2 empty gauge / cell, 3 ink, 4 slate, 5 white text, 6 grey, 7 red, 8 green, 9 purple, 10 cyan, 11 orange, 12 blue, 13 yellow, 14 sky, 15 pink (Confusion). Text is white with an ink shadow. The gender symbols use explicit colours (blue ♂ / red ♀) instead of the shared dynamic text colours.
* **Positions.** Every plate starts at the sprite's `(x − 32, y − 16)`. The 64×64 player singles box keeps the 64×32 template's centre offset. Gimmick indicators sit just left of the level digits (`sIndicatorPositions` in `battle_gimmick.c`).
* `B_HP_PERCENTAGE_DISPLAY` must stay **FALSE** (a `STATIC_ASSERT` enforces it); the plates have no room for it.
