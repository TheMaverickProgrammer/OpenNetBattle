#include <time.h>
#include <queue>
#include <atomic>
#include <cmath>
#include <Swoosh/ActivityController.h>
#include <Swoosh/Ease.h>

#include "bnGame.h"
#include "bnGameSession.h"
#include "bnGameOverScene.h"
#include "bnFakeScene.h"
#include "bnConfigReader.h"
#include "bnFont.h"
#include "bnText.h"
#include "bnResourceHandle.h"
#include "bnInputHandle.h"
#include "bnRandom.h"
#include "overworld/bnOverworldHomepage.h"
#include "SFML/System.hpp"

#ifdef ONB_MOD_SUPPORT
#include "bnScriptResourceManager.h"
#include "bindings/bnScriptedBlock.h"
#include "bindings/bnScriptedCard.h"
#include "bindings/bnScriptedPlayer.h"
#include "bindings/bnScriptedMob.h"
#include "bindings/bnLuaLibrary.h"
#include "bnQueueModRegistration.h"
#endif

#include "bnPackageManager.h"
#include "bnPlayerPackageManager.h"
#include "bnCardPackageManager.h"
#include "bnMobPackageManager.h"
#include "bnBlockPackageManager.h"
#include "bnLuaLibraryPackageManager.h"

#include "sago/platform_folders.h"

char* Game::LocalPartition = "";
char* Game::RemotePartition = "pvp";
char* Game::ServerPartition = "server";

Game::Game(DrawWindow& window, const RendererEntries& renderOptions) :
  renderOptions(renderOptions),
  window(window),
  reader("config.ini"),
  configSettings(),
  netManager(),
  textureManager(),
  audioManager(),
  shaderManager(),
  inputManager(*window.GetRenderWindow()),
  ActivityController(*window.GetRenderWindow(), renderOptions) {

  // figure out system endianness
  {
    int n = 1;
    // little endian if true
    if (*(char*)&n == 1) { endian = Endianness::little; }
  }

  // Link the resource handle to use all the manangers created by the game
  ResourceHandle::audio    = &audioManager;
  ResourceHandle::textures = &textureManager;
  ResourceHandle::shaders  = &shaderManager;

#ifdef ONB_MOD_SUPPORT
  cardPackagePartitioner = new class CardPackagePartitioner();
  playerPackagePartitioner = new class PlayerPackagePartitioner();
  blockPackagePartitioner = new class BlockPackagePartitioner();
  mobPackagePartitioner = new class MobPackagePartitioner();
  luaLibraryPackagePartitioner = new class LuaLibraryPackagePartitioner();

  // ensure we a always have a "local" partition for the client
  cardPackagePartitioner->CreateNamespace(Game::LocalPartition);
  playerPackagePartitioner->CreateNamespace(Game::LocalPartition);
  blockPackagePartitioner->CreateNamespace(Game::LocalPartition);
  mobPackagePartitioner->CreateNamespace(Game::LocalPartition);
  luaLibraryPackagePartitioner->CreateNamespace(Game::LocalPartition);

  // Script bindings needs to know about other sub system
  ResourceHandle::scripts  = &scriptManager;
  scriptManager.SetCardPackagePartitioner(*cardPackagePartitioner);
#endif

  // Link i/o handle to use the input manager created by the game
  InputHandle::input = &inputManager;

  // Link net handle to use manager created by the game
  NetHandle::net = &netManager;

  // Game session object needs to be available to every scene
  session = new GameSession;
  session->SetCardPackageManager(cardPackagePartitioner->GetPartition(Game::LocalPartition));
  session->SetBlockPackageManager(blockPackagePartitioner->GetPartition(Game::LocalPartition));

  // Use the engine's window settings for this platform to create a properly
  // sized render surface...
  unsigned int win_x = static_cast<unsigned int>(window.GetView().getSize().x);
  unsigned int win_y = static_cast<unsigned int>(window.GetView().getSize().y);

  renderSurface.create(win_x, win_y, window.GetRenderWindow()->getSettings());

  // Use our external render surface as the game's screen
  window.SetRenderSurface(renderSurface);
  window.GetRenderWindow()->setActive(false);
}

