#include "bnProgsManBossFight.h"
#include "bnProgsMan.h"
#include "bnBattleItem.h"
#include "bnStringEncoder.h"
#include "bnCard.h"
#include "bnTile.h"
#include "bnField.h"
#include "bnFadeInState.h"

ProgsManBossFight::ProgsManBossFight(Field* field) : MobFactory(field)
{
}


ProgsManBossFight::~ProgsManBossFight()
{
}

Mob* ProgsManBossFight::Build() {
  Mob* mob = new Mob(field);
  
  // Changes music and ranking algorithm
  mob->ToggleBossFlag();

  ////mob->RegisterRankedReward(1, BattleItem(Battle::Card(135, 232, 'P', 300, Element::breaker, "ProgsMan", "Throws ProgBomb", "Throws a projectile at the nearest enemy.", 5)));
  ////mob->RegisterRankedReward(11, BattleItem(Battle::Card(136, 232, 'P', 300, Element::breaker, EX("ProgsMan"), "Throws ProgBomb", "Throws 3 projectiles at the nearest enemy in successsion.", 5)));

  //mob->RegisterRankedReward(1, BattleItem(Battle::Card(100, 139, 'Y', 0, Element::none, "YoYo", "", "", 0)));
  //mob->RegisterRankedReward(4, BattleItem(Battle::Card(100, 139, '*', 0, Element::none, "YoYo", "", "", 0)));

  int x = 5;
  int y = (field->GetHeight() / 2) + 1;

  auto spawner = mob->CreateSpawner<ProgsMan>(ProgsMan::Rank::EX);
  spawner.SpawnAt<FadeInState>(5, 3);

  Battle::Tile* tile = field->GetAt(x, y);
  if (!tile->IsWalkable()) { tile->SetState(TileState::normal); }


  return mob;
}
