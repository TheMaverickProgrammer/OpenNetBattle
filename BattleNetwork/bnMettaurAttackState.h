#pragma once
#include "bnAIState.h"
#include "bnMettaur.h"

/*! \brief Sets animation to attack and adds callback to spawn wave 
 */
class MettaurAttackState : public AIState<Mettaur>
{
  bool usingCard = false;

public:
 
  /**
   * @brief constructor
   */
  MettaurAttackState();
  
  /**
   * @brief deconstructor
   */
  ~MettaurAttackState();

  /**
   * @brief Sets animation and adds callback to fire DoAttack()
   * @param met
   */
  void OnEnter(Mettaur& met);
  
  /**
   * @brief Does nothing
   * @param _elapsed
   * @param met
   */
  void OnUpdate(double _elapsed, Mettaur& met);
  
  /**
   * @brief Passes control to next mettaur in the list
   * @param met
   */
  void OnLeave(Mettaur& met);

  /**
   * @brief Spawns a wave spell and assigns this met as the aggressor
   * @param met
   */
  void DoAttack(Mettaur& met);
};

