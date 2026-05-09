#ifndef IMAGE_H
#define IMAGE_H

#include <memory>

#include "SDLEngine.h"

class Image
{
  SDLEngine* _SDLEngine;

  // Structures internes de la SDL
  std::shared_ptr<SDL_Texture> _texture;
  SDL_Rect _rectangle;
  int _largeur;
  int _hauteur;

public:

  // Cree un objet image vide
  Image();

  // Charge l'image (PNG par exemple) contenue dans le fichier.
  // Si le fichier n'existe pas, une exception de type `runtime_error` est levee
  Image(SDLEngine& SDLEngine, const std::string& nomDuFichier);

  // Selectionne quelle partie de l'image sera dessinee
  void selectionnerRectangle(int x, int y, int largeur, int hauteur);

  // Dessine l'image (ou la partie selectionnee) aux coordonnees voulues
  void dessiner(int x, int y) const;

  // Retourne les dimensions de l'image en pixels
  int getLargeur() const;
  int getHauteur() const;
};

#endif // IMAGE_H
