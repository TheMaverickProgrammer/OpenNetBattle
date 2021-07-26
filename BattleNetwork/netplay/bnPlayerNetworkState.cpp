#include "bnPlayerNetworkState.h"

#include "../bnPlayer.h"
#include "../bnCardAction.h"
#include "../bnTile.h"
#include "../bnSelectedCardsUI.h"
#include "../bnAudioResourceManager.h"

#include <iostream>

PlayerNetworkState::PlayerNetworkState(NetPlayFlags& netflags) : 
  AIState<Player>(), 
  netflags(netflags),
  isChargeHeld(false)
{
}

PlayerNetworkState::~PlayerNetworkState()
{
}

void PlayerNetworkState::OnEnter(Player& player) {
  player.SetAnimation(PLAYER_IDLE);
}

void PlayerNetworkState::OnUpdate(double _elapsed, Player& player) {
  player.SetHealth(netflags.remoteHP);

  bool actionable = player.IsActionable();

  // Actions with animation lockout controls take priority over movement
  bool canMove = player.IsLockoutAnimationComplete();

  // One of our active actions are preventing us from moving
  if (!canMove) {
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
      player.Charge(false);
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

  }
  else if (InputQueueHas(InputEvents::held_shoot)) {
    isChargeHeld = true;
    player.chargeEffect.SetCharging(true);
  }

  // Movement increments are restricted based on anim speed at this time
  if (player.IsMoving()) {
    return;
  }

  Direction direction = Direction::none;
  if (InputQueueHas(InputEvents::pressed_move_up) || InputQueueHas(InputEvents::held_move_up)) {
    direction = Direction::up;
  }
  else if (InputQueueHas(InputEvents::pressed_move_left) || InputQueueHas(InputEvents::held_move_left)) {
    // reverse the input for network PVP
    direction = Direction::right;
  }
  else if (InputQueueHas(InputEvents::pressed_move_down) || InputQueueHas(InputEvents::held_move_down)) {
    direction = Direction::down;
  }
  else if (InputQueueHas(InputEvents::pressed_move_right) || InputQueueHas(InputEvents::held_move_right)) {
    // reverse the input for network PVP
    direction = Direction::left;
  }

  if (direction != Direction::none && actionable) {
    auto next_tile = player.GetTile() + direction;
    auto onMoveBegin = [player = &player, next_tile, this] {
      auto anim = player->GetFirstComponent<AnimationComponent>();

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

void PlayerNetworkState::OnLeave(Player& player) {
  /* Navis lose charge when we leave this state */
  player.Charge(false);
}

const bool PlayerNetworkState::InputQueueHas(const InputEvent& item)
{
  return netflags.remoteInputEvents.cend() != std::find(netflags.remoteInputEvents.cbegin(), netflags.remoteInputEvents.cend(), item);
}