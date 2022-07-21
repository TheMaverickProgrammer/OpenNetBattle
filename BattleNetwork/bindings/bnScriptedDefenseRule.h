#ifdef ONB_MOD_SUPPORT

#pragma once
#include "../bnDefenseRule.h"
#include "dynamic_object.h"

class ScriptedDefenseRule final : public DefenseRule, public dynamic_object {

public:
  /**
  * @brief Constructs a defense rule with a priority level
  */
  ScriptedDefenseRule(const DefensePriority level, const DefenseOrder& order);

  /**
  * @brief Deconstructor
  */
  ~ScriptedDefenseRule();

  /**
   * @brief Filters status effects before applying them to character. e.g. SuperArmor
   * @return reference to modified input statuses
   */
  Hit::Properties& FilterStatuses(Hit::Properties& statuses) override;
  
  /**
    * @brief Returns false if spell passes through this defense, true if defense prevents it
    */
  void CanBlock(DefenseFrameStateJudge& judge, std::shared_ptr<Entity> attacker, std::shared_ptr<Entity> owner) override;

  /**
  * @brief Frame-perfect cleanup code after being replaced
  */
  void OnReplace() override;

  sol::object filter_statuses_func;
  sol::object can_block_func;
  sol::object on_replace_func;
};

#endif
