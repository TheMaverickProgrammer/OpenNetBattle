#ifdef ONB_MOD_SUPPORT
#include "bnScriptedDefenseRule.h"
#include "bnWeakWrapper.h"
#include "bnScopedWrapper.h"
#include "../bnSolHelpers.h"
#include "../bnLogger.h"

ScriptedDefenseRule::ScriptedDefenseRule(const DefensePriority level, const DefenseOrder& order) : 
    DefenseRule(level, order)
{
}

ScriptedDefenseRule::~ScriptedDefenseRule() { }

Hit::Properties& ScriptedDefenseRule::FilterStatuses(Hit::Properties& statuses)
{
  if (filter_statuses_func.valid()) 
  {
    // create a copy to protect against a scripter holding on to this variable for too long
    auto statusCopy = statuses;

    auto result = CallLuaCallbackExpectingValue<Hit::Properties>(filter_statuses_func, statusCopy);

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
      return statuses;
    }

    // overwrite with the copy
    statuses = result.value();
  }

  return statuses;
}

void ScriptedDefenseRule::CanBlock(DefenseFrameStateJudge& judge, std::shared_ptr<Entity> attacker, std::shared_ptr<Entity> owner) {
  if (can_block_func.valid()) 
  {
    auto wrappedJudge = ScopedWrapper(judge);
    auto result = CallLuaCallback(can_block_func, wrappedJudge, WeakWrapper(attacker), WeakWrapper(owner));

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }
}

void ScriptedDefenseRule::OnReplace() {
  if (on_replace_func.valid()) 
  {
    auto result = CallLuaCallback(on_replace_func);

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }
}

#endif
