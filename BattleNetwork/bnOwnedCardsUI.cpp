#include "bnOwnedCardsUI.h"

using std::to_string;

OwnedCardsUI::OwnedCardsUI(Character* owner) :
  SelectedCardsUI(owner)
{
}

OwnedCardsUI::~OwnedCardsUI() {
  while (count+1 > 1) {
    delete ownedCards[count-1];
    count--;
  }

  delete[] ownedCards;
}

void OwnedCardsUI::AddCards(const std::vector<Battle::Card>& cards) {
  ownedCards = new Battle::Card * [cards.size()];

  for (auto& c : cards) {
    ownedCards[count] = new Battle::Card(c);
    count++;
  }

  LoadCards(ownedCards, count);
}