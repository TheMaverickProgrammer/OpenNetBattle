/*! \brief Displays player health in a box for the main player 
 * 
 * UIComponent is drawn on top of everything else in the scene.
 * The health uses glyphs from a text file to look identical to
 * the source material. The glyphs use the green or orange rows
 * when the player is healing or taking damage respectively.
 * 
 * When player health drops low, a beep plays with HIGH priority
 * 
 * When the battle is over, the BattleOverTrigger is activated
 * and sets the internal flag to false which stops beeping
 */

#pragma once
#include <SFML/Graphics.hpp>
#include <sstream>
#include <vector>

#include "bnPlayer.h"
#include "bnUIComponent.h"
#include "bnText.h"

class Entity;
class Player;
class BattleSceneBase;

using sf::Texture;
using sf::Drawable;
using std::vector;
using std::ostringstream;

class PlayerHealthUI : public SceneNode {
public:
  PlayerHealthUI();
  ~PlayerHealthUI();

  void SetFontStyle(Font::Style style);
  void SetHP(int hp);
  void Update(double elapsed);
  void draw(sf::RenderTarget& target, sf::RenderStates states) const override final;

private:
  int targetHP{}; /*!< target value */
  int lastHP{}; /*!< HP of target last frame */
  int currHP{}; /*!< HP of target current frame */
  int startHP{}; /*!< HP of target when this component was attached */
  double cooldown{}; /*!< timer to colorize the health. Set to 0.5 seconds */
  mutable Text glyphs; /*!< numbers to draw */
  SpriteProxyNode uibox; /*!< the box surrounding the health */
  std::shared_ptr<Texture> texture; /*!< the texture of the box */
};

class PlayerHealthUIComponent : public UIComponent {
  BattleSceneBase* scene{ nullptr };
  PlayerHealthUI ui;
  int startHP{}; /*!< HP of target when this component was attached */
  bool isBattleOver{}; /*!< flag when battle scene ends to stop beeping */
public:
  /**
   * \brief Sets the player owner. Sets hp tracker to current health.
   */
  PlayerHealthUIComponent(std::weak_ptr<Player> _player);
  
  /**
   * @brief No memory needs to be freed
   */
  ~PlayerHealthUIComponent();

  /**
   * @brief This component does not need to be injected into the scene
   * @param scene
   */
  void Inject(BattleSceneBase& scene) override;

  /**
   * @brief Uses bitmap glyphs for each number in the health
   * @param target
   * @param states
   */
  void draw(sf::RenderTarget & target, sf::RenderStates states) const override;
  
  /**
   * @brief Quickly depletes health based on game rules and beeps if health is low
   * @param elapsed in seconds
   */
  void OnUpdate(double elapsed) override;
};
