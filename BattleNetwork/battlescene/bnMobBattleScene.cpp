#include "bnMobBattleScene.h"
#include "../bnMob.h"
#include "../bnElementalDamage.h"
#include "../../bnPlayer.h"

#include "States/bnRewardBattleState.h"
#include "States/bnTimeFreezeBattleState.h"
#include "States/bnBattleStartBattleState.h"
#include "States/bnBattleOverBattleState.h"
#include "States/bnFadeOutBattleState.h"
#include "States/bnCombatBattleState.h"
#include "States/bnMobIntroBattleState.h"
#include "States/bnCardSelectBattleState.h"
#include "States/bnCardComboBattleState.h"
#include "States/bnRetreatBattleState.h"
#include "../bnBlockPackageManager.h"

using namespace swoosh;

MobBattleScene::MobBattleScene(ActivityController& controller, MobBattleProperties _props, BattleResultsFunc onEnd) :
  BattleSceneBase(controller, _props.base, onEnd),
  props(std::move(_props))
{
  // Load players in the correct order, then the mob
  Init();

  Mob* current = props.mobs.at(0);

  // in seconds
  constexpr double battleDuration = 10.0;

  // First, we create all of our scene states
  auto intro       = AddState<MobIntroBattleState>(current);
  auto cardSelect  = AddState<CardSelectBattleState>();
  auto combat      = AddState<CombatBattleState>(battleDuration);
  auto combo       = AddState<CardComboBattleState>(this->GetSelectedCardsUI(), props.base.programAdvance);
  auto forms       = AddState<CharacterTransformBattleState>();
  auto battlestart = AddState<BattleStartBattleState>();
  auto battleover  = AddState<BattleOverBattleState>();
  auto timeFreeze  = AddState<TimeFreezeBattleState>();
  auto reward      = AddState<RewardBattleState>(current, &playerHitCount);
  auto fadeout     = AddState<FadeOutBattleState>(FadeOut::black);

  // TODO: create a textbox in the battle scene and supply it to the card select widget and other states...
  auto retreat     = AddState<RetreatBattleState>(GetCardSelectWidget().GetTextBox(), props.mug, props.anim);

  combatPtr = &combat.Unwrap();
  timeFreezePtr = &timeFreeze.Unwrap();

  std::shared_ptr<PlayerSelectedCardsUI> cardUI = GetLocalPlayer()->GetFirstComponent<PlayerSelectedCardsUI>();
  combatPtr->Subscribe(*cardUI);
  timeFreezePtr->Subscribe(*cardUI);

  // Subscribe to player's events
  combatPtr->Subscribe(*GetLocalPlayer());
  timeFreezePtr->Subscribe(*GetLocalPlayer());

  //////////////////////////////////////////////////////////////////
  // Important! State transitions are added in order of priority! //
  //////////////////////////////////////////////////////////////////

  intro.ChangeOnEvent(cardSelect, HookIntro(intro.Unwrap(), timeFreeze.Unwrap(), combat.Unwrap()));

  // Prevent all other conditions if the player tried to retreat
  cardSelect.ChangeOnEvent(retreat, &CardSelectBattleState::RequestedRetreat);

  // Goto the combo check state if new cards are selected...
  cardSelect.ChangeOnEvent(combo, &CardSelectBattleState::SelectedNewChips);

  // ... else if forms were selected, go directly to forms ....
  cardSelect.ChangeOnEvent(forms, &CardSelectBattleState::HasForm);

  // ... Finally if none of the above, just start the battle
  cardSelect.ChangeOnEvent(battlestart, &CardSelectBattleState::OKIsPressed);

  // If we reached the combo state, we must also check if form transformation was next
  // or to just start the battle after
  combo.ChangeOnEvent(forms, [cardSelect, combo]() mutable {return combo->IsDone() && cardSelect->HasForm(); });
  combo.ChangeOnEvent(battlestart, &CardComboBattleState::IsDone);

  // If retreating fails, we jump right into battle start
  retreat.ChangeOnEvent(battlestart, &RetreatBattleState::Fail);

  // Otherwise, we leave
  retreat.ChangeOnEvent(fadeout, HookRetreat(retreat.Unwrap(), fadeout.Unwrap()));

  // Forms is the last state before kicking off the battle
  // if we reached this state...
  forms.ChangeOnEvent(combat, HookFormChangeEnd(forms.Unwrap(), cardSelect.Unwrap()));
  forms.ChangeOnEvent(battlestart, &CharacterTransformBattleState::IsFinished);

  // Combat begins when the battle start animations are finished
  battlestart.ChangeOnEvent(combat, &BattleStartBattleState::IsFinished);

  // Mob rewards players after the battle over animations are finished
  battleover.ChangeOnEvent(reward, &BattleOverBattleState::IsFinished);

  // share some values between states
  combo->ShareCardList(cardSelect->GetCardPtrList());

  // combat has multiple state interruptions based on events
  // so we can chain them together
  combat.ChangeOnEvent(battleover, HookPlayerWon())
    .ChangeOnEvent(forms, HookFormChangeStart(forms.Unwrap()))
    .ChangeOnEvent(fadeout, &CombatBattleState::PlayerDeleted)
    .ChangeOnEvent(cardSelect, &CombatBattleState::PlayerRequestCardSelect)
    .ChangeOnEvent(timeFreeze, &CombatBattleState::HasTimeFreeze);

  // Time freeze state interrupts combat state which must resume after animations are finished
  timeFreeze.ChangeOnEvent(combat, &TimeFreezeBattleState::IsOver);
  
  // Some states are part of the combat routine and need to respect
  // the combat state's timers
  combat->subcombatStates.push_back(&timeFreeze.Unwrap());
  combat->subcombatStates.push_back(&forms.Unwrap());

  // this kicks-off the state graph beginning with the intro state
  this->StartStateGraph(intro);
}

