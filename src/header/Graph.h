#ifndef GRAPH_H
#define GRAPH_H

#include "./Entity.h"
#include <vector>


class Graph{
  private:
    Entity* node;

    std::vector<Graph*> pointed_nodes;

  public:
    Entity* getNode();
    std::vector<Graph*> getPointed();
    void addPointed(Entity* ent);
    void setNode(Entity* ent);
};

#endif // GRAPH_H
