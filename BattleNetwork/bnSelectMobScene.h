/*! \brief Loads a scene to select the mob to battle 
 * 
 * Much like the SelectNaviScene, user can press LEFT/RIGHT to change 
 * the mob. Information about the mob displays on the right.
 * The name and data scramble for effects.
 * 
 * A textbox types out the extra information about the mob while a mugshot
 * animated along with the text
 */

#pragma once
#include <time.h>
#include <SFML/Graphics.hpp>

#include "bnPlayerPackageManager.h"
#include "bnTextArea.h"
#include "bnTile.h"
#include "bnField.h"
#include "bnPlayer.h"
#include "bnMob.h"
#include "bnCamera.h"
#include "bnInputManager.h"
#include "bnAudioResourceManager.h"
#include "bnShaderResourceManager.h"
#include "bnTextureResourceManager.h"
#include "bnDrawWindow.h"
#include "battlescene/bnBattleSceneBase.h"
#include "bnMobFactory.h"
#include "bnScene.h"

#include "Segues/CrossZoom.h" // <-- GPU intensive and runs slowly on old hardware
#include "Segues/WhiteWashFade.h"
#include "Segues/BlackWashFade.h"

using sf::RenderWindow;
using sf::VideoMode;
using sf::Clock;
using sf::Event;

class SelectMobScene : public Scene
{
public:
  struct Properties {
    std::string naviId;
    std::unique_ptr<CardFolder> folder;
    PA& pa;
    std::shared_ptr<Background> background{ nullptr };
  };

private:
  std::string selectedNaviId; /*!< The selected navi */
  std::shared_ptr<Background> defaultBackground; /*!< Inherited background from the previous scene */

  PA& programAdvance;
  
  std::unique_ptr<CardFolder> selectedFolder; /*!< Reference to the selected folder */

  Font font; /*!< Menu title font */
  Text menuLabel; /*!< "Mob Select" */

  double maxSelectInputCooldown; /*!< Maximum time for input delay */
  double selectInputCooldown; /*!< Remaining time for input delay */
  double elapsed; /*!< delta seconds since last frame */
  
  Font uiFont; /*!< UI font */
  Text mobLabel; /*!< name */
  Text attackLabel; /*!< power */
  Text speedLabel; /*!< mob speed */
  Text hpLabel; /*!< mob total health */

  float maxNumberCooldown; /*!< Maximum time for the scramble effect */
  float numberCooldown; /*!< Remaining time for scramble effect */

  bool doOnce; /*!< Flag to trigger pixelate and scramble effects */
  bool showMob;

#ifdef __ANDROID__
  bool canSwipe;
  bool touchStart;

  int touchPosX;
  int touchPosStartX;

  bool releasedB;

  void StartupTouchControls();
  void ShutdownTouchControls();
#endif

  SpriteProxyNode bg; /*!< Background sprite */

  SpriteProxyNode mobSpr; /*!< Current mob sprite */

  SpriteProxyNode cursor; /*!< LEFT / RIGHT cursors */

  SpriteProxyNode navigator; /*!< Mugshot spritesheet */
  
  Animation navigatorAnimator; /*!< Animators mugshot */

  bool gotoNextScene; /*!< Cannot interact with scene if true */

  SmartShader shader; /*!< Pixelate shader effect*/
  
  float factor; /*!< Pixelate factor amount */

  std::string mobSelectionId; /*!< Mob selected */

  TextArea textbox; /*!< textbox message */

public:
  /**
   * @brief Loads graphics and sets original state of all items
   */
  SelectMobScene(swoosh::ActivityController&, Properties props);
  
  /**
   * @brief Deletes all allocated resource. If mob is non null, deletes the mob
   */
  ~SelectMobScene();

  /**
   * @brief sets gotoNextScene to false, allowing the user to interact
   * If mob is non null (from battling), deletes the mob
   */
  void onResume() override;
  
  /**
   * @brief Animators and accepts user input: LEFt/RIGHT A to battle, B to return
   * @param elapsed
   */
  void onUpdate(double elapsed) override;
  
  /**
   * @brief Draws the scene
   * @param surface
   */
  void onDraw(IRenderer& renderer) override;
  
  /**
   * @brief Unpauses textbox, triggers special effects to perform again
   */
  void onStart() override;
  
  /**
   * @brief Pauses text box
   */
  void onLeave() override;
  
  /**
   * @brief Clears the text box
   */
  void onExit() override;
  
  /**
   * @brief nothing
   */
  void onEnter() override;
  
  /**
   * @brief nothing
   */
  void onEnd() override;
};

