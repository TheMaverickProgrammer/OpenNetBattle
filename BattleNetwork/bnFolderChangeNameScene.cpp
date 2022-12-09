#include "bnFolderChangeNameScene.h"
#include "bnInputManager.h"
#include "Segues/BlackWashFade.h"
#include "bnPlayerPackageManager.h"
#include "bnGameSession.h"

#include <Swoosh/ActivityController.h>

#define UPPERCASE 0
#define LOWERCASE 1
#define COMMANDS  2

FolderChangeNameScene::FolderChangeNameScene(ActivityController& controller, std::string& folderName) : 
  Scene(controller), 
  folderName(folderName), 
  font(Font::Style::thin), 
  nameLabel("", font) {
  elapsed = 0;

  leave = true;
  // folder menu graphic
  bg = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_CHANGE_NAME_BG));
  bg.setScale(2.f, 2.f);

  cursorPieceLeft = sf::Sprite(*Textures().LoadFromFile(TexturePaths::LETTER_CURSOR));
  cursorPieceLeft.setScale(2.f, 2.f);
  cursorPieceLeft.setPosition(12 * 2.f, 58 * 2.f);

  cursorPieceRight = cursorPieceLeft;

  letterPos = cursorPosX = cursorPosY = currTable = 0;

  nameLabel.setScale(2.f, 2.f);
  
  name = std::string(8, '_'); // 8 characters allowed

  for(int i = 0; i < name.size(); i++) {
    if (i >= folderName.size()) break;

    if (folderName[i] == ' ') continue;

    name[i] = folderName[i];
  }

  animatorLeft = Animation("resources/ui/letter_cursor.animation");
  animatorLeft.Load();
  animatorLeft.SetAnimation("BLINK_L");
  animatorLeft << Animator::Mode::Loop;

  animatorRight = Animation("resources/ui/letter_cursor.animation");
  animatorRight.Load();
  animatorRight.SetAnimation("BLINK_R");
  animatorRight << Animator::Mode::Loop;

  table = {
    { // First column
      {"A", "B", "C", "D", "E"},
      {"F", "G", "H", "I", "J"},
      {"K", "L", "M", "N", "O"},
      {"P", "Q", "R", "S", "T"},
      {"U", "V", "W", "X", "Y"},
      {"Z"}
    },
    { // Second column
      {"a", "b", "c", "d", "e"},
      {"f", "g", "h", "i", "j"},
      {"k", "l", "m", "n", "o"},
      {"p", "q", "r", "s", "t"},
      {"u", "v", "w", "x", "y"},
      {"z"}
    },
    { // Third column are actions
      {"NEXT"},
      {"BACK"},
      {"OK"},
      {"END"}
    }
  };

  setView(sf::Vector2u(480, 320));
}

FolderChangeNameScene::~FolderChangeNameScene() {

}

void FolderChangeNameScene::ExecuteAction(size_t table, size_t x, size_t y)
{
  auto entry = this->table[table][y][x];

  if (entry == "NEXT") {
    DoNEXT();
  }
  else if (entry == "BACK") {
    DoBACK();
  }
  else if (entry == "OK") {
    DoOK();
  }
  else if (entry == "END") {
    DoEND();
  }
  else { // it's a letter entry
    if (letterPos + 1 == 9) { // denotes end of input, reject
      Audio().Play(AudioType::CHIP_ERROR);
    }
    else { // not end of input, add latter, and increase letterPos
      name[letterPos++] = entry[0];
      letterPos = std::min(letterPos, 8);

      Audio().Play(AudioType::CHIP_CHOOSE);
    }
  }
}

void FolderChangeNameScene::DoBACK()
{
  letterPos = std::max(0, letterPos - 1);

}

void FolderChangeNameScene::DoNEXT()
{
  letterPos = std::min(letterPos + 1, 8);

}

void FolderChangeNameScene::DoOK()
{
  // Take '_' out of names
  auto copy = name;
  std::replace(copy.begin(), copy.end(), '_', ' ');

  // Prompt question
  // TODO

  // We must have a key for the selected navi
  auto naviSelectedStr = getController().Session().GetKeyValue("SelectedNavi");
  if (naviSelectedStr.empty()) 
    naviSelectedStr = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition).FirstValidPackage(); 

  // Save this session data new folder name
  getController().Session().SetKeyValue("FolderFor:" + naviSelectedStr, folderName);

  // Set original variable to new results
  folderName = copy;

  // Finish
  DoEND();
}

void FolderChangeNameScene::DoEND()
{
  using namespace swoosh::types;
  using effect = segue<BlackWashFade, milli<500>>;
  getController().pop<effect>();

  Audio().Play(AudioType::CHIP_DESC_CLOSE);
  leave = true;
}

void FolderChangeNameScene::onStart() {
  leave = false;
}

