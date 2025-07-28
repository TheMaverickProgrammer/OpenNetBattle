#include "bnBusterCardAction.h"
#include "bnCardAction.h"
#include "bnSpriteProxyNode.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnBuster.h"
#include "bnPlayer.h"
#include "bnField.h"

#define NODE_PATH "resources/scenes/battle/spells/buster_shoot.png"
#define NODE_ANIM "resources/scenes/battle/spells/buster_shoot.animation"

BusterCardAction::BusterCardAction(std::weak_ptr<Character> actorWeak, bool charged, int damage) : CardAction(actorWeak, "PLAYER_SHOOTING")
{
  BusterCardAction::damage = damage;
  BusterCardAction::charged = charged;

  std::shared_ptr<Character> actor = actorWeak.lock();

  busterAttachment = &AddAttachment("buster");

  Animation& busterAnim = busterAttachment->GetAnimationObject();
  busterAnim.Load(actor->GetFirstComponent<AnimationComponent>()->GetFilePath());
  busterAnim.SetAnimation("BUSTER");

  buster = busterAttachment->GetSpriteNode();
  buster->setTexture(actor->getTexture());
  buster->SetLayer(-1);
  buster->EnableParentShader(true);
  buster->AddTags({Player::BASE_NODE_TAG});

  busterAnim.Refresh(buster->getSprite());

  SetLockout({ CardAction::LockoutType::async, 0.5 });
}

void BusterCardAction::OnExecute(std::shared_ptr<Character> user) {
  buster->setColor(user->getColor());

  // On shoot frame, drop projectile
  auto onFire = [this, user]() -> void {
    Team team = user->GetTeam();
    std::shared_ptr<Buster> b = std::make_shared<Buster>(team, charged, damage);
    std::shared_ptr<Field> field = user->GetField();

    b->SetMoveDirection(user->GetFacing());

    auto busterRemoved = [this](auto target) {
      EndAction();
    };

    notifier = field->CallbackOnDelete(b->GetID(), busterRemoved);

    field->AddEntity(b, *user->GetTile());
    Audio().Play(AudioType::BUSTER_PEA);

    CardAction::Attachment& attachment = busterAttachment->AddAttachment("endpoint");

    std::shared_ptr<SpriteProxyNode> flare = attachment.GetSpriteNode();
    flare->setTexture(Textures().LoadFromFile(NODE_PATH));
    flare->SetLayer(-1);

    Animation& flareAnim = attachment.GetAnimationObject();
    flareAnim = Animation(NODE_ANIM);
    flareAnim.SetAnimation("DEFAULT");
  };

  AddAnimAction(2, onFire);
}

void BusterCardAction::Update(double _elapsed)
{
  CardAction::Update(_elapsed);
}

void BusterCardAction::OnActionEnd()
{
  std::shared_ptr<Character> actor = GetActor();
  if (!actor) return;

  std::shared_ptr<Field> field = actor->GetField();

  if (field) {
    field->DropNotifier(notifier);
  }
}

void BusterCardAction::OnAnimationEnd()
{
}