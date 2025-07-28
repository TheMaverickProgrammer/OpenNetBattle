#include "bnStuntDouble.h"
#include "bnAnimationComponent.h"

StuntDouble::StuntDouble(std::shared_ptr<Character> ref) : ref(ref)
{
  // Copy attributes & stats
  setTexture(ref->getTexture());
  setScale(ref->getScale());
  SetTeam(ref->GetTeam());
  SetFacing(ref->GetFacing());
  SetHealth(ref->GetHealth());
  defaultColor = ref->getColor();

  // copy nodes
  for (auto node : ref->GetChildNodes()) {
    AddNode(node);
  }
}

void StuntDouble::Init() {
  Character::Init();

  // add common components and copy
  std::shared_ptr<AnimationComponent> anim = CreateComponent<AnimationComponent>(weak_from_this());

  // palette swap if applicable
  if (std::shared_ptr<sf::Texture> palette = ref->GetPalette()) {
    SetPalette(palette);
  }

  // animation component if applicable
  if (std::shared_ptr<AnimationComponent> otherAnim = ref->GetFirstComponent<AnimationComponent>()) {
    anim->CopyFrom(*otherAnim);

    // sync items in the animation if applicable
    for (AnimationComponent::SyncItem& item : otherAnim->GetSyncItems()) {
      anim->AddToSyncList(item);
    }

    // refresh anim and sprite
    anim->SetAnimation(otherAnim->GetAnimationString());
    anim->Refresh();
  }
}

StuntDouble::~StuntDouble()
{

}

void StuntDouble::OnDelete()
{
  Hide();
  ref->Delete();
}

void StuntDouble::OnUpdate(double elapsed)
{
  setColor(ref->getColor());
}

bool StuntDouble::CanMoveTo(Battle::Tile*)
{
  return true;
}
