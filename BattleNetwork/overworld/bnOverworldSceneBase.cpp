#include <Swoosh/ActivityController.h>
#include <Segues/PushIn.h>
#include <Segues/Checkerboard.h>
#include <Segues/PixelateBlackWashFade.h>
#include <Segues/DiamondTileSwipe.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "bnOverworldSceneBase.h"
#include "bnOverworldTiledMapLoader.h"

#include "../bnMath.h"
#include "../Android/bnTouchArea.h"
#include "../bnCurrentTime.h"
#include "../bnBlockPackageManager.h"
#include "../bnMobPackageManager.h"
#include "../bnCardFolderCollection.h"
#include "../bnGameSession.h"
#include "../bnGameUtils.h"

// scenes
#include "../bnFolderScene.h"
#include "../bnSelectNaviScene.h"
#include "../bnSelectMobScene.h"
#include "../bnLibraryScene.h"
#include "../bnConfigScene.h"
#include "../bnFolderScene.h"
#include "../bnKeyItemScene.h"
#include "../bnMailScene.h"
#include "../bnVendorScene.h"
#include "../bnPlayerCustScene.h"
#include "../battlescene/bnMobBattleScene.h"
#include "../netplay/bnMatchMakingScene.h"

// Backgrounds
#include "../bnCustomBackground.h"
#include "../bnLanBackground.h"
#include "../bnACDCBackground.h"
#include "../bnGraveyardBackground.h"
#include "../bnVirusBackground.h"
#include "../bnMedicalBackground.h"
#include "../bnJudgeTreeBackground.h"
#include "../bnMiscBackground.h"
#include "../bnRobotBackground.h"
#include "../bnSecretBackground.h"
#include "../bnUndernetBackground.h"
#include "../bnWeatherBackground.h"

using sf::RenderWindow;
using sf::VideoMode;
using sf::Clock;
using sf::Event;

using namespace swoosh::types;

/// \brief Thunk to populate menu options to callbacks
namespace {
  auto MakeOptions = [](Overworld::SceneBase* scene) -> Overworld::PersonalMenu::OptionsList {
    return {
      { "chip_folder", std::bind(&Overworld::SceneBase::GotoChipFolder, scene) },
      { "navi",        std::bind(&Overworld::SceneBase::GotoNaviSelect, scene) },
      { "mail",        std::bind(&Overworld::SceneBase::GotoMail, scene) },
      { "key_items",   std::bind(&Overworld::SceneBase::GotoKeyItems, scene) },
      { "mob_select",  std::bind(&Overworld::SceneBase::GotoMobSelect, scene) },
      { "config",      std::bind(&Overworld::SceneBase::GotoConfig, scene) },
      { "sync",        std::bind(&Overworld::SceneBase::GotoPVP, scene) }
    };
  };
}

Overworld::SceneBase::SceneBase(swoosh::ActivityController& controller) :
  lastIsConnectedState(false),
  playerSession(std::make_shared<PlayerSession>()),
  personalMenu(std::make_shared<PersonalMenu>(playerSession, "Overworld", ::MakeOptions(this))),
  minimap(std::make_shared<Minimap>()),
  camera(controller.getWindow().getView()),
  Scene(controller),
  map(0, 0, 0, 0),
  playerActor(std::make_shared<Overworld::Actor>("You"))
{

  // Draws the scrolling background
  SetBackground(std::make_shared<LanBackground>());

  // set the missing texture for all actor objects
  auto missingTexture = Textures().LoadFromFile("resources/ow/missing.png");
  Overworld::Actor::SetMissingTexture(missingTexture);

  personalMenu->setScale(2.f, 2.f);

  auto& session = getController().Session();
  bool loaded = session.LoadSession("profile.bin");

  // folders may be blank if session was unable to load a collection
  folders = &session.GetCardFolderCollection();

  if (loaded) {
    NaviEquipSelectedFolder();
  }

  setView(sf::Vector2u(480, 320));

  // Spawn overworld player
  playerActor->setPosition(200, 20);
  playerActor->SetCollisionRadius(4);

  AddActor(playerActor);
  AddSprite(teleportController.GetBeam());

  worldTransform.setScale(2.f, 2.f);

  menuSystem.BindMenu(InputEvents::pressed_pause, personalMenu);
  menuSystem.BindMenu(InputEvents::pressed_map, minimap);
  minimap->setScale(2.f, 2.f);

  cameraPanUITexture = Textures().LoadFromFile(TexturePaths::CAMERA_PAN_UI);
  cameraPanUIAnimation = Animation(AnimationPaths::CAMERA_PAN_UI) << "DEFAULT" << Animator::Mode::Loop;
  cameraPanUI.setTexture(*cameraPanUITexture, false);
  cameraPanUI.setScale(2.f, 2.f);
  cameraPanUIAnimation.Refresh(cameraPanUI);
}

