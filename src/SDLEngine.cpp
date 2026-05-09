#include <SDL_image.h>

#include "header/SDLEngine.h"

SDLEngine::SDLEngine(const std::string& nomDuJeu)
{
  // On lance le moteur
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
    std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;

  // Sous-bibliotheque SDL_image qui permet de charger du PNG
  if (IMG_Init(IMG_INIT_PNG) == 0)
    std::cerr << "IMG_Init failed: " << SDL_GetError() << std::endl;

  // Creation de la fenetre
  _window = SDL_CreateWindow (nomDuJeu.c_str(), // Le titre de la fenetre
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, // Centree en hauteur/largeur
                              LARGEUR_FENETRE * ECHELLE, HAUTEUR_FENETRE * ECHELLE, // Taille
                              SDL_WINDOW_RESIZABLE); // On peut changer la taille de la fenetre
  if (!_window)
    std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;

  // Creation du renderer (qui gere l'affichage)
  _renderer = SDL_CreateRenderer (_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!_renderer)
    std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;

  // On ne fait pas d'interpolation pour garder l'aspect "pixel art"
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

  _temps = SDL_GetTicks();
}

SDLEngine::~SDLEngine()
{
  IMG_Quit ();
  SDL_DestroyRenderer (_renderer);
  SDL_DestroyWindow (_window);
  SDL_Quit ();
}

void SDLEngine::initialiserRendu()
{
  // On remplie la fenetre avec du noir
  SDL_SetRenderDrawColor (_renderer, 0, 0, 0, 255);
  SDL_RenderClear (_renderer);
}

void SDLEngine::finaliserRendu()
{
  SDL_RenderPresent (_renderer);
}
/*
Evenement Moteur::evenementRecu() const
{
  SDL_Event ev;
  while (SDL_PollEvent(&ev) == 1)
  {
    if (ev.type == SDL_QUIT)
      return QUITTER_APPUYE;
    if (ev.type == SDL_KEYDOWN)
    {
      if (ev.key.keysym.sym == SDLK_SPACE)
        return ESPACE_APPUYE;
      if (ev.key.keysym.sym == SDLK_UP)
        return HAUT_APPUYE;
      if (ev.key.keysym.sym == SDLK_RIGHT)
        return DROITE_APPUYE;
      if (ev.key.keysym.sym == SDLK_LEFT)
        return GAUCHE_APPUYE;
      if (ev.key.keysym.sym == SDLK_DOWN)
        return BAS_APPUYE;
      if (ev.key.keysym.sym == SDLK_ESCAPE)
        return QUITTER_APPUYE;
    }
    else if (ev.type == SDL_KEYUP)
    {
      if (ev.key.keysym.sym == SDLK_SPACE)
        return ESPACE_RELACHE;
      if (ev.key.keysym.sym == SDLK_UP)
        return HAUT_RELACHE;
      if (ev.key.keysym.sym == SDLK_RIGHT)
        return DROITE_RELACHE;
      if (ev.key.keysym.sym == SDLK_LEFT)
        return GAUCHE_RELACHE;
      if (ev.key.keysym.sym == SDLK_DOWN)
        return BAS_RELACHE;
      if (ev.key.keysym.sym == SDLK_ESCAPE)
        return QUITTER_RELACHE;
    }
  }
  return AUCUN;
}*/

bool SDLEngine::animationsAmettreAjour()
{
  constexpr int IMAGES_PAR_SECONDE = 8;
  constexpr int DUREE_IMAGE_MS = 1000 / IMAGES_PAR_SECONDE;
  int nouveau_temps = SDL_GetTicks();
  if (nouveau_temps - _temps >= DUREE_IMAGE_MS)
  {
    _temps = nouveau_temps;
    return true;
  }
  // else
  return false;
}

void SDLEngine::attendre(double secondes) const
{
  SDL_Delay(int(secondes * 1000));
}

SDL_Renderer* SDLEngine::getRenderer()
{
  return _renderer;
}
