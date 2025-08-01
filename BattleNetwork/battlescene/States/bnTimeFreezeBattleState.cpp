#include "bnTimeFreezeBattleState.h"
#include "../bnBattleSceneBase.h"
#include "../../bnCard.h"
#include "../../bnCardAction.h"
#include "../../bnCardToActions.h"
#include "../../bnCharacter.h"
#include "../../bnStuntDouble.h"
#include "../../bnField.h"
#include "../../bnPlayer.h"
#include "../../bnPlayerSelectedCardsUI.h"
#include <cmath>

TimeFreezeBattleState::TimeFreezeBattleState() :
  dmg(Font::Style::gradient_orange),
  multiplier(Font::Style::thick)
{
  lockedTimestamp = std::numeric_limits<long long>::max();
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
  if (tfEvents.size()) {
    auto iter = tfEvents.begin();

    BattleSceneBase& scene = GetScene();
    if (iter->stuntDouble) {
      scene.UntrackMobCharacter(iter->stuntDouble);
      scene.GetField()->DeallocEntity(iter->stuntDouble->GetID());
    }
  }
}

std::shared_ptr<Character> TimeFreezeBattleState::CreateStuntDouble(std::shared_ptr<Character> from)
{
  CleanupStuntDouble();
  std::shared_ptr<StuntDouble> stuntDouble = std::make_shared<StuntDouble>(from);
  stuntDouble->Init();
  return stuntDouble;
}

void TimeFreezeBattleState::SkipToAnimateState()
{
  startState = state::animate;
  ExecuteTimeFreeze();
}

void TimeFreezeBattleState::ProcessInputs()
{
  size_t player_idx = 0;
  for (std::shared_ptr<Player>& p : this->GetScene().GetAllPlayers()) {
    p->InputState().Process();

    if (summonTick < tfcStartFrame) {
      continue;
    }

    if (p->InputState().Has(InputEvents::pressed_use_chip)) {
      Logger::Logf(LogLevel::info, "InputEvents::pressed_use_chip for player %i", player_idx);
      std::shared_ptr<PlayerSelectedCardsUI> cardsUI = p->GetFirstComponent<PlayerSelectedCardsUI>();
      if (cardsUI) {
        auto maybe_card = cardsUI->Peek();

        if (maybe_card.has_value()) {
          // convert meta data into a useable action object
          const Battle::Card& card = *maybe_card;

          if (card.IsTimeFreeze() && CanCounter(p)) {
            if (std::shared_ptr<CardAction> action = CardToAction(card, p, &GetScene().getController().CardPackagePartitioner(), card.GetProps())) {
              OnCardActionUsed(action, CurrentTime::AsMilli());
              cardsUI->DropNextCard();
            }
          }
        }
      }
    }
    player_idx++;
  }
}

void TimeFreezeBattleState::onStart(const BattleSceneState*)
{
  Logger::Logf(LogLevel::info, "TimeFreezeBattleState::onStart");
  BattleSceneBase& scene = GetScene();
  scene.GetSelectedCardsUI().Hide();
  scene.GetField()->ToggleTimeFreeze(true); // freeze everything on the field but accept hits
  scene.StopBattleStepTimer();
  currState = startState;

  if (tfEvents.empty()) return;

  const auto& first = tfEvents.begin();
  if (first->action && first->action->GetMetaData().GetProps().skipTimeFreezeIntro) {
    SkipToAnimateState();
  }
}

void TimeFreezeBattleState::onEnd(const BattleSceneState*)
{
  Logger::Logf(LogLevel::info, "TimeFreezeBattleState::onEnd");
  BattleSceneBase& scene = GetScene();
  scene.GetSelectedCardsUI().Reveal();
  scene.GetField()->ToggleTimeFreeze(false);
  scene.HighlightTiles(false);
  scene.StartBattleStepTimer();
  tfEvents.clear();
  summonStart = false;
  summonTick = frames(0);
  startState = state::fadein; // assume fadein for most time freezes
  currState = startState;
}

