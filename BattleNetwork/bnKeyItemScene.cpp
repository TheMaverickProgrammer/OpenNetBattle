#include "bnKeyItemScene.h"
#include "bnTextureResourceManager.h"
#include <Segues/BlackWashFade.h>

KeyItemScene::KeyItemScene(swoosh::ActivityController& controller, const std::vector<Item>& items) :
  items(items),
  label(Font::Style::thin),
  textbox(360,40, Font::Style::thin),
  Scene(controller)
{
  label.setScale(2.f, 2.f);

  moreText.setTexture(*Textures().LoadFromFile(TexturePaths::TEXT_BOX_NEXT_CURSOR));
  moreText.setScale(2.f, 2.f);

  scroll.setTexture(*Textures().LoadFromFile(TexturePaths::FOLDER_SCROLLBAR));
  scroll.setScale(2.f, 2.f);

  cursor.setTexture(*Textures().LoadFromFile(TexturePaths::FOLDER_CURSOR));
  cursor.setScale(2.f, 2.f);

  bg.setTexture(*Textures().LoadFromFile("resources/scenes/items/bg.png"), true);
  bg.setScale(2.f, 2.f);

  // Text box navigator
  textbox.setPosition(100, 205);
  textbox.SetTextColor(sf::Color::Black);
  textbox.Mute();

  // Load key items 
  /*items = std::vector<KeyItemScene::Item>{
    {"SmartWatch", "A watch program"},
    {"N1Pass", "Pass that allows you to enter the N1 tournament"},
    {"Key", "Opens a gate"},
    {"GigaFreeze", "Control cyberworld with this neat trick"},
    {"HeatProg", "Fire program from KonstComp"},
    {"Pizza", "Digital pizza for net navis to nom. Cyberfood is perpetually warm and never gets cold. Yum!"},
    {"NotAVirus", "Given as a gift from a shady navi"},
    {"KCode", "Security code for KonstComp server"},
    {"MCode", "Security code for MarsComp server"}
  };*/

  if (this->items.size()) {
    textbox.SetText(this->items[0].desc);
    textbox.CompleteAll();

    for (auto& item : this->items) {
      item.name = item.name.substr(0, 10);
    }
  }



  scroll.setPosition(450.f, 40.f);
  setView(sf::Vector2u(480, 320));
}

KeyItemScene::~KeyItemScene()
{
}

void KeyItemScene::onLeave()
{
}

void KeyItemScene::onExit()
{
}

void KeyItemScene::onEnter()
{
}

void KeyItemScene::onResume()
{
}

void KeyItemScene::onStart()
{
}

void KeyItemScene::onUpdate(double elapsed)
{
  textbox.Update(elapsed);

  unsigned lastRow = row;
  unsigned lastCol = col;

  if (Input().Has(InputEvents::pressed_ui_up)) {
    row--;
  }
  else if (Input().Has(InputEvents::pressed_ui_down)) {
    row++;
  }
  else if (Input().Has(InputEvents::pressed_ui_left)) {
    col--;
  }
  else if (Input().Has(InputEvents::pressed_ui_right)) {
    col++;
  }
  else if (Input().Has(InputEvents::pressed_confirm)) {
    if (textbox.HasMore()) {
      for (size_t i = 0; i < textbox.GetNumberOfFittingLines(); i++) {
        textbox.ShowNextLine();
      }
      Audio().Play(AudioType::CHIP_CHOOSE);
    }
  }
  else if (Input().Has(InputEvents::pressed_cancel)) {
    using namespace swoosh::types;
    getController().pop<segue<BlackWashFade, milli<500>>>();
    Audio().Play(AudioType::CHIP_DESC_CLOSE);
  }

  signed lastRowOffset = rowOffset;

  if (row >= maxRows) {
    rowOffset++;
    row--;
  }

  if (row < 0) {
    rowOffset--;
    row++;
  }

  signed maxRowOffset = static_cast<signed>(std::ceil((double)items.size() / (double)maxCols));

  row = std::max(0, row);

  if (maxRowOffset > 1) {
    row = std::min(maxRowOffset - 1, row);
  }

  col = std::max(0, col);
  col = std::min(maxCols - 1, col);

  size_t index = static_cast<size_t>((row * maxCols) + col);

  rowOffset = std::max(0, rowOffset);
  rowOffset = std::min(maxRowOffset, rowOffset);

  size_t offset = static_cast<size_t>(rowOffset * maxCols);
  index += offset;

  // Now that we've forced the change of values into a restricted range,
  // check to see if we actually selected anything new
  if (index < items.size() && (rowOffset != lastRowOffset || row != lastRow || col != lastCol)) {
    textbox.SetText(items[index].desc);
    textbox.CompleteAll();
    Audio().Play(AudioType::CHIP_SELECT);
  }
  else {
    row = lastRow;
    col = lastCol;
    rowOffset = lastRowOffset;
  }

  float startPos = 40.f;
  float endPos = 160.f;
  float rowsPerView = std::max(1.f, static_cast<float>(maxRowOffset - maxRows));
  float delta = static_cast<float>(rowOffset) / rowsPerView;
  float y = (endPos * delta) + (startPos * (1.f - delta));

  scroll.setPosition(450.f, y);

  cursor.setPosition(30 + (col * 200) + 1.f, 50 + (row * 30) + 1.f);

  auto bounce = std::sin((float)totalElapsed * 20.0f);
  moreText.setPosition(sf::Vector2f(225, 140+bounce) * 2.0f);
  totalElapsed += static_cast<float>(elapsed);
}

void KeyItemScene::onDraw(IRenderer& renderer)
{
  renderer.submit(&bg);

  if (items.size()) {
    for (size_t i = 0; i < maxCols; i++) {
      for (size_t j = 0; j < maxRows; j++) {
        size_t offset = static_cast<size_t>(rowOffset * maxCols);
        size_t index = offset + static_cast<size_t>((j * maxCols) + i);
        if (index < items.size()) {
          auto& item = items[index];
          label.SetString(item.name);
          label.setPosition(55 + (i * 200) + 2.f, 42 + (j * 30) + 2.f);

          const auto pos = label.getPosition();
          //label.SetColor(sf::Color::Black);
          //surface.draw(label);

          label.setPosition(pos.x - 2.f, pos.y - 2.f);
          label.SetColor(sf::Color::White);
          // TODO: remove Clone()
          renderer.submit(Clone(label));
        }
      }
    }
  }

  renderer.submit(&scroll);
  renderer.submit(&textbox);

  if (textbox.HasMore()) {
    renderer.submit(&moreText);
  }

  renderer.submit(&cursor);
}

void KeyItemScene::onEnd()
{
}
