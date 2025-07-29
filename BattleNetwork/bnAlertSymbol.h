#pragma once
#include "bnArtifact.h"
#include "bnAnimationComponent.h"

class Field;

/**
 * @class AlertSymbol
 * @author mav
 * @date 04/05/19
 * @brief <!> symbol that appears on field when elemental damage occurs
 */
class AlertSymbol : public Artifact
{
private:
  double progress;

public:
  AlertSymbol();
  ~AlertSymbol();

  /**
   * @brief Grow and shrink quickly. Appear over the sprite.
   * @param _elapsed
   */
  void OnUpdate(double _elapsed) override;

  void OnDelete() override;
};
