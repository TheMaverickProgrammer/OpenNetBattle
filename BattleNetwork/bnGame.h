#pragma once
#include <Swoosh/ActivityController.h>
#include <atomic>
#include <thread>
#include <filesystem>

#include "cxxopts/cxxopts.hpp"
#include "bnTaskGroup.h"
#include "bnDrawWindow.h"
#include "bnConfigReader.h"
#include "bnConfigSettings.h"
#include "bnSpriteProxyNode.h"
#include "bnAnimation.h"
#include "bnNetManager.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnShaderResourceManager.h"
#include "bnInputManager.h"
#include "bnPackageManager.h"

#define APP_NAME "OpenNetBattle"

#define ONB_REGION_JAPAN 0
#define ONB_ENABLE_PIXELATE_GFX 0

// GBA draws 60 frames in one seconds
#define FIXED_TIME_STEP 1.0f/60.0f

/*! \file  bnGame.cpp
 *  \brief Main entry for the application.
 *         Encapsulates all resources and loading steps for the game.
 *
 * The bootup step loads resources in real-time
 * and prints status information for each resource in the background
 * of the title screen.
 *
 * All queued 3rd party plugins for navis, cards, and mobs are
 * parsed and loaded into the resource managers after primary
 * resources are loaded.
 *
 * Once all items are loaded, the user is allowed to press start and
 * play. From there, the Swoosh ActivityController controls the state
 * of the app until the user quits. Afterwards all resources
 * are cleaned up.
 */

using swoosh::ActivityController;

// forward decl.
class GameSession;
class CardPackagePartitioner;
class PlayerPackagePartitioner;
class BlockPackagePartitioner;
class MobPackagePartitioner;
class LuaLibraryPackagePartitioner;

enum class Endianness : short {
  big = 0,
  little
};

class Game final : public ActivityController {
private:
  unsigned int randSeed{};
  double mouseAlpha{};
  bool showScreenBars{};
  bool frameByFrame{}, isDebug{}, quitting{ false };
  bool singlethreaded{ false };
  bool isRecording{}, isRecordOutSaving{}, recordPressed{};
  const char* appName{ APP_NAME };

  TextureResourceManager textureManager;
  AudioResourceManager audioManager;
  ShaderResourceManager shaderManager;
#ifdef BN_MOD_SUPPORT 
  ScriptResourceManager scriptManager;
#endif
  InputManager inputManager;
  NetManager netManager;

  CardPackagePartitioner* cardPackagePartitioner{ nullptr };
  PlayerPackagePartitioner* playerPackagePartitioner{ nullptr };
  MobPackagePartitioner* mobPackagePartitioner{ nullptr };
  BlockPackagePartitioner* blockPackagePartitioner{ nullptr };
  LuaLibraryPackagePartitioner* luaLibraryPackagePartitioner{ nullptr };

  DrawWindow& window;
  ConfigReader reader;
  ConfigSettings configSettings;
  GameSession* session;

  // mouse stuff
  std::shared_ptr<sf::Texture> mouseTexture;
  SpriteProxyNode mouse;
  sf::Vector2f lastMousepos;
  Animation mouseAnimation;

  // loading spinner
  SpriteProxyNode spinner;
  Animation spinnerAnimator;

  sf::Shader* postprocess{ nullptr };

  // We need a render surface to draw to so Swoosh ActivityController
  // can add screen transition effects from the title screen
  sf::RenderTexture renderSurface;

  // total elapsed frame time
  frame_time_t elapsed{};

  Endianness endian{ Endianness::big };
  std::vector<cxxopts::KeyValue> commandlineArgs; /*!< User-provided values from the command line*/
  cxxopts::ParseResult const* commandline{ nullptr }; /*!< Final values parsed from the command line configuration*/
  std::vector<std::pair<unsigned, sf::Image>> recordedFrames;
  std::atomic<int> progress{ 0 };
  std::mutex windowMutex;
  std::thread renderThread, recordOutThread;

  void HandleRecordingEvents();
  void UpdateMouse(double dt);
  void ProcessFrame();
  void RunSingleThreaded();
  bool NextFrame();

public:
  Game(DrawWindow& window);
  Game(const Game&) = delete;
  ~Game();

  /**
   * @brief Load all resources required by game
   * @param values command-line values as ParseResult object from cxxopts lib
   */
  TaskGroup Boot(const cxxopts::ParseResult& values);

