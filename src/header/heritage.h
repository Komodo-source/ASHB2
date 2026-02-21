#ifndef HERITAGE_H
#define HERITAGE_H

#include <vector>
#include "./Entity.h"
#include "./BetterRand.h"
#include "iostream"
#include "./Graph.h"


class Heritage{
  public:
    static inline std::vector<Graph> heritage_graph;
    void static add_child(Entity* pointer, Entity* pointed);
    int getSizeGraph();
    static void UnlinkedNode(Entity* node);
};
#endif // HERITAGE_H