Game::~Game() {
  Exit();

  if (renderThread.joinable()) {
    renderThread.join();
  }

  delete session;

#ifdef ONB_MOD_SUPPORT
  if (cardPackagePartitioner->HasNamespace(Game::RemotePartition)) {
    cardPackagePartitioner->GetPartition(Game::RemotePartition).ErasePackages();
  }

  if (playerPackagePartitioner->HasNamespace(Game::RemotePartition)) {
    playerPackagePartitioner->GetPartition(Game::RemotePartition).ErasePackages();
  }

  if (blockPackagePartitioner->HasNamespace(Game::RemotePartition)) {
    blockPackagePartitioner->GetPartition(Game::RemotePartition).ErasePackages();
  }

  if (mobPackagePartitioner->HasNamespace(Game::RemotePartition)) {
    mobPackagePartitioner->GetPartition(Game::RemotePartition).ErasePackages();
  }

  if (luaLibraryPackagePartitioner->HasNamespace(Game::RemotePartition)) {
    luaLibraryPackagePartitioner->GetPartition(Game::RemotePartition).ErasePackages();
  }

  delete cardPackagePartitioner;
  delete playerPackagePartitioner;
  delete blockPackagePartitioner;
  delete mobPackagePartitioner;
  delete luaLibraryPackagePartitioner;
#endif
}

void Game::SetCommandLineValues(const cxxopts::ParseResult& values) {
  commandline = &values;
  commandlineArgs = values.arguments();

  // Now that we have CLI values, we can configure
  // other subsystems that need to read from them...
  unsigned int myPort = CommandLineValue<int>("port");
  uint16_t maxPayloadSize = CommandLineValue<uint16_t>("mtu");
  netManager.BindPort(myPort);

  if (maxPayloadSize != 0) {
    netManager.SetMaxPayloadSize(maxPayloadSize);
  }
}

TaskGroup Game::Boot(const cxxopts::ParseResult& values)
{
  isDebug = CommandLineValue<bool>("debug");
  singlethreaded = CommandLineValue<bool>("singlethreaded");

  if (reader.IsOK()) {
    Logger::Log(LogLevel::warning, "config settings was not OK. Will use internal default key layout.");
  }

  // Initialize the engine and log the startup time
  const clock_t begin_time = clock();

  /**
  * TODO
  DrawWindow::WindowMode mode = configSettings.IsFullscreen() ? DrawWindow::WindowMode::fullscreen : DrawWindow::WindowMode::window;

  window.Initialize(windowMode);
  */

  Logger::Logf(LogLevel::info, "Engine initialized: %f secs", float(clock() - begin_time) / CLOCKS_PER_SEC);

  // does shaders too
  Callback<void()> graphics;
  graphics.Slot(std::bind(&Game::RunGraphicsInit, this, &progress));

  Callback<void()> audio;
  audio.Slot(std::bind(&Game::RunAudioInit, this, &progress));

  Callback<void()> libraries;
  libraries.Slot( std::bind( &Game::RunLuaLibraryInit, this, &progress ) );

  Callback<void()> navis;
  navis.Slot(std::bind(&Game::RunNaviInit, this, &progress));

  Callback<void()> mobs;
  mobs.Slot(std::bind(&Game::RunMobInit, this, &progress));

  Callback<void()> cards;
  cards.Slot(std::bind(&Game::RunCardInit, this, &progress));

  Callback<void()> blocks;
  blocks.Slot(std::bind(&Game::RunBlocksInit, this, &progress));

  Callback<void()> init;
  init.Slot([this] {
    // Tell the input event loop how to behave when the app loses and regains focus
    inputManager.BindLoseFocusEvent(std::bind(&Game::LoseFocus, this));
    inputManager.BindRegainFocusEvent(std::bind(&Game::GainFocus, this));
    inputManager.BindResizedEvent(std::bind(&Game::Resize, this, std::placeholders::_1, std::placeholders::_2));
  });

  this->UpdateConfigSettings(reader.GetConfigSettings());

  TaskGroup tasks;
  tasks.AddTask("Binding window", std::move(init));
  tasks.AddTask("Init graphics", std::move(graphics));
  tasks.AddTask("Init audio", std::move(audio));
  tasks.AddTask("Load Libraries", std::move( libraries ) );
  tasks.AddTask("Load Navis", std::move(navis));
  tasks.AddTask("Load mobs", std::move(mobs));
  tasks.AddTask("Load cards", std::move(cards));
  tasks.AddTask("Load prog blocks", std::move(blocks));

  // Load font symbols immediately...
  textureManager.LoadFromFile(TexturePaths::FONT);

  mouseTexture = textureManager.LoadFromFile("resources/ui/mouse.png");
  mouse.setTexture(mouseTexture);
  mouseAnimation = Animation("resources/ui/mouse.animation");
  mouseAnimation << "DEFAULT" << Animator::Mode::Loop;
  mouseAlpha = 1.0;

  window.GetRenderWindow()->setMouseCursorVisible(false);

  // set a loading spinner on the bottom-right corner of the screen
  spinner.setTexture(textureManager.LoadFromFile("resources/ui/spinner.png"));
  spinner.setScale(2.f, 2.f);
  spinner.setPosition(float(window.GetView().getSize().x - 64), float(window.GetView().getSize().y - 50));

  spinnerAnimator = Animation("resources/ui/spinner.animation") << "SPIN" << Animator::Mode::Loop;

  if (!singlethreaded) {
    renderThread = std::thread(&Game::ProcessFrame, this);
  }

  return tasks;
}

