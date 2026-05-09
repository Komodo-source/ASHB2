#include <SDL_image.h>

#include <exception>

#include "header/Image.h"

struct Texture_deleter
{
  void operator()(SDL_Texture* t) const
  {
    SDL_DestroyTexture(t);
  }
};

Image::Image()
{

}

Image::Image(SDLEngine& SDLEngine, const std::string& nomDuFichier)
  : _SDLEngine(&SDLEngine)
{
  SDL_Surface* surface = IMG_Load(nomDuFichier.c_str());
  if (surface == nullptr)
    throw std::runtime_error("Impossible de charger " + nomDuFichier);

  _texture = std::shared_ptr<SDL_Texture>(SDL_CreateTextureFromSurface(_SDLEngine->getRenderer(), surface),
                                          Texture_deleter());

  _rectangle.x = 0;
  _rectangle.y = 0;
  _rectangle.w = surface->w;
  _rectangle.h = surface->h;

  _hauteur = surface->h;
  _largeur = surface->w;
  SDL_FreeSurface(surface);
}

void Image::selectionnerRectangle(int x, int y, int largeur, int hauteur)
{
  _rectangle.x = x;
  _rectangle.y = y;
  _rectangle.w = largeur;
  _rectangle.h = hauteur;
}

void Image::dessiner(int x, int y) const
{
  SDL_Rect target;
  target.x = x * ECHELLE;
  target.y = y * ECHELLE;
  target.w = _rectangle.w * ECHELLE;
  target.h = _rectangle.h * ECHELLE;
  SDL_RenderCopy(_SDLEngine->getRenderer(), _texture.get(), &_rectangle, &target);
}

int Image::getHauteur() const
{
  return _hauteur;
}

int Image::getLargeur() const
{
  return _largeur;
}
