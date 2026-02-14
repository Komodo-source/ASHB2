#include "./header/heritage.h"


 void Heritage::add_child(Entity* pointer, Entity* pointed){
  for(Graph g: heritage_graph){
    if(g.getNode() == pointer){
      g.addPointed(pointed);
    }
  }
}

void Heritage::UnlinkedNode(Entity* node){
  Graph g;
  g.setNode(node);
  heritage_graph.push_back(g);
}

int Heritage::getSizeGraph(){
  return heritage_graph.size();
}
