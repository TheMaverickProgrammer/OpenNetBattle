#include <string>
using std::to_string;

#include "bnTile.h"
#include "bnPlayerHealthUI.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "battlescene/bnBattleSceneBase.h"

////////////////////////////////////
// class PlayerHealthUI           //
////////////////////////////////////

PlayerHealthUI::PlayerHealthUI() :
  glyphs(Font::Style::gradient)
{
  ResourceHandle handle;

  texture = handle.Textures().LoadFromFile("resources/ui/img_health.png");
  uibox.setTexture(texture);
  uibox.setPosition(2.f, 0.0f);
  glyphs.setPosition(uibox.getLocalBounds().width - 2.f, 3.f);
}

PlayerHealthUI::~PlayerHealthUI()
{
}

void PlayerHealthUI::SetFontStyle(Font::Style style)
{
  glyphs.SetFont(style);
}

void PlayerHealthUI::SetHP(int hp)
{
  targetHP = std::max(hp, 0);

  if (currHP == 0 && lastHP == 0) {
    currHP = lastHP = hp;
  }
}

void PlayerHealthUI::Update(double elapsed)
{
  if (lastHP != targetHP) {
    if (currHP > targetHP) {
      int diff = currHP - targetHP;

      if (diff >= 100) {
        currHP -= 10;
      }
      else if (diff >= 40) {
        currHP -= 5;
      }
      else if (diff >= 3) {
        currHP -= 3;
      }
      else {
        currHP--;
      }

      cooldown = 0.5; // seconds
    }
    else if (currHP < targetHP) {
      int diff = targetHP - currHP;

      if (diff >= 100) {
        currHP += 10;
      }
      else if (diff >= 40) {
        currHP += 5;
      }
      else if (diff >= 3) {
        currHP += 3;
      }
      else {
        currHP++;
      }
    }
    else {
      lastHP = currHP;
    }
  }

  if (cooldown <= 0) { cooldown = 0; }
  else { cooldown -= elapsed; }

  glyphs.SetFont(Font::Style::gradient);

  if (currHP > targetHP || cooldown > 0) {
    glyphs.SetFont(Font::Style::gradient_gold);
  }
  else if (currHP < targetHP) {
    glyphs.SetFont(Font::Style::gradient_green);
  }

  glyphs.SetString(std::to_string(currHP));
  glyphs.setOrigin(glyphs.GetLocalBounds().width, 0.f);
}

void PlayerHealthUI::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  //auto this_states = states;
  //this_states.transform *= getTransform();

  target.draw(this->uibox, states);
  target.draw(glyphs, states);
}

////////////////////////////////////
// class PlayerHealthUIComponent  //
////////////////////////////////////

PlayerHealthUIComponent::PlayerHealthUIComponent(std::weak_ptr<Player> _player) :
  UIComponent(_player)
{
  isBattleOver = false;
  startHP = _player.lock()->GetHealth();
  ui.SetHP(startHP);
  SetDrawOnUIPass(false);
  OnUpdate(0); // refresh and prepare for the 1st frame
}

PlayerHealthUIComponent::~PlayerHealthUIComponent() {
  this->Eject();
}

void PlayerHealthUIComponent::Inject(BattleSceneBase& scene)
{
  scene.Inject(shared_from_base<PlayerHealthUIComponent>());
  this->scene - &scene;
}

void PlayerHealthUIComponent::draw(sf::RenderTarget& target, sf::RenderStates states) const {
  if (this->IsHidden()) return;

  auto this_states = states;
  this_states.transform *= getTransform();

  ui.draw(target, this_states);

  UIComponent::draw(target, this_states);
}

/*
HP drop is not 1 unit per frame. It is:
10 per frame if difference is 100 or more
~5 per frame if difference is 99-40 range
-3 per frame for anything lower
*/
void PlayerHealthUIComponent::OnUpdate(double elapsed) {
  // if battle is ongoing and valid, play high pitch sound when hp is low
  isBattleOver = Injected() ? (Scene()->IsRedTeamCleared() || Scene()->IsBlueTeamCleared()) : true;

  ui.Update(elapsed);
  if (auto player = GetOwnerAs<Player>()) {
    ui.SetHP(player->GetHealth());

    if (player->WillEraseEOF()) {
      player->FreeComponentByID(GetID());
    }

    bool isBurning = false;
    bool isPoisoned = false;

    // If the player is burning or poisoned, turn red to alert them
    if (player->GetTile() && !(player->HasAirShoe() || player->HasFloatShoe())) {
      isBurning = player->GetTile()->GetState() == TileState::lava;
      isBurning = isBurning && player->GetElement() != Element::fire;
      isBurning = isBurning && !player->HasFloatShoe();
      isPoisoned = player->GetTile()->GetState() == TileState::poison;
    }

    if (isBurning || isPoisoned || player->GetHealth() <= startHP * 0.25) {
      ui.SetFontStyle(Font::Style::gradient_gold);

      // If HP is low, play beep with high priority
      if (player->GetHealth() <= startHP * 0.25 && !isBattleOver && scene && scene->GetSelectedCardsUI().IsHidden()) {
        ResourceHandle().Audio().Play(AudioType::LOW_HP, AudioPriority::high);
      }
    }
  }
}
