import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Insert updateSimulationStep and rewrite initialiseSDL
update_sim_step = """
void updateSimulationStep(std::vector<Entity>& entities, std::vector<Entity*>& ent_quad, std::vector<std::vector<Entity*>>& close_entity_together, int& day, int& frameCounter, const int UPDATE_FREQUENCY, bool isPaused, int width, int height, int& selectedEntityIndex, bool& showEntityWindow) {
    //Birthday,
    // une année = 100 jours
    if((day / 60)  % 100 == 1){
        for(Entity& ent : entities){
            ent.IncrementBDay();
        }
    }

    // Check for dead entities and remove them
    for(int i = (int)entities.size() - 1; i >= 0; i--){
        if(entities[i].entityHealth <= 0.0f){
            std::cout << "Entity " << entities[i].getId() << " has died and is being removed from the scene." << std::endl;
            entities.erase(entities.begin() + i);

            // Rebuild ent_quad pointer vector
            ent_quad.clear();
            for(int j = 0; j < (int)entities.size(); j++){
                ent_quad.push_back(&entities[j]);
            }

            // Reset selected entity if it was removed
            if(selectedEntityIndex == i){
                showEntityWindow = false;
                selectedEntityIndex = -1;
            } else if(selectedEntityIndex > i){
                selectedEntityIndex--;
            }
        }
    }

    // Update free will system periodically (only when not paused)
    if (!isPaused) {
        frameCounter++;
        if(frameCounter >= UPDATE_FREQUENCY){
            frameCounter = 0;

            // Recalculate entity groups based on current positions
            close_entity_together = separationQuad(ent_quad, width, height);

            // Apply free will to all entity groups with current day for context
            applyFreeWill(close_entity_together, day);
            std::vector<Entity> new_borns = get_new_borns();
            for(Entity ent: new_borns){
                entities.push_back(ent);
            }
            FreeWillSystem::clear_new_borns();

            // Rebuild ent_quad so new entities appear on map and pointers are fresh
            if (!new_borns.empty()) {
                ent_quad.clear();
                for(int j = 0; j < (int)entities.size(); j++){
                    ent_quad.push_back(&entities[j]);
                }
                // Repair all pointedEntity pointers
                for(Entity& e : entities){
                    for(auto& d : e.list_entityPointedDesire){
                        if(d.pointedEntity){
                            int id = d.pointedEntity->entityId;
                            for(Entity& other : entities)
                                if(other.entityId == id){ d.pointedEntity = &other; break; }
                        }
                    }
                    for(auto& a : e.list_entityPointedAnger){
                        if(a.pointedEntity){
                            int id = a.pointedEntity->entityId;
                            for(Entity& other : entities)
                                if(other.entityId == id){ a.pointedEntity = &other; break; }
                        }
                    }
                    for(auto& s : e.list_entityPointedSocial){
                        if(s.pointedEntity){
                            int id = s.pointedEntity->entityId;
                            for(Entity& other : entities)
                                if(other.entityId == id){ s.pointedEntity = &other; break; }
                        }
                    }
                    for(auto& c : e.list_entityPointedCouple){
                        if(c.pointedEntity){
                            int id = c.pointedEntity->entityId;
                            for(Entity& other : entities)
                                if(other.entityId == id){ c.pointedEntity = &other; break; }
                        }
                    }
                }
            }

            // Export current state to JSON lines for HTML viewer
            exportTickHistory("./src/data/tick_history.jsonl", entities, day);
        }
        day++;
    }
}

void initialiseSDL(std::vector<Entity>& entities, std::vector<Entity*>& ent_quad, std::vector<std::vector<Entity*>>& close_entity_together, int& day, int& frameCounter, const int UPDATE_FREQUENCY, int width, int height, int& selectedEntityIndex, bool& showEntityWindow){
    SDLEngine SDLEngine("ASHB2 DEBUG");
    Image obj(SDLEngine, "assets/background.jpg");
    
    bool running = true;
    SDL_Event event;
    while (running)
    {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Apply free will and simulation updates
        updateSimulationStep(entities, ent_quad, close_entity_together, day, frameCounter, UPDATE_FREQUENCY, false, width, height, selectedEntityIndex, showEntityWindow);

        SDLEngine.initialiserRendu();
        obj.dessiner(0,0);
        
        // Draw all entities as dots in SDL mode
        SDL_SetRenderDrawColor(SDLEngine.getRenderer(), 255, 255, 255, 255);
        for(Entity* ent : ent_quad){
            SDL_Rect r;
            r.x = static_cast<int>(ent->posX);
            r.y = static_cast<int>(ent->posY);
            r.w = 5;
            r.h = 5;
            SDL_RenderFillRect(SDLEngine.getRenderer(), &r);
        }

        SDLEngine.finaliserRendu();
    }
}
"""

