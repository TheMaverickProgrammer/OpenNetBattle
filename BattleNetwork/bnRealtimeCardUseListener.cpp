#include "bnRealtimeCardUseListener.h"
#include "bnCardToActions.h"

void RealtimeCardUseListener::OnCardUse(const Battle::Card& card, Character& character, long long timestamp) {
  if (card.IsTimeFreeze() == false) {
    if (auto* nextAction = CardToAction(card, character)) {
      character.AddAction(CardEvent{ std::shared_ptr<CardAction>(nextAction) }, ActionOrder::voluntary);
    }
  }
}