#include "bnPlayerCustScene.h"
#include "bnGameSession.h"

#include "bnBlockPackageManager.h"
#include "netplay/bnBufferWriter.h"
#include "netplay/bnBufferReader.h"
#include "stx/string.h"
#include <Segues/BlackWashFade.h>

constexpr size_t BAD_GRID_POS = std::numeric_limits<size_t>::max();
constexpr size_t MAX_ALLOWED_TYPES = 4u;
constexpr size_t MAX_ITEMS_ON_SCREEN = 4u; // includes RUN button
constexpr float GRID_START_X = 4.f;
constexpr float GRID_START_Y = 23.f;
constexpr sf::Uint8 PROGRESS_MAX_ALPHA = 200;

using namespace swoosh::types;
using Blocks = PlayerCustScene::Piece::Types;

PlayerCustScene::Piece* GenerateRandomBlock() {
  PlayerCustScene::Piece* p = new PlayerCustScene::Piece();

  size_t x{};
  bool first = true;

  while (x == 0) {
    // generate smaller 3x3 shapes
    for (size_t i = 1; i < 4; i++) { // start one row down
      for (size_t j = 1; j < 4; j++) { // start one column over
        size_t index = (i * p->BLOCK_SIZE) + j;
        uint8_t k = rand() % 2;
        p->shape[index] = k;
        x += k;
      }
    }
  }

  p->CalculateDimensions();

  p->typeIndex = rand() % static_cast<uint8_t>(Blocks::size);
  p->specialType = rand() % 2;
  p->name = stx::rand_alphanum(8);
  p->description = stx::rand_alphanum(30);

  p->description[9] = '\n';
  p->description[19] = '\n';
  return p;
}

PlayerCustScene::PlayerCustScene(swoosh::ActivityController& controller, const std::string& playerUUID, std::vector<Piece*> pieces) :
  playerUUID(playerUUID),
  pieces(pieces),
  infoText(Font::Style::thin),
  itemText(Font::Style::thick),
  hoverText(Font::Style::thick),
  textbox(sf::Vector2f(4, 200)),
  questionInterface(nullptr),
  Scene(controller)
{
  cursorLocation = ((GRID_SIZE / 2) * GRID_SIZE) + (GRID_SIZE / 2);
  gridAnim = Animation("resources/scenes/cust/grid.animation");
  trackAnim = Animation("resources/scenes/cust/track.animation") << "DEFAULT";
  clawAnim = Animation("resources/scenes/cust/claw.animation");
  blockAnim = Animation("resources/scenes/cust/block.animation");
  blockShadowHorizAnim = Animation("resources/scenes/cust/horizontal_shadow.animation");
  blockShadowVertAnim = Animation("resources/scenes/cust/vertical_shadow.animation");
  buttonAnim = Animation("resources/scenes/cust/button.animation") << "DEFAULT";
  cursorAnim = Animation("resources/scenes/cust/red_cursor.animation") << "DEFAULT" << Animator::Mode::Loop;
  menuAnim = Animation("resources/scenes/cust/menu.animation") << "MOVE";

  textbox.SetTextSpeed(1.0f);
  textbox.ChangeAppearance(Textures().LoadFromFile(TexturePaths::FOLDER_TEXTBOX), AnimationPaths::FOLDER_TEXTBOX);
  textbox.ChangeBlipSfx(Audio().LoadFromFile(SoundPaths::COMPILE_BLIP_SFX));

  auto load_audio = [this](const std::string& path) {
    return Audio().LoadFromFile(path);
  };

  compile_start = load_audio("resources/sfx/compile_start.ogg");
  compile_complete = load_audio("resources/sfx/compile_complete.ogg");
  compile_no_item = load_audio("resources/sfx/compile_empty_item.ogg");
  compile_item = load_audio("resources/sfx/compile_item.ogg");

  auto load_texture = [this](const std::string& path) {
    return Textures().LoadFromFile(path);
  };

  cursorTexture = load_texture("resources/ui/textbox_cursor.png");
  itemArrowCursor = sf::Sprite(*cursorTexture);

  sf::FloatRect bounds = itemArrowCursor.getLocalBounds();
  itemArrowCursor.setScale(2.f, 2.f);
  itemArrowCursor.setOrigin({ bounds.width, 0. });

  bgTex = load_texture("resources/scenes/cust/bg.png");
  bg = sf::Sprite(*bgTex);
  bg.setScale(2.f, 2.f);

  bgBottom = sf::Sprite(*bgTex);
  bgBottom.setScale(2.f, 2.f);
  bgBottom.setPosition(sf::Vector2f(0, 150*2));
  bgBottom.setTextureRect(sf::IntRect(sf::Vector2i(0, 150), sf::Vector2i(240, 10)));

  sceneLabel = sf::Sprite(*load_texture("resources/scenes/cust/scene_label.png"));
  sceneLabel.setPosition(sf::Vector2f(20.f, 8.0f));
  sceneLabel.setScale(2.f, 2.f);

  cursor = sf::Sprite(*load_texture("resources/scenes/cust/red_cursor.png"));
  cursor.setScale(2.f, 2.f);
  cursor.setTextureRect({ 0, 0, 16, 16 });

  claw = sf::Sprite(*load_texture("resources/scenes/cust/claw.png"));
  claw.setScale(2.f, 2.f);
  claw.setTextureRect({ 38, 0, 14, 14 });

  gridSprite = sf::Sprite(*load_texture("resources/scenes/cust/grid.png"));
  gridSprite.setTextureRect({ 0, 0, 127, 130 });
  gridSprite.setPosition((GRID_START_X*2.)+12, (GRID_START_Y*2.)-8);
  gridSprite.setScale(2.f, 2.f);
  
  for (uint8_t i = 0; i < static_cast<uint8_t>(Blocks::size); i++) {
    blockTypeInUseTable[i] = 0u;
    blockTextures.push_back(load_texture("resources/scenes/cust/cust_blocks_" + std::to_string(i) + ".png"));
  }

  disabledBlockTexture = load_texture("resources/scenes/cust/cust_blocks_disabled.png");

  blockShadowHorizontal = sf::Sprite(*load_texture("resources/scenes/cust/horiz_shadow.png"));
  blockShadowHorizontal.setScale(2.f, 2.f);
  blockShadowVertical = sf::Sprite(*load_texture("resources/scenes/cust/vert_shadow.png"));
  blockShadowVertical.setScale(2.f, 2.f);

  sf::IntRect buttonRect = {0, 0, 73, 19};
  blueButtonSprite = sf::Sprite(*load_texture("resources/scenes/cust/item_name_container.png"));
  blueButtonSprite.setTextureRect(buttonRect);
  blueButtonSprite.setScale(2.f, 2.f);

  greenButtonSprite = sf::Sprite(*load_texture("resources/scenes/cust/run.png"));
  greenButtonSprite.setTextureRect(buttonRect);
  greenButtonSprite.setScale(2.f, 2.f);

  infoBox = sf::Sprite(*load_texture("resources/scenes/cust/info.png"));
  infoBox.setScale(2.f, 2.f);
  infoBox.setPosition({ 280, 188 });

  previewBox = sf::Sprite(*load_texture("resources/scenes/cust/mini_block_container.png"));
  previewBox.setScale(2.f, 2.f);

  menuBox = sf::Sprite(*load_texture("resources/scenes/cust/menu.png"));
  menuBox.setScale(2.f, 2.f);
  menuBox.setPosition(GRID_START_X + 100.f, GRID_START_Y + 100.f);

  miniblocksTexture = load_texture("resources/scenes/cust/mini_blocks.png");

  sf::Vector2f pos = gridSprite.getPosition();
  pos.x += 22.f;
  pos.y += 121.f;
  track = sf::Sprite(*load_texture("resources/scenes/cust/track.png"));
  track.setScale(2.f, 2.f);
  track.setPosition(pos);

  // info text
  sf::Vector2f textPosition = infoBox.getPosition();
  textPosition.y += 20.f;
  textPosition.x += 8.f;
  infoText.setPosition(textPosition);
  infoText.setScale(2.f, 2.f);

  // item text
  itemText.setScale(2.f, 2.f);

  // hover text copies item text
  hoverText = itemText;

  // progress bar data
  progressBarTexture = load_texture("resources/scenes/cust/progress_bar.png");
  progressBarTexture->setRepeated(true);

  // construct progress bar polygon
  progressBar = sf::ConvexShape(29);
  ResetCompileArrowPolygon();

  // ensure piece dimensions are up-to-date
  for (auto& p : pieces) {
    p->CalculateDimensions();
  }

  // set screen view
  setView(sf::Vector2u(480, 320));

  // load initial layout from profile data
  LoadFromSave();
}

