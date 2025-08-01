#include "bnRealtimeCardUseListener.h"
#include "bnCardAction.h"
#include "bnCharacter.h"

void RealtimeCardActionUseListener::OnCardActionUsed(std::shared_ptr<CardAction> action, uint64_t timestamp) {
  if (action->GetMetaData().GetProps().timeFreeze == false) {
    action->GetActor()->AddAction(CardEvent{ action }, ActionOrder::voluntary);
  }
}