void TimeFreezeBattleState::onUpdate(double elapsed)
{
  ProcessInputs();

  if (summonStart) {
    summonTick += frames(1);
  }

  BattleSceneBase& scene = GetScene();

  scene.GetField()->Update(elapsed);

  switch (currState) {
  case state::fadein:
  {
    if (FadeInBackdrop()) {
      summonStart = true;
      currState = state::display_name;
      Audio().Play(AudioType::TIME_FREEZE, AudioPriority::highest);
    }
  }
    break;
  case state::display_name:
  {
    summonsLabel.SetString(tfEvents.begin()->name);

    if (playerCountered) {
      playerCountered = false;

      Audio().Play(AudioType::TRAP, AudioPriority::high);
      auto last = tfEvents.begin()+1u;
      last->animateCounter = true;
      summonTick = frames(0);
    }

    if (summonTick >= summonTextLength) {
      scene.HighlightTiles(true); // re-enable tile highlighting for new entities
      currState = state::animate; // animate this attack

      // Resize the time freeze queue to a max of 2 attacks
      tfEvents.resize(std::min(tfEvents.size(), (size_t)2));
      tfEvents.shrink_to_fit();

      ExecuteTimeFreeze();
    }

    for (TimeFreezeBattleState::EventData& e : tfEvents) {
      if (e.animateCounter) {
        e.alertFrameCount += frames(1);
        //Set animation to false if we're done animating.
        if (e.alertFrameCount.value > alertAnimFrames.value) {
          e.animateCounter = false;
        }
        //Delay while animating. We can't counter right now.
        summonTick = frames(0);
      }
    }
  }
  break;
  case state::animate:
    {
      bool updateAnim = false;
      const auto& first = tfEvents.begin();

      if (first->action) {
        // update the action until it is is complete
        switch (first->action->GetLockoutType()) {
        case CardAction::LockoutType::sequence:
          updateAnim = !first->action->IsLockoutOver();
          break;
        default:
          updateAnim = !first->action->IsAnimationOver();
          break;
        }
      }

      if (updateAnim) {
        first->action->Update(elapsed);
      }
      else{
        first->user->Reveal();
        first->action->EndAction();
        scene.UntrackMobCharacter(first->stuntDouble);
        scene.GetField()->DeallocEntity(first->stuntDouble->GetID());

        if (tfEvents.size() == 1) {
          // This is the only event in the list
          lockedTimestamp = std::numeric_limits<long long>::max();
          currState = state::fadeout;
        }
        else {
          tfEvents.erase(tfEvents.begin());

          // restart the time freeze animation
          currState = state::animate;

          ExecuteTimeFreeze();
        }
      }
    }
    break;
  }

  for (std::shared_ptr<Player>& player : GetScene().GetAllPlayers()) {
    ChargeEffectSceneNode& chargeNode = player->GetChargeComponent();
    chargeNode.Animate(elapsed);
  }
}

void TimeFreezeBattleState::onDraw(sf::RenderTexture& surface)
{
  static sf::Sprite alertSprite(*Textures().LoadFromFile("resources/ui/alert.png"));
  static sf::RectangleShape bar;

  if (tfEvents.empty()) return;

  BattleSceneBase& scene = GetScene();
  const auto& first = tfEvents.begin();

  double tfcTimerScale = 0;

  double summonTickSeconds = summonTick.asSeconds().value;
  double fadeSeconds = fadeInOutLength.asSeconds().value;


  if (summonTickSeconds > fadeSeconds) {
    tfcTimerScale = swoosh::ease::linear((summonTickSeconds - fadeSeconds), (double)(summonTextLength.asSeconds().value - fadeSeconds), 1.0);
  }

  double scale = swoosh::ease::linear(summonTick.asSeconds().value, fadeInOutLength.asSeconds().value, 1.0);
  scale = std::min(scale, 1.0);

  bar = sf::RectangleShape({ 100.f * static_cast<float>(1.0 - tfcTimerScale), 2.f });
  bar.setScale(2.f, 2.f);

  if (summonTick >= summonTextLength) {
    scale = swoosh::ease::linear((summonTextLength - summonTick).asSeconds().value, fadeSeconds, 1.0);
    scale = std::max(scale, 0.0);
  }

  sf::Vector2f position = sf::Vector2f(64.f, 82.f);

  if (first->team == Team::blue) {
    position = sf::Vector2f(416.f, 82.f);
    bar.setOrigin(bar.getLocalBounds().width, 0.0f);
  }

  
  DrawCardData(position, sf::Vector2f(2.f, scale * 2.f), surface);

  scene.DrawCustGauage(surface);
  surface.draw(scene.GetCardSelectWidget());

  if (currState == state::display_name && first->action->GetMetaData().GetProps().counterable && summonTick > tfcStartFrame) {
    // draw TF bar underneath if conditions are met
    bar.setPosition(position + sf::Vector2f(0.f + 2.f, 12.f + 2.f));
    bar.setFillColor(sf::Color::Black);
    scene.DrawWithPerspective(bar, surface);

    bar.setPosition(position + sf::Vector2f(0.f, 12.f));

    sf::Uint8 b = (sf::Uint8)swoosh::ease::interpolate((1.0-tfcTimerScale), 0.0, 255.0);

    bar.setFillColor(sf::Color(255, 255, b));
    scene.DrawWithPerspective(bar, surface);
  }

  // draw the !! sprite
  for (TimeFreezeBattleState::EventData& e : tfEvents) {
    if (e.animateCounter) {
      double scale = swoosh::ease::wideParabola(e.alertFrameCount.asSeconds().value, this->alertAnimFrames.asSeconds().value, 3.0);
      
      sf::Vector2f position = sf::Vector2f(66.f, 82.f);

      if (e.team == Team::blue) {
        position = sf::Vector2f(416.f, 82.f);
      }

      alertSprite.setScale(2.0f, 2.0f * (float)scale);

      if (e.team == Team::red || e.team == Team::unknown) {
        alertSprite.setOrigin(0, alertSprite.getLocalBounds().height * 0.5f);
      }
      else {
        alertSprite.setOrigin(alertSprite.getLocalBounds().width, alertSprite.getLocalBounds().height * 0.5f);
      }

      alertSprite.setPosition(position);
      scene.DrawWithPerspective(alertSprite, surface);
    }
  }
}

