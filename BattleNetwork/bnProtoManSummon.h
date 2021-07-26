/*! \brief Protoman appears and attacks every enemy he can reach
 * 
 */

#pragma once
#include "bnArtifact.h"
#include "bnAnimationComponent.h"

class Character;

class ProtoManSummon : public Artifact {
public:
  
  /**
   * \brief Scans for enemies. Checks to see if protoman can
   * spawn in front of them. If so, the tile is stored
   * as targets.*/
  ProtoManSummon(Character* user, int damage);
  
  /**
   * @brief deconstructor
   */
  ~ProtoManSummon();

  /**
   * @brief Updates position and animation
   * @param _elapsed in seconds
   */
  void OnUpdate(double _elapsed) override;
  
  /**
   * @brief Attacks the next enemy when the animation ends
   * 
   * Configures the animation callback to approach the next tile
   * And attack the tile in front
   * If the animation ends and there are no more targets,
   * delete protoman
   */
  void DoAttackStep();

  /**
  * @brief Does nothing
  */
  void OnDelete() override;

  void OnSpawn(Battle::Tile& start) override;

  void DropHitboxes(Battle::Tile& tile);

private:
  std::vector<Battle::Tile*> targets; /*!< List of every tile ProtoMan must visit */
  int damage{};
  Character* user{ nullptr };
  AnimationComponent* animationComponent;
};
