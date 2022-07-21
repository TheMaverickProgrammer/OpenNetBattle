#pragma once
#include <functional>
#include "bnDefenseRule.h"

/**
 * @class DefenseBubbleStatus
 * @author mav
 * @date 05/05/19
 * @brief Used by BubbleComponent to spawn hitboxes when bubble is hit
 * 
 * Allows all attacks to hit and passthrough
 */
class DefenseBubbleStatus : public DefenseRule {
private:
  bool popped; /*!< whether or not the defense popped*/
public:
  DefenseBubbleStatus();
  ~DefenseBubbleStatus();
  
  const bool IsPopped() const;

  Hit::Properties& FilterStatuses(Hit::Properties& statuses) override;

  /**
   * @brief Allows all attacks to hit and pass 
   * @param in the attack spell
   * @param owner the character this is attached to
   */
  void CanBlock(DefenseFrameStateJudge& judge, std::shared_ptr<Entity> attacker, std::shared_ptr<Entity> owner) override;
};
