import random
from enum import Enum
from dataclasses import dataclass
from typing import List, Dict, Callable

class Emotion(Enum):
    """Core emotions that influence decision-making"""
    HAPPY = "happy"
    SAD = "sad"
    ANGRY = "angry"
    FEARFUL = "fearful"
    EXCITED = "excited"
    CALM = "calm"
    STRESSED = "stressed"

class Need(Enum):
    """Basic needs that drive behavior"""
    HUNGER = "hunger"
    THIRST = "thirst"
    SLEEP = "sleep"
    SOCIAL = "social"
    SAFETY = "safety"
    ACHIEVEMENT = "achievement"

@dataclass
class PersonalityTrait:
    """Personality traits that affect decision weighting"""
    openness: float  # 0-1: willingness to try new things
    conscientiousness: float  # 0-1: planning vs spontaneity
    extraversion: float  # 0-1: social vs solitary preference
    agreeableness: float  # 0-1: cooperative vs competitive
    neuroticism: float  # 0-1: emotional stability

@dataclass
class Action:
    """Represents a possible action an entity can take"""
    name: str
    description: str
    base_appeal: float  # 0-1: inherent attractiveness
    energy_cost: float  # 0-1: how tiring
    time_cost: float  # hours required
    emotion_modifiers: Dict[Emotion, float]  # how emotions affect appeal
    need_satisfaction: Dict[Need, float]  # which needs this satisfies
    personality_modifiers: Dict[str, float]  # personality influence
    prerequisites: Callable = None  # function to check if action is possible

class EmotionalState:
    """Tracks current emotional state"""
    def __init__(self):
        self.emotions: Dict[Emotion, float] = {e: 0.5 for e in Emotion}
        self.intensity = 0.5  # overall emotional intensity

    def update_emotion(self, emotion: Emotion, change: float):
        """Update emotion level (0-1 scale)"""
        self.emotions[emotion] = max(0, min(1, self.emotions[emotion] + change))
        self.intensity = sum(abs(v - 0.5) for v in self.emotions.values()) / len(self.emotions)

    def get_dominant_emotion(self) -> Emotion:
        """Returns the strongest current emotion"""
        return max(self.emotions.items(), key=lambda x: x[1])[0]

class NeedState:
    """Tracks physiological and psychological needs"""
    def __init__(self):
        self.needs: Dict[Need, float] = {n: 0.5 for n in Need}

    def update_need(self, need: Need, change: float):
        """Update need level (0=satisfied, 1=critical)"""
        self.needs[need] = max(0, min(1, self.needs[need] + change))

    def get_urgent_needs(self, threshold=0.7) -> List[Need]:
        """Returns needs above urgency threshold"""
        return [n for n, v in self.needs.items() if v > threshold]

class Entity:
    """Simulated entity with free will"""
    def __init__(self, name: str, personality: PersonalityTrait):
        self.name = name
        self.personality = personality
        self.emotional_state = EmotionalState()
        self.need_state = NeedState()
        self.memory: List[Dict] = []  # past actions and outcomes
        self.current_context = {}  # environmental factors

    def evaluate_action(self, action: Action) -> float:
        """
        Calculate appeal score for an action based on:
        - Base appeal of action
        - Current emotional state
        - Current needs
        - Personality traits
        - Past experiences (memory)
        - Random variation (free will uncertainty)
        """
        score = action.base_appeal

        # Emotional influence
        for emotion, modifier in action.emotion_modifiers.items():
            emotion_level = self.emotional_state.emotions[emotion]
            score += modifier * emotion_level * 0.3

        # Need satisfaction - urgent needs greatly increase appeal
        for need, satisfaction in action.need_satisfaction.items():
            need_level = self.need_state.needs[need]
            # Urgent needs exponentially increase appeal
            score += satisfaction * (need_level ** 2) * 0.5

        # Personality influence
        for trait, modifier in action.personality_modifiers.items():
            trait_value = getattr(self.personality, trait)
            score += modifier * trait_value * 0.2

        # Energy consideration based on stress
        stress_level = self.emotional_state.emotions[Emotion.STRESSED]
        score -= action.energy_cost * stress_level * 0.2

        # Memory influence - learn from past experiences
        similar_actions = [m for m in self.memory if m['action'] == action.name]
        if similar_actions:
            avg_outcome = sum(m['outcome'] for m in similar_actions) / len(similar_actions)
            score += avg_outcome * 0.15

        # Random variation - the "free will" uncertainty factor
        # Higher neuroticism = more random decisions
        randomness = 0.1 + (self.personality.neuroticism * 0.2)
        score += random.uniform(-randomness, randomness)

        return max(0, min(1, score))

    def choose_action(self, available_actions: List[Action]) -> Action:
        """
        Entity chooses an action from available options
        This is where "free will" happens
        """
        # Filter actions based on prerequisites
        possible_actions = [
            a for a in available_actions
            if a.prerequisites is None or a.prerequisites(self)
        ]

        if not possible_actions:
            return None

        # Evaluate all possible actions
        scores = {action: self.evaluate_action(action) for action in possible_actions}

        # Probabilistic choice weighted by scores (not always the highest!)
        # This adds unpredictability - sometimes choosing suboptimal actions
        weights = [score ** 2 for score in scores.values()]  # square to amplify differences
        total_weight = sum(weights)

        if total_weight == 0:
            chosen = random.choice(possible_actions)
        else:
            normalized_weights = [w / total_weight for w in weights]
            chosen = random.choices(possible_actions, weights=normalized_weights)[0]

        return chosen

    def execute_action(self, action: Action) -> Dict:
        """Execute chosen action and update internal state"""
        # Satisfy needs
        for need, satisfaction in action.need_satisfaction.items():
            self.need_state.update_need(need, -satisfaction)

        # Update emotional state based on action outcome
        outcome_quality = random.uniform(0.7, 1.0)  # how well action went

        # Record in memory
        memory_entry = {
            'action': action.name,
            'outcome': outcome_quality,
            'emotional_state': self.emotional_state.emotions.copy(),
            'needs': self.need_state.needs.copy()
        }
        self.memory.append(memory_entry)

        # Limit memory size
        if len(self.memory) > 50:
            self.memory.pop(0)

        return {
            'entity': self.name,
            'action': action.name,
            'outcome': outcome_quality,
            'reasoning': f"Chose based on emotions and needs"
        }

