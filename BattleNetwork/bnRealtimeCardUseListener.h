/*! \brief Describes what happens when a card is used by any Character */

#pragma once

#include "bnAudioResourceManager.h"
#include "bnCardUseListener.h"
#include "bnCard.h"

class RealtimeCardUseListener : public CardUseListener {
public:
  RealtimeCardUseListener() : CardUseListener() {}
  
  /**
   * @brief What happens when a card is used
   * @param card Card used
   * @param character Character using card
   */
  void OnCardUse(const Battle::Card& card, Character& character, long long timestamp);
};
