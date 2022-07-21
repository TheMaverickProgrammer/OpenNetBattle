#pragma once
#include "bnHitProperties.h"
#include "bnDefenseFrameStateJudge.h"
#include <memory>

class Entity;

enum class DefensePriority : uint8_t {
  Internal,
  Intangible,
  Barrier,
  Body,
  CardAction,
  Trap,
  Last, // special case, appends instead of replaces
};

enum class DefenseOrder : int {
  collisionOnly,
  always
};

/**
 * @class DefenseRule
 * @author mav
 * @date 05/05/19
 * @brief This class is used specifically for checks in the attack step
 * 
 * Build custom defense rules for special cards using this class
 * and then add them to entities
 */
class DefenseRule: public std::enable_shared_from_this<DefenseRule> {
private:
  DefenseOrder order; /*!< Some defenses only check if there was a collision */
  DefensePriority priorityLevel; /*!< Lowest priority goes first */
  bool replaced{}; /*!< If this rule has been replaced by another one in the entity*/
  bool added{};

public:
  friend class Entity;

  /**
  * @brief Constructs a defense rule with a priority level
  */
  DefenseRule(const DefensePriority level, const DefenseOrder& order);

  /**
  * @brief Returns the priority level of this defense rule
  */
  const DefensePriority GetPriorityLevel() const;

  /**
  * @brief Returns the defense order of this defense rule
  */
  const DefenseOrder GetDefenseOrder() const;

  /**
  * @brief True if the defense rule has been flagged for replacement by another with a priority level of equal value
  */
  const bool IsReplaced() const;

  /**
  * @brief True if the defense rule has been added to an entity
  */
  bool Added() const;

  /**
  * @brief Marks as `added`
  */
  void OnAdd();

  /**
  * @brief Frame-perfect cleanup code after being replaced
  */
  virtual void OnReplace();

  /**
  * @brief Deconstructor
  */
  virtual ~DefenseRule();

  /**
   * @brief Filters status effects before applying them to character. e.g. SuperArmor
   * @return reference to modified input statuses
   */
  virtual Hit::Properties& FilterStatuses(Hit::Properties& statuses);
  
  /**
    * @brief Returns false if the attacker passes through this defense, true if defense prevents it
    */
  virtual void CanBlock(DefenseFrameStateJudge& judge, std::shared_ptr<Entity> attacker, std::shared_ptr<Entity> owner) = 0;
};