PlayerCustScene::~PlayerCustScene()
{
  // Delete pieces from our inventory (right sidebar)
  for (Piece* p : pieces) {
    delete p;
  }

  // Delete pieces on the grid
  for (auto& [piece, center] : centerHash) {
    if (center == BAD_GRID_POS) continue; // not a valid ptr

    delete piece;
  }
}

// cheaper check to see if piece can fit inside the region the grid cursor is located
bool PlayerCustScene::CanPieceFit(Piece* piece, size_t loc)
{
  size_t x = loc % GRID_SIZE;
  size_t y = loc / GRID_SIZE;

  if (x == 0 && piece->startX <= 1)
    return false;
  else if (x == 1 && piece->startX == 0)
    return false;
  else if (x == 5 && (piece->startX + piece->maxWidth) == Piece::BLOCK_SIZE)
    return false;
  else if (x == 6 && (piece->startX + piece->maxWidth) >= Piece::BLOCK_SIZE - 1u)
    return false;

  if (y == 0 && piece->startY <= 1)
    return false;
  else if (y == 1 && piece->startY == 0)
    return false;
  else if (y == 5 && (piece->startY + piece->maxHeight) == Piece::BLOCK_SIZE)
    return false;
  else if (y == 6 && (piece->startY + piece->maxHeight) >= Piece::BLOCK_SIZE - 1u)
    return false;

  return true;
}

bool PlayerCustScene::DoesPieceOverlap(Piece* piece, size_t loc)
{
  size_t start = GetPieceStart(piece, loc);

  bool entirelyOutOfBounds = true; // assume

  for (size_t i = piece->startY; i < piece->maxHeight+piece->startY; i++) {
    size_t idx = 0;
    for (size_t j = piece->startX; j < piece->maxWidth+piece->startX; j++) {
      if (piece->shape[(i * Piece::BLOCK_SIZE) + j]) {
        size_t x = (start + idx) % GRID_SIZE;
        size_t y = (start + idx) / GRID_SIZE;

        if (x > 0 && x + 1u < GRID_SIZE && y > 0 && y + 1u < GRID_SIZE) {
          entirelyOutOfBounds = false;
        }

        // prevent inserting corner spots
        bool tl = (x == 0 && y == 0);
        bool tr = (x + 1u == GRID_SIZE && y == 0);
        bool bl = (x == 0 && y + 1u == GRID_SIZE);
        bool br = (x + 1u == GRID_SIZE && y + 1u == GRID_SIZE);
        bool corner = tl || tr || bl || br;
        if (grid[start + idx] || corner) return true;
      }
      idx++;
    }
    start += GRID_SIZE; // next row
  }
  
  return entirelyOutOfBounds;
}

bool PlayerCustScene::InsertPiece(Piece* piece, size_t loc)
{
  GameSession& session = getController().Session();
  if (!CanPieceFit(piece, loc) || DoesPieceOverlap(piece, loc))
    return false;

  size_t start = GetPieceStart(piece, loc);

  for (size_t i = piece->startY; i < piece->maxHeight+piece->startY; i++) {
    size_t idx = 0;

    for (size_t j = piece->startX; j < piece->maxWidth+piece->startX; j++) {
      if (piece->shape[(i * Piece::BLOCK_SIZE) + j]) {
        grid[start + idx] = piece;
      }
      idx++;
    }
    start += GRID_SIZE; // next row
  }

  centerHash[piece] = loc;

  // update the block types in use table
  blockTypeInUseTable[piece->typeIndex]++;

  piece->Commit();

  return true;
}

void PlayerCustScene::RemovePiece(Piece* piece)
{
  size_t index = GetPieceCenter(piece);
  if (index == BAD_GRID_POS) return;

  size_t start = GetPieceStart(piece, index);

  bool scan = true;

  for (size_t i = piece->startY; i < piece->maxHeight+piece->startY && scan; i++) {
    size_t idx = 0;

    for (size_t j = piece->startX; j < piece->maxWidth+piece->startX && scan; j++) {
      // crash prevention, shouldn't be necessary if everything is programmed correctly
      scan = (start + idx) < grid.size();

      if (scan && piece->shape[(i * Piece::BLOCK_SIZE) + j]) {
        grid[start + idx] = nullptr;
      }

      idx++;
    }
    start += GRID_SIZE;
  }

  // update the block types in use table
  blockTypeInUseTable[piece->typeIndex]--;

  centerHash[piece] = BAD_GRID_POS;
}

