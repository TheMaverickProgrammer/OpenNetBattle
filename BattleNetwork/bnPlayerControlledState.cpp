#include "bnPlayerControlledState.h"
#include "bnInputManager.h"
#include "bnPlayer.h"
#include "bnCardAction.h"
#include "bnTile.h"
#include "bnSelectedCardsUI.h"
#include "bnAudioResourceManager.h"
#include "netplay/bnPlayerInputReplicator.h"

#include <iostream>

PlayerControlledState::PlayerControlledState() : 
  AIState<Player>(), 
  InputHandle(), 
  replicator(nullptr),
  isChargeHeld(false)
{
}


PlayerControlledState::~PlayerControlledState()
{
}

void PlayerControlledState::OnEnter(Player& player) {
  player.MakeActionable();
  replicator = player.GetFirstComponent<PlayerInputReplicator>();
}

void PlayerControlledState::OnUpdate(double _elapsed, Player& player) {
  if (replicator) {
    currLag = replicator->GetAvgLatency(); // NOTE: in milliseconds
  }

  // Drop inputs less than or equal to 0 frames left
  InputQueueCleanup();

  // For all inputs in the queue, reduce their wait time for this new frame
  for (auto& item : inputQueue) {
    item.wait -= from_seconds(_elapsed);
  }

  // For all new input events, set the wait time based on the network latency and append
  const auto events_this_frame = Input().StateThisFrame();

  std::vector<InputEvent> outEvents;
  for (auto& [name, state] : events_this_frame) {
    InputEvent copy;
    copy.name = name;
    copy.state = state;

    outEvents.push_back(copy);

    copy.wait = from_seconds(currLag / 1000.0); // note: this is dumb. casting to seconds just to cast back to milli internally...
    inputQueue.push_back(copy);
  }

  if (replicator) {
    replicator->SendInputEvents(outEvents);
  }

  // Actions with animation lockout controls take priority over movement
  bool lockout = player.IsLockoutAnimationComplete();
  bool actionable = player.IsActionable();

  // One of our ongoing animations is preventing us from charging
  if (!lockout) {
    player.chargeEffect.SetCharging(false);
    isChargeHeld = false;
    return;
  }

  bool missChargeKey = isChargeHeld && !InputQueueHas(InputEvents::held_shoot);

  // Are we creating an action this frame?
  if (InputQueueHas(InputEvents::pressed_use_chip)) {
    auto cardsUI = player.GetFirstComponent<PlayerSelectedCardsUI>();
    if (cardsUI && player.CanAttack()) {
      cardsUI->UseNextCard();
      player.chargeEffect.SetCharging(false);
      isChargeHeld = false;
    }
    // If the card used was successful, we may have a card in queue
  }
  else if (InputQueueHas(InputEvents::released_special)) {
    const auto actions = player.AsyncActionList();
    bool canUseSpecial = player.CanAttack();

    // Just make sure one of these actions are not from an ability
    for (auto action : actions) {
      canUseSpecial = canUseSpecial && action->GetLockoutGroup() != CardAction::LockoutGroup::ability;
    }

    if (canUseSpecial) {
      player.UseSpecial();
    }
  } // queue attack based on input behavior (buster or charge?)
  else if (InputQueueHas(InputEvents::released_shoot) || missChargeKey) {
    // This routine is responsible for determining the outcome of the attack
    isChargeHeld = false;
    player.chargeEffect.SetCharging(false);
    player.Attack();

  } else if (InputQueueHas(InputEvents::held_shoot)) {
    isChargeHeld = true;
    player.chargeEffect.SetCharging(true);
  }

  // Movement increments are restricted based on anim speed at this time
  if (player.IsMoving()) {
    if (replicator) {
      auto tile = player.GetTile();
    }
    return;
  }

  Direction direction = Direction::none;
  if (InputQueueHas(InputEvents::pressed_move_up) || InputQueueHas(InputEvents::held_move_up)) {
    direction = Direction::up;
  }
  else if (InputQueueHas(InputEvents::pressed_move_left) || InputQueueHas(InputEvents::held_move_left)) {
    direction = Direction::left;
  }
  else if (InputQueueHas(InputEvents::pressed_move_down) || InputQueueHas(InputEvents::held_move_down)) {
    direction = Direction::down;
  }
  else if (InputQueueHas(InputEvents::pressed_move_right) || InputQueueHas(InputEvents::held_move_right)) {
    direction = Direction::right;
  }

  if(direction != Direction::none && actionable) {
    auto next_tile = player.GetTile() + direction;
    auto anim = player.GetFirstComponent<AnimationComponent>();

    auto onMoveBegin = [player = &player, next_tile, this, anim] {
      const std::string& move_anim = player->GetMoveAnimHash();

      anim->CancelCallbacks();

      auto idle_callback = [player]() {
        player->MakeActionable();
      };

      anim->SetAnimation(move_anim, [idle_callback] {
        idle_callback();
      });

      anim->SetInterruptCallback(idle_callback);
    };

    if (player.playerControllerSlide) {
      player.Slide(next_tile, player.slideFrames, frames(0), ActionOrder::voluntary);
    }
    else {
      player.Teleport(next_tile, ActionOrder::voluntary, onMoveBegin);
    }
  } 
}

void PlayerControlledState::OnLeave(Player& player) {
  /* Navis lose charge when we leave this state */
  player.Charge(false);
  Logger::Logf("PlayerControlledState::OnLeave()");
}

const bool PlayerControlledState::InputQueueHas(const InputEvent& item)
{
  auto iter = std::find(inputQueue.cbegin(), inputQueue.cend(), item);
  
  // An input is available for reads if its wait time is zero or less frames
  if (iter != inputQueue.cend()) {
    return iter->wait <= frames(0);
  }

  return false;
}

void PlayerControlledState::InputQueueCleanup()
{
  // Drop inputs that are already processed at the end of the last frame
  for (auto iter = inputQueue.begin(); iter != inputQueue.end();) {
    if (iter->wait <= frames(0)) {
      iter = inputQueue.erase(iter);
      continue;
    }

    iter++;
  }
}
