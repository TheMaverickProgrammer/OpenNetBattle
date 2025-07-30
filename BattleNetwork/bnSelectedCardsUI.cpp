#include <string>
#include <memory>
#include <Swoosh/Ease.h>

#include "bnField.h"
#include "bnSelectedCardsUI.h"
#include "bnResourceHandle.h"
#include "bnTextureResourceManager.h"
#include "bnInputManager.h"
#include "bnCurrentTime.h"
#include "bnCard.h"
#include "bnCardAction.h"
#include "bnCardToActions.h"
#include "bnCardPackageManager.h"
#include "battlescene/bnBattleSceneBase.h"

using std::to_string;

SelectedCardsUI::SelectedCardsUI(std::weak_ptr<Character> owner, CardPackagePartitioner* partition) :
  partition(partition),
  CardActionUsePublisher(), 
  UIComponent(owner)
{
  ResourceHandle handle;

  curr = 0;
  sf::IntRect iconRect = sf::IntRect(0, 0, 14, 14);
  icon.setTextureRect(iconRect);
  icon.setScale(sf::Vector2f(2.f, 2.f));

  frame.setTexture(handle.Textures().LoadFromFile(TexturePaths::CHIP_FRAME));
  frame.setScale(sf::Vector2f(2.f, 2.f));

  firstFrame = true; // first time drawn, update positions

  // temp until chips are loaded
  selectedCards = std::make_shared<std::vector<Battle::Card>>();

  // used when card is missing data
  noIcon = handle.Textures().LoadFromFile(TexturePaths::CHIP_ICON_MISSINGDATA);
}

SelectedCardsUI::~SelectedCardsUI() {
}

void SelectedCardsUI::draw(sf::RenderTarget & target, sf::RenderStates states) const {
  if (IsHidden()) return;

  const std::shared_ptr<Entity> owner = GetOwner();

  //auto this_states = states;
  //this_states.transform *= getTransform();
  states.transform *= getTransform();

  int cardOrder = 0;

  // i = curr so we only see the cards that are left
  for (int i = curr; i < selectedCards->size(); i++) {
    // The first card appears in front
    // But the trick is that we start at i which is a remainder of the whole list
    // We first draw the last card in the list down to i
    int drawOrderIndex = selectedCards->size() - i + curr - 1;

    // If stacked, the algorithm makes a jagged pattern that goes up and to the left:
    // x = ( ( i - curr ) * spacing ) - spacing
    // y = -121.f - (i - curr) * (-spacing )
    sf::Vector2f dest = owner->getPosition() + sf::Vector2f(((i - curr) * 4.0f) - 4.f, -(owner->GetHeight()*2.0f) - 20.f - (i - curr) * -4.0f);

    // interpolate
    icon.setPosition(dest);

    // The black border needs to sit 1 pixel outside of the icon
    // 1px * 2 (scale) = 2px
    frame.setPosition(icon.getPosition());
    frame.setPosition(frame.getPosition() - sf::Vector2f(2.f, 2.f));
    target.draw(frame, states);

    // Grab the ID of the card and draw that icon from the spritesheet
    std::shared_ptr<sf::Texture> texture;
    std::string id = (*selectedCards)[drawOrderIndex].GetUUID();

    bool found = false;
    stx::result_t<PackageAddress> maybe_addr = PackageAddress::FromStr(id);

    if (!maybe_addr.is_error()) {
      PackageAddress addr = maybe_addr.value();

      CardPackageManager& packageManager = partition->GetPartition(addr.namespaceId);
      if (packageManager.HasPackage(addr.packageId)) {
        texture = packageManager.FindPackageByID(addr.packageId).GetIconTexture();
        icon.setTexture(texture);
      }
    }

    if (!found) {
      icon.setTexture(noIcon);
    }

    target.draw(icon, states);
  }

  UIComponent::draw(target, states);
};

void SelectedCardsUI::OnUpdate(double _elapsed) {
  elapsed = _elapsed;

  if (std::shared_ptr<Character> character = GetOwnerAs<Character>()) {
    if (character->IsDeleted()) {
      Hide();
    }
  }
}

void SelectedCardsUI::LoadCards(std::vector<Battle::Card> incoming) {
  *selectedCards = incoming;
  curr = 0;
}

bool SelectedCardsUI::UseNextCard() {
  std::shared_ptr<Character> owner = GetOwnerAs<Character>();

  if (!owner) return false;

  const std::vector<std::shared_ptr<CardAction>> actions = owner->AsyncActionList();
  bool hasNextCard = curr < selectedCards->size();
  bool canUseCard = true;

  // We could be using an ability, just make sure one of these actions are not from a card
  // Cards cannot be used if another card is still active
  for (std::shared_ptr<CardAction> action : actions) {
    canUseCard = canUseCard && (action->GetLockoutGroup() != CardAction::LockoutGroup::card);
  }

  canUseCard = canUseCard && hasNextCard;

  if (!canUseCard) {
    return false;
  }

  Battle::Card& card = (*selectedCards)[curr];

  if (card.CanBoost()) {
    card.MultiplyDamage(multiplierValue);
    multiplierValue = 1; // multiplier is reset because it has been consumed 
  }

  // add a peek event to the action queue
  owner->AddAction(PeekCardEvent{ this }, ActionOrder::voluntary);
  return true;
}

void SelectedCardsUI::Broadcast(std::shared_ptr<CardAction> action)
{
  DropNextCard();
  CardActionUsePublisher::Broadcast(action, CurrentTime::AsMilli());
}

std::optional<std::reference_wrapper<const Battle::Card>> SelectedCardsUI::Peek()
{
  if (curr < selectedCards->size()) {
    using RefType = std::reference_wrapper<const Battle::Card>;
    return std::optional<RefType>(std::ref((*selectedCards)[curr]));
  }

  return {};
}

bool SelectedCardsUI::HandlePlayEvent(std::shared_ptr<Character> from)
{
  auto maybe_card = Peek();

  if (maybe_card.has_value()) {
    // convert meta data into a useable action object
    const Battle::Card& card = *maybe_card;

    // could act on metadata later:
    // from->OnCard(card)

    if (std::shared_ptr<CardAction> action = CardToAction(card, from, partition, card.props)) {
      Broadcast(action); // tell the rest of the subsystems
    }

    return true;
  }

  return false;
}

void SelectedCardsUI::DropNextCard()
{
  curr++;
}

std::vector<Battle::Card> SelectedCardsUI::GetRemainingCards()
{
  std::vector<Battle::Card> res;

  for (int i = curr; i < selectedCards->size(); i++) {
    res.push_back((*selectedCards)[i]);
  }

  return res;
}

const int SelectedCardsUI::GetCurrentCardIndex() const
{
  return curr;
}

const unsigned SelectedCardsUI::GetMultiplier() const
{
  return multiplierValue;
}

std::vector<Battle::Card>& SelectedCardsUI::GetSelectedCards() const
{
  return *selectedCards;
}

SpriteProxyNode& SelectedCardsUI::IconNode() const
{
  return icon;
}

SpriteProxyNode& SelectedCardsUI::FrameNode() const
{
  return frame;
}

void SelectedCardsUI::Inject(BattleSceneBase& scene) {
  scene.Inject(shared_from_base<SelectedCardsUI>());
}

void SelectedCardsUI::SetMultiplier(unsigned mult)
{
  multiplierValue = mult;
}