void PlayerCustScene::DrawEdgeBlock(IRenderer& renderer, Piece* piece, size_t y, size_t x)
{
  if ((x == 0 || x + 1u == GRID_SIZE) && y == 3) return;

  sf::Vector2f pos = BlockToScreen(y, x);

  // assume if false, it is a horizontal piece
  bool vert = x == 0 || x + 1u == GRID_SIZE;

  if ((state == state::compiling || state == state::finishing) && compiledHash[piece]) {
    if (vert) {
      if (blockShadowVertAnim.GetAnimationString() != "BLINK") {
        blockShadowVertAnim << "BLINK" << Animator::Mode::Loop;
      }
    }
    else {
      if (blockShadowHorizAnim.GetAnimationString() != "BLINK") {
        blockShadowHorizAnim << "BLINK" << Animator::Mode::Loop;
      }
    }
  }
  else {
    if (vert) {
      if (blockShadowVertAnim.GetAnimationString() != "DIMMED") {
        blockShadowVertAnim.SetAnimation("DIMMED");
      }
    }
    else {
      if (blockShadowHorizAnim.GetAnimationString() != "DIMMED") {
        blockShadowHorizAnim.SetAnimation("DIMMED");
      }
    }
  }

  if (x == 0) {
    pos.x += 26.f;
  }

  if (y == 0) {
    pos.y += 26.f;
  }

  if (vert) {
    blockShadowVertical.setPosition(pos);
    blockShadowVertAnim.Refresh(blockShadowVertical);
    // TODO: remove Clone()
    renderer.submit(Clone(blockShadowVertical));
  }
  else {
    blockShadowHorizontal.setPosition(pos);
    blockShadowHorizAnim.Refresh(blockShadowHorizontal);
    // TODO: remove Clone()
    renderer.submit(Clone(blockShadowHorizontal));
  }
}

bool PlayerCustScene::IsGridEdge(size_t y, size_t x)
{
  return x == 0 || x == GRID_SIZE-1u || y == GRID_SIZE-1u || y == 0;
}

void PlayerCustScene::StartCompile()
{
  textbox.Close();
  questionInterface = nullptr;
  infoText.SetString("RUNNING...");
  progress = 0;
  compiledHash.clear();
  state = state::compiling;
  sf::Color color = sf::Color(255, 255, 255, PROGRESS_MAX_ALPHA);
  progressBarUVs.width = 0;
  ResetCompileArrowPolygon();
  progressBar.setTextureRect(progressBarUVs);
  progressBar.setFillColor(color);
  Audio().Play(compile_start);
}

bool PlayerCustScene::IsCompileFinished()
{
  return progress >= maxProgressTime; // in seconds
}

// static public function to return valid packages from the session key
std::vector<PackageAddress> PlayerCustScene::GetInstalledBlocks(const std::string& playerID, const GameSession& session)
{
  std::vector<PackageAddress> res;

  std::string value = session.GetKeyValue(playerID + ":" + "blocks");
  if (value.empty()) return res;

  Poco::Buffer<char> buffer{ value.c_str(), value.size() };
  BufferReader reader;

  size_t size = reader.Read<size_t>(buffer);
  for (size_t i = 0; i < size; i++) {
    std::string uuid = reader.ReadTerminatedString(buffer);

    bool isValid = reader.Read<bool>(buffer);
    reader.Read<size_t>(buffer); // skip center
    reader.Read<size_t>(buffer); // skip rot

    if (isValid && session.IsBlockAllowed(uuid)) {
      res.push_back({ Game::LocalPartition, uuid });
    }
  }

  return res;
}

void PlayerCustScene::LoadFromSave()
{
  std::string value = getController().Session().GetKeyValue(playerUUID + ":" + "blocks");
  if (value.empty()) return;

  Poco::Buffer<char> buffer{ value.c_str(), value.size() };
  BufferReader reader;

  size_t size = reader.Read<size_t>(buffer);
  for (size_t i = 0; i < size; i++) {
    std::string uuid = reader.ReadTerminatedString(buffer);
    bool valid = reader.Read<bool>(buffer); // unused
    size_t center = reader.Read<size_t>(buffer);
    size_t rot = reader.Read<size_t>(buffer);

    for (auto iter = pieces.begin(); iter != pieces.end(); /* skip */) {
      if ((*iter)->uuid == uuid && !(*iter)->Empty()) {
        for (size_t c = 0; c < rot; c++) {
          (*iter)->RotateLeft();
        }

        if (InsertPiece(*iter, center)) {
          iter = pieces.erase(iter);
          continue;
        }
      }
      iter++;
    }
  }
}

void PlayerCustScene::CompleteAndSave()
{
  state = state::waiting;
  infoText.SetString("OK");
  Audio().Play(compile_complete);

  Poco::Buffer<char> buffer{ 0 };
  BufferWriter writer;

  size_t valid_entry_len = 0;

  // scan ahead to discover the real hash length
  for (auto& [piece, center] : centerHash) {
    if (center == BAD_GRID_POS) continue;
    valid_entry_len++;
  }

  std::vector<Piece*> installed;

  // collect the pieces
  for (auto& [piece, center] : centerHash) {
    if (center == BAD_GRID_POS) continue;
    if (!piece || piece->Empty()) continue; // erroneous piece
    installed.push_back(piece);
  }

  // sort them to achieve predictable installation orders
  std::sort(installed.begin(), installed.end(), [](Piece* left, Piece* right) {
    return strcmp(left->uuid.c_str(), right->uuid.c_str()) > 0;
  });

  // write to disk
  writer.Write(buffer, valid_entry_len);
  for (Piece* piece : installed) {
    bool isValid = IsBlockBugged(piece);

    writer.WriteTerminatedString(buffer, piece->uuid);
    writer.Write(buffer, isValid);
    writer.Write(buffer, centerHash[piece]);
    writer.Write(buffer, piece->finalRot);
  }

  auto& session = getController().Session();
  
  session.SetKeyValue(playerUUID + ":" + "blocks", std::string(buffer.begin(), buffer.size()));
  session.SaveSession("profile.bin");
}

sf::Vector2f PlayerCustScene::BlockToScreen(size_t y, size_t x)
{
  float offsetX = (GRID_START_X * 2) + (x * 20.f * 2.f) - 4.f;
  float offsetY = (GRID_START_Y * 2) + (y * 20.f * 2.f) - 4.f;

  return sf::Vector2f{ offsetX, offsetY };
}

bool PlayerCustScene::HasLeftInput()
{
  return Input().Has(InputEvents::pressed_ui_left) || Input().Has(InputEvents::held_ui_left);
}

bool PlayerCustScene::HasRightInput()
{
  return Input().Has(InputEvents::pressed_ui_right) || Input().Has(InputEvents::held_ui_right);
}

bool PlayerCustScene::HasUpInput()
{
  return Input().Has(InputEvents::pressed_ui_up) || Input().Has(InputEvents::held_ui_up);
}

bool PlayerCustScene::HasDownInput()
{
  return Input().Has(InputEvents::pressed_ui_down) || Input().Has(InputEvents::held_ui_down);
}

