# ASHB2 Simulation Report System

## Overview
This document describes the logging system implemented for the Artificial Simulation of Human Behavior (ASHB2) project. The system captures all simulation outputs and organizes them into categorized log files for detailed analysis.

## Log Files Structure

### 1. Main Command Log (`./data/cmd_log.txt`)
- **Purpose**: Contains all std::cout outputs from the simulation
- **Content**: Every console output, including initialization messages, entity actions, deaths, diseases, etc.
- **Use**: Primary file for AI analysis to generate comprehensive simulation summaries

### 2. Specialized Log Files

#### Deaths Log (`./data/deaths_log.txt`)
- **Format**: `[timestamp] Entity ID (Name, age X) died: cause`
- **Examples**:
  - `[2024-01-15 14:30:25] Entity 5 (Alice, age 45) died: health depletion`
  - `[2024-01-15 14:35:10] Entity 12 (Bob, age 32) died: murder by Charlie`

#### Diseases Log (`./data/diseases_log.txt`)
- **Format**: `[timestamp] Entity ID (Name) contracted/cured disease_name`
- **Examples**:
  - `[2024-01-15 14:20:15] Entity 3 (Diana) contracted Plague`
  - `[2024-01-15 14:25:30] Entity 3 (Diana) cured from Plague`

#### Actions Log (`./data/actions_log.txt`)
- **Format**: `[timestamp] Entity ID (Name) performed: action_name targeting target_name - details`
- **Examples**:
  - `[2024-01-15 14:22:45] Entity 1 (Alice) performed: Socialize targeting Bob - targeted action`
  - `[2024-01-15 14:23:10] Entity 2 (Bob) performed: Work - self-directed action`

#### Relationships Log (`./data/relationships_log.txt`)
- **Format**: `[timestamp] Relationship: entity1_name (ID1) and entity2_name (ID2) - relationship_type - details`
- **Examples**:
  - `[2024-01-15 14:18:20] Relationship: Alice (1) and Bob (2) - couple formed`
  - `[2024-01-15 14:19:45] Relationship: Charlie (3) and Diana (4) - social bond strengthened`

#### Movements Log (`./data/movements_log.txt`)
- **Format**: `[timestamp] Entity ID (Name) moved from (x1,y1) to (x2,y2) - reason`
- **Examples**:
  - `[2024-01-15 14:21:30] Entity 1 (Alice) moved from (100,100) to (105,98) - environmental factors`
  - `[2024-01-15 14:22:15] Entity 2 (Bob) moved from (200,150) to (195,152) - seeking safety`

#### Births Log (`./data/births_log.txt`)
- **Format**: `[timestamp] Birth: Entity ID (Name) born to parent1_name (ID1) and parent2_name (ID2)`
- **Examples**:
  - `[2024-01-15 14:40:00] Birth: Entity 15 (Eve) born to Alice (1) and Bob (2)`

#### Events Log (`./data/events_log.txt`)
- **Format**: `[timestamp] event_type: details`
- **Examples**:
  - `[2024-01-15 14:16:45] simulation_start: Simulation initialized with 10 entities`
  - `[2024-01-15 14:35:20] breeding: Reproduction between Alice and Bob`

## AI Analysis Prompt

Use the following prompt when asking an AI to analyze the simulation logs:

---

**Please analyze the attached simulation log file (`cmd_log.txt`) and generate a comprehensive scientific report summarizing what happened during this run of the Artificial Simulation of Human Behavior (ASHB2).**

**Context:**
- This is a multi-agent simulation modeling human-like entities with needs, emotions, relationships, and behaviors
- Entities have attributes: health, happiness, stress, loneliness, personality traits (extraversion, agreeableness, conscientiousness, neuroticism, openness)
- Simulation includes: disease spread, social interactions, reproduction, movement, aging, and death
- Time progresses in "days" with entities making decisions every few frames

**Analysis Requirements:**

1. **Executive Summary** (2-3 paragraphs)
   - Overview of simulation duration and key outcomes
   - Major events and turning points
   - Population dynamics (births, deaths, final count)

2. **Population Analysis**
   - Initial vs final population
   - Age distribution and demographics
   - Key individuals and their stories

3. **Disease and Health Dynamics**
   - Disease outbreaks and their impacts
   - Recovery patterns
   - Health trends across the population

4. **Social Dynamics**
   - Relationship formations and breakups
   - Social conflicts and resolutions
   - Group formations and interactions

5. **Behavioral Patterns**
   - Common actions and their frequencies
   - Personality trait influences on behavior
   - Environmental factor impacts

6. **Key Events Timeline**
   - Chronological summary of major events
   - Cause-and-effect relationships
   - Turning points in the simulation

7. **Conclusions and Insights**
   - Emergent behaviors observed
   - Simulation stability and realism
   - Potential implications for understanding human behavior

**Report Style:**
- Write in formal scientific/academic tone
- Use clear headings and subheadings
- Include specific data points, entity names, and timestamps where relevant
- Focus on patterns, trends, and insights rather than listing every individual event
- Maintain objectivity while noting interesting emergent behaviors

**Important Notes:**
- The simulation runs in real-time with entities making autonomous decisions
- Deaths can occur from health depletion, murder, or other causes
- Reproduction requires compatible couples with sufficient desire and social bonds
- Environmental factors (weather, safety, noise, crowding) influence behavior

---

## Additional Data Files

The simulation also generates several CSV and JSON files that can provide quantitative data for deeper analysis:

- **Entity Stats CSVs** (`./src/data/[entity_id].csv`): Time series of each entity's vital statistics
- **Action Stats CSVs** (`./src/data/act_[entity_id].csv`): Records of actions performed by each entity
- **Tick History** (`./src/data/tick_history.jsonl`): Complete state snapshots of all entities at each time step
- **HTML Viewer** (`./src/data/viewer.html`): Interactive visualization of the simulation data

## Usage Instructions

1. Run the simulation - all logs are automatically generated
2. Use `cmd_log.txt` as the primary input for AI analysis
3. Refer to specialized logs for specific aspects (e.g., `deaths_log.txt` for mortality analysis)
4. Combine with quantitative data files for comprehensive reports

This system ensures comprehensive documentation of simulation runs while maintaining performance and providing multiple analysis pathways.