bool Game::NextFrame()
{
  bool nextFrameKey = inputManager.Has(InputEvents::pressed_advance_frame);
  bool resumeKey = inputManager.Has(InputEvents::pressed_resume_frames);

  if (nextFrameKey && isDebug) {
    frameByFrame = true;
  }
  else if (resumeKey) {
    frameByFrame = false;
  }

  return (frameByFrame && nextFrameKey) || !frameByFrame;
}

void Game::HandleRecordingEvents()
{
  if (!inputManager.Has(InputEvents::pressed_record_frames)) {
    recordPressed = false;
    return;
  }

  if (recordPressed)
    return;

  recordPressed = true;

  if (!IsRecording()) {
    StartRecording();
    window.SetSubtitle("[RECORDING]");
  } else {
    StopRecording();
    window.SetSubtitle("");
  }
}

void Game::UpdateMouse(double dt)
{
  auto& renderWindow = *window.GetRenderWindow();
  sf::Vector2f mousepos = renderWindow.mapPixelToCoords(sf::Mouse::getPosition(renderWindow));

  mouse.setPosition(mousepos);
  mouse.setColor(sf::Color::White);
  mouseAnimation.Update(dt, mouse.getSprite());
}

void Game::ProcessFrame()
{
  sf::Clock clock;
  float scope_elapsed = 0.0f;
  window.GetRenderWindow()->setActive(true);

  while (!quitting) {
    clock.restart();

    double delta = 1.0 / static_cast<double>(frame_time_t::frames_per_second);
    this->elapsed += from_seconds(delta);

    // Poll net code
    netManager.Update(delta);

    inputManager.Update(); // process inputs
    UpdateMouse(delta);

    window.Clear(); // clear screen

    if (NextFrame()) {
      HandleRecordingEvents();
      this->update(delta);  // update game logic
    }

    // Draw a light tracking the cursor
    auto iter = std::next(renderOptions.begin(), getCurrentRendererIndex());
    iter->renderer.submit(Light(1000.0f, WithZ(mouse.getPosition(), 100.f), sf::Color(255U, 255U, 255U), 10.0));
    iter->renderer.submit(Light(2000.0f, WithZ(window.GetView().getCenter(), 100.f), sf::Color(255U, 255U, 255U), 0.0));

    this->draw();        // draw game
    mouse.draw(*window.GetRenderWindow());
    window.Display(); // display to screen

    if (frameRecorder) {
      frameRecorder->capture();
    }

    scope_elapsed = clock.getElapsedTime().asSeconds();
  }
}

