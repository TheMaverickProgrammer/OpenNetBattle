#pragma once
#include "frame_time_t.h"
#include "bnDefenseRule.h"
#include <vector>
#include <functional>

struct IntangibleRule {
  frame_time_t duration { frames(120) };
  Hit::Flags hitWeaknesses { Hit::pierce_invis };
  std::vector<Element> elementWeaknesses;
  std::function<void()> onDeactivate;
};

class DefenseIntangible : public DefenseRule {
public:
  DefenseIntangible();
  ~DefenseIntangible();

  void Update();
  bool IsEnabled();
  void Enable(const IntangibleRule& rule);
  void Disable();
  bool Pierces(const Hit::Properties& statuses);
  bool TryPierce(const Hit::Properties& statuses);
  bool IsRetangible();

  Hit::Properties& FilterStatuses(Hit::Properties& statuses) override;
  void CanBlock(DefenseFrameStateJudge& judge, std::shared_ptr<Entity> attacker, std::shared_ptr<Entity> owner) override;

private:
  frame_time_t cooldown{};
  Hit::Flags hitWeaknesses{};
  std::vector<Element> elemWeaknesses;
  std::function<void()> onDeactivate;
  bool retangible;
};
