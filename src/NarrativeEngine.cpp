#include "header/NarrativeEngine.h"
#include "header/Entity.h"
#include <map>
#include <vector>
#include <cstdlib>

// ── Global definitions ───────────────────────────────────────────────────────
std::deque<std::string> globalNarrativeLog;
std::deque<std::string> protagonistNarrativeLog;
int protagonistEntityId = -1;

static const int MAX_LOG_SIZE = 200;

// ── Helpers ──────────────────────────────────────────────────────────────────

static std::string pick(const std::vector<std::string>& v) {
    if (v.empty()) return "";
    return v[std::rand() % v.size()];
}

// Replace {name} and {target} tokens in a template string
static std::string fill(const std::string& tmpl,
                         const std::string& name,
                         const std::string& target = "") {
    std::string out = tmpl;
    auto replace = [&](const std::string& tok, const std::string& val) {
        size_t pos = 0;
        while ((pos = out.find(tok, pos)) != std::string::npos) {
            out.replace(pos, tok.size(), val);
            pos += val.size();
        }
    };
    replace("{name}",   name);
    replace("{target}", target.empty() ? "someone" : target);
    return out;
}

// ── Sentence templates ───────────────────────────────────────────────────────

static const std::map<std::string, std::vector<std::string>> SELF_TEMPLATES = {
    { "Sleep",    { "{name} settled in for the night.",
                    "Exhaustion won out — {name} drifted off to sleep.",
                    "The day had been long. {name} finally slept." } },
    { "Rest",     { "{name} sat down and let the day slow.",
                    "A few quiet minutes. {name} rested.",
                    "{name} found a bench and took a breath." } },
    { "Eat",      { "{name} grabbed something to eat.",
                    "Hunger finally attended to — {name} sat down for a meal.",
                    "{name} ate, almost forgetting how hungry they were." } },
    { "Wash",     { "{name} cleaned up.",
                    "A shower — small thing, but {name} felt better after.",
                    "{name} washed up, shedding the weight of the day." } },
    { "Exercise", { "{name} pushed through a workout.",
                    "Movement cleared the head — {name} exercised.",
                    "{name} ran until the noise inside quieted." } },
    { "Work",     { "{name} settled in at work, getting through the day's tasks.",
                    "Another day at the office. {name} got to it.",
                    "{name} buried themselves in work, finding comfort in routine." } },
    { "Study",    { "{name} cracked open their notes and studied.",
                    "Something to learn. {name} sat down with it.",
                    "{name} studied, mind grinding through the material." } },
    { "Meditate", { "{name} found a quiet corner and breathed through the noise.",
                    "Stillness. {name} sat and let the mind settle.",
                    "{name} meditated — the chatter inside softened." } },
    { "Pray",     { "{name} bowed their head in prayer.",
                    "In the quiet, {name} prayed.",
                    "{name} turned inward and prayed for something nameless." } },
    { "CreateArt",{ "{name} lost themselves in creating something.",
                    "The creative urge wouldn't quiet — {name} made something.",
                    "{name} worked on a piece, time dissolving around them." } },
};

static const std::map<std::string, std::vector<std::string>> SOCIAL_TEMPLATES = {
    { "Socialize",     { "{name} struck up a conversation with {target}.",
                         "{name} and {target} talked for a while — it felt good.",
                         "Running into {target}, {name} stopped to chat." } },
    { "GoodConnection",{ "{name} had a genuinely good interaction with {target}.",
                         "Something clicked — {name} and {target} connected.",
                         "{name} laughed with {target}. A real laugh." } },
    { "Gossip",        { "{name} whispered something about someone to {target}.",
                         "Old rumors found new ears — {name} passed one to {target}.",
                         "{name} leaned in close and gossiped with {target}." } },
    { "HelpSupport",   { "{name} noticed {target} struggling and offered help.",
                         "Without being asked, {name} stepped in for {target}.",
                         "{name} sat with {target}. Sometimes that's enough." } },
    { "Apologize",     { "{name} swallowed their pride and apologized to {target}.",
                         "It took courage, but {name} said sorry to {target}.",
                         "{name} looked {target} in the eye and apologized." } },
    { "Reconcile",     { "{name} reached out to {target}, trying to fix things.",
                         "The grudge had been heavy. {name} tried to reconcile with {target}.",
                         "{name} and {target} talked it through." } },
    { "Flirt",         { "{name} turned on the charm with {target}.",
                         "There was something between them — {name} leaned into it with {target}.",
                         "{name} flirted shamelessly with {target}." } },
    { "Desire",        { "{name} pulled {target} closer, drawn by something deeper.",
                         "The attraction was obvious. {name} acted on it with {target}.",
                         "{name} and {target} — the pull between them was hard to ignore." } },
    { "Date",          { "{name} and {target} went on a date.",
                         "Time together, just the two of them — {name} and {target}.",
                         "{name} took {target} out. A good evening." } },
    { "couple",        { "{name} and {target} made it official.",
                         "Something shifted — {name} and {target} became a couple.",
                         "{name} and {target} chose each other." } },
    { "BreakUp",       { "It was over. {name} ended things with {target}.",
                         "{name} and {target} parted ways.",
                         "Painful and quiet — {name} broke up with {target}." } },
    { "SetBoundaries", { "{name} drew a clear line with {target}.",
                         "{target} got pushback — {name} set a boundary.",
                         "{name} told {target} what they needed. Firmly." } },
    { "IgnoreAvoid",   { "{name} kept their distance from {target}.",
                         "Sometimes the answer is silence. {name} avoided {target}.",
                         "{name} walked past {target} without a word." } },
    { "AngerConnection",{ "Tension between {name} and {target} boiled over.",
                           "{name} confronted {target} — words were exchanged.",
                           "It had been building. {name} and {target} finally clashed." } },
    { "Insult",        { "Words flew — {name} said something cutting to {target}.",
                         "{name} let {target} have it.",
                         "Cruel words from {name}, aimed straight at {target}." } },
    { "Manipulate",    { "{name} played their cards carefully with {target}.",
                         "Words chosen with calculation — {name} worked on {target}.",
                         "{name} twisted things in their favor with {target}." } },
    { "Jealousy",      { "Green-eyed, {name} couldn't stop watching {target}.",
                         "{name} seethed with jealousy over {target}.",
                         "The envy was ugly. {name} couldn't help it with {target}." } },
    { "Betray",        { "{name} betrayed {target}'s trust — some lines stay crossed.",
                         "A quiet, cold betrayal. {name} went behind {target}'s back.",
                         "{target} trusted {name}. That was the mistake." } },
    { "Discrimination",{ "An ugly moment — {name} treated {target} unfairly.",
                         "{name} discriminated against {target}.",
                         "{target} was on the receiving end of {name}'s bias." } },
    { "Murder",        { "{name} attacked {target} in a blind rage.",
                         "Something snapped in {name} — they went after {target}.",
                         "Violence. {name} turned on {target}." } },
    { "breeding",      { "{name} and {target} chose to have a child together.",
                         "A new life — {name} and {target} decided to start one.",
                         "{name} and {target}. A child on the way." } },
};