void Game::RunSingleThreaded()
{
  sf::Clock clock;
  float scope_elapsed = 0.0f;
  window.GetRenderWindow()->setActive(true);

  while (window.Running() && !quitting) {
    clock.restart();

    // Poll window events
    inputManager.EventPoll();

    // unused images need to be free'd
    textureManager.HandleExpiredTextureCache();
    audioManager.HandleExpiredAudioCache();

    double delta = 1.0 / static_cast<double>(frame_time_t::frames_per_second);
    this->elapsed += from_seconds(delta);

    // Poll net code
    netManager.Update(delta);
    inputManager.Update(); // process inputs
    UpdateMouse(delta);

    window.Clear(); // clear screen

    if (NextFrame()) {
      HandleRecordingEvents();
      this->update(delta);  // update game logic
    }
    
    // Draw a light tracking the cursor
    renderOptions.begin()->renderer.submit(Light(256.0f, WithZ(mouse.getPosition(), 10.f), sf::Color(100U, 100U, 150U), 1.0));

    this->draw();        // draw game
    mouse.draw(*window.GetRenderWindow());
    window.Display(); // display to screen

    if (frameRecorder) {
      frameRecorder->capture();
    }

    quitting = getStackSize() == 0;

    scope_elapsed = clock.getElapsedTime().asSeconds();
  }
}

void Game::Exit()
{
  quitting = true;
}

void Game::Run()
{
  if (singlethreaded) {
    RunSingleThreaded();
    return;
  }

  while (window.Running() && !quitting) {
    // Poll window events
    inputManager.EventPoll();

    // unused images need to be free'd
    textureManager.HandleExpiredTextureCache();
    audioManager.HandleExpiredAudioCache();

    quitting = getStackSize() == 0;
  }
}

void Game::SetWindowMode(DrawWindow::WindowMode mode)
{
  windowMode = mode;
}

void Game::Postprocess(ShaderType shaderType)
{
  this->postprocess = shaderManager.GetShader(shaderType);
}

void Game::NoPostprocess()
{
  this->postprocess = nullptr;
}

void Game::PrintCommandLineArgs()
{
  Logger::Log(LogLevel::debug, "Command line args provided");
  for (auto&& kv : commandlineArgs) {
    Logger::Logf(LogLevel::debug, "%s : %s", kv.key().c_str(), kv.value().c_str());
  }
}

const sf::Vector2f Game::CameraViewOffset(Camera& camera)
{
  return window.GetView().getCenter() - camera.GetView().getCenter();
}

int64_t Game::FrameNumber() const
{
  return this->elapsed.count();
}

const Endianness Game::GetEndianness()
{
  return endian;
}

void Game::LoadConfigSettings()
{
  // try to read the config file
  configSettings = reader.GetConfigSettings();
}

void Game::UpdateConfigSettings(const struct ConfigSettings& new_settings)
{
  if (!new_settings.IsOK())
    return;

  configSettings = new_settings;

  if (configSettings.GetShaderLevel() > 0) {
    window.SupportShaders(true);
    ActivityController::optimizeForPerformance(swoosh::quality::realtime);
    ActivityController::enableShaders(true);
  }
  else {
    window.SupportShaders(false);
    ActivityController::optimizeForPerformance(swoosh::quality::mobile);
    ActivityController::enableShaders(false);
  }

  // If the file is good, use the Audio() and
  // controller settings from the config
  audioManager.EnableAudio(configSettings.IsAudioEnabled());
  audioManager.SetStreamVolume(((configSettings.GetMusicLevel()-1) / 3.0f) * 100.0f);
  audioManager.SetChannelVolume(((configSettings.GetSFXLevel()-1) / 3.0f) * 100.0f);

  inputManager.SupportConfigSettings(configSettings);
}