bool PlayerCustScene::IsBlockBugged(Piece* piece)
{
  // NOTE: We can use this function to determine any bug statuses as well

  // Restriction: special pieces must be on the "compiled" line. Other pieces do not.
  return (compiledHash[piece] && piece->specialType) || !piece->specialType;
}

size_t PlayerCustScene::GetPieceCenter(Piece* piece)
{
  auto iter = centerHash.find(piece);
  if (iter != centerHash.end()) {
    return iter->second;
  }

  return BAD_GRID_POS;
}

size_t PlayerCustScene::GetPieceStart(Piece* piece, size_t center)
{
  size_t shape_half = ((Piece::BLOCK_SIZE / 2) * GRID_SIZE) + (GRID_SIZE - Piece::BLOCK_SIZE);
  size_t offset = ((piece->startY * GRID_SIZE) + piece->startX);

  size_t start_top_left = (center - (shape_half - offset));
  size_t start_bottom_right = (center + (offset - shape_half));
  bool afterCenter = offset > shape_half;

  return afterCenter ? start_bottom_right : start_top_left;
}

void PlayerCustScene::HandleGrabAction()
{
  state = state::block_prompt;
  menuAnim.SetAnimation("MOVE");
  menuAnim.Refresh(menuBox);
  Audio().Play(AudioType::CHIP_DESC);
  UpdateMenuPosition();
  UpdateCursorHoverInfo();
}

void PlayerCustScene::HandleMenuUIKeys(double elapsed)
{
  if (keyRepeater.HandleInput(InputEvents::ui_up_group, std::bind(&PlayerCustScene::ExecuteUpKey, this))) {
    return;
  }

  if (keyRepeater.HandleInput(InputEvents::ui_down_group, std::bind(&PlayerCustScene::ExecuteDownKey, this))) {
    return;
  }

  keyRepeater.Reset();

  if (Input().Has(InputEvents::pressed_cancel)) {
    state = state::usermode;
    Audio().Play(AudioType::CHIP_DESC_CLOSE);
    return;
  }

  Piece* piece = grid[cursorLocation];

  if (!piece) return;

  if (Input().Has(InputEvents::pressed_confirm)) {
    if (menuAnim.GetAnimationString() == "MOVE") {
      // interpolate to the center of the block
      grabStartLocation = GetPieceCenter(piece);
      cursorLocation = std::min(grabStartLocation, grid.size() - 1u);

      RemovePiece(piece);
      grabbingPiece = piece;
      state = state::usermode;
      Audio().Play(AudioType::CHIP_DESC);

      return;
    }

    if (menuAnim.GetAnimationString() == "REMOVE") {
      RemovePiece(piece);
      pieces.push_back(piece);
      state = state::usermode;
      Audio().Play(AudioType::CHIP_DESC_CLOSE);
      UpdateCursorHoverInfo();
    }
  }
}

sf::Vector2f PlayerCustScene::GridCursorToScreen()
{
  float cursorLocationX = (cursorLocation % GRID_SIZE) * 20.f * 2.f;
  float cursorLocationY = (cursorLocation / GRID_SIZE) * 20.f * 2.f;
  return { (GRID_START_X * 2) + cursorLocationX, (GRID_START_Y * 2) + cursorLocationY };
}

void PlayerCustScene::DrawPiece(IRenderer& renderer, Piece* piece, const sf::Vector2f& pos)
{
  static GameSession& session = getController().Session();

  sf::Sprite blockSprite;
  blockSprite.setScale(2.f, 2.f);

  sf::Color color = blockSprite.getColor();
  color.a = 140;
  blockSprite.setColor(color);

  for (size_t i = 0; i < Piece::BLOCK_SIZE; i++) {
    for (size_t j = 0; j < Piece::BLOCK_SIZE; j++) {
      size_t index = (i * Piece::BLOCK_SIZE) + j;
      if (piece->shape[index]) {
        
        float halfOffset = (((Piece::BLOCK_SIZE / 2)) * -20.f)*2.f;
        float offsetX = halfOffset + (j * 20.f * 2.f)-4.f;
        float offsetY = halfOffset + (i * 20.f * 2.f)-4.f;
        sf::Texture& tex = session.IsBlockAllowed(piece->uuid) ? *blockTextures[piece->typeIndex] : *disabledBlockTexture;
        blockSprite.setTexture(tex, true);
        blockSprite.setPosition({ pos.x + offsetX, pos.y + offsetY });
        RefreshBlock(piece, blockSprite);
        // TODO: remove Clone()
        renderer.submit(Clone(blockSprite));
      }
    }
  }
}

void PlayerCustScene::DrawPreview(IRenderer& renderer, Piece* piece, const sf::Vector2f& pos)
{
  sf::Sprite blockSprite;
  blockSprite.setScale(2.f, 2.f);

  previewBox.setPosition(pos);
  renderer.submit(&previewBox);

  for (size_t i = 0; i < Piece::BLOCK_SIZE; i++) {
    for (size_t j = 0; j < Piece::BLOCK_SIZE; j++) {
      size_t index = (i * Piece::BLOCK_SIZE) + j;
      if (piece->shape[index]) {
        float offsetX = (j * 3.f * 2.f) + 4.f;
        float offsetY = (i * 3.f * 2.f) + 4.f;
        blockSprite.setTexture(*miniblocksTexture, true);
        blockSprite.setTextureRect({ static_cast<int>(piece->typeIndex*3), 0, 3, 3 });
        blockSprite.setPosition({ pos.x + offsetX, pos.y + offsetY });
        // TODO: remove Clone()
        renderer.submit(Clone(blockSprite));
      }
    }
  }
}

void PlayerCustScene::ConsolePrintGrid()
{
  for (size_t i = 0; i < GRID_SIZE; i++) {
    for (size_t j = 0; j < GRID_SIZE; j++) {
      if (grid[(i * GRID_SIZE) + j]) {
        std::cout << "X";
      }
      else {
        std::cout << ".";
      }
    }
    std::cout << std::endl;
  }
}

void PlayerCustScene::StartScaffolding()
{
  scaffolding = 0.f;
}

void PlayerCustScene::AnimateButton(double elapsed)
{
  buttonFlashElapsed += elapsed;
  buttonAnim.SyncTime(from_seconds(buttonFlashElapsed));
  buttonAnim.Refresh(blueButtonSprite);
  buttonAnim.Refresh(greenButtonSprite);
}

void PlayerCustScene::AnimateCursor(double elapsed)
{
  if (grabbingPiece) {
    if (clawAnim.GetAnimationString() != "CLAW_CLOSED") {
      clawAnim.SetAnimation("CLAW_CLOSED");
    }
  }
  else {
    if (clawAnim.GetAnimationString() != "CLAW") {
      clawAnim << "CLAW" << Animator::Mode::Loop;
    }
  }

  cursorAnim.Update(elapsed, cursor);
  clawAnim.Update(elapsed, claw);
}

