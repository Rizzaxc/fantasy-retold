# Fantasy Retold

What if pokemon battle mechanics resembles the anime's? **Fantasy Retold** is a comprehensive overhaul and architectural redesign for Pokemon Emerald, transforming the traditional high-frequency-switching, type-immunity-driven meta of vanilla Pokémon into an individual-focused, resource-managed tactical JRPG battle system. The type chart still matters, but not as much as how you build and train your team. A single pokemon now cannot carry a battle on its own, but will need the support from both a smart trainer and its teammate.

## Core System Highlights

- **Unified Defense & Repurposed Stamina:** The traditional Defense stat handles mitigation for both Physical and Special attacks. Special Defense is stripped of its mitigation role and rebuilt as **Stamina** (`BaseSta`), governing action speed, execution costs, and defensive thresholds.

- **The Stamina Economy & Speed Decay:** Pokemon speed is now dynamic. A fast but exhasted mon will move slower than a slow but fresher opponent. Exhausting your Stamina also drops you into the *Exhaustion Zone*, where incoming hits trigger automatic critical hits.

- **Damage Modality (Health vs. Trauma):** A single HP bar tracks two metrics: current HP and accumulated **Trauma** (unhealable permanent damage). Special attacks and critical hits channel a percentage of damage straight into Trauma, bypassing standard recovery and acting as an inescapable clock against defensive walls.

- **Innate System Commands (L/R Buttons):** Access high-priority tactical options directly from the move menu:
  
  - **Dodge:** An active mitigation maneuver tied to speed tiers and consecutive-use compounding costs.
  
  - **Recuperate:** A sustain engine usable at $\le 40\%$ Stamina that restores fuel, heals HP, and purges a Level 1 status condition.
  
  - **Signature Moves:** Fully evolved Pokémon integrate their most-used moves into independent signature slots.

- **Reworked Natures & Systemic Status Rails:** Natures grant bespoke innate abilities (such as *Sheer Force*, *Battle Armor*, *Resilient*, and *Regenerator*). Non-volatile status effects operate on multi-stage rails where secondary effects stack up from volatility to persistent conditions.

## Development & Engine Architecture

Built on top of upstream `pokeemerald-expansion`, Fantasy Retold accelerates overworld logic simulation (`4x`) while maintaining stable VBlank timing for music and save clocks, alongside battle presentation optimizations to ensure fluid combat pacing without menu hangs.