void Game::SeedRand(unsigned int seed)
{
  srand(seed);
  SeedSyncedRand(seed);
  randSeed = seed;
}

const unsigned int Game::GetRandSeed() const
{
  return randSeed;
}

bool Game::IsSingleThreaded() const
{
  return singlethreaded;
}

bool Game::IsRecording() const
{
  return frameRecorder != nullptr;
}

void Game::StartRecording()
{
  frameRecorder = std::make_unique<FrameRecorder>(*window.GetRenderWindow());
}

void Game::StopRecording()
{
  frameRecorder = nullptr;
}

void Game::SetSubtitle(const std::string& subtitle)
{
  window.SetSubtitle(subtitle);
}

const std::string& Game::GetBuildHash() const
{
  static std::string hash = ONB_BUILD_HASH;
  return hash;
}

const std::filesystem::path Game::AppDataPath()
{
  return std::filesystem::u8path(sago::getDataHome()) / appName;
}

const std::filesystem::path Game::CacheDataPath()
{
  return std::filesystem::u8path(sago::getCacheDir()) / appName;
}

const std::filesystem::path Game::DesktopPath()
{
  return std::filesystem::u8path(sago::getDesktopFolder());
}

const std::filesystem::path Game::DownloadsPath()
{
  return std::filesystem::u8path(sago::getDownloadFolder());
}

const std::filesystem::path Game::DocumentsPath()
{
  return std::filesystem::u8path(sago::getDocumentsFolder());
}

const std::filesystem::path Game::VideosPath()
{
  return std::filesystem::u8path(sago::getVideoFolder());
}

const std::filesystem::path Game::PicturesPath()
{
  return std::filesystem::u8path(sago::getPicturesFolder());
}

const std::filesystem::path Game::SaveGamesPath()
{
  return std::filesystem::u8path(sago::getSaveGamesFolder1());
}

CardPackagePartitioner& Game::CardPackagePartitioner()
{
  return *cardPackagePartitioner;
}

PlayerPackagePartitioner& Game::PlayerPackagePartitioner()
{
  return *playerPackagePartitioner;
}

MobPackagePartitioner& Game::MobPackagePartitioner()
{
  return *mobPackagePartitioner;
}

BlockPackagePartitioner& Game::BlockPackagePartitioner()
{
  return *blockPackagePartitioner;
}

LuaLibraryPackagePartitioner& Game::LuaLibraryPackagePartitioner()
{
  return *luaLibraryPackagePartitioner;
}

ConfigSettings& Game::ConfigSettings()
{
  return configSettings;
}

GameSession& Game::Session()
{
  return *session;
}

void Game::RunNaviInit(std::atomic<int>* progress) {
  clock_t begin_time = clock();

  auto LoadPlayerMods = QueueModRegistration<class PlayerPackageManager, ScriptedPlayer>;
  LoadPlayerMods(playerPackagePartitioner->GetPartition(Game::LocalPartition), "resources/mods/players", "Player Mods");
  playerPackagePartitioner->GetPartition(Game::LocalPartition).LoadAllPackages(*progress);

  Logger::Logf(LogLevel::info, "Loaded registered navis: %f secs", float(clock() - begin_time) / CLOCKS_PER_SEC);
}

void Game::RunBlocksInit(std::atomic<int>* progress)
{
  clock_t begin_time = clock();

  auto LoadBlockMods = QueueModRegistration<class BlockPackageManager, ScriptedBlock>;
  LoadBlockMods(blockPackagePartitioner->GetPartition(Game::LocalPartition), "resources/mods/blocks", "Prog Block Mods");
  blockPackagePartitioner->GetPartition(Game::LocalPartition).LoadAllPackages(*progress);

  Logger::Logf(LogLevel::info, "Loaded registered prog blocks: %f secs", float(clock() - begin_time) / CLOCKS_PER_SEC);
}

