#pragma once
#include <SFML/Graphics.hpp>
#include <sstream>
#include <vector>

#include "bnUIComponent.h"

class Entity;
class Player;

using sf::Font;
using sf::Text;
using sf::Sprite;
using sf::Texture;
using sf::Drawable;
using std::vector;
using std::ostringstream;

class AuraHealthUI :  public UIComponent {
public:
  AuraHealthUI(Character* owner);
  ~AuraHealthUI();

  virtual void draw(sf::RenderTarget & target, sf::RenderStates states) const;

  virtual void Update(float elapsed);
  virtual void Inject(BattleScene& scene) { }

private:
  int lastHP;
  int currHP;
  int startHP;
  mutable Sprite font;
  Character* owner;
};