void TimeFreezeBattleState::ExecuteTimeFreeze()
{
  if (tfEvents.empty()) return;

  TimeFreezeBattleState::EventData& first = *tfEvents.begin();

  if (first.action && first.action->CanExecute()) {
    first.user->Hide();
    if (GetScene().GetField()->AddEntity(first.stuntDouble, *first.user->GetTile()) != Field::AddEntityStatus::deleted) {
      first.action->Execute(first.user);
    }
    else {
      currState = state::fadeout;
    }
  }
}

bool TimeFreezeBattleState::IsOver() {
  return state::fadeout == currState && FadeOutBackdrop();
}

void TimeFreezeBattleState::DrawCardData(const sf::Vector2f& pos, const sf::Vector2f& scale, sf::RenderTarget& target) {
  TimeFreezeBattleState::EventData& event = *tfEvents.begin();
  const auto orange = sf::Color(225, 140, 0);
  bool canBoost{};
  float multiplierOffset = 0.f;
  float dmgOffset = 0.f;

  // helper function
  auto setSummonLabelOrigin = [](Team team, Text& text) {
    if (team == Team::red) {
      text.setOrigin(0, text.GetLocalBounds().height * 0.5f);
    }
    else {
      text.setOrigin(text.GetLocalBounds().width, text.GetLocalBounds().height * 0.5f);
    }
  };

  // We want the other text to use the origin of the 
  // summons label for the y so that they are all 
  // sitting on the same line when they render
  auto setOrigin = [setSummonLabelOrigin, this](Team team, Text& text) {
    setSummonLabelOrigin(team, text);
    text.setOrigin(text.getOrigin().x, this->summonsLabel.getOrigin().y);
  };

  summonsLabel.SetString(event.name);
  summonsLabel.setScale(scale);
  setSummonLabelOrigin(event.team, summonsLabel);

  dmg.SetString("");
  multiplier.SetString("");

  const Battle::Card& card = event.action->GetMetaData();
  canBoost = card.CanBoost();

  // Calculate the delta damage values to correctly draw the modifiers
  const unsigned int multiplierValue = card.GetMultiplier();
  int unmodDamage = card.GetBaseProps().damage;
  int damage = card.GetProps().damage;

  if (multiplierValue) {
    damage /= multiplierValue;
  }

  int delta = damage - unmodDamage;
  sf::String dmgText = std::to_string(unmodDamage);

  if (delta != 0) {
    dmgText = dmgText + sf::String("+") + sf::String(std::to_string(std::abs(delta)));
  }

  // attacks that normally show no damage will show if the modifer adds damage
  if (delta > 0 || unmodDamage > 0) {
    dmg.SetString(dmgText);
    dmg.setScale(scale);
    setOrigin(event.team, dmg);
    dmgOffset = 10.0f;
  }

  if (multiplierValue != 1 && unmodDamage != 0) {
    // add "x N" where N is the multiplier
    std::string multStr = "x" + std::to_string(multiplierValue);
    multiplier.SetString(multStr);
    multiplier.setScale(scale);
    setOrigin(event.team, multiplier);
    multiplierOffset = 3.0f;
  }

  // based on team, render the text from left-to-right or right-to-left alignment
  if (event.team == Team::red) {
    summonsLabel.setPosition(pos);
    dmg.setPosition(summonsLabel.getPosition().x + summonsLabel.GetWorldBounds().width + dmgOffset, pos.y);
    multiplier.setPosition(dmg.getPosition().x + dmg.GetWorldBounds().width + multiplierOffset, pos.y);
  }
  else { /* team == Team::blue or other */
    multiplier.setPosition(pos);
    dmg.setPosition(multiplier.getPosition().x - multiplier.GetWorldBounds().width - multiplierOffset, pos.y);
    summonsLabel.setPosition(dmg.getPosition().x - dmg.GetWorldBounds().width - dmgOffset, pos.y);
  }

  // shadow beneath
  auto textPos = summonsLabel.getPosition();
  summonsLabel.SetColor(sf::Color::Black);
  summonsLabel.setPosition(textPos.x + 2.f, textPos.y + 2.f);
  target.draw(summonsLabel);

  // font on top
  summonsLabel.setPosition(textPos);
  summonsLabel.SetColor(sf::Color::White);
  target.draw(summonsLabel);

  // our number font has shadow baked in
  target.draw(dmg);

  if (canBoost) {
    // shadow
    auto multiPos = multiplier.getPosition();
    multiplier.SetColor(sf::Color::Black);
    multiplier.setPosition(multiPos.x + 2.f, multiPos.y + 2.f);
    target.draw(multiplier);

    // font on top
    multiplier.setPosition(multiPos);
    multiplier.SetColor(sf::Color::White);
    target.draw(multiplier);
  }
}

