#include "./header/Entity.h"
#include "./header/BetterRand.h"
#include "./header/UI.h"
#include "./header/Graph.h"
#include <iostream>


Entity* Graph::getNode(){
  return node;
}


std::vector<Graph*> Graph::getPointed(){
  return pointed_nodes;
}

void Graph::addPointed(Entity* ent){
  if(ent == nullptr){
    return ;
  }
  Graph g;
  g.setNode(ent);

  pointed_nodes.push_back(&g);
}


void Graph::setNode(Entity* ent){
  node = ent;
}