void Game::RunMobInit(std::atomic<int>* progress) {
  clock_t begin_time = clock();

  auto LoadEnemyMods = QueueModRegistration<class MobPackageManager, ScriptedMob>;
  LoadEnemyMods(mobPackagePartitioner->GetPartition(Game::LocalPartition), "resources/mods/enemies", "Enemy Mods");
  mobPackagePartitioner->GetPartition(Game::LocalPartition).LoadAllPackages(*progress);

  Logger::Logf(LogLevel::info, "Loaded registered mobs: %f secs", float(clock() - begin_time) / CLOCKS_PER_SEC);
}

void Game::RunCardInit(std::atomic<int>* progress) {
  clock_t begin_time = clock();

  auto LoadCardMods = QueueModRegistration<class CardPackageManager, ScriptedCard>;
  LoadCardMods(cardPackagePartitioner->GetPartition(Game::LocalPartition), "resources/mods/cards", "Card Mods");
  cardPackagePartitioner->GetPartition(Game::LocalPartition).LoadAllPackages(*progress);

  Logger::Logf(LogLevel::info, "Loaded registered cards: %f secs", float(clock() - begin_time) / CLOCKS_PER_SEC);
}

void Game::RunLuaLibraryInit(std::atomic<int>* progress) {
  clock_t begin_time = clock();

  auto LoadCoreLibraryMods = QueueModRegistration<class LuaLibraryPackageManager, LuaLibrary>;
  LoadCoreLibraryMods(luaLibraryPackagePartitioner->GetPartition(Game::LocalPartition), "resources/mods/libs", "Core Libs Mods");
  luaLibraryPackagePartitioner->GetPartition(Game::LocalPartition).LoadAllPackages(*progress);

  Logger::Logf(LogLevel::info, "Loaded registered libraries: %f secs", float(clock() - begin_time) / CLOCKS_PER_SEC);
}

void Game::RunGraphicsInit(std::atomic<int> * progress) {
  clock_t begin_time = clock();
  textureManager.LoadAllTextures(*progress);

  Logger::Logf(LogLevel::info, "Loaded textures: %f secs", float(clock() - begin_time) / CLOCKS_PER_SEC);

  if (reader.GetConfigSettings().GetShaderLevel() > 0) {
    ActivityController::optimizeForPerformance(swoosh::quality::realtime);
    begin_time = clock();
    shaderManager.LoadAllShaders(*progress);

    Logger::Logf(LogLevel::info, "Loaded shaders: %f secs", float(clock() - begin_time) / CLOCKS_PER_SEC);
  }
  else {
    ActivityController::optimizeForPerformance(swoosh::quality::mobile);
    Logger::Log(LogLevel::info, "Shader support is disabled");
  }
}

void Game::RunAudioInit(std::atomic<int> * progress) {
  const clock_t begin_time = clock();
  audioManager.LoadAllSources(*progress);

  Logger::Logf(LogLevel::info, "Loaded Audio() sources: %f secs", float(clock() - begin_time) / CLOCKS_PER_SEC);
}

void Game::GainFocus() {
  window.RegainFocus();
  audioManager.Mute(false);

#ifdef __ANDROID__
  // TODO: Reload all graphics and somehow reassign all gl IDs to all allocated sfml graphics structures
  // Textures().RecoverLostGLContext();
  // ENGINE.RecoverLostGLContext(); // <- does the window need recreation too?
#endif
}

void Game::LoseFocus() {
  audioManager.Mute(true);
}

void Game::Resize(int newWidth, int newHeight) {
  float windowRatio = (float)newWidth / (float)newHeight;
  float viewRatio = window.GetView().getSize().x / window.GetView().getSize().y;

  showScreenBars = windowRatio >= viewRatio;
  window.Resize(newWidth, newHeight);
}