void TimeFreezeBattleState::OnCardActionUsed(std::shared_ptr<CardAction> action, uint64_t timestamp)
{
  const Battle::Card::Properties& props = action->GetMetaData().GetProps();
  Logger::Logf(LogLevel::info, "OnCardActionUsed(): %s, summonTick: %i, summonTextLength: %i", props.shortname.c_str(), summonTick.count(), summonTextLength.count());

  if (!(action && action->GetMetaData().GetProps().timeFreeze)) return;

  if (CanCounter(action->GetActor())) {
    HandleTimeFreezeCounter(action, timestamp);
  }
}

const bool TimeFreezeBattleState::CanCounter(std::shared_ptr<Character> user)
{
  // tfc window ended
  if (summonTick > summonTextLength) return false;

  // bool addEvent = true;

  if (!tfEvents.empty()) {
    // Don't counter during alert symbol. BN6 accurate. See notes from Alrysc.
    std::shared_ptr<CardAction> action = tfEvents.begin()->action;

    for (TimeFreezeBattleState::EventData& e : tfEvents) {
      if (e.animateCounter) {
        return false;
      }
    }
    // some actions cannot be countered
    if (!action->GetMetaData().GetProps().counterable) return false;

    // only opposing players can counter
    std::shared_ptr<Character> lastActor = action->GetActor();
    if (!lastActor->Teammate(user->GetTeam())) {
      playerCountered = true;
      Logger::Logf(LogLevel::info, "Player was countered!");
    }
    else {
      return false;
    }
  }

  return true;
}

void TimeFreezeBattleState::HandleTimeFreezeCounter(std::shared_ptr<CardAction> action, uint64_t timestamp)
{
  TimeFreezeBattleState::EventData data;
  data.action = action;
  data.name = action->GetMetaData().GetProps().shortname;
  data.team = action->GetActor()->GetTeam();
  data.user = action->GetActor();
  lockedTimestamp = timestamp;

  data.action = action;
  data.stuntDouble = CreateStuntDouble(data.user);
  action->UseStuntDouble(data.stuntDouble);

  tfEvents.insert(tfEvents.begin(), data);

  Logger::Logf(LogLevel::debug, "Added chip event: %s", data.name.c_str());
}

