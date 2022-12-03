#include "bnCombatBattleState.h"

#include "../bnBattleSceneBase.h"

#include "../../bnPlayer.h"
#include "../../bnPlayerSelectedCardsUI.h"
#include "../../bnMob.h"
#include "../../bnTeam.h"
#include "../../bnEntity.h"
#include "../../bnCharacter.h"
#include "../../bnObstacle.h"
#include "../../bnCard.h"
#include "../../bnCardAction.h"
#include "../../bnInputManager.h"
#include "../../bnShaderResourceManager.h"

CombatBattleState::CombatBattleState(double customDuration) :
  pauseShader(Shaders().GetShader(ShaderType::BLACK_FADE))
{
  // PAUSE
  pause.setTexture(*Textures().LoadFromFile("resources/ui/pause.png"));
  pause.setScale(2.f, 2.f);
  pause.setOrigin(pause.getLocalBounds().width * 0.5f, pause.getLocalBounds().height * 0.5f);
  pause.setPosition(sf::Vector2f(240.f, 145.f));

 
  if (pauseShader) {
    pauseShader->setUniform("texture", sf::Shader::CurrentTexture);
    pauseShader->setUniform("opacity", 0.25f);
  }

  // COMBO DELETE AND COUNTER LABELS
  auto labelPosition = sf::Vector2f(240.0f, 50.f);

  doubleDelete = sf::Sprite(*Textures().LoadFromFile(TexturePaths::DOUBLE_DELETE));
  doubleDelete.setOrigin(doubleDelete.getLocalBounds().width / 2.0f, doubleDelete.getLocalBounds().height / 2.0f);
  doubleDelete.setPosition(labelPosition);
  doubleDelete.setScale(2.f, 2.f);

  tripleDelete = doubleDelete;
  tripleDelete.setTexture(*Textures().LoadFromFile(TexturePaths::TRIPLE_DELETE));

  counterHit = doubleDelete;
  counterHit.setTexture(*Textures().LoadFromFile(TexturePaths::COUNTER_HIT));
}

const bool CombatBattleState::IsMobCleared() const
{
  const BattleSceneBase& scene = GetScene();
  bool redTeamCleared = scene.IsRedTeamCleared();
  bool blueTeamCleared = scene.IsBlueTeamCleared();
  bool isCleared = false;

  Team localTeam = scene.GetLocalPlayer()->GetTeam();

  if (localTeam == Team::red) {
    isCleared = blueTeamCleared;
  }
  else if (localTeam == Team::blue) {
    isCleared = redTeamCleared;
  }
  else {
    // unknown
    isCleared = blueTeamCleared && redTeamCleared;
  }

  return isCleared;
}

const bool CombatBattleState::HasTimeFreeze() const {
  return hasTimeFreeze && !isPaused;
}

const bool CombatBattleState::RedTeamWon() const
{
  // we add `ComboDeleteSize() == 0` to our state check to give us a few more frames of display before switching states
  return GetScene().IsBlueTeamCleared() && GetScene().ComboDeleteSize() == 0;
}

const bool CombatBattleState::BlueTeamWon() const
{
  return GetScene().IsRedTeamCleared() && GetScene().ComboDeleteSize() == 0;
}

const bool CombatBattleState::PlayerDeleted() const
{
  return GetScene().IsPlayerDeleted();
}

const bool CombatBattleState::PlayerRequestCardSelect()
{
  BattleSceneBase& scene = GetScene();
  return !this->isPaused && scene.IsCustGaugeFull() && !IsMobCleared() && scene.GetLocalPlayer()->InputState().Has(InputEvents::pressed_cust_menu);
}

void CombatBattleState::EnablePausing(bool enable)
{
  canPause = enable;
}

void CombatBattleState::onStart(const BattleSceneState* last)
{
  BattleSceneBase& scene = GetScene();
  scene.HighlightTiles(true); // re-enable tile highlighting

  if ((scene.GetLocalPlayer()->GetHealth() > 0) && this->HandleNextRoundSetup(last)) {
    scene.StartBattleStepTimer();
    scene.GetField()->ToggleTimeFreeze(false);

    hasTimeFreeze = false;

    // reset bar and related flags
    scene.SetCustomBarProgress(0);
  }
}

void CombatBattleState::onEnd(const BattleSceneState* next)
{
  BattleSceneBase& scene = GetScene();

  // If the next state is not a substate of combat
  // then reset our combat and custom progress values
  if (this->HandleNextRoundSetup(next)) {
    scene.StopBattleStepTimer();

    // reset bar 
    scene.SetCustomBarProgress(0);
  }

  scene.HighlightTiles(false);
  hasTimeFreeze = false; 
}

void CombatBattleState::onUpdate(double elapsed)
{  
  BattleSceneBase& scene = GetScene();
  Player& player = *GetScene().GetLocalPlayer();

  if ((IsMobCleared() || player.GetHealth() == 0 )&& !clearedMob) {
    std::shared_ptr<PlayerSelectedCardsUI> cardUI = player.GetFirstComponent<PlayerSelectedCardsUI>();

    if (cardUI) {
      cardUI->Hide();
    }

    scene.BroadcastBattleStop();

    clearedMob = true;
  }

  if (canPause && Input().Has(InputEvents::pressed_pause) && !IsMobCleared()) {
    if (isPaused) {
      // unpauses
      // Require to stop the battle step timer and all battle-related component updates
      scene.StartBattleStepTimer();
    }
    else {
      // pauses
      Audio().Play(AudioType::PAUSE);

      // Require to start the timer again and all battle-related component updates
      scene.StopBattleStepTimer();
    }

    // toggles 
    isPaused = !isPaused;
  }

  if (isPaused) return; // do not update anything else

  scene.SetCustomBarProgress(scene.GetCustomBarProgress() + elapsed);

  // Update the field. This includes the player.
  // After this function, the player may have used a card.
  scene.GetField()->Update((float)elapsed);
}

void CombatBattleState::onDraw(IRenderer& renderer)
{
  BattleSceneBase& scene = GetScene();
  const int comboDeleteSize = scene.ComboDeleteSize();

  if (!scene.Countered()) {
    if (comboDeleteSize == 2) {
      renderer.submit(&doubleDelete);
    } else if(comboDeleteSize > 2) {
      renderer.submit(&tripleDelete);
    }
  }
  else {
    renderer.submit(&counterHit);
  }

  renderer.submit(&scene.GetCardSelectWidget());

  scene.DrawCustGauage(renderer);

  if (isPaused) {
    // render on top
    renderer.submit(&pause);
  }
}

void CombatBattleState::OnCardActionUsed(std::shared_ptr<CardAction> action, uint64_t timestamp)
{
  BattleSceneBase& scene = GetScene();
  // Only intercept this event if we are active
  if (scene.GetCurrentState() != this) return;

  Logger::Logf(LogLevel::debug, "CombatBattleState::OnCardActionUsed() on frame #%i", scene.FrameNumber().count());
  if (!IsMobCleared()) {
    hasTimeFreeze = action->GetMetaData().GetProps().timeFreeze;
  }
}

const bool CombatBattleState::HandleNextRoundSetup(const BattleSceneState* state)
{
  // Only stop battlestep timers and effects for states
  // that are not part of the combat state list
  return !IsStateCombat(state);
}

const bool CombatBattleState::IsStateCombat(const BattleSceneState* state)
{
  auto iter = std::find_if(subcombatStates.begin(), subcombatStates.end(),
    [state](const BattleSceneState* in) {
      return in == state;
    }
  );

  return (iter != subcombatStates.end()) || (state == this);
}