void FolderChangeNameScene::onUpdate(double elapsed) {
  FolderChangeNameScene::elapsed += float(elapsed);

  bool executeAction = false; // Did we execute the action this frame?

  auto lastX = cursorPosX;
  auto lastY = cursorPosY;

  if (!leave) {
    if (Input().Has(InputEvents::pressed_cancel)) {
      if (letterPos != 0 || name[letterPos] != '_') {
        Audio().Play(AudioType::CHIP_CANCEL);
      }
      else {
        Audio().Play(AudioType::CHIP_ERROR);
      }
      
      if (letterPos + 1 == 9) {
        letterPos--;
      }
      else {
        name[letterPos--] = '_';
        letterPos = std::max(0, letterPos);
      }
    }
    else if (Input().Has(InputEvents::pressed_ui_left)) {
    cursorPosX--;
    }
    else if (Input().Has(InputEvents::pressed_ui_right)) {
    cursorPosX++;
    }
    else if (Input().Has(InputEvents::pressed_ui_up)) {
    cursorPosY--;
    }
    else if (Input().Has(InputEvents::pressed_ui_down)) {
    cursorPosY++;
    }
    else if (Input().Has(InputEvents::pressed_shoulder_left)) {
      DoBACK();
    }
    else if (Input().Has(InputEvents::pressed_shoulder_right)) {
      DoNEXT();
    }
    else if (Input().Has(InputEvents::pressed_option)) {
      // hard coded, if columns change this must change too
      cursorPosX = 0;
      cursorPosY = 2;
      currTable = 2;
    }
    else if (Input().Has(InputEvents::pressed_confirm)) {
      executeAction = true;
    }
  }

  // Check bounds of cursorPosX and cursorPosY, update current table and pos if need to

  // Check X pos
  if (lastX != cursorPosX) {
    Audio().Play(AudioType::CHIP_SELECT);

    if (cursorPosX < 0) {
      currTable--;

      if (currTable < 0) {
        currTable = 0;
        cursorPosX = 0;
      }
      else {
        cursorPosY = std::min(cursorPosY, int(table[currTable].size()) - 1);

        cursorPosX = int(table[currTable][cursorPosY].size()) - 1;
      }
    }

    cursorPosY = std::min(cursorPosY, int(table[currTable].size()) - 1);

    if (cursorPosX > table[currTable][cursorPosY].size() - 1) {
      currTable++;

      // base 0, 2 is max of the three tables
      if (currTable > 2) {
        currTable = 2;
        cursorPosY = std::min(cursorPosY, int(table[currTable].size()) - 1);
        cursorPosX = int(table[currTable][cursorPosY].size()) - 1;
      }
      else {
        cursorPosY = std::min(cursorPosY, int(table[currTable].size()) - 1);
        cursorPosX = 0;
      }
    }
  }

  // Check Y pos
  if (lastY != cursorPosY) {
    Audio().Play(AudioType::CHIP_SELECT);

    if (cursorPosY < 0) {
      cursorPosY = 0;
    }

    if (cursorPosY > table[currTable].size() - 1) {
      cursorPosY = int(table[currTable].size()) - 1;
    }

    // keep x in bounds too
    cursorPosX = std::min(cursorPosX, int(table[currTable][cursorPosY].size() - 1));
  }

  // Update flashing cursor position
  auto tableOffset = (88.f)*currTable;
  auto startX = 12.f;
  auto startY = 60.f;
  auto offsetx = 16.f * cursorPosX;
  auto offsety = 16.f * cursorPosY;

  cursorPieceLeft.setPosition (2.f*(startX + offsetx + tableOffset), 2.f*(startY + offsety));

  // Last column has wide text e.g. "NEXT", "BACK", ... ensure we fit those words
  if (currTable == 2) { offsetx += 22; if (cursorPosY == 2) { offsetx -= 14; } else if (cursorPosY == 3) { offsetx -= 8; } }

  cursorPieceRight.setPosition(2.f*(startX + offsetx + tableOffset + 4.0f), 2.f*(startY + offsety));

  // Resolve action command if any
  if (executeAction) {
    ExecuteAction(currTable, cursorPosX, cursorPosY);
  }

  animatorLeft.Update ((float)elapsed, cursorPieceLeft);
  animatorRight.Update((float)elapsed, cursorPieceRight);

  auto c = name[letterPos];
}

void FolderChangeNameScene::onDraw(IRenderer& renderer) {
  renderer.submit(&bg);
  renderer.submit(&cursorPieceLeft);
  renderer.submit(&cursorPieceRight);

  bool blink = (int(elapsed * 3000) % 1000) < 500;
  float labelTop = 18 * 2.f - 2.f;

  for (int i = 0; i < name.size(); i++) {
    if(i == letterPos && blink)
      continue;

    nameLabel.SetString(name[i]);
    nameLabel.setPosition((65 + (i*8)) * 2.f, labelTop);
    renderer.submit(UI{ &nameLabel });
  }

  // 9th character is a special * asterisk to denote the end of input
  if (letterPos + 1 == 9 && !blink) {
    nameLabel.SetString('*');
    nameLabel.setPosition((65 + (8 * 8)) * 2.f, labelTop);
    renderer.submit(UI{ &nameLabel });
  }
}
