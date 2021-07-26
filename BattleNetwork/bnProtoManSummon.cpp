#include <random>
#include <time.h>
#include <algorithm>

#include "bnProtoManSummon.h"
#include "bnCharacter.h"
#include "bnObstacle.h"
#include "bnTile.h"
#include "bnField.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnBasicSword.h"
#include "bnSwordEffect.h"

#define RESOURCE_PATH "resources/spells/protoman_summon.animation"

ProtoManSummon::ProtoManSummon(Character* user, int damage) : 
  damage(damage),
  user(user),
  Artifact()
{
  int lr = (user->GetTeam() == Team::red) ? 1 : -1;
  SetTeam(user->GetTeam());
  setScale(2.0f*lr, 2.0f);

  Audio().Play(AudioType::APPEAR);

  setTexture(Textures().LoadTextureFromFile("resources/spells/protoman_summon.png"), true);

  animationComponent = CreateComponent<AnimationComponent>(this);
  animationComponent->SetPath(RESOURCE_PATH);
  animationComponent->Load();

  // TODO: noodely callbacks desgin might be best abstracted by ActionLists (use CardAction::Step) 2/27/2021
  animationComponent->SetAnimation("APPEAR", [this] {
  auto handleAttack = [this] () {
      DoAttackStep();
    };
    animationComponent->SetAnimation("MOVE", handleAttack);
  });
}

ProtoManSummon::~ProtoManSummon() {
}

void ProtoManSummon::DoAttackStep() {
  if (targets.size() > 0) {
    int step = -1;

    if (GetTeam() == Team::blue) {
      step = 1;
    }

    Battle::Tile* prev = field->GetAt(targets[0]->GetX() + step, targets[0]->GetY());
    GetTile()->RemoveEntityByID(GetID());
    prev->AddEntity(*this);

    animationComponent->SetAnimation("ATTACK", [this, prev] {
      animationComponent->SetAnimation("MOVE", [this, prev] {
        DoAttackStep();
      });
    });

    animationComponent->AddCallback(4, [this]() {
      this->DropHitboxes(*(*targets.begin()));
      targets.erase(targets.begin());
    }, true);
  }
  else {
    animationComponent->SetAnimation("MOVE", [this] {
      Delete();
      user->Reveal();
    });
  }
}

void ProtoManSummon::OnDelete()
{
  Remove();
}

void ProtoManSummon::OnSpawn(Battle::Tile& start)
{
  Battle::Tile* next = nullptr;

  auto allTiles = field->FindTiles([](Battle::Tile* tile) { return true; });
  auto iter = allTiles.begin();

  while (iter != allTiles.end()) {
    next = (*iter);

    if (next->ContainsEntityType<Character>() && !next->ContainsEntityType<Obstacle>() && next->GetTeam() != GetTeam()) {
      int step = -1;

      if (GetTeam() == Team::blue) {
        step = 1;
      }

      Battle::Tile* prev = field->GetAt(next->GetX() + step, next->GetY());

      auto characters = prev->FindCharacters([this](Character* in) { return in->GetTeam() != Team::unknown; });

      bool blocked = (characters.size() > 0) || !prev->IsWalkable();

      if (!blocked) {
        targets.push_back(next);
      }
    }

    iter++;
  }
}

void ProtoManSummon::DropHitboxes(Battle::Tile& tile)
{
  auto field = GetField();

  SwordEffect* e = new SwordEffect;

  if (GetTeam() == Team::blue) {
    e->setScale(-2.f, 2.f);
  }

  e->SetAnimation("WIDE");
  field->AddEntity(*e, tile.GetX(), tile.GetY());

  auto hitboxProps = Hit::DefaultProperties;
  hitboxProps.damage = damage;
  hitboxProps.flags |= Hit::flash;
  hitboxProps.aggressor = user->GetID();

  BasicSword* b;
  
  b = new BasicSword(GetTeam(), 0);
  b->SetHitboxProperties(hitboxProps);
  field->AddEntity(*b, tile.GetX(), tile.GetY());

  b = new BasicSword(GetTeam(), 0);
  b->SetHitboxProperties(hitboxProps);
  field->AddEntity(*b, tile.GetX(), tile.GetY() + 1);

  b = new BasicSword(GetTeam(), 0);
  b->SetHitboxProperties(hitboxProps);
  field->AddEntity(*b, tile.GetX(), tile.GetY() - 1);

  Audio().Play(AudioType::SWORD_SWING);
}

void ProtoManSummon::OnUpdate(double _elapsed) {
  if (tile != nullptr) {
    setPosition(tile->getPosition());
  }
}