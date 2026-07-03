# More interview questions — filling the remaining gaps

Existing question.md covered: arbitration (1), appraisal→emotion (4), body→mind (5),
validation (6). These continue from there. Each one is grounded in a real system in the
code and is meant to fix a structural problem, not a detail.

────────────────────────────────────────────────────────────────────────
Gap 7 — What counts as "good"? (the reward signal)
────────────────────────────────────────────────────────────────────────
In LearningAdaptation, your RL Q-values and your HabitStrength.reinforce(reward) both
need a number that says "that was good / that was bad." Right now I mostly feed them
"outcomeSuccess" and need-satisfaction. So my agents learn to repeat whatever met a need.

The problem: real brains don't reward getting what you expected — they reward the
SURPRISE (got more than expected = learn it; got exactly what you expected = learn
nothing). If I keep rewarding raw success, my agents will over-learn boring routines.

Ask: "When someone does an action and it works out, what exactly should make them
'learn' from it — the result itself, or the difference between what they expected and
what actually happened? And does a bad surprise teach faster than a good one?"

Answer:


────────────────────────────────────────────────────────────────────────
Gap 8 — How fast should a mind change its mind? (update speed)
────────────────────────────────────────────────────────────────────────
In CognitiveArchitecture I hand-set beliefPersistence = 0.7 and opennessToEvidence = 0.5,
and in the RL code learningRate = 0.1. These are guesses. They decide how fast beliefs,
habits, and trust move after new evidence.

The problem: if updates are too fast the agent flip-flops on every event; too slow and it
never adapts. There's probably a real answer for WHEN a mind should update fast vs slow.

Ask: "After something surprising happens, when should a person change their belief a lot
versus barely at all? What makes the difference — how sure they were before, how strong
the evidence is, how emotional it was, or how many times they've seen it?"

Answer:


────────────────────────────────────────────────────────────────────────
Gap 9 — What holds one person together? (the integration / self problem)
────────────────────────────────────────────────────────────────────────
This is my scariest gap. One entity is really ~12 separate systems bolted together:
drives, MBTI cognitive stack, beliefs, moral foundations, PAD emotion, reputation,
relationships, habits. Nothing in the code is "the self" that ties them into one coherent
person. So an agent can act brave in one system and terrified in another at the same tick.

Ask: "A person has drives, a personality, beliefs, emotions, memories — but they still
feel like ONE self, not a committee. What actually makes all of that act as a single
coherent person instead of separate parts pulling in different directions? Is there one
thing in charge, or is the 'self' just a story the mind tells afterward?"

Answer:


────────────────────────────────────────────────────────────────────────
Gap 10 — Different clocks (timescales)
────────────────────────────────────────────────────────────────────────
Everything in my sim updates on the same tick. But in real life hunger changes in minutes,
mood in hours, habits in weeks, skills in months, beliefs and personality over years.
My Drive.load accumulator and my SkillPortfolio and my BeliefSystem all move at one speed.

The problem (an engineering one): mixing fast and slow systems on one clock makes the slow
stuff jittery and the fast stuff sluggish.

Ask: "Different parts of a person change at totally different speeds — feelings in
seconds, habits in weeks, who-you-are over years. How should a model keep a fast-changing
mood from constantly disturbing a slow-changing personality? What separates the fast layer
from the slow layer in a real mind?"

Answer:


────────────────────────────────────────────────────────────────────────
Gap 11 — Does personality actually change over a life?
────────────────────────────────────────────────────────────────────────
I derive Big Five → MBTI stack ONCE in JungianStack.deriveFromBigFive() and then it's
basically fixed for life. Only the "grip" (temporary shadow takeover under load) moves.

Ask: "Is the core personality someone is born with mostly fixed for life, or does it
genuinely change with experience? If it changes, what kinds of events actually move it,
how slowly, and is there an age after which it basically locks in?"

Answer:


