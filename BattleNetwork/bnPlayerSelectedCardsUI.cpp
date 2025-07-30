#include <string>
#include <Swoosh/Ease.h>

#include "bnPlayer.h"
#include "bnField.h"
#include "bnPlayerSelectedCardsUI.h"
#include "bnResourceHandle.h"
#include "bnTextureResourceManager.h"
#include "bnInputManager.h"
#include "bnCurrentTime.h"
#include "bnCard.h"
#include "bnCardAction.h"
#include "battlescene/bnBattleSceneBase.h"

using std::to_string;

PlayerSelectedCardsUI::PlayerSelectedCardsUI(std::weak_ptr<Player> _player, CardPackagePartitioner* partition) :
  SelectedCardsUI(_player, partition),
  text(Font::Style::thick),
  dmg(Font::Style::gradient_orange),
  multiplier(Font::Style::thick)
{
  text.setScale(2.f, 2.f);
  dmg.setScale(2.f, 2.f);
  multiplier.setScale(2.f, 2.f);

  interpolTimeFlat = interpolTimeDest = 0;
  interpolDur = sf::seconds(0.2f);

  firstFrame = true; // first time drawn, update positions
  spread = false;
}

PlayerSelectedCardsUI::~PlayerSelectedCardsUI() {
}

void PlayerSelectedCardsUI::draw(sf::RenderTarget& target, sf::RenderStates states) const {
  if (IsHidden()) return;

  const sf::Color orange = sf::Color(225, 140, 0);
  bool canBoost{};

  text.SetString("");
  dmg.SetString("");
  multiplier.SetString("");

  //auto this_states = states;
  //this_states.transform *= getTransform();
  states.transform *= getTransform();

  if (std::shared_ptr<Player> player = GetOwnerAs<Player>()) {
    int cardOrder = 0;

    // i = curr so we only see the cards that are left
    int curr = GetCurrentCardIndex();
    unsigned multiplierValue = GetMultiplier();
    std::vector<Battle::Card>& selectedCards = GetSelectedCards();
    int cardCount = selectedCards.size();
    SpriteProxyNode& icon = IconNode();
    SpriteProxyNode& frame = FrameNode();

    for (int i = curr; i < cardCount; i++) {
      // The first card appears in front
      // But the trick is that we start at i which is a remainder of the whole list
      // We first draw the last card in the list down to i
      int drawOrderIndex = cardCount - i + curr - 1;

      if (spread) {
        // If we spread the cards, this algorithm is simple:
        // x = (i_drawOrderIndex - curr ) * size of card) - spacing
        // y = Place -121 screen pixels above the user
        sf::Vector2f flat = player->getPosition() + sf::Vector2f(((drawOrderIndex - curr) * 32.0f) - 4.f, -(player->GetHeight() * 2.0f) - 20.f);

        // We want to smoothly move from above the head to the spread position
        double alpha = swoosh::ease::linear(interpolTimeFlat, (double)interpolDur.asSeconds(), 1.0);

        // interpolate
        icon.setPosition(((float)alpha * flat) + ((float)(1.0 - alpha) * icon.getPosition()));

        interpolTimeDest = 0;
        interpolTimeFlat += elapsed;
      }
      else {
        // If stacked, the algorithm makes a jagged pattern that goes up and to the left:
        // x = ( ( i - curr ) * spacing ) - spacing
        // y = -121.f - (i - curr) * (-spacing )
        sf::Vector2f dest = player->getPosition() + sf::Vector2f(((i - curr) * 4.0f) - 4.f, -(player->GetHeight() * 2.0f) - 20.f - (i - curr) * -4.0f);

        // We want to smoothly move from the spread position to the stacked position
        double alpha = swoosh::ease::linear(interpolTimeDest, (double)interpolDur.asSeconds(), 1.0);

        if (alpha >= 1.0f) {
          if (firstFrame) {
            firstFrame = false;
          }
        }
        // interpolate
        icon.setPosition(((float)alpha * dest) + ((float)(1.0 - alpha) * icon.getPosition()));

        interpolTimeDest += elapsed;

        interpolTimeFlat = 0;
      }

      if (!firstFrame) {
        // The black border needs to sit 1 pixel outside of the icon
        // 1px * 2 (scale) = 2px
        frame.setPosition(icon.getPosition());
        frame.setPosition(frame.getPosition() - sf::Vector2f(2.f, 2.f));
        target.draw(frame, states);

        // Grab the ID of the card and draw that icon from the spritesheet
        std::shared_ptr<sf::Texture> texture;
        std::string id = selectedCards[drawOrderIndex].GetUUID();
        stx::result_t<PackageAddress> maybe_addr = PackageAddress::FromStr(id);
        bool found = false;

        if (!maybe_addr.is_error() && SelectedCardsUI::partition) {
          PackageAddress addr = maybe_addr.value();

          if (SelectedCardsUI::partition->HasPackage(addr)) {
            CardPackageManager& packages = SelectedCardsUI::partition->GetPartition(addr.namespaceId);
            texture = packages.FindPackageByID(id).GetIconTexture();
            icon.setTexture(texture);
          }
          else {
            icon.setTexture(noIcon);
          }

          target.draw(icon, states);
        }
      }
    }

    if (!player->CanAttack() && player->IsMoving()) return;

    // If we have a valid card, update and draw the data
    if (cardCount > 0 && curr < cardCount) {
      Battle::Card& currCard = selectedCards[curr];

      canBoost = currCard.CanBoost();

      // Text sits at the bottom-left of the screen
      text.SetString(currCard.GetShortName());
      text.setOrigin(0, 0);
      text.setPosition(2.0f, 296.0f);

      // Text sits at the bottom-left of the screen
      int unmodDamage = currCard.GetUnmoddedProps().damage;
      int delta = currCard.GetDamage() - unmodDamage;
      sf::String dmgText = std::to_string(unmodDamage);

      if (delta != 0) {
        dmgText = dmgText + sf::String("+") + sf::String(std::to_string(std::abs(delta)));
      }

      // attacks that normally show no damage will show if the modifer adds damage
      if (delta > 0 || unmodDamage > 0) {
        dmg.SetString(dmgText);
        dmg.setOrigin(0, 0);
        dmg.setPosition((text.GetLocalBounds().width * text.getScale().x) + 10.f, 296.f);
      }

      if (multiplierValue != 1 && unmodDamage != 0) {
        // add "x N" where N is the multiplier
        std::string multStr = "x" + std::to_string(multiplierValue);
        multiplier.SetString(multStr);
        multiplier.setOrigin(0, 0);
        multiplier.setPosition(dmg.getPosition().x + (dmg.GetLocalBounds().width * dmg.getScale().x) + 3.0f, 296.0f);
      }
    }
  }

  // shadow beneath
  sf::Vector2f textPos = text.getPosition();
  text.SetColor(sf::Color::Black);
  text.setPosition(textPos.x + 2.f, textPos.y + 2.f);
  target.draw(text);

  // font on top
  text.setPosition(textPos);
  text.SetColor(sf::Color::White);
  target.draw(text);

  // our number font has shadow baked in
  target.draw(dmg);

  if (canBoost) {
    // shadow
    sf::Vector2f multiPos = multiplier.getPosition();
    multiplier.SetColor(sf::Color::Black);
    multiplier.setPosition(multiPos.x + 2.f, multiPos.y + 2.f);
    target.draw(multiplier);

    // font on top
    multiplier.setPosition(multiPos);
    multiplier.SetColor(sf::Color::White);
    target.draw(multiplier);
  }

  UIComponent::draw(target, states);
};

void PlayerSelectedCardsUI::OnUpdate(double _elapsed) {
  if (Input().Has(InputEvents::held_option)) {
    spread = true;
  }
  else {
    spread = false;
  }

  elapsed = _elapsed;
}

void PlayerSelectedCardsUI::Broadcast(std::shared_ptr<CardAction> action)
{
  std::shared_ptr<Player> player = GetOwnerAs<Player>();

  bool angry = player && player->GetEmotion() == Emotion::angry;
  if (angry && action->GetMetaData().canBoost) {
    player->SetEmotion(Emotion::normal);
  }

  SelectedCardsUI::Broadcast(action);
}