void Overworld::SceneBase::onStart() {
#ifdef __ANDROID__
  StartupTouchControls();
#endif

  // TODO: this is all just test stuff 
  Inbox& inbox = playerSession->inbox;

  std::shared_ptr<Background> bg = GetBackground();
  PA* pa = &GetProgramAdvance();
  Game* controller = &getController();
  Inbox::OnMailReadCallback virusTrigger;
  virusTrigger.Slot([bg, pa, controller, this](Inbox::Mail& mail) {
    std::string navi = this->GetCurrentNaviID();
    mail.body = "This mail had a virus. It's clean now.";

    {
      PlayerPackageManager& playerPackages = controller->PlayerPackagePartitioner().GetPartition(Game::LocalPartition);
      MobPackageManager& mobPackages = controller->MobPackagePartitioner().GetPartition(Game::LocalPartition);

      if (mobPackages.Size() == 0 || playerPackages.Size() == 0) return;

      PlayerMeta& playerMeta = playerPackages.FindPackageByID(navi);
      MobMeta& mobMeta = mobPackages.FindPackageByID(mobPackages.FirstValidPackage());

      GameUtils(*controller).LaunchMobBattle(playerMeta, mobMeta, bg, *pa, nullptr);
    }
  });

  sf::Texture mugshot = *Textures().LoadFromFile("resources/ow/prog/prog_mug.png");
  inbox.AddMail(Inbox::Mail{ Inbox::Icons::announcement, "Welcome", "NO-TITLE", "This is your first email!", mugshot });
  inbox.AddMail(Inbox::Mail{ Inbox::Icons::dm, "HELLO", "KERISTERO", "try gravy" });
  inbox.AddMail(Inbox::Mail{ Inbox::Icons::dm_w_attachment, "ELLO", "DESTROYED", "ello govna" });
  inbox.AddMail(Inbox::Mail{ Inbox::Icons::important, "FIRE", "NO-TITLE", "There's a fire in the undernet!", mugshot });
  inbox.AddMail(Inbox::Mail{ Inbox::Icons::mission, "MISSING", "ANON", "Can you find my missing data? It would really help me out right now... Or don't if it's too hard, I understand..." });
  inbox.AddMail(Inbox::Mail{ Inbox::Icons::dm, "Test", "NO-TITLE", "Just another test-\n\n\nWait! This email contained a virus!", mugshot, virusTrigger });
}

void Overworld::SceneBase::onUpdate(double elapsed) {
  playerController.ListenToInputEvents(!IsInputLocked());

  if (IsInFocus()) {
    HandleInput();
  }

  // handle custom tile collision
  if (!HasTeleportedAway() && teleportController.IsComplete()) {
    this->OnTileCollision();
  }

  /**
  * update all overworld objects and animations
  */

  // animations
  animElapsed += elapsed;

  // expecting glitches, manually update when actors move?
  spatialMap.Update();

  // update tile animations
  map.Update(*this, animElapsed);

  if (IsInFocus()) {
    playerController.Update(elapsed);
    teleportController.Update(elapsed);

    // factor in player's position for the minimap
    sf::Vector2f screenPos = map.WorldToScreen(playerActor->getPosition());
    screenPos.y -= playerActor->GetElevation() * map.GetTileSize().y * 0.5f;
    screenPos.x = std::floor(screenPos.x);
    screenPos.y = std::floor(screenPos.y);

    // update minimap
    auto playerTilePos = map.WorldToTileSpace(playerActor->getPosition());
    bool isConcealed = map.IsConcealed(sf::Vector2i(playerTilePos), playerActor->GetLayer());
    minimap->SetPlayerPosition(screenPos, isConcealed);
  }

  for (auto& actor : actors) {
    actor->Update((float)elapsed, map, spatialMap);
  }

#ifdef __ANDROID__
  if (!IsInFocus())
    return; // keep the screen looking the same when we come back
#endif

  auto layerCount = map.GetLayerCount() + 1;

  if (spriteLayers.size() != layerCount) {
    spriteLayers.resize(layerCount);
  }

  for (auto i = 0; i < layerCount; i++) {
    auto& spriteLayer = spriteLayers[i];
    auto elevation = (float)i;
    bool isTopLayer = elevation + 1 == layerCount;
    bool isBottomLayer = elevation == 0;

    spriteLayer.clear();

    // match sprites to layer
    for (auto& sprite : sprites) {
      // use ceil(elevation) + 1 instead of GetLayer to prevent sorting issues with stairs
      auto spriteElevation = std::ceil(sprite->GetElevation()) + 1;
      if (spriteElevation == elevation || (isTopLayer && spriteElevation >= layerCount) || (isBottomLayer && spriteElevation < 0)) {
        spriteLayer.push_back(sprite);
      }
    }

    // sort sprites within the layer
    std::sort(spriteLayer.begin(), spriteLayer.end(),
      [](const std::shared_ptr<WorldSprite>& A, const std::shared_ptr<WorldSprite>& B) {
      auto& A_pos = A->getPosition();
      auto& B_pos = B->getPosition();
      auto A_compare = A_pos.x + A_pos.y;
      auto B_compare = B_pos.x + B_pos.y;

      return A_compare < B_compare;
    });
  }

  // grabbing the camera pos for parallax
  const sf::Vector2f& cameraPos = camera.GetView().getCenter();

  // Update bg
  if (bg) {
    bg->Update((float)elapsed);

    // Apply parallax
    bg->SetOffset(bg->GetOffset() + Floor(cameraPos * backgroundParallaxFactor));
  }

  // Update the textbox
  menuSystem.Update((float)elapsed);

  // camera pan ui animates
  cameraPanUIAnimation.Update(elapsed, cameraPanUI);

  HandleCamera((float)elapsed);

  // Update foreground
  if (fg) {
    fg->Update((float)elapsed);

    // Apply parallax
    fg->SetOffset(fg->GetOffset() + Floor(cameraPos * foregroundParallaxFactor));
  }
}

