/*! \brief Displays the cards above the player, publishes use card on behalf of the player, and 
      can spread the cards out using a special key binding
 */
#pragma once
#include <SFML/Graphics.hpp>
#include "bnInputHandle.h"
#include "bnSelectedCardsUI.h"

using sf::Sprite;
using sf::Texture;
using sf::Drawable;

class Player;
class Card;
class BattleSceneBase;

class PlayerSelectedCardsUI final : public SelectedCardsUI, public InputHandle {
public:
  /**
   * \brief Loads the graphics and sets spread duration to .2 seconds
   * \param _player the player to attach to
   */
  PlayerSelectedCardsUI(Player* _player);
  
  /**
   * @brief destructor
   */
  ~PlayerSelectedCardsUI();

  void draw(sf::RenderTarget & target, sf::RenderStates states) const override final;

  /**
   * @brief Hold START to spread the cards out
   * @param _elapsed in seconds
   */
  void OnUpdate(double _elapsed) override;

  /**
 * @brief Broadcasts the card information to all listeners
 * @param card being used
 * @param user using the card
 */
  void Broadcast(const Battle::Card& card, Character& user) override final;

private:
  double elapsed{}; /*!< Used by draw function, delta time since last update frame */
  mutable double interpolTimeFlat{}; /*!< Interpolation time for spread cards */
  mutable double interpolTimeDest{}; /*!< Interpolation time for default card stack */
  bool spread{ false }; /*!< If true, spread the cards, otherwise stack like the game */
  mutable bool firstFrame{ true }; /*!< If true, this UI graphic is being drawn for the first time*/
  sf::Time interpolDur; /*!< Max duration for interpolation 0.2 seconds */
  Player* player{ nullptr }; /*!< Player this component is attached to */
  mutable Text text; /*!< Text displays card name */
  mutable Text multiplier;
  mutable Text dmg; /*!< Text displays card damage */
};
