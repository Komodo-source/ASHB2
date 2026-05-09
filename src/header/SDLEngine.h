#ifndef SDLEngine_H
#define SDLEngine_H

#include <iostream>

#include <SDL.h>

const int taille_case = 10;
const int NB_CASE_LARGEUR = 100;
const int NB_CASE_HAUTEUR = 100;

const int LARGEUR_FENETRE =  NB_CASE_LARGEUR;
const int HAUTEUR_FENETRE =  NB_CASE_HAUTEUR;

// Les images sont en "pixel art", elles sont toutes petites.  Pour
// que ca s'affiche bien, on multiplie tout par une echelle de 5.
// L'univers du jeu se passe dans 160x120 pixel, mais la fenetre fera
// 5*160x5*128pixels.
const int ECHELLE = 5;
/*
// Les differents types d'evenements disponibles
enum Evenement {
  AUCUN,
  GAUCHE_APPUYE,
  GAUCHE_RELACHE,
  DROITE_APPUYE,
  DROITE_RELACHE,
  HAUT_APPUYE,
  HAUT_RELACHE,
  BAS_APPUYE,
  BAS_RELACHE,
  ESPACE_APPUYE,
  ESPACE_RELACHE,
  QUITTER_APPUYE,
  QUITTER_RELACHE
};
 */

class SDLEngine
{
  // Structures internes de la SDL
  SDL_Window* _window;
  SDL_Renderer* _renderer;
  int _temps;

public:

  // Constructeur qui initialise le SDLEngine, la fenetre, etc.
  // Par defaut la fenetre fait 160 x 128 pixels (grossi 5 fois pour l'affichage)
  // -> l'univers de jeu se fait sur 160 x 128 pixels (c'est arbitraire),
  //    soit une grille de 10x8 cases de 16 pixels de côté
  SDLEngine(const std::string& nomDuJeu);

  // Destructeur qui ferme le SDLEngine et libere la memoire
  ~SDLEngine();

  // Initalise l'image par un ecran noir
  void initialiserRendu();

  // Finalise l'image et l'envoie a la carte graphique pour affichage sur l'ecran
  void finaliserRendu();

  // Renvoie le dernier evenement recu (AUCUN si rien n'est recu)
  //Evenement evenementRecu() const;

  // Renvoie true s'il faut mettre a jour les animations
  bool animationsAmettreAjour();

  // Met le jeu en pause pendant un certain nombre de secondes
  // (ATTENTION : le jeu ne répond plus lorsqu'il est en pause,
  // les evenements ne seront traites qu'apres la pause)
  void attendre(double secondes) const;

  // (Pour un usage interne, vous n'aurez jamais besoin de cette methode.)
  SDL_Renderer* getRenderer();
};

#endif // SDLEngine_H