void PlayerCustScene::AnimateScaffolding(double elapsed)
{
  if (scaffolding < 1.) {
    scaffolding = std::min(scaffolding + (10.f*static_cast<float>(elapsed)), 1.f);
  }
}

void PlayerCustScene::AnimateGrid()
{
  if (itemListSelected) {
    if (gridAnim.GetAnimationString() != "DIMMED") {
      gridAnim.SetAnimation("DIMMED");
      gridAnim.Refresh(gridSprite);
    }
  }
  else {
    if (gridAnim.GetAnimationString() != "DEFAULT") {
      gridAnim.SetAnimation("DEFAULT");
      gridAnim.Refresh(gridSprite);
    }
  }
}

void PlayerCustScene::AnimateBlock(double elapsed, Piece* p)
{
  blockFlashElapsed += elapsed;
  blockShadowVertAnim.SyncTime(from_seconds(blockFlashElapsed));
  blockShadowHorizAnim.SyncTime(from_seconds(blockFlashElapsed));

  if (!p) return;

  if (p == insertingPiece || p == grabbingPiece) {
    if (p->specialType) {
      if (blockAnim.GetAnimationString() != "FLAT_INSERT") {
        blockAnim << "FLAT_INSERT";
      }
    }
    else {
      if (blockAnim.GetAnimationString() != "SQUARE_INSERT") {
        blockAnim << "SQUARE_INSERT";
      }
    }

    blockAnim.SyncTime(from_seconds(blockFlashElapsed));
    return;
  }

  if ((state == state::compiling || state == state::finishing) && compiledHash[p]) {
    if (p->specialType) {
      if (blockAnim.GetAnimationString() != "FLAT_BLINK") {
        blockAnim << "FLAT_BLINK" << Animator::Mode::Loop;
      }
    }
    else {
      if (blockAnim.GetAnimationString() != "SQUARE_BLINK") {
        blockAnim << "SQUARE_BLINK" << Animator::Mode::Loop;
      }
    }
    
    blockAnim.SyncTime(from_seconds(blockFlashElapsed));
    return;
  }

  if (p->specialType) {
    if (blockAnim.GetAnimationString() != "FLAT_DIMMED") {
      blockAnim << "FLAT_DIMMED" << Animator::Mode::Loop;
    }
  }
  else {
    if (blockAnim.GetAnimationString() != "SQUARE_DIMMED") {
      blockAnim << "SQUARE_DIMMED" << Animator::Mode::Loop;
    }
  }

  blockAnim.SyncTime(from_seconds(blockFlashElapsed));
}

void PlayerCustScene::RefreshBlock(Piece* p, sf::Sprite& sprite)
{
  AnimateBlock(0, p);
  blockAnim.Refresh(sprite);
}

void PlayerCustScene::RefreshButton(size_t idx)
{
  if (idx == listStart+currItemIndex && itemListSelected && state == state::usermode) {
    if (buttonAnim.GetAnimationString() != "BLINK") {
      buttonAnim << "BLINK" << Animator::Mode::Loop;
    }
  }
  else if (buttonAnim.GetAnimationString() != "DEFAULT") {
    buttonAnim << "DEFAULT";
  }

  blueButtonSprite.setScale(2.f, scaffolding * 2.f);
  greenButtonSprite.setScale(2.f, scaffolding * 2.f);
  AnimateButton(0);
}

void PlayerCustScene::RefreshTrack()
{
  if (itemListSelected) {
    if (trackAnim.GetAnimationString() != "DIMMED") {
      trackAnim.SetAnimation("DIMMED");
    }
  }
  else {
    if (trackAnim.GetAnimationString() != "DEFAULT") {
      trackAnim.SetAnimation("DEFAULT");
    }
  }

  trackAnim.Refresh(track);
}

void PlayerCustScene::ResetCompileArrowPolygon()
{
  progressBar.setPoint(0, sf::Vector2f(-0.1, 0));

  size_t lastPoint = 0u;
  int lastX = 0;
  int lastY = 0;

  progressBar.setPoint(++lastPoint, sf::Vector2f(lastX, lastY));

  for (size_t i = 0; i < 7; i++) {
    progressBar.setPoint(++lastPoint, sf::Vector2f(lastX, lastY + 1));

    if (i < 6) {
      lastX += 1;
      progressBar.setPoint(++lastPoint, sf::Vector2f(lastX, lastY + 1));
    }

    lastY += 1;
  }

  for (size_t i = 0; i < 7; i++) {
    progressBar.setPoint(++lastPoint, sf::Vector2f(lastX, lastY + 1));

    if (i < 6) {
      lastX -= 1;
      progressBar.setPoint(++lastPoint, sf::Vector2f(lastX, lastY + 1));
    }

    lastY += 1;
  }

  progressBar.setPoint(++lastPoint, sf::Vector2f(-0.1, lastY));

  progressBarUVs = sf::IntRect{ 0, 0, 118, 14 };
  progressBar.setTexture(progressBarTexture.get());
  progressBar.setScale(2.f, 2.f);
  progressBar.setPosition((GRID_START_X * 2.) + 20, 168);
  progressBar.setTextureRect(progressBarUVs); // source is 69x14 pixels
  progressBar.setFillColor(sf::Color(255, 255, 255, 0));

  progressBarDefault = progressBar;
}

void PlayerCustScene::UpdateCompileArrowPolygon(double delta)
{
  double offset = std::fmin(delta, 1.0) * (56.0*2);

  size_t len = progressBar.getPointCount();

  for (size_t i = 1; i < len-1; i++) {
    const sf::Vector2f p = progressBarDefault.getPoint(i);
    progressBar.setPoint(i, sf::Vector2f(p.x+offset, p.y));
  }
}

bool PlayerCustScene::HandleSelectItemFromList()
{
  if (listStart >= pieces.size()) return false;

  auto iter = std::next(pieces.begin(), listStart);

  if ((*iter)->Empty()) {
    Audio().Play(AudioType::CHIP_ERROR);
    return true;
  }

  insertingPiece = *iter;
  pieces.erase(iter);

  // move cursor to the center of the grid
  cursorLocation = ((GRID_SIZE / 2) * GRID_SIZE) + (GRID_SIZE/2);
  itemListSelected = false;

  Audio().Play(AudioType::CHIP_DESC);

  return true;
}

void PlayerCustScene::ExecuteLeftKey()
{
  if (itemListSelected) {
    SelectGridUI();
    return;
  }

  if (cursorLocation % GRID_SIZE > 0) {
    cursorLocation--;
    UpdateCursorHoverInfo();
    Audio().Play(AudioType::CHIP_SELECT, AudioPriority::low);
  }
}

