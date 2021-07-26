#include "bnTwoMettaurMob.h"
#include "bnField.h"
#include "bnWebClientMananger.h"
#include "bnCardUUIDs.h"
#include "bnFadeInState.h"
#include "bnOwnedCardsUI.h"

namespace {
  static std::vector<Battle::Card> GenCards() {
    auto list = BuiltInCards::AsList;

    int rand_size = (rand() % 4)+1;

    std::vector<Battle::Card> cards;

    while (rand_size > 0) {
      int offset = rand() % list.size();
      cards.push_back(WEBCLIENT.MakeBattleCardFromWebCardData(*(list.begin() + offset)));
      rand_size--;
    }

    return cards;
  }
}

TwoMettaurMob::TwoMettaurMob(Field* field) : MobFactory(field)
{
}


TwoMettaurMob::~TwoMettaurMob()
{
}

Mob* TwoMettaurMob::Build() {
  // Construct a new mob 
  Mob* mob = new Mob(field);

  // Assign rewards based on rank
  mob->RegisterRankedReward(1, BattleItem(WEBCLIENT.MakeBattleCardFromWebCardData(BuiltInCards::Reflect_A)));
  mob->RegisterRankedReward(6, BattleItem(WEBCLIENT.MakeBattleCardFromWebCardData(BuiltInCards::Cannon_A)));

  //mob->RegisterRankedReward(1, BattleItem(Battle::Card(72, 99, 'C', 60, Element::none, "Rflctr1", "Defends and reflects", "Press A to bring up a shield that protects you and reflects damage.", 2)));
  //mob->RegisterRankedReward(1, BattleItem(Battle::Card(72, 99, 'B', 60, Element::none, "Rflctr1", "Defends and reflects", "Press A to bring up a shield that protects you and reflects damage.", 2)));
  //mob->RegisterRankedReward(3, BattleItem(Battle::Card(72, 99, 'A', 60, Element::none, "Rflctr1", "Defends and reflects", "Press A to bring up a shield that protects you and reflects damage.", 2)));
  //mob->RegisterRankedReward(1, BattleItem(Battle::Card(83, 158, 'K', 0, Element::none, "CrckPanel", "Cracks a panel", "Cracks the tiles in the column immediately in front", 2)));

  int count = 3;

  // place a hole somewhere
  field->GetAt( 4 + (rand() % 3), 1 + (rand() % 3))->SetState(TileState::volcano);
  field->GetAt(1, 1)->SetState(TileState::volcano);

  while (count > 0) {
    for (int i = 0; i < field->GetWidth(); i++) {
      for (int j = 0; j < field->GetHeight(); j++) {
        Battle::Tile* tile = field->GetAt(i + 1, j + 1);

        if (tile->IsWalkable() && !tile->IsReservedByCharacter() && tile->GetTeam() == Team::blue) {
          if (rand() % 50 > 25 && count-- > 0) {
            auto spawner = mob->CreateSpawner<Mettaur>(Mettaur::Rank::Rare2);
            Mob::Mutator* mutator = spawner.SpawnAt<FadeInState>(i + 1, j + 1);

            // randomly spawn met with random card list
            // provided from our available built-in card list
            if (rand() % 50 > 25) {
              mutator->Mutate([mob](Character& in) {
                OwnedCardsUI* ui = in.CreateComponent<OwnedCardsUI>(&in);
                ui->AddCards(::GenCards());
                ui->setPosition(0, -in.GetHeight());
                });
            }
          }
        }
      }
    }
  }

  return mob;
}
