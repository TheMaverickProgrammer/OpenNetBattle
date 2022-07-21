#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnInputManager.h"
#include "bnField.h"
#include "bnDefenseBubbleStatus.h"
#include "bnBubbleTrap.h"

using sf::IntRect;

#define RESOURCE_PATH "resources/spells/bubble_trap.animation"

BubbleTrap::BubbleTrap(std::weak_ptr<Entity> owner) : 
  willDelete(false), defense(nullptr), duration(3), 
  ResourceHandle(), InputHandle(),
  SpriteProxyNode(), Component(owner)
{
  SetLayer(1);
  setTexture(Textures().LoadFromFile(TexturePaths::SPELL_BUBBLE_TRAP));
  bubble = getSprite();

  //Components setup and load
  animation = Animation(RESOURCE_PATH);
  animation.Reload();

  animation << "DEFAULT" << Animator::Mode::Loop;

  animation.Update(0, getSprite());
}

void BubbleTrap::Inject(BattleSceneBase& bs) {
}

void BubbleTrap::OnUpdate(double _elapsed) {
  auto asCharacter = GetOwnerAs<Character>();

  if (!asCharacter || asCharacter->IsDeleted()) {
    this->Eject();
  }

  if (!init) {
    // Bubbles have to pop when hit
    defense = std::make_shared<DefenseBubbleStatus>();
    asCharacter->AddDefenseRule(defense);
    asCharacter->AddNode(shared_from_base<BubbleTrap>());

    init = true;
  }

  auto keyTestThunk = [this](const InputEvent& key) {
    bool pass = false;

    if (Input().Has(key)) {
      auto iter = std::find(lastFrameStates.begin(), lastFrameStates.end(), key);

      if (iter == lastFrameStates.end()) {
        lastFrameStates.clear();
        pass = true;
      } 

      lastFrameStates.push_back(key);
    }

    return pass;
  };

  bool anyKey = keyTestThunk(InputEvents::pressed_use_chip);
  anyKey = anyKey || keyTestThunk(InputEvents::pressed_cust_menu);
  anyKey = anyKey || keyTestThunk(InputEvents::pressed_move_down);
  anyKey = anyKey || keyTestThunk(InputEvents::pressed_move_up);
  anyKey = anyKey || keyTestThunk(InputEvents::pressed_move_left);
  anyKey = anyKey || keyTestThunk(InputEvents::pressed_move_right);
  anyKey = anyKey || keyTestThunk(InputEvents::pressed_shoot);
  anyKey = anyKey || keyTestThunk(InputEvents::pressed_pause);
  anyKey = anyKey || keyTestThunk(InputEvents::pressed_special);

  if (anyKey) {
    duration -= seconds_cast<double>(frames(1));
  }

  duration -= _elapsed;

  // either the timer runs out or the defense was popped by an attack
  bool shouldpop = duration <= 0 && animation.GetAnimationString() != "POP";
  shouldpop = shouldpop || (defense->IsPopped() && animation.GetAnimationString() != "POP");

  if (shouldpop) {
    Pop();
  }

  auto y = -asCharacter->GetHeight() / 4.0f;
  setPosition(0.f, y);

  animation.Update(_elapsed, getSprite());
}

void BubbleTrap::Pop()
{
  auto onFinish = [this]() {
    if (auto character = GetOwnerAs<Character>()) {
      character->RemoveNode(this);
      character->RemoveDefenseRule(defense);
      this->Eject();
    }
    willDelete = true;
  };

  animation << "POP" << onFinish;
}

const double BubbleTrap::GetDuration() const
{
  return duration;
}
