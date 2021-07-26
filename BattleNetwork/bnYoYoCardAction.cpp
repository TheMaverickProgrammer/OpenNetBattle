#include "bnYoYoCardAction.h"
#include "bnCardAction.h"
#include "bnSpriteProxyNode.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnYoYo.h"
#include "bnField.h"

#define NODE_PATH "resources/spells/buster_yoyo.png"
#define NODE_ANIM "resources/spells/buster_yoyo.animation"

#define FRAME1 { 1, 0.05 }
#define FRAME3 { 1, 5.0 }

#define FRAMES FRAME1, FRAME3

YoYoCardAction::YoYoCardAction(Character& actor, int damage) :
  attachmentAnim(NODE_ANIM), yoyo(nullptr),
  CardAction(actor, "PLAYER_SHOOTING") {
  YoYoCardAction::damage = damage;

  attachment = new SpriteProxyNode();
  attachment->setTexture(Textures().LoadTextureFromFile(NODE_PATH));
  attachment->SetLayer(-1);

  attachmentAnim = Animation(NODE_ANIM);
  attachmentAnim.SetAnimation("DEFAULT");

  // add override anims
  OverrideAnimationFrames({ FRAMES });

  AddAttachment(actor, "buster", *attachment).UseAnimation(attachmentAnim);

}

YoYoCardAction::~YoYoCardAction()
{
}

void YoYoCardAction::OnExecute(Character* user) {
  // On shoot frame, drop projectile
  auto onFire = [=]() -> void {
    auto actor = &GetActor();

    Audio().Play(AudioType::TOSS_ITEM_LITE);

    Team team = actor->GetTeam();
    YoYo* y = new YoYo(team, damage);

    y->SetDirection(team == Team::red? Direction::right : Direction::left);
    auto props = y->GetHitboxProperties();
    props.aggressor = user->GetID();
    y->SetHitboxProperties(props);
    yoyo = y;

    int step = 1;

    if (user->GetFacing() == Direction::left) {
      step = -1;
    }

    auto tile = actor->GetTile()->Offset(step, 0);

    if (tile) {
      actor->GetField()->AddEntity(*y, *tile);
    }
  };

  AddAnimAction(1, onFire);
}

void YoYoCardAction::Update(double _elapsed)
{
  CardAction::Update(_elapsed);

  if (yoyo && yoyo->WillRemoveLater()) {
    yoyo = nullptr;

    //GetActor().GetFirstComponent<AnimationComponent>()->SetAnimation("PLAYER_IDLE");
    EndAction();
  }
}

void YoYoCardAction::OnAnimationEnd()
{
}

void YoYoCardAction::OnActionEnd()
{
  if (yoyo) {
    yoyo->Delete();
  }
}
