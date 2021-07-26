#include "bnWindRackCardAction.h"
#include "bnWind.h"
#include "bnField.h"
#include "bnSwordEffect.h"
#include "bnBasicSword.h"

#define FRAME1 { 1, 0.1666 }
#define FRAME2 { 2, 0.05 }
#define FRAME3 { 3, 0.05 }
#define FRAME4 { 4, 0.5 }

#define FRAMES FRAME1, FRAME2, FRAME3, FRAME4

WindRackCardAction::WindRackCardAction(Character& actor, int damage) :
  damage(damage),
  CardAction(actor, "PLAYER_SWORD") {
  hilt = new SpriteProxyNode();
  hilt->setTexture(actor.getTexture());
  hilt->SetLayer(-1);
  hilt->EnableParentShader(true);

  auto userAnim = GetActor().GetFirstComponent<AnimationComponent>();
  hiltAnim = Animation(userAnim->GetFilePath());
  hiltAnim.Reload();

  hiltAnim.OverrideAnimationFrames("HILT", { { 1, 0.05 }, { 2, 0.05 }, { 3, 0.5 } }, newHilt);
  hiltAnim.SetAnimation(newHilt);

  attachment = new SpriteProxyNode();
  attachment->setTexture(Textures().LoadTextureFromFile("resources/spells/WindRack/attachment.png"));

  attachment->SetLayer(-1);
  attachment->EnableParentShader(false);

  attachmentAnim = Animation("resources/spells/WindRack/attachment.animation");

  attachmentAnim.OverrideAnimationFrames("DEFAULT", { { 1, 0.05 }, { 2, 0.05 }, { 3, 0.05 }, {4, 0.45} }, newDefault);
  attachmentAnim.SetAnimation(newDefault);

  OverrideAnimationFrames({ FRAMES });
}

WindRackCardAction::~WindRackCardAction()
{
  delete attachment;
  delete hilt;
}

void WindRackCardAction::ReplaceRack(SpriteProxyNode* node, const Animation& newAnim)
{
  if (attachment) {
    delete attachment;
    attachment = nullptr;
  }

  if (hilt) {
    delete hilt;
  }

  hilt = node;
  hiltAnim = newAnim;
}

void WindRackCardAction::OnExecute(Character* user)
{
  auto actor = &GetActor();
  auto team = actor->GetTeam();
  auto field = actor->GetField();
  Entity::ID_t userId{ user->GetID() };

  // On throw frame, spawn projectile
  auto onThrow = [=]() -> void {
    auto userAnim = GetActor().GetFirstComponent<AnimationComponent>();
    auto& hiltAttachment = AddAttachment(userAnim->GetAnimationObject(), "HILT", *hilt).UseAnimation(hiltAnim);

    if (attachment) {
      hiltAttachment.AddAttachment(hiltAnim, "ENDPOINT", *attachment).UseAnimation(attachmentAnim);
    }

    Direction direction{ Direction::right };
    int step = 1;

    if (actor->GetTeam() == Team::blue) {
      direction = Direction::left;
      step = -1;
    }

    // tiles
    auto tiles = std::vector{
      actor->GetTile()->Offset(step, 0),
      actor->GetTile()->Offset(step, 1),
      actor->GetTile()->Offset(step,-1)
    };

    // big push effect
    auto wind = new Wind(team);
    wind->Hide();
    wind->SetDirection(direction);
    field->AddEntity(*wind, *tiles[0]);

    wind = new Wind(team);
    wind->Hide();
    wind->SetDirection(direction);
    field->AddEntity(*wind, *tiles[1]);

    wind = new Wind(team);
    wind->Hide();
    wind->SetDirection(direction);
    field->AddEntity(*wind, *tiles[2]);

    // hitboxes

    SwordEffect* e = new SwordEffect;

    if (actor->GetFacing() == Direction::left) {
      e->setScale(-2.f, 2.f);
    }

    e->SetAnimation("WIDE");
    field->AddEntity(*e, *tiles[0]);

    BasicSword* b = new BasicSword(actor->GetTeam(), damage);
    auto props = b->GetHitboxProperties();
    props.aggressor = userId;
    b->SetHitboxProperties(props);

    Audio().Play(AudioType::SWORD_SWING);
    field->AddEntity(*b, *tiles[0]);

    b = new BasicSword(actor->GetTeam(), damage);
    b->SetHitboxProperties(props);
    field->AddEntity(*b, *tiles[1]);

    b = new BasicSword(actor->GetTeam(), damage);
    b->SetHitboxProperties(props);
    field->AddEntity(*b, *tiles[2]);
  };

  AddAnimAction(2, onThrow);
}

void WindRackCardAction::OnActionEnd()
{
}

void WindRackCardAction::OnAnimationEnd()
{
}