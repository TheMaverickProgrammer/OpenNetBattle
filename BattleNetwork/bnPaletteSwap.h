#pragma once
#include "bnComponent.h"
#include <SFML/Graphics.hpp>
class BattleScene;
class Entity;

class PaletteSwap : public Component {
private:
  sf::Texture palette;
  sf::Texture base;
  sf::Shader* paletteSwap;
  bool enabled; /*!< Turn this effect on/off */
public:
  PaletteSwap(Entity* owner, sf::Texture base);
  ~PaletteSwap();
  void OnUpdate(float _elapsed);
  void Inject(BattleScene&);
  void LoadPaletteTexture(std::string);
  void SetTexture(sf::Texture& texture);
  void Revert();
  void Enable(bool enabled = true);
};