void PlayerCustScene::ExecuteRightKey()
{
  if ((cursorLocation + 1) % GRID_SIZE > 0) {
    cursorLocation++;
    UpdateCursorHoverInfo();
    Audio().Play(AudioType::CHIP_SELECT, AudioPriority::low);
  }
  else if(!grabbingPiece && !insertingPiece) {
    SelectItemUI(listStart);
  }
}

void PlayerCustScene::ExecuteUpKey()
{
  if (state == state::block_prompt) {
    if (menuAnim.GetAnimationString() == "MOVE")
      return;

    menuAnim.SetAnimation("MOVE");
    menuAnim.Refresh(menuBox);
    Audio().Play(AudioType::CHIP_SELECT);
    return;
  }

  if (itemListSelected) {
    if (listStart > 0) {
      listStart--;
      UpdateItemListHoverInfo();
      Audio().Play(AudioType::CHIP_SELECT, AudioPriority::low);
    }
    return;
  }

  if (cursorLocation >= GRID_SIZE) {
    cursorLocation -= GRID_SIZE;
    UpdateCursorHoverInfo();
    Audio().Play(AudioType::CHIP_SELECT, AudioPriority::low);
  }
}

void PlayerCustScene::ExecuteDownKey()
{
  if (state == state::block_prompt) {
    if (menuAnim.GetAnimationString() == "REMOVE")
      return;

    menuAnim.SetAnimation("REMOVE");
    menuAnim.Refresh(menuBox);
    Audio().Play(AudioType::CHIP_SELECT);

    return;
  }

  if (itemListSelected) {
    // pieces end iterator doubles as the `run` action item
    if (listStart+1u <= pieces.size()) {
      listStart++;
      UpdateItemListHoverInfo();
      Audio().Play(AudioType::CHIP_SELECT, AudioPriority::low);
    }
    return;
  }

  if (cursorLocation + GRID_SIZE < grid.size()) {
    cursorLocation += GRID_SIZE;
    UpdateCursorHoverInfo();
    Audio().Play(AudioType::CHIP_SELECT, AudioPriority::low);
  }
}

void PlayerCustScene::ExecuteCancelInsert()
{
  pieces.push_back(insertingPiece);
  insertingPiece = nullptr;
}

void PlayerCustScene::ExecuteCancelInsertOnGrid()
{
  ExecuteCancelInsert(); SelectGridUI();
}

void PlayerCustScene::ExecuteCancelGrab()
{
  InsertPiece(grabbingPiece, grabStartLocation);
  cursorLocation = grabStartLocation;
  grabbingPiece = nullptr;
}

void PlayerCustScene::ExecuteCancelGrabOnGrid()
{
  ExecuteCancelGrab(); SelectGridUI();
}

bool PlayerCustScene::HandleUIKeys(double elapsed)
{
  if (keyRepeater.HandleInput(InputEvents::ui_up_group, std::bind(&PlayerCustScene::ExecuteUpKey, this))) {
    return true;
  }

  if (keyRepeater.HandleInput(InputEvents::ui_right_group, std::bind(&PlayerCustScene::ExecuteRightKey, this))) {
    return true;
  }

  if (keyRepeater.HandleInput(InputEvents::ui_down_group, std::bind(&PlayerCustScene::ExecuteDownKey, this))) {
    return true;
  }

  if (keyRepeater.HandleInput(InputEvents::ui_left_group, std::bind(&PlayerCustScene::ExecuteLeftKey, this))) {
    return true;
  }

  keyRepeater.Reset();

  if (Input().Has(InputEvents::pressed_option) && !itemListSelected) {
    if (grabbingPiece) {
      grabbingPiece->Revert();
      ExecuteCancelGrab();
    }

    if (insertingPiece) {
      insertingPiece->Revert();
      ExecuteCancelInsert();
    }

    SelectItemUI(pieces.size());
    StartScaffolding();
    return true;
  }

  if (Input().Has(InputEvents::pressed_cancel)) {
    if (!itemListSelected) {
      SelectItemUI(listStart);
      return true;
    }

    if (itemListSelected) {
      auto onYes = [this]() {
        QuitScene();
      };

      auto onNo = [this]() {
        SelectItemUI(listStart);
      };

      Audio().Play(AudioType::PAUSE, AudioPriority::low);
      questionInterface = new Question("Quit programming?", onYes, onNo);
      textbox.EnqueMessage(questionInterface);
      textbox.Open();
      textbox.CompleteCurrentBlock();
      return true;
    }
  }

  return false;
}

void PlayerCustScene::SelectGridUI()
{
  if (textbox.IsOpen()) {
    textbox.Close();
    questionInterface = nullptr;
  }

  keyRepeater.Reset();
  state = state::usermode;
  itemListSelected = false;
  UpdateCursorHoverInfo();
  Audio().Play(AudioType::CHIP_DESC, AudioPriority::low);
}

void PlayerCustScene::SelectItemUI(size_t idx)
{
  if (textbox.IsOpen()) {
    textbox.Close();
    questionInterface = nullptr;
  }

  state = state::usermode;
  keyRepeater.Reset();

  if (itemListSelected) return;

  itemListSelected = true;
  listStart = idx;
  UpdateItemListHoverInfo();
  Audio().Play(AudioType::CHIP_DESC_CLOSE, AudioPriority::low);
}

void PlayerCustScene::QuitScene()
{
  textbox.Close();

  using effect = swoosh::types::segue<BlackWashFade, milliseconds<500>>;
  getController().pop<effect>();
}

bool PlayerCustScene::HandlePieceAction(Piece*& piece, void(PlayerCustScene::* cancelFunc)())
{
  if (!piece) return false;

  if (Input().Has(InputEvents::pressed_cancel)) {
    piece->Revert();
    (this->*cancelFunc)();
    return true;
  }

  if (Input().Has(InputEvents::pressed_confirm)) {
    if (InsertPiece(piece, cursorLocation)) {
      piece = nullptr;
      Audio().Play(AudioType::CHIP_DESC_CLOSE);
      UpdateCursorHoverInfo();
    }
    else {
      Audio().Play(AudioType::CHIP_ERROR, AudioPriority::lowest);
    }
    return true;
  }

  if (Input().Has(InputEvents::pressed_shoulder_left)) {
    piece->RotateLeft();
    Audio().Play(AudioType::CHIP_SELECT, AudioPriority::low);
    return true;
  }

  if (Input().Has(InputEvents::pressed_shoulder_right)) {
    piece->RotateRight();
    Audio().Play(AudioType::CHIP_SELECT, AudioPriority::low);
    return true;
  }

  return false;
}