MobBattleScene::~MobBattleScene() {
  for (Mob* m : props.mobs) {
    delete m;
  }

  props.mobs.clear();
}

void MobBattleScene::Init()
{
  Mob& mob = *props.mobs.front();

  // Simple error reporting if the scene was not fed any mobs
  if (props.mobs.size() == 0) {
    Logger::Log(LogLevel::warning, std::string("Mob list was empty when battle started."));
  }
  else if (mob.GetMobCount() == 0) {
    Logger::Log(LogLevel::warning, std::string("Current mob was empty when battle started. Mob Type: ") + typeid(mob).name());
  }
  else {
    LoadBlueTeamMob(mob);
  }

  if (mob.HasPlayerSpawnPoint(1)) {
    Mob::PlayerSpawnData data = mob.GetPlayerSpawnPoint(1);
    SpawnLocalPlayer(data.tileX, data.tileY);
  }
  else {
    SpawnLocalPlayer(2, 2);
  }

  // Run block programs on the remote player now that they are spawned
  BlockPackageManager& blockPackages = getController().BlockPackagePartitioner().GetPartition(Game::LocalPartition);
  for (const std::string& blockID : props.blocks) {
    if (!blockPackages.HasPackage(blockID)) continue;

    auto& blockMeta = blockPackages.FindPackageByID(blockID);
    blockMeta.mutator(*GetLocalPlayer());
  }

  GetCardSelectWidget().SetSpeaker(props.mug, props.anim);
  GetEmotionWindow().SetTexture(props.emotion);
}