void Overworld::SceneBase::HandleCamera(float elapsed) {
  if (!cameraLocked) {
    // Follow the navi
    sf::Vector2f pos = playerActor->getPosition();

    if (teleportedOut) {
      pos = { returnPoint.x, returnPoint.y };
    }

    pos = map.WorldToScreen(pos);
    pos.y -= playerActor->GetElevation() * map.GetTileSize().y / 2.0f;

    teleportedOut ? camera.MoveCamera(pos, sf::seconds(0.5)) : camera.PlaceCamera(pos);
    camera.Update(elapsed);
    return;
  }

  camera.Update(elapsed);
}

void Overworld::SceneBase::HandleInput() {
  auto& window = getController().getWindow();
  auto& input = Input();
  auto mousei = sf::Mouse::getPosition(window);
  auto mousef = window.mapPixelToCoords(mousei);

  if (cameraControlsEnabled) {
    LockCamera();

    sf::Vector2f panningOffset;
    float fast = 1.f;

    if (input.Has(InputEvents::held_cancel)) {
      fast = 3.f;
    }

    if (input.Has(InputEvents::held_ui_left)) {
      panningOffset.x -= 2.f;
    }

    if (input.Has(InputEvents::held_ui_right)) {
      panningOffset.x += 2.f;
    }

    if (input.Has(InputEvents::held_ui_up)) {
      panningOffset.y -= 2.f;
    }

    if (input.Has(InputEvents::held_ui_down)) {
      panningOffset.y += 2.f;
    }

    if (input.GetConfigSettings().GetInvertMinimap()) {
      panningOffset.x *= -2.f;
      panningOffset.y *= -2.f;
    }

    if (input.Has(InputEvents::pressed_shoulder_left)) {
      cameraControlsEnabled = false;
      UnlockInput();
      UnlockCamera();
    }

    sf::Vector2f newPos = camera.GetView().getCenter() + (panningOffset * fast);
    newPos.x = std::clamp(newPos.x, cameraControlsStart.x - cameraControlsRange.x, cameraControlsStart.x + cameraControlsRange.x);
    newPos.y = std::clamp(newPos.y, cameraControlsStart.y - cameraControlsRange.y, cameraControlsStart.y + cameraControlsRange.y);
    camera.PlaceCamera(newPos);
    return;
  }

  auto menuSystemOpen = !menuSystem.IsClosed();

  menuSystem.HandleInput(input, mousef);

  if (menuSystemOpen) {
    return;
  }

  if (cameraZoomEnabled) {
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Home)) {
      worldTransform.setScale(2.f, 2.f);
    }
    else if (sf::Keyboard::isKeyPressed(sf::Keyboard::PageUp) && !cameraZoomKeyPressed) {
      worldTransform.setScale(worldTransform.getScale() * 2.f);
      cameraZoomKeyPressed = true;
    }
    else if (sf::Keyboard::isKeyPressed(sf::Keyboard::PageDown) && !cameraZoomKeyPressed) {
      worldTransform.setScale(worldTransform.getScale() * 0.5f);
      cameraZoomKeyPressed = true;
    }
  }

  if (cameraZoomKeyPressed
    && !sf::Keyboard::isKeyPressed(sf::Keyboard::PageUp)
    && !sf::Keyboard::isKeyPressed(sf::Keyboard::PageDown)) {
    cameraZoomKeyPressed = false;
  }

  // check to see if talk button was pressed
  if (!IsInputLocked()) {
    if (input.Has(InputEvents::pressed_interact)) {
      OnInteract(Interaction::action);
    }

    if (input.Has(InputEvents::pressed_shoulder_left)) {
      OnInteract(Interaction::inspect);
    }
  }
}

void Overworld::SceneBase::onLeave() {
#ifdef __ANDROID__
  ShutdownTouchControls();
#endif
}

void Overworld::SceneBase::onExit()
{
}

void Overworld::SceneBase::onEnter()
{
  RefreshNaviSprite();

  if (!this->map.GetSongPath().empty()) {
    Audio().Stream(GetPath(map.GetSongPath()), true);
  }
}

void Overworld::SceneBase::onResume() {
  getController().Session().SaveSession("profile.bin");

  // if we left this scene for a new OW scene... return to our warp area
  if (teleportedOut) {
    playerController.ReleaseActor();

    auto& command = teleportController.TeleportIn(
      playerActor,
      returnPoint,
      Reverse(playerActor->GetHeading())
    );

    teleportedOut = false;
    command.onFinish.Slot([=] {
      playerController.ControlActor(playerActor);
    });
  }

  if (!map.GetSongPath().empty()) {
    Audio().Stream(GetPath(map.GetSongPath()), true);
  }

#ifdef __ANDROID__
  StartupTouchControls();
#endif
}

void Overworld::SceneBase::onDraw(IRenderer& renderer) {
  if (menuSystem.IsFullscreen()) {
    renderer.submit(&menuSystem);
    return;
  }

  if (bg) {
    renderer.submit(bg.get());
  }

  DrawWorld(renderer, sf::RenderStates::Default);

  if (fg) {
    renderer.submit(fg.get());
  }

  if (personalMenu->IsClosed() && !cameraControlsEnabled) {
    // menuSystem will not draw personal menu if it's closed
    // might make sense to extract some parts of menu system as the closed UI has different requirements
    renderer.submit(personalMenu.get());
  }

  // This will mask everything before this line with camera fx
  renderer.submit(&camera.GetLens());

  // camera pan ui on top
  if (cameraControlsEnabled) {
    renderer.submit(&cameraPanUI);
  }

  // always see menus
  renderer.submit(&menuSystem);
}

