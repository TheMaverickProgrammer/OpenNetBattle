#pragma once

#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>
#include <Swoosh/Renderers/Renderer.h>
#include <array>
#include <memory>

#include "bnResourceHandle.h"
#include "bnText.h"
#include "bnFont.h"
#include "bnEmotions.h"
#include "bnCardPackageManager.h"

class Mob;
class BattleItem;

struct BattleResults {
  struct MobData {
    std::string id;
    int health{};
  };

  int moveCount{};
  int hitCount{};
  int turns{};
  int counterCount{};
  int score{};
  int playerHealth{};
  bool doubleDelete{};
  bool tripleDelete{};
  bool runaway{true};
  bool synchro{};
  Emotion finalEmotion{};
  sf::Time battleLength;
  std::vector<MobData> mobStatus;
  static BattleResults& CalculateScore(BattleResults& results, Mob* mob);
};

/**
 * @class BattleResultsWidget
 * @author mav
 * @date 13/05/19
 * @brief BattleResultsWidget is a modal that appears at the end of the BattleScene when Player wins
 *
 * Exposes an API to interact with the modal
 */
class BattleResultsWidget : public ResourceHandle {
private:
  sf::Sprite resultsSprite; /*!< This modals graphic */
  sf::Sprite rewardCard; /*!< Reward card graphics */
  sf::Sprite pressA; /*!< Press A sprite */
  sf::Sprite star; /*!< Counter stars */
  Text time; /*!< Formatted time label */
  Text rank; /*!< Battle scored rank */
  Text reward; /*!< Name of reward */
  Text cardCode; /*!< Code for cards */

  bool isRevealed{}; /*!< Flag if modal is revealed */
  bool playSoundOnce{}; /*!< Flag to play sounds once */
  bool rewardIsCard{}; /*!< Is current reward a card */

  BattleItem* item{ nullptr }; /*!< The item stored in this modal */
  int score{}; /*!< 1-10 or 11+ as S rank */
  int counterCount{}; /*!< How many times player countered */
  int finalHealth{}; /*!< Player's health at EOB*/
  double totalElapsed{}; /*!< delta time this frame */

  std::array<int, 7*6> hideCardMatrix; /*~< blocks are 7x6 block space to uncover at 8x8 pixels*/
  int cardMatrixIndex{};

  /**
   * @brief Format the time to look like BN time stamp
   * @param time
   * @return formatted string
   */
  std::string FormatString(sf::Time time);

public:
  /**
   * @brief Uses MMBN6 algorithm to score player based on various moves
   * @param battleLength duration of the battle
   * @param moveCount how many times player moved
   * @param hitCount how many times player got hit
   * @param playerHealth final health for player at EOB
   * @param counterCount how many time player countered enemies
   * @param doubleDelete if player double deleted enemies
   * @param tripleDelete if player triple deleted enemies
   * @param runaway if player fled from battle
   * @param mob extra mob information e.g. if considered boss mob or common mob
   */
  BattleResultsWidget(const BattleResults& results, Mob* mob, CardPackageManager& packageManager);
  ~BattleResultsWidget();

  /**
   * @brief Confirms actions
   * @return true if successful, false otherwise
   */
  bool CursorAction();

  /**
   * @brief Query if modal is out of view
   * @return true if out of view at target destination, false otherwise
   */
  bool IsOutOfView();

  /**
   * @brief Query if modal is in view
   * @return true if in view at target destination, false otherwise
   */
  bool IsInView();

  /**
   * @brief Offset the modal on screen by delta pixels
   * @param delta
   */
  void Move(sf::Vector2f delta);

  /**
   * @brief Update modal and animations
   * @param elapsed in seconds
   */
  void Update(double elapsed);

  /**
   * @brief Perform draw steps
   */
  void Draw(IRenderer& renderer);

  /**
   * @brief Query if BattleRewards modal has completed all rewards
   * @return true if no more rewards, false if there are more the player needs to retrieve
   */
  bool IsFinished();

  /**
   * @brief Get the current reward item
   * @return BattleItem* if an item exists, nullptr if no reward was granted
   *
   * Will update the internal pointer to the next reward or nullptr if no more rewards
   */
  BattleItem* GetReward();
};

