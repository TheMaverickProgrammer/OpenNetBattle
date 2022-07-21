#pragma once
#include <vector>

#include "bnArtifact.h"
#include "bnComponent.h"
#include "bnField.h"
#include "bnDefenseBubbleStatus.h"
#include "bnInputHandle.h"
/**
 * @class BubbleTrap
 * @author mav
 * @date 05/05/19
 * @brief Visual artifact (bubble animates) that can be attached to an entity
 * 
 * Creates a defense rule to absord first damage and forces this bubble to pop
 * On pop, frees owner and deletes self
 */
class BubbleTrap : public SpriteProxyNode, public Component, public ResourceHandle, public InputHandle
{
private:
  Animation animation;
  sf::Sprite bubble;
  double duration; /*!< when this reaches zero, pops */
  std::shared_ptr<DefenseBubbleStatus> defense; /*!< Add BubbleWrapTrap defense rule */
  bool willDelete;
  std::vector<InputEvent> lastFrameStates;
  bool init{};
public:
  /**
   * @brief Attaches to the owner, sets the animation */
  BubbleTrap(std::weak_ptr<Entity> owner);
  
  /**
   * @brief Does not inject into the battle scene
   * 
   */
  void Inject(BattleSceneBase&) override;
  
  /**
   * @brief Animators bubble. When timer runs out, pops the bubble
   * @param _elapsed in seconds
   */
  void OnUpdate(double _elapsed) override;

  /**
   * @brief Sets the pop animation and deletes self when over
   */
  void Pop();

  const double GetDuration() const;
};