void Overworld::SceneBase::DrawWorld(IRenderer& renderer, sf::RenderStates states) {
  const sf::Vector2f& mapScale = worldTransform.getScale();
  sf::Vector2f cameraCenter = camera.GetView().getCenter();
  cameraCenter.x = std::floor(cameraCenter.x) * mapScale.x;
  cameraCenter.y = std::floor(cameraCenter.y) * mapScale.y;

  sf::Vector2f offset = getView().getCenter() - cameraCenter;

  // prevents stitching artifacts between tiles
  offset.x = std::floor(offset.x);
  offset.y = std::floor(offset.y);

  states.transform.translate(offset);
  states.transform *= worldTransform.getTransform();

  sf::Vector2i tileSize = map.GetTileSize();
  size_t mapLayerCount = map.GetLayerCount();

  // there should be mapLayerCount + 1 sprite layers
  for (size_t i = 0; i < mapLayerCount + 1; i++) {

    // loop is based on expected sprite layers
    // make sure we dont try to draw an extra map layer and segfault
    if (i < mapLayerCount) {
      DrawMapLayer(renderer, states, i, mapLayerCount);
    }

    // save from possible map layer count change after OverworldSceneBase::Update
    if (i < spriteLayers.size()) {
      DrawSpriteLayer(renderer, states, i);
    }

    // translate next layer
    states.transform.translate(0.f, -tileSize.y * 0.5f);
  }
}

void Overworld::SceneBase::DrawMapLayer(IRenderer& renderer, sf::RenderStates states, size_t index, size_t maxLayers) {
  Map::Layer& layer = map.GetLayer(index);

  if (!layer.IsVisible()) {
    return;
  }

  unsigned int rows = map.GetRows();
  unsigned int cols = map.GetCols();
  sf::Vector2i tileSize = map.GetTileSize();

  const int TILE_PADDING = 3;
  sf::Vector2f screenSize = sf::Vector2f(getController().getVirtualWindowSize()) / worldTransform.getScale().x;

  int verticalTileCount = (int)std::ceil((screenSize.y / (float)tileSize.y) * 2.0f);
  int horizontalTileCount = (int)std::ceil(screenSize.x / (float)tileSize.x);

  sf::Vector2f screenTopLeft = camera.GetView().getCenter() - screenSize / 2.0f;
  sf::Vector2i tileSpaceStart = sf::Vector2i(map.WorldToTileSpace(map.ScreenToWorld(screenTopLeft)));
  int verticalOffset = (int)index;

  for (int i = -TILE_PADDING; i < verticalTileCount + TILE_PADDING; i++) {
    int verticalStart = (verticalOffset + i) / 2;
    int rowOffset = (verticalOffset + i) % 2;

    for (int j = -TILE_PADDING; j < horizontalTileCount + rowOffset + TILE_PADDING; j++) {
      int row = tileSpaceStart.y + verticalStart - j + rowOffset;
      int col = tileSpaceStart.x + verticalStart + j;

      auto tile = layer.GetTile(col, row);
      if (!tile || tile->gid == 0) continue;

      TileMetaPtr tileMeta = map.GetTileMeta(tile->gid);

      // failed to load tile
      if (tileMeta == nullptr) continue;

      // invisible tile
      if (tileMeta->type == TileType::invisible) continue;

      sf::Sprite& tileSprite = tileMeta->sprite;
      sf::FloatRect spriteBounds = tileSprite.getLocalBounds();

      const sf::Vector2f& originalOrigin = tileSprite.getOrigin();
      tileSprite.setOrigin(sf::Vector2f(sf::Vector2i(
        (int)spriteBounds.width / 2,
        tileSize.y / 2
      )));

      sf::Vector2i pos((col * tileSize.x) / 2, row * tileSize.y);
      sf::Vector2f ortho = map.WorldToScreen(sf::Vector2f(pos));
      sf::Vector2f tileOffset = sf::Vector2f(sf::Vector2i(
        -tileSize.x / 2 + (int)spriteBounds.width / 2,
        tileSize.y + tileSize.y / 2 - (int)spriteBounds.height
      ));

      tileSprite.setPosition(ortho + tileMeta->drawingOffset + tileOffset);
      tileSprite.setRotation(tile->rotated ? 90.0f : 0.0f);
      tileSprite.setScale(
        tile->flippedHorizontal ? -1.0f : 1.0f,
        tile->flippedVertical ? -1.0f : 1.0f
      );

      sf::Color originalColor = tileSprite.getColor();
      if (map.HasShadow({ col, row }, (int)index)) {
        sf::Uint8 r, g, b;
        r = sf::Uint8(originalColor.r * 0.65);
        b = sf::Uint8(originalColor.b * 0.65);
        g = sf::Uint8(originalColor.g * 0.65);

        tileSprite.setColor(sf::Color(r, g, b, originalColor.a));
      }
      // TODO: remove Clone() and add `states` variable back
      renderer.submit(Clone(tileSprite));
      tileSprite.setColor(originalColor);
      tileSprite.setOrigin(originalOrigin);
    }
  }
}


