#pragma once
#include "bnEntity.h"
#include "bnAIState.h"
#include "bnExplosionSpriteNode.h"
#include "bnAnimationComponent.h"
#include "bnShaderResourceManager.h"

/**
 * @class ExplodeState
 * @author mav
 * @date 04/05/19
 * @brief Locks an entity into this state and spawns explosions
 * 
 * This state can be used by any Entity in the engine. 
 * It uses constraints to ensure the type passed in Any 
 * is a subclass of Entity. 
 *
 * This state spawns an explosion and flickers the 
 * entity at it's current animation. Once the explosion
 * is finished, the entity is tried for deletion. Since 
 * this state is used when health < 0, the deletion will
 * succeed.
 */
template<typename Any>
class ExplodeState : public AIState<Any>
{
protected:
  std::shared_ptr<ExplosionSpriteNode> explosion; /*!< The root explosion object */
  sf::Shader* whiteout{ nullptr }; /*!< Flash the dying entity white */
  double elapsed{};
  int numOfExplosions{}; /*!< Number of explosions to spawn */
  double playbackSpeed{}; /*!< how fast the animation should be */

  void CleanupExplosions(Any& e);

public:
  inline static const int PriorityLevel = 0; // Highest

  ExplodeState(int _numOfExplosions=2, double _playbackSpeed=1.0);
  virtual ~ExplodeState();

  void OnEnter(Any& e);
  void OnUpdate(double _elapsed, Any& e);
  void OnLeave(Any& e);
};

#include "bnField.h"
#include "bnLogger.h"

template<typename Any>
ExplodeState<Any>::ExplodeState(int _numOfExplosions, double _playbackSpeed) : 
  elapsed(0), 
  explosion(nullptr), 
  numOfExplosions(_numOfExplosions), 
  playbackSpeed(_playbackSpeed), 
  AIState<Any>() {
  whiteout = ResourceHandle{}.Shaders().GetShader(ShaderType::WHITE);
  this->PriorityLock();
}

template<typename Any>
ExplodeState<Any>::~ExplodeState() {
}

template<typename Any>
void ExplodeState<Any>::OnEnter(Any& e) {
  e.SetPassthrough(true); // Shoot through dying enemies

  /* explode over the sprite */
  explosion = std::make_shared<ExplosionSpriteNode>(&e, numOfExplosions, playbackSpeed);
  e.AddNode(explosion);
  
  // Define the area relative to origin to spawn explosions around
  // based on a fraction of the current frame's size
  sf::Vector2f area = sf::Vector2f(e.getLocalBounds().width / 4.0f, e.getLocalBounds().height*0.8f);

  explosion->SetOffsetArea(area);

  std::shared_ptr<AnimationComponent> animation = e.template GetFirstComponent<AnimationComponent>();

  if (animation) {
    animation->SetPlaybackSpeed(0);
    animation->CancelCallbacks();
  }

  e.Reveal();
}

template<typename Any>
void ExplodeState<Any>::OnUpdate(double _elapsed, Any& e) {
  elapsed += _elapsed;

  // flicker white every 2 frames 
  unsigned frame = from_seconds(elapsed).count() % 4;
  if (frame < 2) {
    e.SetShader(whiteout);
  }

  /* If root explosion is over, finally remove the entity that entered this state
     This ends the effect
     */
  if (explosion) {
    explosion->Update(_elapsed);

    if (explosion->IsSequenceComplete()) {
      e.Erase();
    }
  }
}

template<typename Any>
void ExplodeState<Any>::OnLeave(Any& e) {
  CleanupExplosions(e);
}

template<typename Any>
inline void ExplodeState<Any>::CleanupExplosions(Any& e)
{
  if (explosion == nullptr) return;

  for (std::shared_ptr<ExplosionSpriteNode>& element : explosion->GetChain()) {
    e.RemoveNode(element);
  }

  e.RemoveNode(explosion);

  explosion = nullptr;
}