void PlayerCustScene::UpdateCursorHoverInfo()
{
  std::string infoTextString = "";

  if (grabbingPiece) {
    infoTextString = grabbingPiece->description;
  }
  else if (Piece* p = grid[cursorLocation]; p && state != state::block_prompt) {
    infoTextString = p->description;
    hoverText.SetString(p->name);

    sf::Vector2f pos = GridCursorToScreen();
    pos.y += 20.f * 2.f; // below the block it is on
    hoverText.setPosition(pos);

    sf::FloatRect bounds = hoverText.GetLocalBounds();

    float x = (cursorLocation % GRID_SIZE)/static_cast<float>(GRID_SIZE) * bounds.width;
    float y = (cursorLocation / GRID_SIZE) + 1u == GRID_SIZE ? 20.f*2.f : 0.f;
    hoverText.setOrigin({ x, y});
  }
  else {
    hoverText.SetString("");
  }

  infoText.SetString(stx::format_to_fit(infoTextString, 11, 3));
}

void PlayerCustScene::UpdateMenuPosition() {
  sf::Vector2f pos = GridCursorToScreen();

  // some offsets since the cursor sprite isn't centered...
  pos.x += 10.f * 2.f;
  pos.y += 20.f * 2.f;

  float y = (cursorLocation / GRID_SIZE) + 1u >= (GRID_SIZE - 1u) ? 60.f *2.f : 0.f;
  pos.y -= y;

  menuBox.setPosition(pos);
}

void PlayerCustScene::UpdateItemListHoverInfo()
{
  if (listStart < pieces.size()) {
    infoText.SetString(stx::format_to_fit(pieces[listStart]->description, 11, 3));
  }
  else {
    infoText.SetString("RUN?");
  }
}

void PlayerCustScene::onLeave()
{
}

void PlayerCustScene::onExit()
{
}

void PlayerCustScene::onEnter()
{
}

void PlayerCustScene::onResume()
{
}

void PlayerCustScene::onStart()
{
  gotoNextScene = false;
}

void PlayerCustScene::onUpdate(double elapsed)
{
  float delta = (progress / maxProgressTime);
  textbox.Update(elapsed);
  progressBarUVs.left -= IsCompileFinished()?1:2;
  progressBarUVs.width = static_cast<int>(std::min(118., (118.*delta)));
  progressBar.setTextureRect(progressBarUVs);

  UpdateCompileArrowPolygon(delta);

  sf::Vector2f dest = GridCursorToScreen();
  sf::Vector2f cursorPos = cursor.getPosition();
  cursorPos.x = swoosh::ease::interpolate(0.5f, cursorPos.x, dest.x);
  cursorPos.y = swoosh::ease::interpolate(0.5f, cursorPos.y, dest.y);
  claw.setPosition(cursorPos);
  cursor.setPosition(cursorPos);

  AnimateGrid();
  AnimateCursor(elapsed);
  AnimateButton(elapsed);
  AnimateBlock(elapsed);
  AnimateScaffolding(elapsed);

  // textbox takes priority
  if (questionInterface) {
    if (!textbox.IsOpen()) return;

    if (HasLeftInput()) {
      questionInterface->SelectYes();
      return;
    }

    if (HasRightInput()) {
      questionInterface->SelectNo();
      return;
    }

    if (Input().Has(InputEvents::pressed_confirm)) {
      if (!textbox.IsEndOfBlock()) {
        textbox.CompleteCurrentBlock();
        return;
      }

      if (textbox.IsEndOfBlock() && !textbox.IsFinalBlock()) {
        textbox.ShowNextLines();
        return;
      }

      questionInterface->ConfirmSelection();
      return;
    }

    return;
  }

  // handle the compile sequence states
  if (state == state::compiling || state == state::waiting) {
    progress += elapsed;

    if (state == state::waiting) {
      if (Input().Has(InputEvents::pressed_confirm)) {
        state = state::finishing;
        return;
      }
    }
    if (IsCompileFinished() && state != state::waiting) {
      CompleteAndSave();
      return;
    }

    if(!IsCompileFinished()) {
      double percent = (progress / static_cast<double>(maxProgressTime));
      double block_time = static_cast<double>(maxProgressTime/ GRID_SIZE);

      // complete the text when we're 75% through the block
      std::string label = "None";
      double COMPLETE_AT_PROGRESS = .75;
      double block_progress = std::fmod(progress, block_time) / block_time;
      double text_progress = std::min(block_progress * 1. / COMPLETE_AT_PROGRESS, 1.);

      size_t xoffset = static_cast<size_t>(percent * GRID_SIZE);
      size_t gridLoc = (3u * GRID_SIZE) + xoffset;
      Piece* p = grid[gridLoc];

      if (p) {
        label = p->name;
      }

      if (currCompileIndex != gridLoc) {
        if (p) {
          compiledHash[p] = true;
          Audio().Play(compile_item);
        }
        else {
          Audio().Play(compile_no_item);
        }

        currCompileIndex = gridLoc;
      }

      size_t len = static_cast<size_t>(std::ceil(label.size() * text_progress));
      infoText.SetString("Running...\n" + stx::format_to_fit(label.substr(0, len), 11, 2));
    }

    return;
  }
  
  // if finishing the compilation sequence
  if(state == state::finishing) {
    sf::Color color = progressBar.getFillColor();

    if (color.a > 0) {
      color.a -= 4;
      progressBarUVs.left -= 1; // another speedup at this step
      progressBar.setFillColor(color);
    }
    else {
      // the animation is over, stop blinking
      compiledHash.clear();

      auto onYes = [this]() {
        QuitScene();
      };

      auto onNo = [this]() {
        SelectItemUI(pieces.size());
      };

      std::string msg = "OK!\nRUN complete!\n\n";
      msg += "Good job, \n" + getController().Session().GetNick() + "!\n\n";
      msg += "Quit the Navi Customizer?";
      questionInterface = new Question(msg, onYes, onNo);
      textbox.EnqueMessage(questionInterface);
      textbox.Open();
    }

    return;
  }

  // handle input
  keyRepeater.Update(elapsed);

  if (itemListSelected) {
    if (HandleUIKeys(elapsed)) {
      return;
    }

    if (!Input().Has(InputEvents::pressed_confirm)) return;
    
    if (HandleSelectItemFromList()) return;

    StartCompile();

    return;
  }

  if (state == state::block_prompt) {
    HandleMenuUIKeys(elapsed);
    return;
  }

  if (!grabbingPiece && !insertingPiece) {
    // DEBUG: generate random pieces
    /*if (Input().GetAnyKey() == sf::Keyboard::Space) {
      insertingPiece = GenerateRandomBlock();
      return;
    }*/

    if (Input().Has(InputEvents::pressed_confirm)) {
      if (Piece* piece = grid[cursorLocation]) {
        HandleGrabAction();
      }

      return;
    }
  }
  else if(HandlePieceAction(insertingPiece, &PlayerCustScene::ExecuteCancelInsertOnGrid)) {
    return;
  }
  else if(HandlePieceAction(grabbingPiece, &PlayerCustScene::ExecuteCancelGrabOnGrid)) {
    return;
  }

  if (HandleUIKeys(elapsed)) {
    return;
  }
}

