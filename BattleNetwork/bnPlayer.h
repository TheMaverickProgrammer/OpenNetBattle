/*! \brief Player controlled net navi prefab 
 * 
 * This class is the foundation for all player controlled characters
 * To write a new netnavi inherit from this class
 * And simply load new sprites and animation file
 * And change some properties
 */

#pragma once

#include "bnCharacter.h"
#include "bnEmotions.h"
#include "bnPlayerState.h"
#include "bnTextureType.h"
#include "bnChargeEffectSceneNode.h"
#include "bnAnimationComponent.h"
#include "bnAI.h"
#include "bnPlayerControlledState.h"
#include "bnPlayerIdleState.h"
#include "bnPlayerHitState.h"
#include "bnPlayerChangeFormState.h"
#include "bnPlayerForm.h"
#include "bnDefenseSuperArmor.h"

#include <array>

using sf::IntRect;

class CardAction;

struct PlayerStats {
  static constexpr unsigned MAX_CHARGE_LEVEL = 5u;
  static constexpr unsigned MAX_ATTACK_LEVEL = 5u;

  unsigned charge{1}, attack{1};
  Element element{Element::none};
};

struct BusterEvent {
  frame_time_t deltaFrames{}; //!< e.g. how long it animates
  frame_time_t endlagFrames{}; //!< Wait period after completition

  // Default is false which is shoot-then-move
  bool blocking{}; //!< If true, blocks incoming move events for auto-fire behavior
  std::shared_ptr<CardAction> action{ nullptr };
};

class Player : public Character, public AI<Player> {
private:
  friend class PlayerControlledState;
  friend class PlayerNetworkState;
  friend class PlayerIdleState;
  friend class PlayerHitState;
  friend class PlayerChangeFormState;

  void SaveStats();
  void RevertStats();
  void CreateMoveAnimHash();
  void TagBaseNodes();

public:
  using DefaultState = PlayerControlledState;
  static constexpr size_t MAX_FORM_SIZE = 5;
  static constexpr const char* BASE_NODE_TAG = "Base Node";
  static constexpr const char* FORM_NODE_TAG = "Form Node";

   /**
   * @brief Loads graphics and adds a charge component
   */
  Player();
  
  /**
   * @brief deconstructor
   */
  virtual ~Player();

  /**
   * @brief Polls for interrupted states and fires delete state when deleted
   * @param _elapsed for secs
   */
  virtual void OnUpdate(double _elapsed);

  void MakeActionable() override final;
  bool IsActionable() const override final;

  /**
   * @brief Fires a buster
   */
  void Attack();

  void UseSpecial();

  void HandleBusterEvent(const BusterEvent& event, const ActionQueue::ExecutionType& exec);

  /**
   * @brief when player is deleted, changes state to delete state and hide charge component
   */
  virtual void OnDelete();

  virtual const float GetHeight() const;

  /**
   * @brief Get how many times the player has moved across the grid
   * @return int
   */
  int GetMoveCount() const;

  /**
   * @brief Toggles the charge component
   * @param state
   */
  void Charge(bool state);

  void SetAttackLevel(unsigned lvl);
  const unsigned GetAttackLevel();

  void SetChargeLevel(unsigned lvl);
  const unsigned GetChargeLevel();
  
  /** Sets the player's emotion. @note setting an emotion (e.g. full_synchro) does not trigger its effects, it merely
   * tracks state */
  void SetEmotion(Emotion emotion);
  Emotion GetEmotion() const;

  /**
   * @brief Set the animation and on finish callback
   * @param _state name of the animation
   */
  void SetAnimation(string _state, std::function<void()> onFinish = nullptr);
  const std::string GetMoveAnimHash();

  void SlideWhenMoving(bool enable = true, const frame_time_t& = frames(1));

  virtual CardAction* OnExecuteBusterAction() = 0;
  virtual CardAction* OnExecuteChargedBusterAction() = 0;
  virtual CardAction* OnExecuteSpecialAction();
  virtual frame_time_t CalculateChargeTime(const unsigned changeLevel);

  CardAction* ExecuteBuster();
  CardAction* ExecuteChargedBuster();
  CardAction* ExecuteSpecial();

  void ActivateFormAt(int index); 
  void DeactivateForm();
  const bool IsInForm() const;

  const std::vector<PlayerFormMeta*> GetForms();

  ChargeEffectSceneNode& GetChargeComponent();

  void OverrideSpecialAbility(const std::function<CardAction* ()>& func);

protected:
  // functions
  void FinishConstructor();
  bool RegisterForm(PlayerFormMeta* info);

  template<typename T>
  PlayerFormMeta* AddForm();

  // member vars
  string state; /*!< Animation state name */
  string moveAnimHash;
  frame_time_t slideFrames{ frames(1) };
  bool playerControllerSlide{};
  bool fullyCharged{}; //!< Per-frame value of the charge
  AnimationComponent* animationComponent;
  ChargeEffectSceneNode chargeEffect; /*!< Handles charge effect */

  std::vector<PlayerFormMeta*> forms;
  PlayerForm* activeForm{ nullptr };
  Emotion emotion{ Emotion::normal };
  PlayerStats stats{}, savedStats{};
  std::function<CardAction* ()> specialOverride{};
  DefenseSuperArmor* superArmor{ nullptr };
};

template<typename T>
PlayerFormMeta* Player::AddForm() {
  PlayerFormMeta* info = new PlayerFormMeta(forms.size()+1);
  info->SetFormClass<T>();
  
  if (!RegisterForm(info)) {
    delete info;
    info = nullptr;
  }

  return info;
}