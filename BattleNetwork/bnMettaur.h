#pragma once
#include "bnCharacter.h"
#include "bnMettaurIdleState.h"
#include "bnAI.h"
#include "bnAnimationComponent.h"
#include "bnTextureType.h"
#include "bnMobHealthUI.h"
#include "bnTurnOrderTrait.h"

/*! \brief Basic mettaur enemy */
class Mettaur : public Character, public AI<Mettaur>, public TurnOrderTrait<Mettaur> {
  friend class MettaurIdleState;
  friend class MettaurMoveState;
  friend class MettaurAttackState;

public:
  using DefaultState = MettaurIdleState;

    /**
   * @brief Loads animations and gives itself a turn ID
   */
  Mettaur(Rank _rank = Rank::_1);

  /**
   * @brief Removes its turn ID from the list of active mettaurs
   */
  ~Mettaur();

  /**
   * @brief Uses AI state to move around. Deletes when health is below zero.
   * @param _elapsed in seconds
   */
  void OnUpdate(double _elapsed) override;

  void OnDelete() override;

  /**
   * @brief Get the hit height of this entity
   * @return const float
   */
  const float GetHeight() const override;

private:
  TextureType textureType;
  AnimationComponent* animation;
  DefenseRule* virusBody;
};