// ── Public methods ────────────────────────────────────────────────────────────

std::string NarrativeEngine::formatHour(int hour) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:00", hour);
    return buf;
}

std::string NarrativeEngine::actionToSentence(const Entity* ent, const std::string& action,
                                               const Entity* target, int hour,
                                               const std::string& locationName) {
    const std::string& name       = ent->name;
    const std::string  targetName = target ? target->name : "";

    // Check social (targeted) templates first
    auto it = SOCIAL_TEMPLATES.find(action);
    if (it != SOCIAL_TEMPLATES.end())
        return fill(pick(it->second), name, targetName);

    // Check self templates
    auto it2 = SELF_TEMPLATES.find(action);
    if (it2 != SELF_TEMPLATES.end())
        return fill(pick(it2->second), name);

    // Generic fallback
    if (!targetName.empty())
        return name + " did something with " + targetName + ".";
    return name + " " + action + ".";
}

std::string NarrativeEngine::innerMonologue(const Entity* ent) {
    // Check grief first
    for (const auto& grief : ent->griefStates)
        if (grief.intensity > 0.5f)
            return pick({ "The loss is still raw. Some days I don't know how to carry it.",
                          "They're gone. I keep forgetting, then remembering.",
                          "Grief doesn't warn you when it's coming back." });

    if (ent->entityMentalHealth < 25.0f)
        return pick({ "The darkness keeps coming back. I don't know what's wrong.",
                      "My mind won't let me rest.",
                      "Everything feels grey and very far away." });
    if (ent->entityStress > 80.0f)
        return pick({ "Everything feels like too much. I can't catch my breath.",
                      "The pressure is building and I don't know where to put it.",
                      "I'm running on empty and the day isn't even over." });
    if (ent->entityHealth < 30.0f)
        return pick({ "I feel terrible. I should see someone about this.",
                      "My body is sending signals I keep ignoring.",
                      "Something's wrong. I can feel it." });
    if (ent->entityLoneliness > 80.0f)
        return pick({ "It's been so long since I've really connected with anyone.",
                      "People everywhere, still alone somehow.",
                      "The silence in this crowd is the loudest thing." });
    if (ent->entityGeneralAnger > 70.0f)
        return pick({ "I'm furious and I don't know what to do with it.",
                      "The anger sits right under the surface.",
                      "I need to cool down. I know that. I just can't." });
    if (ent->entityHapiness > 75.0f)
        return pick({ "Things are actually good right now. I should let myself feel that.",
                      "There's a lightness today. I'll take it.",
                      "Something feels right. Don't overthink it." });
    if (ent->entityBoredom > 75.0f)
        return pick({ "Same day, different number. Something needs to change.",
                      "The days all feel the same lately.",
                      "Is this it? There has to be more than this." });
    if (ent->entityStress > 55.0f)
        return pick({ "There's a low-grade tension I can't shake.",
                      "I'm fine. Mostly fine. Somewhat fine.",
                      "I keep waiting for things to settle down." });
    if (ent->entityLoneliness > 50.0f)
        return pick({ "I should reach out to someone. I keep not doing it.",
                      "A familiar ache — I need more people in my life.",
                      "Everyone seems busy. Or maybe I'm just not reaching." });
    if (ent->entityHapiness > 55.0f)
        return pick({ "Not bad. Things could be worse, and they've been worse.",
                      "One of the okay days. I'll take it.",
                      "Quietly content. That counts for something." });

    return pick({ "Just getting through the day.",
                  "One thing at a time.",
                  "It is what it is.",
                  "Another day. Keep moving." });
}