  void Exit();
  void Run();
  void SetWindowMode(DrawWindow::WindowMode mode);
  void Postprocess(ShaderType shaderType);
  void NoPostprocess();
  void PrintCommandLineArgs();
  const sf::Vector2f CameraViewOffset(Camera& camera);
  unsigned FrameNumber() const;
  const Endianness GetEndianness();
  void UpdateConfigSettings(const struct ConfigSettings& new_settings);
  void SeedRand(unsigned int seed);
  const unsigned int GetRandSeed() const;
  bool IsSingleThreaded() const;
  bool IsRecording() const;
  void Record(bool enabled = true);
  void SetSubtitle(const std::string& subtitle);

  const std::filesystem::path AppDataPath();
  const std::filesystem::path CacheDataPath();
  const std::filesystem::path DesktopPath();
  const std::filesystem::path DownloadsPath();
  const std::filesystem::path DocumentsPath();
  const std::filesystem::path VideosPath();
  const std::filesystem::path PicturesPath();
  const std::filesystem::path SaveGamesPath();

  CardPackagePartitioner& CardPackagePartitioner();
  PlayerPackagePartitioner& PlayerPackagePartitioner();
  MobPackagePartitioner& MobPackagePartitioner();
  BlockPackagePartitioner& BlockPackagePartitioner();
  LuaLibraryPackagePartitioner& LuaLibraryPackagePartitioner();

  static char* LocalPartition;
  static char* RemotePartition;
  static char* ServerPartition;
  static char* Version;

  ConfigSettings& ConfigSettings();
  GameSession& Session();

  /**
   * @brief Store parsed command line values into the engine for easy access
   * @param values ParseResult object from cxxopts lib
   */
  void SetCommandLineValues(const cxxopts::ParseResult& values);

  /**
   * @brief Returns a value from the command line as type T
   * @param key String key for the command line arg
   * @return value for key as type T. If none are found T{} is returned.
   */
  template<typename T>
  const T CommandLineValue(const std::string& key) {
    return (*commandline)[key].as<T>();
  }

  // todo: remove when this is public from swoosh
  const swoosh::Activity* getCurrentActivity() const {
    return ActivityController::getCurrentActivity();
  }
private:
  DrawWindow::WindowMode windowMode{};

  void LoadConfigSettings();

  /*! \brief This thread initializes all navis
  *
  * Uses an std::atomic<int> pointer
  * to keep track of all successfully loaded
  * objects.
  *
  * After the media resources are loaded we
  * safely load all registered navis.
  * Loaded navis will show up in navi select
  * screen and can be chosen to play as in
  * battle.
  */
  void RunNaviInit(std::atomic<int>* progress);

  /*! \brief This thread initializes all cards
  *
  * Uses an std::atomic<int> pointer
  * to keep track of all successfully loaded
  * objects.
  *
  * After the media resources are loaded we
  * safely load all registered cards.
  * Loaded cards will show up in available library
  * screen and can be chosen to play with in
  * battle.
  */
  void RunCardInit(std::atomic<int>* progress);

  /*! \brief This thread initializes all prog blocks
  *
  * Uses an std::atomic<int> pointer
  * to keep track of all successfully loaded
  * objects.
  *
  * After the media resources are loaded we
  * safely load all registered prog blocks.
  * Loaded prog blocks will show up in available cust
  * screen and can be chosen to play with in
  * battle.
  */
  void RunBlocksInit(std::atomic<int>* progress);

  /*! \brief This thread tnitializes all mobs
  *
  * @see RunNaviInit()
  *
  * After the media resources are loaded we
  * safely load all registed mobs.
  * Loaded mobs will show up in mob select
  * screen and can be chosen to battle
  * against.
  */
  void RunMobInit(std::atomic<int>* progress);

  /*! \brief This thread tnitializes all shared lua libraries
  *
  * @see RunNaviInit()
  *
  * After the media resources are loaded we
  * safely load all registed shared libraries.
  * Loaded shared libraries will be able to be
  * included by other scripts.
  */
  void RunLuaLibraryInit(std::atomic<int>* progress);

  /*! \brief This thread loads textures and shaders
  *
  * Uses and std::atomic<int> pointer to keep
  * count of successfully loaded objects
  */
  void RunGraphicsInit(std::atomic<int> * progress);

  /*! \brief This thread loads sound effects
  *
  * Uses and std::atomic<int> pointer to keep
  * count of successfully loaded objects
  */
  void RunAudioInit(std::atomic<int> * progress);

  /*! \brief This function describes how the app behaves on focus regain
  *
  * Refresh the graphics context and enable Audio() again
  */
  void GainFocus();

  /*! \brief This function describes how the app behaves on focus lost
  *
  * Mute the Audio()
  */
  void LoseFocus();

  /*! \brief This function describes how the app behaves on window resize
  *
  * Refresh the viewport
  */
  void Resize(int newWidth, int newHeight);
};