void Overworld::SceneBase::DrawSpriteLayer(IRenderer& renderer, sf::RenderStates states, size_t index) {
  unsigned int rows = map.GetRows();
  unsigned int cols = map.GetCols();
  sf::Vector2i tileSize = map.GetTileSize();
  size_t mapLayerCount = map.GetLayerCount();
  float layerElevation = (float)index;

  for (WorldSpritePtr& sprite : spriteLayers[index]) {
    const sf::Vector2f worldPos = sprite->getPosition();
    sf::Vector2f screenPos = map.WorldToScreen(worldPos);
    screenPos.y -= (sprite->GetElevation() - layerElevation) * tileSize.y * 0.5f;

    // prevents blurring and camera jittering with the player
    screenPos.x = std::floor(screenPos.x);
    screenPos.y = std::floor(screenPos.y);

    sprite->setPosition(screenPos);

    sf::Vector2i gridPos = sf::Vector2i(map.WorldToTileSpace(worldPos));

    if (/*cam && cam->IsInView(sprite->getSprite())*/ true) {
      sf::Color originalColor = sprite->getColor();
      // round to the closest layer for handling online players on stairs
      int layer = (int)std::roundf(sprite->GetElevation());

      if (map.HasShadow(gridPos, layer)) {
        sf::Uint8 r, g, b;
        r = sf::Uint8(originalColor.r * 0.5);
        b = sf::Uint8(originalColor.b * 0.5);
        g = sf::Uint8(originalColor.g * 0.5);
        sprite->setColor(sf::Color(r, g, b, originalColor.a));
      }

      renderer.submit(sprite.get(), states);
      sprite->setColor(originalColor);
    }

    sprite->setPosition(worldPos);
  }
}

void Overworld::SceneBase::onEnd() {
#ifdef __ANDROID__
  ShutdownTouchControls();
#endif
}

void Overworld::SceneBase::RefreshNaviSprite()
{
  // Only refresh all data and graphics if this is a new navi
  if (lastSelectedNaviId == currentNaviId && !lastSelectedNaviId.empty()) return;

  PlayerPackageManager& packageManager = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition);
  if (!packageManager.HasPackage(currentNaviId)) {
    currentNaviId = packageManager.FirstValidPackage();
    getController().Session().SetKeyValue("SelectedNavi", currentNaviId);
  }

  lastSelectedNaviId = currentNaviId;

  auto& meta = packageManager.FindPackageByID(currentNaviId);

  // refresh menu widget too
  playerSession->health = meta.GetHP();
  playerSession->maxHealth = meta.GetHP();

  // If coming back from navi select, the navi has changed, update it
  const std::filesystem::path& owPath = meta.GetOverworldAnimationPath();

  if (!owPath.empty()) {
    if (auto tex = Textures().LoadFromFile(meta.GetOverworldTexturePath())) {
      playerActor->setTexture(tex);
    }
    playerActor->LoadAnimations(owPath);

    auto iconTexture = meta.GetIconTexture();

    if (iconTexture) {
      personalMenu->UseIconTexture(iconTexture);
    }
    else {
      personalMenu->ResetIconTexture();
    }
  }
  else {
    Logger::Logf(LogLevel::critical, "Overworld animation not found for navi package %s", currentNaviId.c_str());
  }
}

void Overworld::SceneBase::NaviEquipSelectedFolder()
{
  GameSession& session = getController().Session();
  std::string naviId = session.GetKeyValue("SelectedNavi");
  if (!naviId.empty()) {
    currentNaviId = naviId;
    RefreshNaviSprite();

    std::string folderStr = session.GetKeyValue("FolderFor:" + naviId);
    if (!folderStr.empty()) {
      // preserve our selected folder
      if (int index = folders->FindFolder(folderStr); index >= 0) {
        folders->SwapOrder(index, 0); // Select this folder again
      }
    }
  }
  else {
    currentNaviId = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition).FirstValidPackage();
    session.SetKeyValue("SelectedNavi", currentNaviId);
  }
}

void Overworld::SceneBase::LoadBackground(const Map& map, const std::string& value)
{
  float parallax = map.GetBackgroundParallax();

  if (value == "undernet") {
    SetBackground(std::make_shared<UndernetBackground>(), parallax);
  }
  else if (value == "robot") {
    SetBackground(std::make_shared<RobotBackground>(), parallax);
  }
  else if (value == "misc") {
    SetBackground(std::make_shared<MiscBackground>(), parallax);
  }
  else if (value == "grave") {
    SetBackground(std::make_shared<GraveyardBackground>(), parallax);
  }
  else if (value == "weather") {
    SetBackground(std::make_shared<WeatherBackground>(), parallax);
  }
  else if (value == "medical") {
    SetBackground(std::make_shared<MedicalBackground>(), parallax);
  }
  else if (value == "acdc") {
    SetBackground(std::make_shared<ACDCBackground>(), parallax);
  }
  else if (value == "virus") {
    SetBackground(std::make_shared<VirusBackground>(), parallax);
  }
  else if (value == "judge") {
    SetBackground(std::make_shared<JudgeTreeBackground>(), parallax);
  }
  else if (value == "secret") {
    SetBackground(std::make_shared<SecretBackground>(), parallax);
  }
  else if (value == "lan") {
    SetBackground(std::make_shared<LanBackground>(), parallax);
  }
  else {
    const auto& texture = GetTexture(map.GetBackgroundCustomTexturePath());
    const auto& animationData = GetText(map.GetBackgroundCustomAnimationPath());
    const auto& velocity = map.GetBackgroundCustomVelocity();

    Animation animation;
    animation.LoadWithData(animationData);

    SetBackground(std::make_shared<CustomBackground>(texture, animation, velocity), parallax);
  }
}

void Overworld::SceneBase::LoadForeground(const Map& map)
{
  auto texturePath = map.GetForegroundTexturePath();

  if (texturePath.empty()) {
    fg.reset();
    return;
  }

  const auto& texture = GetTexture(texturePath);
  const auto& animationData = GetText(map.GetForegroundAnimationPath());
  const auto& velocity = map.GetForegroundVelocity();
  auto parallaxFactor = map.GetForegroundParallax();

  Animation animation;
  animation.LoadWithData(animationData);

  SetForeground(std::make_shared<CustomBackground>(texture, animation, velocity), parallaxFactor);
}

