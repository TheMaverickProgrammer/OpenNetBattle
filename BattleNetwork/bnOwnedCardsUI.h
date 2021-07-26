/*! \brief Same as SelectedCardsUI but cleans up memory
 */
#pragma once
#include "bnSelectedCardsUI.h"

class Card;
class OwnedCardsUI : public SelectedCardsUI {
public:
  /**
   * \param character Character to attach to
   */
  OwnedCardsUI(Character* owner);

  /**
   * @brief destructor
   */
  virtual ~OwnedCardsUI();

  void AddCards(const std::vector<Battle::Card>& cards);
private:
  Battle::Card** ownedCards{ nullptr }; /*!< list of provided cards. */
  unsigned count{};
};
