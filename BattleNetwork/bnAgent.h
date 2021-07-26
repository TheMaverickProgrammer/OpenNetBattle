#pragma once

class Character;

/**
 * @class Agent
 * @author mav
 * @date 06/05/19
 * @file bnAgent.h
 * @brief Agents pursue a target
 */
class Agent {
private:
  Character* target;
public:
  void SetTarget(Character* _target) {
    target = _target;
  }

  void FreeTarget() {
    SetTarget(nullptr);
  }

  Character* GetTarget() const { return target; }
};