content = re.sub(r'void initialiseSDL\(\)\{.*?\}\n\}', update_sim_step, content, flags=re.DOTALL)

# 2. Extract entity setup and move above the if(renderingType == 1)
# And add srand(time(NULL)); above it!
entity_setup_regex = r'(        std::vector<Entity> entities;.*?                count\+\+;\n            \}\n        \}\n\n        static bool showEntityWindow = false;\n        static int selectedEntityIndex = -1;\n        std::vector<Entity\*> ent_quad;\n        for\(int i=0; i<entities\.size\(\); i\+\+\)\{\n            ent_quad\.push_back\(&entities\[i\]\);\n        \}\n\n        std::vector<std::vector<Entity\*>> close_entity_together = separationQuad\(ent_quad, width, height\);\n\n        // ici on applique.*?        int frameCounter = 0;\n        int day = FreeWillSystem::day;\n\n        const int UPDATE_FREQUENCY = 60; // Update free will every 60 frames\n)'

match = re.search(entity_setup_regex, content, re.DOTALL)
if match:
    entity_setup = match.group(1)
    
    # We remove "static" from showEntityWindow and selectedEntityIndex in the setup to define them locally
    entity_setup = entity_setup.replace('static bool showEntityWindow', 'bool showEntityWindow')
    entity_setup = entity_setup.replace('static int selectedEntityIndex', 'int selectedEntityIndex')
    
    # Also add srand and height/width defs
    setup_to_inject = f"""    srand(time(NULL));
    const int height = 1050;
    const int width = 1400;

{entity_setup}"""
    # Remove from inside renderingType == 1
    content = content.replace(match.group(1), '')
    
    # Remove "srand(time(NULL));\n        if (!glfwInit()) return -1;\n\n        const int height = 1050;\n        const int width = 1400;"
    content = re.sub(r'        srand\(time\(NULL\)\);\n        if \(\!glfwInit\(\)\) return -1;\n\n        const int height = 1050;\n        const int width = 1400;', r'        if (!glfwInit()) return -1;', content)
    
    # Inject before if(renderingType == 1)
    content = content.replace('    if(renderingType == 1){', setup_to_inject + '    if(renderingType == 1){')

# 3. Replace ImGui simulation loop with updateSimulationStep
imgui_sim_regex = r'(        while \(\!glfwWindowShouldClose\(window\)\) \{.*?            ImGui::NewFrame\(\);\n\n).*?(            std::string saveFilename;)'
match = re.search(imgui_sim_regex, content, re.DOTALL)
if match:
    replacement = match.group(1) + '            updateSimulationStep(entities, ent_quad, close_entity_together, day, frameCounter, UPDATE_FREQUENCY, instanceUI.isSimulationPaused(), width, height, selectedEntityIndex, showEntityWindow);\n\n' + match.group(2)
    content = content.replace(match.group(0), replacement)
    
# Remove day++ at the end of ImGui loop
content = re.sub(r'            if \(\!instanceUI\.isSimulationPaused\(\)\) \{\n                day\+\+;\n            \}\n', '', content)

# 4. Update the else block to pass arguments
content = re.sub(r'    \}else\{\/\/ sdl rendering\n        initialiseSDL\(\);\n    \}', r'    }else{// sdl rendering\n        initialiseSDL(entities, ent_quad, close_entity_together, day, frameCounter, UPDATE_FREQUENCY, width, height, selectedEntityIndex, showEntityWindow);\n    }', content)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("done")