void PlayerCustScene::onDraw(IRenderer& renderer)
{
  static GameSession& session = getController().Session();

  renderer.submit(&bg);
  renderer.submit(&sceneLabel);
  renderer.submit(&gridSprite);

  sf::Sprite blockSprite;
  blockSprite.setScale(2.f, 2.f);

  size_t count{};
  for (auto& [key, value] : blockTypeInUseTable) {
    if (value == 0) continue;
    float x = (gridSprite.getPosition().x + (8.f*2.f)) + (count*(14.f+1.f)*2.f);
    float y = gridSprite.getPosition().y + 2.f*2.f;
    blockSprite.setTexture(*blockTextures[key], true);
    blockSprite.setTextureRect({ 60, 0, 14, 9 });
    blockSprite.setPosition({ x, y });
    // TODO: remove Clone()
    renderer.submit(Clone(blockSprite));
    count++;
  }

  for (size_t i = 0; i < GRID_SIZE; i++) {
    for (size_t j = 0; j < GRID_SIZE; j++) {
      size_t index = (i * GRID_SIZE) + j;
      if (Piece* p = grid[index]) {
        if (IsGridEdge(i, j)) {
          DrawEdgeBlock(renderer, p, i, j);
        }
        else {
          sf::Vector2f blockPos = BlockToScreen(i, j);

          sf::Texture& tex = session.IsBlockAllowed(grid[index]->uuid) ? *blockTextures[grid[index]->typeIndex] : *disabledBlockTexture;
          blockSprite.setTexture(tex, true);
          blockSprite.setPosition({ blockPos.x, blockPos.y });
          RefreshBlock(p, blockSprite);
          // TODO: remove Clone()
          renderer.submit(Clone(blockSprite));
        }
      }
    }
  }

  // draw track
  RefreshTrack();
  renderer.submit(&track);

  // draw items
  float yoffset = 0.;
  
  if (listStart+1u > MAX_ITEMS_ON_SCREEN) {
    yoffset = -blueButtonSprite.getLocalBounds().height * ((listStart+1u) - MAX_ITEMS_ON_SCREEN);
  }

  sf::Vector2f previewPos{};
  sf::Vector2f bottom;
  bottom.x = 140.f;

  for (size_t i = 0; i < pieces.size(); i++) {
    RefreshButton(i);
    blueButtonSprite.setPosition(bottom.x * 2.f, (bottom.y + yoffset) * 2.f);
    // TODO: remove Clone()
    renderer.submit(Clone(blueButtonSprite));

    if (listStart == i) {
      sf::Vector2f dest = blueButtonSprite.getPosition();
      sf::Vector2f pos = itemArrowCursor.getPosition();
      pos.x = swoosh::ease::interpolate(0.5f, pos.x, dest.x);
      pos.y = swoosh::ease::interpolate(0.5f, pos.y, dest.y);
      itemArrowCursor.setPosition(pos);
      previewPos = { dest.x + (blueButtonSprite.getGlobalBounds().width+10.f), dest.y };
    }

    if (scaffolding == 1.f) {
      sf::Vector2f textOffset = { 4.f, 8.f };
      itemText.SetString(pieces[i]->name);
      itemText.SetColor(sf::Color(33, 115, 140));
      itemText.setPosition((bottom.x + textOffset.x) * itemText.getScale().x, ((bottom.y + yoffset) * itemText.getScale().y) + textOffset.y + 2.f);
      // TODO: remove Clone()
      renderer.submit(Clone(itemText));
      itemText.SetColor(sf::Color::White);
      itemText.setPosition((bottom.x + textOffset.x) * itemText.getScale().x, ((bottom.y + yoffset) * itemText.getScale().y) + textOffset.y);
      // TODO: remove Clone()
      renderer.submit(Clone(itemText));
    }

    bottom.y += 19; // pixel height of button
  }

  if (bottom.y == 0) {
    bottom.y = 19; // pixel height of button
  }

  // draw run button
  RefreshButton(pieces.size());
  greenButtonSprite.setPosition(bottom.x*2.f, (bottom.y+yoffset)*2.f);
  // TODO: remove Clone()
  renderer.submit(Clone(greenButtonSprite));

  // bgBottom overlays on top of the list so it doesn't peak underneath the info box
  renderer.submit(&bgBottom);

  if (itemListSelected) {
    if (state == state::usermode) {
      renderer.submit(&itemArrowCursor);
    }

    if (listStart == pieces.size()) {
      sf::Vector2f dest = greenButtonSprite.getPosition();
      sf::Vector2f pos = itemArrowCursor.getPosition();
      pos.x = swoosh::ease::interpolate(0.5f, pos.x, dest.x);
      pos.y = swoosh::ease::interpolate(0.5f, pos.y, dest.y);
      itemArrowCursor.setPosition(pos);
    }
    else {
      // check if this value was set
      if (previewPos.x > 0.) {
        DrawPreview(renderer, pieces[listStart], previewPos);
      }
    }
  }

  // draw info box
  renderer.submit(&infoBox);

  sf::Vector2f pos = infoText.getPosition();
  infoText.SetColor(sf::Color(33, 115, 140));
  infoText.setPosition(pos.x+2.f, pos.y+2.f);
  // TODO: remove Clone()
  renderer.submit(Clone(infoText));
  infoText.SetColor(sf::Color::White);
  infoText.setPosition(pos);
  // TODO: remove Clone()
  renderer.submit(Clone(infoText));

  // progress bar shows when compiling
  renderer.submit(&progressBar);
  
  if(!itemListSelected) {
    // claw/cursor always draws on top
    Piece* p = insertingPiece ? insertingPiece : grabbingPiece ? grabbingPiece : nullptr;
    sf::Vector2f piecePos = insertingPiece ? cursor.getPosition() : GridCursorToScreen();
    if (p) {
      DrawPiece(renderer, p, piecePos);

      if (grabbingPiece) {
        renderer.submit(&claw);
      }
    }
    else {
      // display any hovering text
      sf::Vector2f pos = hoverText.getPosition();
      hoverText.SetColor(sf::Color::Black);
      hoverText.setPosition(pos.x + 2.f, pos.y + 2.f);
      // TODO: remove Clone()
      renderer.submit(Clone(hoverText));
      hoverText.SetColor(sf::Color::White);
      hoverText.setPosition(pos);
      // TODO: remove Clone()
      renderer.submit(Clone(hoverText));

      if (grid[cursorLocation]) {
        renderer.submit(&claw);
      }
      else {
        renderer.submit(&cursor);
      }
    }
  }

  // grab prompt
  if (state == state::block_prompt) {
    renderer.submit(&menuBox);
  }

  // textbox is top over everything
  renderer.submit(&textbox);
}

void PlayerCustScene::onEnd()
{
}