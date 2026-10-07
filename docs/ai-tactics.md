# AI combat tactics from a local language model (shelved)

Status: **shelved on 2026-10-07** after a spike. Kept here so the idea can be picked up when better hardware is available.

## The idea
Let a small language model on the home server choose tactics for intelligent monsters (and, later, NPCs), reached through a small gateway service, with Tailscale for playing away from home.

The design that came out of planning:
- **The model never decides mechanics.** The engine lists the legal options (each target, a breath weapon, each castable spell), and the model returns the id of one. A JSON schema on the request makes it return only a valid id, and dice still decide everything.
- **Only "smart" creatures** (INT 10+, not beasts) would ask the model. The rest keep the rules AI.
- **Any failure** (server off, slow, nonsense) falls back to the rules AI, so the game plays the same with no server.
- A thin game-specific gateway (Python, next to the model) owns the prompts; the app talks only to it, over plain HTTP inside Tailscale.

## Why it was shelved
The spike ran a realistic turn (a Mage, its guard, and three PCs) against the model on `agent`: Qwen2.5-1.5B-Instruct (Q4_K_M) on llama.cpp's server, on an i5-5300U with no GPU, shared with another project (one request at a time).

- **Speed:** about 5 s a turn for a one-word answer when the battlefield changed, and about 11 s with a one-line reason. The target was about 3 s.
- **Quality:** the JSON schema worked: every answer was a legal option. The tactics didn't: offered Fireball on two clustered PCs, Magic Missile on a nearly dead fighter, or dagger attacks, it stabbed the fighter with a dagger every time, and gave reasons that weren't true ("he's the closest", though there are no positions).

## To revisit
- A bigger model (3B to 7B) on faster hardware, such as a GPU, would need about 1 s a turn and sensible choices. Rerun the same spike first.
- Splitting monster turns into "list the options, then pick one" helps whatever picks: it could also back a smarter rules AI (focus the wounded, Fireball clusters, break concentration).
- A cheaper use of the small model: battle cries or taunts written in the background, never holding up a turn.