std::filesystem::path Overworld::SceneBase::GetPath(const std::string& path) {
  return std::filesystem::u8path(path);
}

std::string Overworld::SceneBase::GetText(const std::string& path) {
  return FileUtil::Read(std::filesystem::u8path(path));
}

std::shared_ptr<sf::Texture> Overworld::SceneBase::GetTexture(const std::string& path) {
  return Textures().LoadFromFile(std::filesystem::u8path(path));
}

std::shared_ptr<sf::SoundBuffer> Overworld::SceneBase::GetAudio(const std::string& path) {
  return Audio().LoadFromFile(std::filesystem::u8path(path));
}


void Overworld::SceneBase::LoadMap(const std::string& data)
{
  auto optionalMap = LoadTiledMap(*this, data);

  if (!optionalMap) {
    Logger::Log(LogLevel::critical, "Failed to load map");
    return;
  }

  auto map = std::move(optionalMap.value());

  bool backgroundDiffers = map.GetBackgroundName() != this->map.GetBackgroundName() ||
    map.GetBackgroundCustomTexturePath() != this->map.GetBackgroundCustomTexturePath() ||
    map.GetBackgroundCustomAnimationPath() != this->map.GetBackgroundCustomAnimationPath() ||
    map.GetBackgroundCustomVelocity() != this->map.GetBackgroundCustomVelocity();

  if (backgroundDiffers) {
    LoadBackground(map, map.GetBackgroundName());
  } else {
    backgroundParallaxFactor = map.GetBackgroundParallax();
  }

  bool foregroundDiffers = map.GetForegroundTexturePath() != this->map.GetForegroundTexturePath() ||
    map.GetForegroundAnimationPath() != this->map.GetForegroundAnimationPath() ||
    map.GetForegroundVelocity() != this->map.GetForegroundVelocity();

  if (foregroundDiffers) {
    LoadForeground(map);
  } else {
    foregroundParallaxFactor = map.GetForegroundParallax();
  }

  if (this->map.GetSongPath() != map.GetSongPath()) {
    Audio().Stream(GetPath(map.GetSongPath()), true);
  }

  personalMenu->SetArea(map.GetName());

  // cleanup data from the previous map
  this->map.RemoveSprites(*this);

  // replace the previous map
  this->map = std::move(map);

  // update map to trigger recalculating shadows for minimap
  this->map.Update(*this, 0.0f);
  minimap->Update(this->map.GetName(), this->map);
}

void Overworld::SceneBase::TeleportUponReturn(const sf::Vector3f& position)
{
  teleportedOut = true;
  returnPoint = position;
}

const bool Overworld::SceneBase::HasTeleportedAway() const
{
  return teleportedOut;
}

void Overworld::SceneBase::SetBackground(const std::shared_ptr<Background>& background, float parallax)
{
  this->bg = background;
  backgroundParallaxFactor = parallax;
}

void Overworld::SceneBase::SetForeground(const std::shared_ptr<Background>& foreground, float parallax)
{
  this->fg = foreground;
  foregroundParallaxFactor = parallax;
}

void Overworld::SceneBase::AddSprite(const std::shared_ptr<WorldSprite>& sprite)
{
  sprites.push_back(sprite);
}

void Overworld::SceneBase::RemoveSprite(const std::shared_ptr<WorldSprite>& sprite) {
  auto pos = std::find(sprites.begin(), sprites.end(), sprite);

  if (pos != sprites.end())
    sprites.erase(pos);
}

void Overworld::SceneBase::AddActor(const std::shared_ptr<Actor>& actor) {
  actors.push_back(actor);
  spatialMap.AddActor(actor);
  AddSprite(actor);
}

void Overworld::SceneBase::RemoveActor(const std::shared_ptr<Actor>& actor) {
  spatialMap.RemoveActor(actor);
  RemoveSprite(actor);

  auto pos = std::find(actors.begin(), actors.end(), actor);

  if (pos != actors.end())
    actors.erase(pos);
}

bool Overworld::SceneBase::IsInputLocked() {
  return
    inputLocked 
    || !IsInFocus() 
    || menuSystem.ShouldLockInput() 
    || !teleportController.IsComplete() 
    || teleportController.TeleportedOut() 
    || cameraControlsEnabled;
}

bool Overworld::SceneBase::IsCameraControlsEnabled()
{
  return cameraControlsEnabled;
}

void Overworld::SceneBase::LockInput() {
  inputLocked = true;
}

void Overworld::SceneBase::UnlockInput() {
  inputLocked = false;
}

void Overworld::SceneBase::LockCamera() {
  cameraLocked = true;
}

void Overworld::SceneBase::UnlockCamera() {
  cameraLocked = false;
}

void Overworld::SceneBase::ToggleCameraZoom(bool enabled)
{
  cameraZoomEnabled = enabled;

  if (!cameraZoomEnabled) {
    worldTransform.setScale(2.f, 2.f);
  }
}

void Overworld::SceneBase::EnableCameraControls(float distX, float distY)
{
  // reset camera zoom
  worldTransform.setScale(2.f, 2.f);
  cameraControlsEnabled = true;
  cameraControlsRange = sf::Vector2f(std::fabs(distX), std::fabs(distY));
  cameraControlsStart = camera.GetView().getCenter();
}

