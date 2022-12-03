#pragma once

#include "../bnBattleSceneState.h"

// modes for fading out style
enum class FadeOut : int {
    white = 0,
    black,
    pixelate
};

class Player;

/* 
    \brief This structure accepts the battle scene as input and asks to quit
    This is a dummy state because the constructor asks the scene in return to use its 
    Quit() routine
*/
class FadeOutBattleState final : public BattleSceneState {
  bool keepPlaying{ true };
  FadeOut mode;
  double wait{ 2 }; // in seconds
public:
  FadeOutBattleState(const FadeOut& mode);

  void onStart(const BattleSceneState* last) override;
  void onEnd(const BattleSceneState* next) override;
  void onUpdate(double);
  void onDraw(IRenderer& renderer);
  void EnableKeepPlaying(bool enable);
};