#include <Swoosh/Ease.h>

#include "bnCharacter.h"
#include "bnSelectedCardsUI.h"
#include "bnDefenseRule.h"
#include "bnDefenseSuperArmor.h"
#include "bnSpell.h"
#include "bnTile.h"
#include "bnField.h"
#include "bnAlertSymbol.h"
#include "bnShaderResourceManager.h"
#include "bnAnimationComponent.h"
#include "bnShakingEffect.h"
#include "bnBubbleTrap.h"
#include "bnBubbleState.h"
#include "bnPlayer.h"
#include "bnCardAction.h"
#include "bnCardToActions.h"

Character::Character(Rank _rank) :
  rank(_rank),
  CardActionUsePublisher(),
  Entity() {

  EnableTilePush(true);

  using namespace std::placeholders;
  auto cardHandler = std::bind(&Character::HandleCardEvent, this, _1, _2);
  actionQueue.RegisterType<CardEvent>(ActionTypes::card, cardHandler);

  auto peekHandler = std::bind(&Character::HandlePeekEvent, this, _1, _2);
  actionQueue.RegisterType<PeekCardEvent>(ActionTypes::peek_card, peekHandler);

  RegisterStatusCallback(Hit::bubble, [this] {
    actionQueue.ClearQueue(ActionQueue::CleanupType::allow_interrupts);
    CreateComponent<BubbleTrap>(weak_from_this());
    
    // TODO: take out this ugly hack
    //       Make BubbleState a CardAction
    if (auto ai = dynamic_cast<Player*>(this)) {
      ai->ChangeState<BubbleState<Player>>(); 
    }
  });
}

Character::~Character() {
}

void Character::Cleanup() {
  for (std::shared_ptr<CardAction>& action : asyncActions) {
    action->EndAction();
  }

  Entity::Cleanup();
}

void Character::OnBattleStop()
{
  asyncActions.clear();
}

const Character::Rank Character::GetRank() const {
  return rank;
}

const bool Character::IsLockoutAnimationComplete()
{
  if (currCardAction) {
    return currCardAction->IsAnimationOver();
  }

  return true;
}

void Character::Update(double _elapsed) {
  Entity::Update(_elapsed);

  // process async actions
  std::vector<std::shared_ptr<CardAction>> asyncCopy = asyncActions;
  for (std::shared_ptr<CardAction>& action : asyncCopy) {
    action->Update(_elapsed);

    if (action->IsLockoutOver()) {
      auto iter = std::find(asyncActions.begin(), asyncActions.end(), action);
      if (iter != asyncActions.end()) {
        asyncActions.erase(iter);
      }
    }
  }

  // If we have an attack from the action queue...
  if (currCardAction) {

    // if we have yet to invoke this attack...
    if (currCardAction->CanExecute() && IsActionable()) {

      // reduce the artificial delay
      cardActionStartDelay -= from_seconds(_elapsed);

      // execute when delay is over
      if (cardActionStartDelay <= frames(0)) {
        for(std::shared_ptr<AnimationComponent>& anim : this->GetComponents<AnimationComponent>()) {
          anim->CancelCallbacks();
        }
        MakeActionable();
        std::shared_ptr<Character> characterPtr = shared_from_base<Character>();
        currCardAction->Execute(characterPtr);
      }
    }

    // update will exit early if not executed (configured) 
    currCardAction->Update(_elapsed);

    // once the animation is complete, 
    // cleanup the attack and pop the action queue
    if (currCardAction->IsAnimationOver()) {
      currCardAction->EndAction();

      if (currCardAction->GetLockoutType() == CardAction::LockoutType::async) {
        asyncActions.push_back(currCardAction);
      }

      currCardAction = nullptr;
      actionQueue.Pop();
    }
  }
}

bool Character::CanMoveTo(Battle::Tile * next)
{
  if (std::shared_ptr<CardAction> action = this->CurrentCardAction()) {
    std::optional<bool> res = action->CanMoveTo(next);

    if (res) {
      return *res;
    }
  }

  auto occupied = [this](std::shared_ptr<Entity>& in) {
    auto c = dynamic_cast<Character*>(in.get());

    return c && c != this && !c->CanShareTileSpace();
  };

  bool result = (Entity::CanMoveTo(next) && next->FindEntities(occupied).size() == 0);
  result = result && !next->IsEdgeTile();

  return result;
}

const bool Character::CanAttack() const
{
  return !currCardAction && IsActionable();
}

void Character::MakeActionable()
{
  // impl. defined
}

bool Character::IsActionable() const
{
  return true; // impl. defined
}

const std::vector<std::shared_ptr<CardAction>> Character::AsyncActionList() const
{
  return asyncActions;
}

std::shared_ptr<CardAction> Character::CurrentCardAction()
{
  return currCardAction;
}

void Character::AddAction(const CardEvent& event, const ActionOrder& order)
{
  actionQueue.Add(event, order, ActionDiscardOp::until_resolve);
}

void Character::AddAction(const PeekCardEvent& event, const ActionOrder& order)
{
  actionQueue.Add(event, order, ActionDiscardOp::until_eof);
}

void Character::HandleCardEvent(const CardEvent& event, const ActionQueue::ExecutionType& exec)
{
  if (currCardAction == nullptr) {
    if (event.action->GetMetaData().timeFreeze) {
      CardActionUsePublisher::Broadcast(event.action, CurrentTime::AsMilli());
      actionQueue.Pop();
    }
    else {
      cardActionStartDelay = frames(5);
      currCardAction = event.action;
    }
  }

  if (currCardAction == nullptr) return;

  if (exec == ActionQueue::ExecutionType::interrupt) {
    currCardAction->EndAction();
    currCardAction = nullptr;
  }
}

void Character::HandlePeekEvent(const PeekCardEvent& event, const ActionQueue::ExecutionType& exec)
{
  SelectedCardsUI* publisher = event.publisher;
  if (publisher) {
    std::shared_ptr<Character> characterPtr = shared_from_base<Character>();

    // If we have a card via Peeking, then Play it
    if (publisher->HandlePlayEvent(characterPtr)) {
      // prepare for this frame's action animation (we must be actionable)
      MakeActionable();
    }
  }

  actionQueue.Pop();
}
