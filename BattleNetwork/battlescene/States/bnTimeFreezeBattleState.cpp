#include "bnTimeFreezeBattleState.h"

#include "../bnBattleSceneBase.h"
#include "../../bnCard.h"
#include "../../bnCardAction.h"
#include "../../bnCardToActions.h"
#include "../../bnCharacter.h"
#include "../../bnStuntDouble.h"
#include "../../bnField.h"

TimeFreezeBattleState::TimeFreezeBattleState()
{
  lockedTimestamp = lockedTimestamp = std::numeric_limits<long long>::max();
}

TimeFreezeBattleState::~TimeFreezeBattleState()
{
}

const bool TimeFreezeBattleState::FadeInBackdrop()
{
  return GetScene().FadeInBackdrop(backdropInc, 0.5, true);
}

const bool TimeFreezeBattleState::FadeOutBackdrop()
{
  return GetScene().FadeOutBackdrop(backdropInc);
}

void TimeFreezeBattleState::CleanupStuntDouble()
{
  if (stuntDouble) {
    GetScene().GetField()->DeallocEntity(stuntDouble->GetID());
  }

  stuntDouble = nullptr;
}

Character* TimeFreezeBattleState::CreateStuntDouble(Character* from)
{
  CleanupStuntDouble();
  stuntDouble = new StuntDouble(*from);
  return stuntDouble;
}

void TimeFreezeBattleState::SkipToAnimateState()
{
  startState = state::animate;
}

void TimeFreezeBattleState::onStart(const BattleSceneState*)
{
  GetScene().GetSelectedCardsUI().Hide();
  GetScene().GetField()->ToggleTimeFreeze(true); // freeze everything on the field but accept hits
  summonTimer.reset();
  summonTimer.pause(); // if returning to this state, make sure the timer is not ticking at first
  currState = startState;
}

void TimeFreezeBattleState::onEnd(const BattleSceneState*)
{
  GetScene().GetSelectedCardsUI().Reveal();
  GetScene().GetField()->ToggleTimeFreeze(false);
  GetScene().HighlightTiles(false);
  action = nullptr;
  user = nullptr;
  startState = state::fadein; // assume fadein for most time freezes
  executeOnce = false; // reset execute flag
}

void TimeFreezeBattleState::onUpdate(double elapsed)
{
  summonTimer.update(sf::seconds(static_cast<float>(elapsed)));

  switch (currState) {
  case state::fadein:
  {
    if (FadeInBackdrop()) {
      currState = state::display_name;
      summonTimer.start();
      Audio().Play(AudioType::TIME_FREEZE, AudioPriority::highest);
    }
  }
    break;
  case state::display_name:
  {
    double summonSecs = summonTimer.getElapsed().asSeconds();

    if (summonSecs >= summonTextLength) {
      GetScene().HighlightTiles(true); // re-enable tile highlighting for new entities
      currState = state::animate; // animate this attack
      ExecuteTimeFreeze();
    }
  }
  break;
  case state::animate:
    {
      bool updateAnim = false;

      if (action) {
        // update the action until it is is complete
        switch (action->GetLockoutType()) {
        case CardAction::LockoutType::sequence:
          updateAnim = !action->IsLockoutOver();
          break;
        default:
          updateAnim = !action->IsAnimationOver();
          break;
        }
      }

      if (updateAnim) {
        action->Update(elapsed);
      }
      else{
        currState = state::fadeout;
        user->Reveal();
        CleanupStuntDouble();
        lockedTimestamp = std::numeric_limits<long long>::max();
      }
    }
    break;
  }
  GetScene().GetField()->Update(elapsed);
}

void TimeFreezeBattleState::onDraw(sf::RenderTexture& surface)
{
  Text summonsLabel = Text(name, Font::Style::thick);

  double summonSecs = summonTimer.getElapsed().asSeconds();
  double scale = swoosh::ease::wideParabola(summonSecs, summonTextLength, 3.0);

  sf::Vector2f position = sf::Vector2f(40.0f, 80.f);

  if (team == Team::blue) {
    position = sf::Vector2f(470.0f, 80.0f);
  }

  summonsLabel.setScale(2.0f, 2.0f*(float)scale);

  if (team == Team::red) {
    summonsLabel.setOrigin(0, summonsLabel.GetLocalBounds().height*0.5f);
  }
  else {
    summonsLabel.setOrigin(summonsLabel.GetLocalBounds().width, summonsLabel.GetLocalBounds().height*0.5f);
  }

  surface.draw(GetScene().GetCardSelectWidget());

  summonsLabel.SetColor(sf::Color::Black);
  summonsLabel.setPosition(position.x + 2.f, position.y + 2.f);
  surface.draw(summonsLabel);
  summonsLabel.SetColor(sf::Color::White);
  summonsLabel.setPosition(position);
  surface.draw(summonsLabel);
}

void TimeFreezeBattleState::ExecuteTimeFreeze()
{
  if (action && action->CanExecute()) {
    user->Hide();
    GetScene().GetField()->AddEntity(*stuntDouble, *user->GetTile());
    action->Execute(user);
  }
}

bool TimeFreezeBattleState::IsOver() {
  return state::fadeout == currState && FadeOutBackdrop();
}

void TimeFreezeBattleState::OnCardUse(const Battle::Card& card, Character& user, long long timestamp)
{
  if (card.IsTimeFreeze() && timestamp < lockedTimestamp) {
    this->name = card.GetShortName();
    this->team = user.GetTeam();
    this->user = &user;
    lockedTimestamp = timestamp;

    stuntDouble = CreateStuntDouble(this->user);
    action = CardToAction(card, *stuntDouble);
  }
}