────────────────────────────────────────────────────────────────────────
Gap 12 — What makes a bond form, deepen, or break?
────────────────────────────────────────────────────────────────────────
My RelationshipLifecycle has stages (acquaintance → close → etc.) and a satisfaction
number, but the rules for moving between stages are hand-tuned thresholds. So relationships
in my sim grow and die for arbitrary reasons.

Ask: "Between two people, what actually decides whether a relationship gets closer, stalls,
or falls apart? Is it just how good the interactions feel, or things like how often they
meet, whether they depend on each other, or matching expectations? And what's the real
reason a close bond breaks for good?"

Answer:


────────────────────────────────────────────────────────────────────────
Gap 13 — How much should other people change my behavior?
────────────────────────────────────────────────────────────────────────
In FreeWillSystem I add a "social influence" term and a SocialNorm "normPressure" to the
action score, with hand-picked weights. So conformity is just a slider I guessed.

Ask: "When does a person go along with the group even when they privately disagree, and
when do they push back? What raises the pressure to conform — group size, how much they
need the group, how public the act is — and what lets a person resist it?"

Answer:


────────────────────────────────────────────────────────────────────────
Gap 14 — Where do social norms come from?
────────────────────────────────────────────────────────────────────────
My SocialNormSystem treats norms as given values (prevalence, pressure) sitting in a group.
They don't emerge from what individuals actually do — I write them in by hand.

Ask: "Norms like 'this is just how we do things here' — do those get handed down from the
top, or do they build up from the bottom out of what individuals keep doing? If it's
bottom-up, what makes a common behavior cross the line into a rule people feel they MUST
follow?"

Answer:


────────────────────────────────────────────────────────────────────────
Gap 15 — Long-term goals vs right-now drives
────────────────────────────────────────────────────────────────────────
I have a PlanningSystem (long horizon) and a Drive system (immediate needs), and they
compete inside the same action score. So a hungry agent with a 10-year goal has no
principled way to decide which wins this minute.

Ask: "A person can have a big long-term goal and an urgent need right now pulling opposite
ways. What decides which one wins at this moment? Is there an order to needs (survival
first, meaning later), and can a strong enough long-term goal ever override hunger or fear?"

Answer:


────────────────────────────────────────────────────────────────────────
Gap 16 — Remembering and forgetting
────────────────────────────────────────────────────────────────────────
My SemanticMemorySystem embeds and keeps memories, weights them by emotion and recency, and
flags some as "formative." But I have no real forgetting curve and the "formative"
threshold is a guess. So my agents either remember everything or forget at a flat rate.

Ask: "Why do we forget almost all of an ordinary day but keep some moments for life? What
decides which memories stick, which fade, and which get rewritten over time? And when an
old memory comes back, is it the real event or a reconstruction?"

Answer:


────────────────────────────────────────────────────────────────────────
Gap 17 — Recovering from chronic stress
────────────────────────────────────────────────────────────────────────
My Drive.load accumulator builds up "wear" from time spent in the danger zone, and high
load triggers the MBTI "grip." But I have almost nothing for RECOVERY — how someone climbs
back out. So my agents can break but barely heal.

Ask: "When someone has been under heavy stress for a long time and finally gets relief, how
do they recover — does it just reverse slowly, or do they need specific things (rest, safety,
other people, making sense of it)? And does long stress leave a permanent mark even after
they recover?"

Answer:


────────────────────────────────────────────────────────────────────────
Gap 18 — How different should two people be? (variance)
────────────────────────────────────────────────────────────────────────
Every entity runs on the same constants with a little random noise. My worry is everyone
ends up behaving too similarly — a gray average crowd with no real outliers.

Ask: "If I create a thousand people, how spread out should they really be — should most be
near the middle with a few extreme outliers, and how extreme do the rare ones get? Put
differently, how much of how someone behaves is their fixed nature versus the situation
they happen to be in?"

Answer:
