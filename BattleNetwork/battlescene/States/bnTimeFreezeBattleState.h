#pragma once

#include <Swoosh/Timer.h>
#include <SFML/Graphics/Font.hpp>
#include <memory>
#include "../bnBattleSceneState.h"
#include "../../bnTeam.h"
#include "../../bnFont.h"
#include "../../bnText.h"
#include "../../bnCardAction.h"
#include "../../bnCardUseListener.h"
#include "../../frame_time_t.h"

class Character;
namespace Battle { class Card; }

/* 
  \brief This state governs transitions and combat rules while in TF
*/
struct TimeFreezeBattleState final : public BattleSceneState, CardActionUseListener {
  enum class state : int {
    fadein = 0,
    display_name,
    animate,
    fadeout
  } currState{ state::fadein }, startState{ state::fadein };

  struct EventData {
    std::string name;
    Team team{ Team::unknown };
    std::shared_ptr<Character> user{ nullptr }, stuntDouble{ nullptr };
    std::shared_ptr<CardAction> action{ nullptr };
    frame_time_t alertFrameCount{ frames(0) };
    bool counterStart{}, animateCounter{};
  };

  bool summonStart{}, playerCountered{};
  long long lockedTimestamp{ 0 };
  frame_time_t alertAnimFrames{ frames(20) };
  frame_time_t fadeInOutLength{ frames(6) };
  frame_time_t tfcStartFrame{ frames(10) };
  frame_time_t summonTextLength{ frames(60) }; /*!< How long TFC label should stay on screen */
  frame_time_t summonTick{ frames(0) };
  double backdropInc{ 1.25 }; //!< alpha increase per frame (max 255)
  std::vector<EventData> tfEvents;
  Text summonsLabel = Text(Font::Style::thick);
  mutable Text multiplier;
  mutable Text dmg; /*!< Text displays card damage */

  TimeFreezeBattleState();
  ~TimeFreezeBattleState();

  void OnCardActionUsed(std::shared_ptr<CardAction> action, uint64_t timestamp) override;
  void HandleTimeFreezeCounter(std::shared_ptr<CardAction> action, uint64_t timestamp);
  const bool CanCounter(std::shared_ptr<Character> user);
  void CleanupStuntDouble();
  void SkipToAnimateState();
  void ProcessInputs();
  void onStart(const BattleSceneState* last) override;
  void onEnd(const BattleSceneState* next) override;
  void onUpdate(double elapsed) override;
  void onDraw(sf::RenderTexture& surface) override;
  void ExecuteTimeFreeze();
  const bool FadeOutBackdrop();
  const bool FadeInBackdrop();
  bool IsOver();
  
  void DrawCardData(const sf::Vector2f& pos, const sf::Vector2f& scale, sf::RenderTarget& target);

  std::shared_ptr<Character> CreateStuntDouble(std::shared_ptr<Character> from);
};
