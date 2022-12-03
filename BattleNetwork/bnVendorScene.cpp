#include "bnVendorScene.h"
#include "bnMessage.h"
#include "bnTextureResourceManager.h"

#include <filesystem>
#include <Segues/BlackWashFade.h>

//
// Background
//

#define TILEW 64
#define TILEH 64

#define PATH std::filesystem::path("resources/scenes/vendors")

VendorScene::VendorBackground::VendorBackground() :
  x(0.0f),
  y(0.0f),
  Background(Textures().LoadFromFile(PATH / "bg.png"), 240, 180)
{
  FillScreen(sf::Vector2u(TILEW, TILEH));
}

VendorScene::VendorBackground::~VendorBackground()
{
}

void VendorScene::VendorBackground::Update(double _elapsed)
{
  y -= 0.75f * static_cast<float>(_elapsed);
  x -= 0.75f * static_cast<float>(_elapsed);

  if (x < 0) x = 1;
  if (y < 0) y = 1;

  Wrap(sf::Vector2f(x, y));
}

//
// VendorScene
//

void VendorScene::ShowDefaultMessage()
{
  // Not using the class `message` member variable
  // so we can know when we are reading iformative text versus
  // seeing the default message
  auto message = new Message(defaultMessage);
  message->ShowEndMessageCursor(false);
  textbox.EnqueMessage(mugshot, anim, message);
  textbox.CompleteCurrentBlock();
  this->showDescription = false;
}

VendorScene::VendorScene(
  swoosh::ActivityController& controller,
  const std::vector<VendorScene::Item>& items,
  uint32_t monies,
  const std::shared_ptr<sf::Texture>& mugshotTexture,
  const Animation& anim,
  const VendorScene::PurchaseCallback& callback
) :
  callback(callback),
  monies(monies),
  items(items),
  label(Font::Style::thin),
  textbox({ 4, 255 }),
  mugshotTexture(mugshotTexture),
  mugshot(*mugshotTexture),
  anim(anim),
  Scene(controller)
{
  label.setScale(2.f, 2.f);

  moreItems.setTexture(*Textures().LoadFromFile(TexturePaths::TEXT_BOX_NEXT_CURSOR));
  moreItems.setScale(2.f, 2.f);

  wallet.setTexture(*Textures().LoadFromFile("resources/scenes/vendors/price.png"), true);
  wallet.setScale(0.f, 0.f); // hide
  wallet.setPosition(340, 0.f);

  list.setTexture(*Textures().LoadFromFile("resources/scenes/vendors/list.png"), true);
  list.setScale(0.f, 0.f); // hide
  list.setPosition(0.f, 0.f);

  cursor.setTexture(*Textures().LoadFromFile(TexturePaths::TEXT_BOX_CURSOR));
  cursor.setScale(2.f, 2.f);

  bg = new VendorBackground;

  defaultMessage = "How can I help you? OPTION:Description CANCEL:Back";
  this->ShowDefaultMessage();

  textbox.Mute();
  textbox.Open();

  setView(sf::Vector2u(480, 320));
}

VendorScene::~VendorScene()
{
  delete bg;
}

void VendorScene::onLeave()
{
}

void VendorScene::onExit()
{
}

void VendorScene::onEnter()
{
}

void VendorScene::onResume()
{
}

void VendorScene::onStart()
{
  stateTimer.start();
}