void MobBattleScene::OnHit(Entity& victim, const Hit::Properties& props)
{
  std::shared_ptr<Player> player = GetLocalPlayer();
  if (player.get() == &victim && props.damage > 0) {
    playerHitCount++;

    if (props.damage >= 300) {
      player->SetEmotion(Emotion::angry);
      GetSelectedCardsUI().SetMultiplier(2);
    }

    if (player->IsSuperEffective(props.element)) {
      playerDecross = true;
    }
  }

  bool freezeBreak = victim.IsIceFrozen() && ((props.flags & Hit::breaking) == Hit::breaking);
  bool superEffective = victim.IsSuperEffective(props.element) && props.damage > 0;

  if (freezeBreak || superEffective) {
    std::shared_ptr<ElementalDamage> seSymbol = std::make_shared<ElementalDamage>();
    seSymbol->SetLayer(-100);
    seSymbol->SetHeight(victim.GetHeight()+(victim.getLocalBounds().height*0.5f)); // place it at sprite height
    GetField()->AddEntity(seSymbol, victim.GetTile()->GetX(), victim.GetTile()->GetY());
  }
}

void MobBattleScene::onUpdate(double elapsed)
{
  if (combatPtr->IsStateCombat(GetCurrentState())) {
    ProcessLocalPlayerInputQueue();
  }

  BattleSceneBase::onUpdate(elapsed);
}

void MobBattleScene::onStart()
{
  BattleSceneBase::onStart();
}

void MobBattleScene::onExit()
{
}

void MobBattleScene::onEnter()
{
}

void MobBattleScene::onResume()
{
}

void MobBattleScene::onLeave()
{
  BattleSceneBase::onLeave();
}

std::function<bool()> MobBattleScene::HookIntro(MobIntroBattleState& intro, TimeFreezeBattleState& timefreeze, CombatBattleState& combat)
{
  auto isIntroOver = [this, &intro, &timefreeze, &combat]() mutable {
    if (intro.IsOver()) {
      // Mob's mutated at spawn may have card use publishers.
      // Share the scene's subscriptions at this point in time with
      // those substates.
      for (auto& publisher : this->GetCardActionSubscriptions()) {
        timefreeze.Subscribe(publisher);
        combat.Subscribe(publisher);
      }
      return true;
    }

    return false;
  };

  return isIntroOver;
}

std::function<bool()> MobBattleScene::HookRetreat(RetreatBattleState& retreat, FadeOutBattleState& fadeout)
{
  auto lambda = [&retreat, &fadeout]() mutable {
    if (retreat.Success()) {
      // fadeout pauses animations only when retreating
      fadeout.EnableKeepPlaying(false);
      return true;
    }

    return false;
  };

  return lambda;
}

std::function<bool()> MobBattleScene::HookFormChangeEnd(CharacterTransformBattleState& form, CardSelectBattleState& cardSelect)
{
  auto lambda = [&form, &cardSelect, this]() mutable {
    bool triggered = form.IsFinished() && (GetLocalPlayer()->GetHealth() == 0 || playerDecross);

    if (triggered) {
      playerDecross = false; // reset our decross flag

      // update the card select gui and state
      // since the state has its own records
      cardSelect.ResetSelectedForm();
    }

    return triggered;
  };

  return lambda;
}

std::function<bool()> MobBattleScene::HookFormChangeStart(CharacterTransformBattleState& form)
{
  // special condition: if in combat and should decross, trigger the character transform states
  auto lambda = [this, &form]() mutable {
    std::shared_ptr<Player> localPlayer = GetLocalPlayer();
    TrackedFormData& formData = GetPlayerFormData(localPlayer);

    bool changeState = localPlayer->GetHealth() == 0;
    changeState = changeState || playerDecross;
    changeState = changeState && (formData.selectedForm != -1);

    if (changeState) {
      formData.selectedForm = -1;
      formData.animationComplete = false;
      form.SkipBackdrop();
    }

    return changeState;
  };

  return lambda;
}

std::function<bool()> MobBattleScene::HookPlayerWon()
{
  auto lambda = [this] {
    std::shared_ptr<Player> localPlayer = GetLocalPlayer();

    if (localPlayer->GetTeam() == Team::red) {
      return IsBlueTeamCleared();
    }
    else if (localPlayer->GetTeam() == Team::blue) {
      return IsRedTeamCleared();
    }

    return IsBlueTeamCleared() && IsRedTeamCleared();
  };

  return lambda;
}
