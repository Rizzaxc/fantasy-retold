# Fantasy Retold

What if pokemon battle mechanics resembles the anime's? **Fantasy Retold** is a comprehensive overhaul and architectural redesign for Pokemon Emerald, transforming the traditional high-frequency-switching, type-immunity-driven meta of vanilla Pokémon into an individual-focused, resource-managed tactical JRPG battle system. The type chart still matters, but not as much as how you build and train your team. A single pokemon now cannot carry a battle on its own, but will need the support from both a smart trainer and its teammate.

## Core System Highlights

- **Reworked Battle Pacing**: effective damage is halved to allow each pokemon to maneuver and shine individually. This is countered by the fact that unhealable trauma damage is very present, so plan your moves carefully.

- **Unified Defense & Repurposed Stamina:** The traditional Defense stat handles mitigation for both Physical and Special attacks. Special Defense is stripped of its mitigation role and rebuilt as **Stamina** (`BaseSta`), governing action speed, execution costs, and defensive thresholds.

- **The Stamina Economy & Speed Decay:** Pokemon speed is now dynamic. A fast but exhausted mon will move slower than a slow but fresher opponent. Exhausting your Stamina also drops you into the *Exhaustion Zone*, where incoming hits trigger automatic critical hits.

- **Damage Modality (Health vs. Trauma):** A single HP bar tracks two metrics: current HP and accumulated **Trauma** (unhealable permanent damage). Special attacks and critical hits channel a percentage of damage straight into Trauma, bypassing standard recovery and acting as an inescapable clock against defensive walls.

- **Reworked Statuses**: now statuses are not just hax, but fair and visible chance you can build your strategy around. Original statuses are split into stackable stages, and a pokemon can be affected by multiple statues. 

- **Innate System Commands:** Access high-priority tactical options directly from the move menu:
  
  - **Dodge:** An active mitigation maneuver. See a big hit coming? Just dodge it!
  
  - **Recuperate:** Your pokemon will need to take a breather lest it risks unhealable Trauma!
  
  - **Signature Moves:** Now your most trusted pokemons can have a Signature Move that do not take up a moveslot, allowing you to train your favorites the way you want. Beware, a signature is not without its limits!

- **Rebalanced Natures:** now all natures have a place in your team. An offensive pokemon with a defensive nature is now a viable strategy.
  
  
  The game is based on `pokeemerald-expansion`, so it has all the QoLs you expect from a modern hack. EV training is also taken out; you can freely allocate EV points every time your pokemon levels up.

## Development & Engine Architecture

Built on top of upstream `pokeemerald-expansion`, Fantasy Retold accelerates overworld logic simulation (`4x`) while maintaining stable VBlank timing for music and save clocks, alongside battle presentation optimizations to ensure fluid combat pacing without menu hangs.