# Example usage and action definitions
def create_example_actions() -> List[Action]:
    """Create a set of example actions"""
    return [
        Action(
            name="eat_meal",
            description="Eat a nutritious meal",
            base_appeal=0.5,
            energy_cost=0.1,
            time_cost=0.5,
            emotion_modifiers={Emotion.HAPPY: 0.3, Emotion.STRESSED: -0.1},
            need_satisfaction={Need.HUNGER: 0.8, Need.SOCIAL: 0.1},
            personality_modifiers={'conscientiousness': 0.1}
        ),
        Action(
            name="socialize",
            description="Spend time with friends",
            base_appeal=0.6,
            energy_cost=0.3,
            time_cost=2.0,
            emotion_modifiers={Emotion.HAPPY: 0.4, Emotion.SAD: 0.2, Emotion.FEARFUL: -0.3},
            need_satisfaction={Need.SOCIAL: 0.9},
            personality_modifiers={'extraversion': 0.5, 'agreeableness': 0.2}
        ),
        Action(
            name="work_project",
            description="Work on important project",
            base_appeal=0.4,
            energy_cost=0.6,
            time_cost=3.0,
            emotion_modifiers={Emotion.EXCITED: 0.3, Emotion.STRESSED: -0.4},
            need_satisfaction={Need.ACHIEVEMENT: 0.8},
            personality_modifiers={'conscientiousness': 0.6, 'neuroticism': -0.2}
        ),
        Action(
            name="rest",
            description="Take a break and relax",
            base_appeal=0.5,
            energy_cost=-0.5,  # restores energy
            time_cost=1.0,
            emotion_modifiers={Emotion.STRESSED: 0.5, Emotion.CALM: 0.3},
            need_satisfaction={Need.SLEEP: 0.4},
            personality_modifiers={'neuroticism': 0.3}
        ),
        Action(
            name="explore_new_place",
            description="Visit somewhere new",
            base_appeal=0.6,
            energy_cost=0.4,
            time_cost=3.0,
            emotion_modifiers={Emotion.EXCITED: 0.5, Emotion.HAPPY: 0.3, Emotion.FEARFUL: -0.4},
            need_satisfaction={Need.ACHIEVEMENT: 0.3, Need.SOCIAL: 0.2},
            personality_modifiers={'openness': 0.7, 'extraversion': 0.3}
        )
    ]

# Simulation example
def run_simulation_example():
    """Demonstrate the free will system"""
    # Create an entity
    personality = PersonalityTrait(
        openness=0.7,
        conscientiousness=0.6,
        extraversion=0.8,
        agreeableness=0.7,
        neuroticism=0.3
    )
    entity = Entity("Alice", personality)

    # Set initial state
    entity.emotional_state.update_emotion(Emotion.HAPPY, 0.2)
    entity.need_state.update_need(Need.HUNGER, 0.3)
    entity.need_state.update_need(Need.SOCIAL, 0.4)

    actions = create_example_actions()

    print(f"=== Simulating {entity.name}'s decisions ===\n")
    print(f"Personality: Openness={personality.openness}, Extraversion={personality.extraversion}")
    print(f"Current emotion: {entity.emotional_state.get_dominant_emotion().value}")
    print(f"Urgent needs: {[n.value for n in entity.need_state.get_urgent_needs(0.6)]}\n")

    # Simulate multiple decision points
    for i in range(5):
        print(f"--- Decision {i+1} ---")

        # Entity chooses action
        chosen = entity.choose_action(actions)

        if chosen:
            print(f"Chose to: {chosen.name} - {chosen.description}")

            # Show scores for transparency
            scores = {a.name: entity.evaluate_action(a) for a in actions}
            print(f"Action scores: {', '.join(f'{k}: {v:.2f}' for k, v in scores.items())}")

            # Execute action
            result = entity.execute_action(chosen)
            print(f"Outcome quality: {result['outcome']:.2f}\n")

            # Update needs naturally over time
            entity.need_state.update_need(Need.HUNGER, 0.1)
            entity.need_state.update_need(Need.SOCIAL, 0.05)
        else:
            print("No valid actions available\n")

if __name__ == "__main__":
    run_simulation_example()
