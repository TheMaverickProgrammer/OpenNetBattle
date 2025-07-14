#include "bnCharacterTransformBattleState.h"

#include "../bnBattleSceneBase.h"
#include "../../bnPlayer.h"
#include "../../bnSceneNode.h"
#include "../../bnAudioResourceManager.h"
#include "../../bnTextureResourceManager.h"

CharacterTransformBattleState::CharacterTransformBattleState()
{
  shine = sf::Sprite(*Textures().LoadFromFile(TexturePaths::MOB_BOSS_SHINE));
  shine.setScale(2.f, 2.f);
}

const bool CharacterTransformBattleState::FadeInBackdrop()
{
  return GetScene().FadeInBackdrop(backdropInc, 1.0, true);
}

const bool CharacterTransformBattleState::FadeOutBackdrop()
{
  return GetScene().FadeOutBackdrop(backdropInc);
}

void CharacterTransformBattleState::UpdateAnimation(double elapsed)
{
  bool allCompleted = true;

  size_t count = 0;
  for (std::shared_ptr<Player>& player : GetScene().GetAllPlayers()) {
    auto& [index, complete] = GetScene().GetPlayerFormData(player);

    if (complete) continue;

    allCompleted = false;

    player->Reveal(); // If flickering, stablizes the sprite for the animation

    // The list of nodes change when adding the shine overlay and new form overlay nodes
    std::shared_ptr<std::vector<SceneNode*>> originals(new std::vector<SceneNode*>());
    SmartShader* originalShader = &player->GetShader();

    // I found out structured bindings cannot be captured...
    int _index = index;

    auto onTransform = [=]
    () {
      // The next form has a switch based on health
      // This way dying will cancel the form
      player->ClearActionQueue();
      player->ActivateFormAt(_index);
      player->SetColorMode(ColorMode::additive);
      player->setColor(NoopCompositeColor(ColorMode::additive));
      
      if (auto animComp = player->GetFirstComponent<AnimationComponent>()) {
        animComp->Refresh();
      }

      if (_index == -1) {
        Audio().Play(AudioType::DEFORM);
      }
      else {
        player->MakeActionable();

        if (player == GetScene().GetLocalPlayer()) {
          // only client player should remove their index information (e.g. PVP battles)
          auto& widget = GetScene().GetCardSelectWidget();
          widget.LockInPlayerFormSelection();
          widget.ErasePlayerFormOption(_index);
        }

        GetScene().HandleCounterLoss(*player, false);
        Audio().Play(AudioType::SHINE);
      }

      player->SetShader(Shaders().GetShader(ShaderType::WHITE));
    };

    bool* completePtr = &complete;
    auto onFinish = [=]
    () {
      player->RefreshShader();
      *completePtr = true; // set tracking data `complete` to true
    };

    if (shineAnimations[count].GetAnimationString() != "SHINE") {
      shineAnimations[count] << "SHINE";

      if (index == -1) {
        // If decross, turn white immediately
        shineAnimations[count] << Animator::On(1, [=] {      
          player->SetShader(Shaders().GetShader(ShaderType::WHITE));
        }) << Animator::On(10, onTransform);
      }
      else {
        // Else collect child nodes on frame 10 instead
        // We do not want to duplicate the child node collection
        shineAnimations[count] << Animator::On(10, [onTransform] {
          onTransform();
        });
      }

      // Finish on frame 20
      shineAnimations[count] << Animator::On(20, onFinish);

    } // else, it is already "SHINE" so wait the animation out...

    // update for draw call later
    frameElapsed = elapsed;

    count++;
  }

  if (allCompleted) {
    currState = state::fadeout; // we are done
  }
}

void CharacterTransformBattleState::SkipBackdrop()
{
  skipBackdrop = true;
}

bool CharacterTransformBattleState::IsFinished() {
  if (state::fadeout == currState && FadeOutBackdrop()) {
    Logger::Log(LogLevel::net, "Transformation has finished");
  }
  return state::fadeout == currState && FadeOutBackdrop();
}

void CharacterTransformBattleState::onStart(const BattleSceneState*) {
  Logger::Log(LogLevel::net, "Transformation has started");
  if (skipBackdrop) {
    currState = state::animate;
  }
  else {
    currState = state::fadein;
  }

  skipBackdrop = false; // reset this flag
}

void CharacterTransformBattleState::onUpdate(double elapsed) {
  Logger::Log(LogLevel::net, "Transform state update");
  while (shineAnimations.size() < GetScene().GetAllPlayers().size()) {
    Animation animation = Animation("resources/scenes/battle/boss_shine.animation");
    animation.Load();
    shineAnimations.push_back(animation);
  }

  switch (currState) {
  case state::fadein:
    if (FadeInBackdrop()) {
      currState = state::animate;
    }
    break;
  case state::animate:
    UpdateAnimation(elapsed);
    break;
  }
}

void CharacterTransformBattleState::onEnd(const BattleSceneState*)
{
  for (auto&& anims : shineAnimations) {
    anims.SetAnimation(""); // ends the shine anim
  }

  frameElapsed = 0;
}

void CharacterTransformBattleState::onDraw(sf::RenderTexture& surface)
{
  BattleSceneBase& scene = GetScene();

  size_t count = 0;
  for (std::shared_ptr<Player>& player : scene.GetAllPlayers()) {
    auto& [index, complete] = scene.GetPlayerFormData(player);

    Animation& anim = shineAnimations[count];
    anim.Update(static_cast<float>(frameElapsed), shine);

    if (index != -1 && !complete) {
      // re-use the shine graphic for all animating player-types 
      const sf::Vector2f pos = player->getPosition();
      shine.setPosition(pos.x, pos.y - (player->GetHeight() / 2.0f));
      
      scene.DrawWithPerspective(shine, surface);
    }

    count++;
  }
}