void Overworld::SceneBase::GotoChipFolder()
{
  Audio().Play(AudioType::CHIP_DESC);

  if (!folders) {
    Logger::Log(LogLevel::debug, "folder collection was nullptr");
    return;
  }

  using effect = segue<PushIn<direction::left>, milliseconds<500>>;
  getController().push<effect::to<FolderScene>>(*folders);
}

void Overworld::SceneBase::GotoNaviSelect()
{
  // Navi select
  Audio().Play(AudioType::CHIP_DESC);

  using effect = segue<Checkerboard, milliseconds<250>>;
  getController().push<effect::to<SelectNaviScene>>(currentNaviId);
}

void Overworld::SceneBase::GotoConfig()
{
  // Config Select on PC
  Audio().Play(AudioType::CHIP_DESC);

  using effect = segue<DiamondTileSwipe<direction::right>, milliseconds<500>>;
  getController().push<effect::to<ConfigScene>>();
}

void Overworld::SceneBase::GotoMobSelect()
{
  MobPackageManager& pm = getController().MobPackagePartitioner().GetPartition(Game::LocalPartition);
  if (pm.Size() == 0) {
    personalMenu->Close();
    menuSystem.EnqueueMessage("No enemy mods installed.");
    return;
  }

  std::unique_ptr<CardFolder> folder;
  CardFolder* f = nullptr;

  if (folders->GetFolder(0, f)) {
    folder = f->Clone();
  } else {
    folder = std::make_unique<CardFolder>();
  }

  SelectMobScene::Properties props{ currentNaviId, std::move(folder), programAdvance, bg };
  using effect = segue<PixelateBlackWashFade, milliseconds<500>>;
  Audio().Play(AudioType::CHIP_DESC);
  getController().push<effect::to<SelectMobScene>>(std::move(props));
}

void Overworld::SceneBase::GotoPVP()
{
  std::unique_ptr<CardFolder> folder;

  CardFolder* f = nullptr;

  if (folders->GetFolder(0, f)) {
    folder = f->Clone();
  } else {
    folder = std::make_unique<CardFolder>();
  }

  Audio().Play(AudioType::CHIP_DESC);
  using effect = segue<PushIn<direction::down>, milliseconds<500>>;
  getController().push<effect::to<MatchMakingScene>>(currentNaviId, std::move(folder), programAdvance);
}

void Overworld::SceneBase::GotoMail()
{
  Audio().Play(AudioType::CHIP_DESC);

  using effect = segue<BlackWashFade, milliseconds<500>>;
  getController().push<effect::to<MailScene>>(playerSession->inbox);
}

void Overworld::SceneBase::GotoKeyItems()
{
  // Config Select on PC
  Audio().Play(AudioType::CHIP_DESC);

  using effect = segue<BlackWashFade, milliseconds<500>>;

  getController().push<effect::to<KeyItemScene>>(items);
}

Overworld::PersonalMenu& Overworld::SceneBase::GetPersonalMenu()
{
  return *personalMenu;
}

Overworld::Minimap& Overworld::SceneBase::GetMinimap()
{
  return *minimap;
}

Overworld::SpatialMap& Overworld::SceneBase::GetSpatialMap()
{
  return spatialMap;
}

std::vector<std::shared_ptr<Overworld::Actor>>& Overworld::SceneBase::GetActors()
{
  return actors;
}

Camera& Overworld::SceneBase::GetCamera()
{
  return camera;
}

sf::Transformable& Overworld::SceneBase::GetWorldTransform()
{
  return worldTransform;
}

Overworld::Map& Overworld::SceneBase::GetMap()
{
  return map;
}

std::shared_ptr<Overworld::PlayerSession>& Overworld::SceneBase::GetPlayerSession()
{
  return playerSession;
}

std::shared_ptr<Overworld::Actor> Overworld::SceneBase::GetPlayer()
{
  return playerActor;
}

Overworld::PlayerController& Overworld::SceneBase::GetPlayerController()
{
  return playerController;
}

Overworld::TeleportController& Overworld::SceneBase::GetTeleportController()
{
  return teleportController;
}

std::string& Overworld::SceneBase::GetCurrentNaviID()
{
  return currentNaviId;
}

std::shared_ptr<Background> Overworld::SceneBase::GetBackground()
{
  return this->bg;
}

PA& Overworld::SceneBase::GetProgramAdvance() {
  return programAdvance;
}

std::optional<CardFolder*> Overworld::SceneBase::GetSelectedFolder() {
  CardFolder* folder;

  if (folders->GetFolder(0, folder)) {
    return folder;
  }
  else {
    return {};
  }
}

Overworld::MenuSystem& Overworld::SceneBase::GetMenuSystem()
{
  return menuSystem;
}

void Overworld::SceneBase::AddItem(const std::string& id, const std::string& name, const std::string& description)
{
  items.push_back({ id, name, description });
}

void Overworld::SceneBase::RemoveItem(const std::string& id)
{
  auto iter = std::find_if(items.begin(), items.end(), [&id](auto& item) { return item.id == id; });

  if (iter != items.end()) {
    items.erase(iter);
  }
}

bool Overworld::SceneBase::IsInFocus()
{
  return getController().getCurrentActivity() == this;
}

