ASHB2 — Global Simulation Summary

ASHB2 is an agent-based civilization & life simulator written in C++17, rendered with GLFW + Dear ImGui (and SDL2/ImPlot). It simulates individual agents ("entities") who live, form relationships, reproduce, and aggregate into tribes, religions, and economies — bridging individual psychology up to macro-historical dynamics.

Architecture (two layers)

1. Micro — the individual (Entity)
Each entity is a deeply modeled person with:
- Big Five personality (extraversion, agreeableness, conscientiousness, neuroticism, openness) + attachment style (secure / anxious / avoidant / disorganized) driving behavior and movement.
- Needs & stats that tick continuously: hunger, fatigue, hygiene, stress, loneliness, boredom, mental health, happiness, health, plus a food store with metabolism (eat or starve).
- FreeWillSystem — a needs-driven action engine. Each tick an entity scores candidate actions against requirements/context and chooses one (Socialize, Flirt, Date, couple, breeding, Murder, Trade, Preach, TeachSkill, DeclareWar, etc.). It also runs romantic and hostile "side-drives."
- Relationship links to other entities: social bonds, desire, anger, and couples — each grows from proximity and decays over time, with a Dunbar-style cap to keep them balanced.
- Inner life: grief states (Kübler-Ross stages), life memories (formative events permanently shift personality), goals (find_partner, build_career, build_family…), a self-concept, mental models of others, semantic memory, a Tree-of-Thoughts planner, and generated first-person inner monologue.

2. Macro — the civilization (CivilizationEngine)
Aggregates entities into emergent structures:
- Tribes with cultural values (militarism, spiritualism, collectivism, innovation), leaders, a communal granary + division of labor (farmers feed specialists).
- Religions founded from a leader's personality, with moral codes, rituals, schisms.
- Innovation diffusion + a prerequisite-gated TechTree researched over time.
- Diplomacy: tribe stances (ally/rival/at war), treaties, ethnic/hate wars with civilian attrition (war blocks cross-tribe reproduction).
- Kinship / SocialOrder: ID-based family registry with incest avoidance; patron-client networks, social classes, debt-to-slavery cascade, inheritance.

World & environment

- Procedural Planet (noise-based), biomes, a ResourceSystem, and a predator-prey Ecosystem.
- An EnvironmentModel drives seasons and yearly harvest luck (droughts/blights vs. bumper years) → modulates food yields → hunger → population. Climate genuinely gates survival.

Simulation loop (main.cpp)

Each step: advance environment/season → group entities by proximity + social bonds → apply disease spread → tick every entity's needs, relationships, grief → choose & execute actions (with targeted social interactions) → run force-based movement (couples/family attract, enemies/sick repel, personal-space repulsion, personality wander) → handle births and deaths (grief propagates to survivors).

Supporting systems

Disease/contagion, SaveLoad persistence, deterministic WorldSeed RNG (reproducibility), extensive Logging (births, deaths, actions, relationships, diseases, events), a NarrativeEngine, and validation/scalability/observability modules. The UI exposes tribe views, a pannable social graph, entity stat panels, and a color-coded History & Report window.

Recent focus (from session history)

Heavy iteration on population sustainability (rebalancing reproduction so births outpace deaths), social decision-making realism, kinship/social-order systems, ethnic warfare, tech tree, treaties/diplomacy, the ecosystem, and a diagnostic on excessive "hardship" deaths (hunger/starvation rebalanced to grind agents down slowly rather than kill instantly).

---
In one line: ASHB2 is a from-scratch C++ "society in a box" — psychologically rich individual agents whose moment-to-moment decisions, relationships, and deaths emergently produce tribes, religions, economies, wars, and history, visualized live through an ImGui interface.

Want me to go deeper on any one layer (e.g. the action/decision engine, the civilization-emergence rules, or the relationship dynamics)?
