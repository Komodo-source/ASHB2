#ifndef WORLD_PLANETVIEW_H
#define WORLD_PLANETVIEW_H

#include <vector>
class Entity;
class Planet;

// Self-contained ImGui window that renders the planet map + entity positions.
// Parallel to the social-network DrawGrid; does not touch it.
void DrawPlanetWindow(const Planet* planet, std::vector<Entity*>& entities);

// History/divergence panel: live run signature + per-region population.
void DrawHistoryWindow();

#endif // WORLD_PLANETVIEW_H