std::pair<unsigned, unsigned> Overworld::SceneBase::PixelToRowCol(const sf::Vector2i& px, const sf::RenderWindow& window) const
{
  sf::Vector2f ortho = window.mapPixelToCoords(px);

  // consider the point on screen relative to the camera focus
  auto pos = ortho - window.getView().getCenter() - camera.GetView().getCenter();

  auto& scale = worldTransform.getScale();
  auto iso = map.ScreenToWorld(pos);
  iso.x /= scale.x;
  iso.y /= scale.y;

  auto tileSize = map.GetTileSize();

  // divide by the tile size to get the integer grid values
  unsigned x = static_cast<unsigned>(iso.x / tileSize.x / 2);
  unsigned y = static_cast<unsigned>(iso.y / tileSize.y);

  return { y, x };
}

const bool Overworld::SceneBase::IsMouseHovering(const WorldSprite& src)
{
  sf::Vector2f mapScale = GetWorldTransform().getScale();
  auto cameraOffset = GetCamera().GetCenterOffset(mapScale, *this);

  auto textureRect = src.getSprite().getTextureRect();
  auto origin = src.getSprite().getOrigin();

  auto& map = GetMap();
  auto tileSize = map.GetTileSize();
  auto& scale = worldTransform.getScale();

  auto position = src.getPosition();
  auto screenPosition = map.WorldToScreen(position);
  screenPosition.y -= src.GetElevation() * tileSize.y / 2.0f;

  auto bounds = sf::FloatRect(
    (screenPosition.x - origin.x) * scale.x,
    (screenPosition.y - origin.y) * scale.y,
    std::abs(textureRect.width) * scale.x,
    std::abs(textureRect.height) * scale.y
  );

  return Input().IsMouseHovering(getController().getWindow(), bounds, cameraOffset);
}

#ifdef __ANDROID__
void Overworld::SceneBase::StartupTouchControls() {
  ui.setScale(2.f, 2.f);

  uiAnimator.SetAnimation("CHIP_FOLDER_LABEL");
  uiAnimator.SetFrame(1, ui);
  ui.setPosition(100.f, 50.f);

  auto bounds = ui.getGlobalBounds();
  auto rect = sf::IntRect(int(bounds.left), int(bounds.top), int(bounds.width), int(bounds.height));
  auto& folderBtn = TouchArea::create(rect);

  folderBtn.onRelease([this](sf::Vector2i delta) {
    Logger::Log("folder released");

    Audio().Play(AudioType::CHIP_DESC);

    using swoosh::intent::direction;
    using segue = swoosh::intent::segue<PushIn<direction::left>, swoosh::intent::milli<500>>;
    getController().push<segue::to<FolderScene>>(data);
  });

  folderBtn.onTouch([this]() {
    menuSelectionIndex = 0;
  });

  uiAnimator.SetAnimation("LIBRARY_LABEL");
  uiAnimator.SetFrame(1, ui);
  ui.setPosition(100.f, 120.f);

  bounds = ui.getGlobalBounds();
  rect = sf::IntRect(int(bounds.left), int(bounds.top), int(bounds.width), int(bounds.height));

  Logger::Log(std::string("rect: ") + std::to_string(rect.left) + ", " + std::to_string(rect.top) + ", " + std::to_string(rect.width) + ", " + std::to_string(rect.height));

  auto& libraryBtn = TouchArea::create(rect);

  libraryBtn.onRelease([this](sf::Vector2i delta) {
    Logger::Log("library released");

    Audio().Play(AudioType::CHIP_DESC);

    using swoosh::intent::direction;
    using segue = swoosh::intent::segue<PushIn<direction::right>>;
    getController().push<segue::to<LibraryScene>, swoosh::intent::milli<500>>();
  });

  libraryBtn.onTouch([this]() {
    menuSelectionIndex = 1;
  });


  uiAnimator.SetAnimation("NAVI_LABEL");
  uiAnimator.SetFrame(1, ui);
  ui.setPosition(100.f, 190.f);

  bounds = ui.getGlobalBounds();
  rect = sf::IntRect(int(bounds.left), int(bounds.top), int(bounds.width), int(bounds.height));
  auto& naviBtn = TouchArea::create(rect);

  naviBtn.onRelease([this](sf::Vector2i delta) {
    Audio().Play(AudioType::CHIP_DESC);
    using segue = swoosh::intent::segue<Checkerboard, swoosh::intent::milli<500>>;
    using intent = segue::to<SelectNaviScene>;
    getController().push<intent>(currentNavi);
  });

  naviBtn.onTouch([this]() {
    menuSelectionIndex = 2;
  });

  uiAnimator.SetAnimation("MOB_SELECT_LABEL");
  uiAnimator.SetFrame(1, ui);
  ui.setPosition(100.f, 260.f);

  bounds = ui.getGlobalBounds();
  rect = sf::IntRect(int(bounds.left), int(bounds.top), int(bounds.width), int(bounds.height));
  auto& mobBtn = TouchArea::create(rect);

  mobBtn.onRelease([this](sf::Vector2i delta) {
    CardFolder* folder = nullptr;

    if (data.GetFolder("Default", folder)) {
      Audio().Play(AudioType::CHIP_DESC);
      using segue = swoosh::intent::segue<PixelateBlackWashFade, swoosh::intent::milli<500>>::to<SelectMobScene>;
      getController().push<segue>(currentNavi, folder->Clone());
    }
    else {
      Audio().Play(AudioType::CHIP_ERROR);
      Logger::Log("Cannot proceed to mob select. Error selecting folder 'Default'.");
    }
  });

  mobBtn.onTouch([this]() {
    menuSelectionIndex = 3;
  });
}

void Overworld::SceneBase::ShutdownTouchControls() {
  TouchArea::free();
}
#endif
