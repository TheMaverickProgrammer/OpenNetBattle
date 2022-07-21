#include <random>
#include <time.h>

#include "bnBuster.h"
#include "bnBusterHit.h"
#include "bnTile.h"
#include "bnField.h"
#include "bnPlayer.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnRandom.h"

Buster::Buster(Team _team, bool _charged, int damage) : isCharged(_charged), Spell(_team) {
  EnableHitbox(false);
  SetLayer(-100);

  cooldown = frames(2);

  progress = 0.0f;

  if (isCharged) {
    hitHeight = 20.0f;
  }
  else {
    hitHeight = 0.0f;
  }

  random = 0;

  setScale(2.f, 2.f);

  Audio().Play(AudioType::BUSTER_PEA, AudioPriority::high);

  auto props = Hit::DefaultProperties;
  props.flags = props.flags & ~(Hit::flinch | Hit::flash);

  props.damage = damage;
  SetHitboxProperties(props);

  contact = nullptr;
  spawnGuard = false;
}

void Buster::Init()
{
  Spell::Init();
  animationComponent = CreateComponent<AnimationComponent>(weak_from_this());

  if (isCharged) {
    texture = Textures().LoadFromFile(TexturePaths::SPELL_CHARGED_BULLET_HIT);
    animationComponent->SetPath("resources/scenes/battle/spells/spell_charged_bullet_hit.animation");
    animationComponent->Reload();
    animationComponent->SetAnimation("HIT");
  } else {
    texture = Textures().LoadFromFile(TexturePaths::SPELL_BULLET_HIT);
    animationComponent->SetPath("resources/scenes/battle/spells/spell_bullet_hit.animation");
    animationComponent->Reload();
    animationComponent->SetAnimation("HIT");
  }
}

Buster::~Buster() {
}

void Buster::OnUpdate(double _elapsed) {
  GetTile()->AffectEntities(*this);

  cooldown -= from_seconds(_elapsed);
  if (cooldown <= frames(0)) {
    if (Teleport(GetTile() + GetMoveDirection())) {
      cooldown = frames(2);
    }
    else {
      Delete();
    }
  }

}

bool Buster::CanMoveTo(Battle::Tile * next)
{
  return true;
}

void Buster::OnDelete()
{
  Erase();
}

void Buster::OnCollision(const std::shared_ptr<Entity> entity)
{
  random = entity->getLocalBounds().width / 2.0f;
  hitHeight = std::floor(entity->GetHeight());

  if (!isCharged) {
    random *= SyncedRandBelow(2) == 0 ? -1.0f : 1.0f;

    if (hitHeight > 0) {
      hitHeight = (float)(SyncedRandBelow(hitHeight));
    }
  }
  else {
    random = 0;
    hitHeight /= 2;
  }

  auto bhit = std::make_shared<BusterHit>(isCharged ? BusterHit::Type::CHARGED : BusterHit::Type::PEA);
  bhit->SetOffset({ random, -(GetHeight() + hitHeight) });
  GetField()->AddEntity(bhit, *GetTile());

  Delete();
  Audio().Play(AudioType::HURT);
}

void Buster::Attack(std::shared_ptr<Entity> _entity) {
  _entity->Hit(GetHitboxProperties());
}
