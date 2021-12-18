#include <string>
using std::to_string;

#include <Swoosh/Game.h>
#include "battlescene/bnBattleSceneBase.h"
#include "bnMobHealthUI.h"
#include "bnCharacter.h"
#include "bnTextureResourceManager.h"
#include "bnLogger.h"
#include "bnField.h"

#define PIXELS_FROM_BOTTOM 10.0f

MobHealthUI::MobHealthUI(std::weak_ptr<Character> mob) : UIComponent(mob) {
  healthCounter = mob.lock()->GetHealth();
  cooldown = 0;
  color = sf::Color::White;
  glyphs.setTexture(ResourceHandle().Textures().LoadFromFile(TexturePaths::ENEMY_HP_NUMSET));
  glyphs.setScale(2.f, 2.f);

  /*auto onMobDelete = [this](auto target) {
    mob = nullptr;
  };

  mob->GetField()->CallbackOnDelete(mob->GetID(), onMobDelete);*/
}

MobHealthUI::~MobHealthUI() {
  // delete onMobDelete;
}

/*
HP drop is not 1 unit per frame. It is:
10 per frame if difference is 100 or more
~5 per frame if difference is 99-40 range
-3 per frame for anything lower
*/
void MobHealthUI::OnUpdate(double elapsed) {
  if (Input().Has(InputEvents::held_option)) {
    this->Hide();
  }
  else {
    this->Reveal();
  }

  if (auto mob = GetOwnerAs<Character>()) {
    if (cooldown <= 0) { cooldown = 0; }
    else { cooldown -= elapsed; }

    if (healthCounter > mob->GetHealth()) {
      int diff = healthCounter - mob->GetHealth();

      if (diff >= 100) {
        healthCounter -= 10;
      }
      else if (diff >= 40) {
        healthCounter -= 5;
      }
      else if (diff >= 3) {
        healthCounter -= 3;
      }
      else {
        healthCounter--;
      }

      cooldown = seconds_cast<double>(frames(10));
    }
    else if (healthCounter < mob->GetHealth()) {
      healthCounter++;
      color = sf::Color(0, 255, 80);
    }
    else {
      color = sf::Color::White;
    }

    if (cooldown > 0) {
      color = sf::Color(255, 165, 0);
    }

    if (healthCounter < 0 || mob->GetHealth() <= 0) { healthCounter = 0; }
  }
}

void MobHealthUI::Inject(BattleSceneBase& scene)
{
  scene.Inject(shared_from_base<MobHealthUI>());
}

void MobHealthUI::draw(sf::RenderTarget & target, sf::RenderStates states) const
{
  if (IsHidden()) return;

  auto this_states = states;
  this_states.transform *= getTransform();

  // Glyphs are 8x10
  // First glyph is 9 the last is 0
  // There's 1px space between the glyphs

  auto mob = GetOwnerAs<Character>();

  if (healthCounter > 0 && mob && mob->GetTile()) {
    int size = (int)(std::to_string(healthCounter).size());
    int hp = healthCounter;
    float offsetx = -(((size)*8.0f) / 2.0f)*glyphs.getScale().x;
    int index = 0;
    while (index < size) {
      auto str = std::to_string(healthCounter);
      auto substr = str.substr(index, 1);
      const char* cc = substr.c_str();
      int number = std::atoi(cc);

      int row = (10-number-1);
      int rowStart = row + (row * 10);

      auto glyphsRect = sf::IntRect(0, rowStart, 8, 10);
      glyphs.setTextureRect(glyphsRect);
      glyphs.setPosition(sf::Vector2f(offsetx, PIXELS_FROM_BOTTOM) + mob->getPosition());
      glyphs.setColor(color);

      target.draw(glyphs, this_states);

      offsetx += 8.0f*glyphs.getScale().x;
      index++;
    }
  }

  UIComponent::draw(target, states);
}