void VendorScene::onUpdate(double elapsed)
{
  if (items.empty()) {
    currState = state::slide_out;
  }

  const float maxStateTime = static_cast<float>(frames(7).asSeconds().value);
  textbox.Update(elapsed);
  bg->Update(elapsed);
  stateTimer.update(sf::seconds(static_cast<float>(elapsed)));
  totalElapsed += elapsed;
  unsigned bob = from_seconds(this->totalElapsed * 0.25).count() % 5; // 5 pixel bobs
  float bobf = static_cast<float>(bob);

  if (currState == state::slide_in) {
    wallet.setScale(2.f, 2.f); // reveal
    list.setScale(2.f, 2.f); // reveal

    float startPos = 480.0f + wallet.getLocalBounds().width*wallet.getScale().x;
    float endPos = 340.f;
    float delta = swoosh::ease::linear(stateTimer.getElapsed().asSeconds(), maxStateTime, 1.0f);
    float pos = (endPos * delta) + (startPos * (1.f - delta));
    wallet.setPosition(pos, 0.f);

    startPos = -list.getLocalBounds().width*list.getScale().x;
    endPos = 0.f;
    pos = (endPos * delta) + (startPos * (1.f - delta));
    list.setPosition(pos, 0.f);

    if (stateTimer.getElapsed().asSeconds() >= maxStateTime) {
      currState = state::active;
    }

    return;
  }
  else if(currState == state::slide_out) {
    float startPos = 480.0f + wallet.getLocalBounds().width*wallet.getScale().x;
    float endPos = 340.f;
    float delta = swoosh::ease::linear(stateTimer.getElapsed().asSeconds(), maxStateTime, 1.0f);
    float pos = (startPos * delta) + (endPos * (1.f - delta));
    wallet.setPosition(pos, 0.f);

    startPos = -list.getLocalBounds().width*list.getScale().x;
    endPos = 0.f;
    pos = (startPos * delta) + (endPos * (1.f - delta));
    list.setPosition(pos, 0.f);

    return;
  }

  //
  // case currState == state::active
  //

  unsigned lastRow = row;
  bool updateDescription = false;

  if (question) {
    if (Input().Has(InputEvents::pressed_ui_left)) {
      question->SelectYes() ? Audio().Play(AudioType::CHIP_SELECT) : 0;
    }
    else if (Input().Has(InputEvents::pressed_ui_right)) {
      question->SelectNo() ? Audio().Play(AudioType::CHIP_SELECT) : 0;
    }
    else if (Input().Has(InputEvents::pressed_confirm)) {
      question->ConfirmSelection();
      textbox.CompleteCurrentBlock();
    }
    else if (Input().Has(InputEvents::pressed_cancel)) {
      question->SelectNo();
      question->ConfirmSelection();
      textbox.CompleteCurrentBlock();
    }
  }
  else {
    if (Input().Has(InputEvents::pressed_ui_up)) {
      row--;
    }
    else if (Input().Has(InputEvents::pressed_ui_down)) {
      row++;
    }
    else if (Input().Has(InputEvents::pressed_confirm)) {
      // todo: long item descriptions?
      if (!showDescription) {
         if(!message) {
          size_t index = static_cast<size_t>(row) + rowOffset;
          std::string itemName = items[index].name;
          int itemCost = items[index].cost;
          std::string msg = "\"" + itemName + "\"\nAre you sure?";
          Audio().Play(AudioType::CHIP_DESC);
          question = new Question(msg,
            [this, itemName, itemCost]() {
              if (static_cast<int>(monies) >= itemCost) {
                monies -= itemCost;
                message = new Message("I bought " + itemName);
                textbox.EnqueMessage(message);
                textbox.CompleteCurrentBlock();
                question = nullptr;
                Audio().Play(AudioType::ITEM_GET);

                ShowDefaultMessage(); //enqueues default message
                callback(itemName);
              }
              else {
                message = new Message("Not enough monies!");
                textbox.EnqueMessage(message);
                textbox.CompleteCurrentBlock();
                question = nullptr;
                Audio().Play(AudioType::CHIP_ERROR);

                ShowDefaultMessage(); //enqueues default message
              }
            },
            [this]() {
              this->ShowDefaultMessage();
              question = nullptr;
              Audio().Play(AudioType::CHIP_DESC_CLOSE);
            });

          textbox.ClearAllMessages();
          textbox.EnqueMessage(mugshot, anim, question);
          textbox.CompleteCurrentBlock();
         }
         else {
           // message ptr is valid, we're reading the purchase text
           if (textbox.IsEndOfBlock()) {
             if (message->Continue() == Message::ContinueResult::dequeued) {
               textbox.CompleteCurrentBlock();
               message = nullptr;
             }
           }
         }
      }
    }
    else if (Input().Has(InputEvents::pressed_cancel)) {
      // Don't ask to leave if in the middle of another state (including another question)
      if (!question && !message) {
        question = new Question("Leaving so soon?",
          [this]() {
            textbox.EnqueMessage(mugshot, anim, new Message("Goodbye!"));
            textbox.CompleteCurrentBlock();
            using namespace swoosh::types;
            getController().pop<segue<BlackWashFade, milli<500>>>();
            Audio().Play(AudioType::CHIP_DESC_CLOSE);
            question = nullptr;
            currState = state::slide_out;
            stateTimer.reset();
          },
          [this]() {
            this->ShowDefaultMessage();
            question = nullptr;
            Audio().Play(AudioType::CHIP_DESC_CLOSE);
          });

        textbox.ClearAllMessages();
        textbox.EnqueMessage(mugshot, anim, question);
        textbox.CompleteCurrentBlock();
        Audio().Play(AudioType::CHIP_DESC);
      }
    }
    else if (Input().Has(InputEvents::pressed_option)) {
      // Only allow toggling if displaying the default message...
      if (!message) {
        showDescription = !showDescription;

        if (!showDescription) {
          textbox.ClearAllMessages();
          this->ShowDefaultMessage();
        }

        Audio().Play(AudioType::CHIP_DESC);

        updateDescription = true;
      }
    }
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

  signed maxRowOffset = static_cast<signed>(items.size());

  row = std::max(0, row);
  row = std::min(maxRowOffset - 1, row);

  size_t index = static_cast<size_t>(row);

  rowOffset = std::max(0, rowOffset);
  rowOffset = std::min(maxRowOffset, rowOffset);

  size_t offset = static_cast<size_t>(rowOffset);
  index += offset;

  // Now that we've forced the change of values into a restricted range,
  // check to see if we actually selected anything new
  if (index < items.size() && (updateDescription || rowOffset != lastRowOffset || row != lastRow)) {
    if (rowOffset != lastRowOffset || row != lastRow) {
      Audio().Play(AudioType::CHIP_SELECT);
    };

    if (showDescription) {
      // do not mistake this for VendorScene::message
      auto message = new Message(items[index].desc);
      message->ShowEndMessageCursor(false);
      textbox.ClearAllMessages();
      textbox.EnqueMessage(message);
      textbox.CompleteCurrentBlock();
    }
  }
  else {
    row = lastRow;
    rowOffset = lastRowOffset;
  }

  float startPos = 40.f;
  float endPos = 160.f;
  float rowsPerView = std::max(1.f, static_cast<float>(maxRowOffset - maxRows));
  float delta = static_cast<float>(rowOffset) / rowsPerView;
  float y = (endPos * delta) + (startPos * (1.f - delta));

  cursor.setPosition(6.f + bobf, 18 + (row * 32) + 1.f);

  auto bounce = std::sin((float)totalElapsed * 20.0f);
  moreItems.setPosition(sf::Vector2f(225, 140 + bounce) * 2.0f);
}

void VendorScene::onDraw(IRenderer& renderer)
{
  renderer.submit(bg);
  renderer.submit(&list);
  renderer.submit(&wallet);

  label.SetColor(sf::Color(41, 99, 140));
  label.SetString(std::to_string(monies) + "$");
  label.setOrigin(label.GetLocalBounds().width, label.GetLocalBounds().height);
  label.setPosition(wallet.getPosition() + sf::Vector2f{(wallet.getLocalBounds().width*2.f) - 10.f, 56.f});
  renderer.submit(&label);

  if (items.size() && currState == state::active) {
    for (size_t j = 0; j < maxRows; j++) {
      size_t offset = static_cast<size_t>(rowOffset);
      size_t index = offset + static_cast<size_t>(j);
      if (index < items.size()) {
        auto& item = items[index];
        label.SetString(item.name);
        label.setPosition(35, 13.f + (j * 32.f));
        label.setOrigin(0.f, 0.f);
        // TODO: remove Clone()
        renderer.submit(Clone(label));

        label.SetString(std::to_string(item.cost)+"$");
        label.setOrigin(label.GetLocalBounds().width, 0.f);
        label.setPosition(322.f, 13.f + (j * 32.f));
        // TODO: remove Clone()
        renderer.submit(Clone(label));
      }
    }
  }

  renderer.submit(&textbox);

  /*
  if (textbox.HasMore()) {
    surface.draw(moreText);
  }*/

  if (currState == state::active) {
    renderer.submit(&cursor);
  }
}

void VendorScene::onEnd()
{
}
