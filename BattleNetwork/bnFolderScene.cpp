#include <time.h>

#include <Swoosh/Activity.h>
#include <Swoosh/ActivityController.h>
#include <Swoosh/Ease.h>
#include <Swoosh/Timer.h>

#include "bnGameSession.h"
#include "bnFolderScene.h"
#include "bnFolderEditScene.h"
#include "bnFolderChangeNameScene.h"
#include "Segues/BlackWashFade.h"
#include "bnCardFolder.h"
#include "bnPlayerPackageManager.h"
#include "bnCardPackageManager.h"
#include "Android/bnTouchArea.h"
#include "bnMessageQuestion.h"

#include <SFML/Graphics.hpp>
using sf::RenderWindow;
using sf::VideoMode;
using sf::Clock;
using sf::Event;

using namespace swoosh::types;

#include "Segues/PushIn.h"

FolderScene::FolderScene(swoosh::ActivityController &controller, CardFolderCollection& collection) :
  collection(collection),
  folderSwitch(true),
  font(Font::Style::wide),
  menuLabel("CHIP FOLDER", font),
  cardFont(Font::Style::thick),
  cardLabel("", cardFont),
  numberFont(Font::Style::thick),
  numberLabel("", numberFont),
  textbox(sf::Vector2f(4, 255)),
  questionInterface(nullptr),
  Scene(controller)
{
  textbox.SetTextSpeed(1.0f);

  promptOptions = false;
  enterText = false;
  gotoNextScene = true;

  // Menu name
  menuLabel.setPosition(sf::Vector2f(20.f, 8.0f));
  menuLabel.setScale(2.f, 2.f);

  // Selection input delays
  maxSelectInputCooldown = 0.25; // 4th of a second
  selectInputCooldown = maxSelectInputCooldown;

  // Card UI font
  cardLabel.setPosition(275.f, 15.f);
  cardLabel.setScale(2.f, 2.f);

  numberLabel.SetColor(sf::Color(48, 56, 80));
  numberLabel.setScale(2.f, 2.f);
  numberLabel.setOrigin(numberLabel.GetWorldBounds().width, 0);
  numberLabel.setPosition(sf::Vector2f(170.f, 28.0f));

  // folder menu graphic
  bg = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_INFO_BG));
  bg.setScale(2.f, 2.f);

  scrollbar = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_SCROLLBAR));
  scrollbar.setScale(2.f, 2.f);
  scrollbar.setPosition(410.f, 60.f);

  folderBox = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_BOX));
  folderBox.setScale(2.f, 2.f);

  folderDisabled = sf::Sprite(*Textures().LoadFromFile("resources/ui/folder_disabled.png"));
  folderDisabled.setScale(2.f, 2.f);

  RefreshOptions();

  folderCursor = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_BOX_CURSOR));
  folderCursor.setScale(2.f, 2.f);

  folderEquip = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_EQUIP));
  folderEquip.setScale(2.f, 2.f);

  cursor = sf::Sprite(*Textures().LoadFromFile(TexturePaths::TEXT_BOX_CURSOR));
  cursor.setScale(2.f, 2.f);
  cursor.setPosition(2.0, 155.0f);

  element = sf::Sprite(*Textures().LoadFromFile(TexturePaths::ELEMENT_ICON));
  element.setScale(2.f, 2.f);
  element.setPosition(2.f*25.f, 146.f);

  cardIcon = sf::Sprite();
  cardIcon.setScale(2.f, 2.f);
  cardIcon.setTextureRect(sf::IntRect(0, 0, 14, 14));

  mbPlaceholder = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_MB));
  mbPlaceholder.setScale(2.f, 2.f);

  equipAnimation = Animation("resources/ui/folder_equip.animation");
  equipAnimation.SetAnimation("BLINK");
  equipAnimation << Animator::Mode::Loop;

  folderCursorAnimation = Animation("resources/ui/folder_cursor.animation");
  folderCursorAnimation.SetAnimation("BLINK");
  folderCursorAnimation << Animator::Mode::Loop;

  equipAnimation.Update(0,folderEquip);
  folderCursorAnimation.Update(0, folderCursor);

  noPreview = Textures().LoadFromFile(TexturePaths::CHIP_MISSINGDATA);
  noIcon = Textures().LoadFromFile(TexturePaths::CHIP_ICON_MISSINGDATA);

  maxCardsOnScreen = 5;
  currCardIndex = 0;
  currFolderIndex = lastFolderIndex = 0;
  selectedFolderIndex = optionIndex = 0;

  totalTimeElapsed = frameElapsed = folderOffsetX = 0.0;

  folder = nullptr;

  folderNames = collection.GetFolderNames();

  if (collection.GetFolderNames().size() > 0) {
    collection.GetFolder(0, folder);

    numOfCards = folder->GetSize();
  }
  else {
    numOfCards = 0;
  }

#ifdef __ANDROID__
  canSwipe = true;
  touchStart = false;
  touchPosX = touchPosStartX = 0;
  releasedB = false;
#endif

  setView(sf::Vector2u(480, 320));
}

FolderScene::~FolderScene() { ; }

void FolderScene::onStart() {
  gotoNextScene = false;

#ifdef __ANDROID__
    StartupTouchControls();
#endif
}

void FolderScene::onUpdate(double elapsed) {
  GameSession& session = getController().Session();

  frameElapsed = elapsed;
  totalTimeElapsed += elapsed;

  folderCursorAnimation.Update((float)elapsed, folderCursor);
  equipAnimation.Update((float)elapsed, folderEquip);
  textbox.Update(elapsed);

  int lastCardIndex = currCardIndex;
  int lastFolderIndex = currFolderIndex;
  int lastOptionIndex = optionIndex;

  // Prioritize textbox input
  if (textbox.IsOpen() && questionInterface) {
    if (Input().Has(InputEvents::pressed_ui_left)) {
      questionInterface->SelectYes();
    } else if (Input().Has(InputEvents::pressed_ui_right)) {
      questionInterface->SelectNo();
    }
    else if (Input().Has(InputEvents::pressed_confirm)) {
      if (!textbox.IsEndOfBlock()) {
        textbox.CompleteCurrentBlock();
      }
      else if (!textbox.IsEndOfMessage()) {
        questionInterface->Continue();
      }
      else {
        questionInterface->ConfirmSelection();
      }
    }
    else if (Input().Has(InputEvents::pressed_cancel)) {
      if (!textbox.IsEndOfBlock()) {
        textbox.CompleteCurrentBlock();
      }
      else if (textbox.IsEndOfMessage()) {
        questionInterface->SelectNo();
        questionInterface->ConfirmSelection();
      }
    }

    return;
  }

  // Scene keyboard controls
  if (enterText) {
    InputEvent  cancelButton = InputEvents::released_cancel;

#ifdef __ANDROID__
    cancelButton = PRESSED_B;
#endif

    if (Input().Has(cancelButton)) {
#ifdef __ANDROID__
        sf::Keyboard::setVirtualKeyboardVisible(false);
#endif
      enterText = false;
      bool changed = collection.SetFolderName(folderNames[currFolderIndex], folder);

      if (!changed) {
        folderNames[currFolderIndex] = collection.GetFolderNames()[currFolderIndex];
      }

      Input().GetInputTextBuffer().EndCapture();
    }
    else {
      folderNames[currFolderIndex] = Input().GetInputTextBuffer().ToString();

#ifdef __ANDROID__
        sf::Keyboard::setVirtualKeyboardVisible(true);
#endif
    }
  } else if (!gotoNextScene) {
    if (Input().Has(InputEvents::pressed_ui_up) || Input().Has(InputEvents::held_ui_up)) {
      selectInputCooldown -= elapsed;

      if (selectInputCooldown <= 0) {
        if (!extendedHold) {
          selectInputCooldown = maxSelectInputCooldown;
          extendedHold = true;
        }
        else {
          selectInputCooldown = maxSelectInputCooldown * 0.25;
        }

        if (!promptOptions) {
          currCardIndex--;
        }
        else {
          optionIndex--;
        }
      }
    }
    else if (Input().Has(InputEvents::pressed_ui_down) || Input().Has(InputEvents::held_ui_down)) {
      selectInputCooldown -= elapsed;

      if (selectInputCooldown <= 0) {
        if (!extendedHold) {
          selectInputCooldown = maxSelectInputCooldown;
          extendedHold = true;
        }
        else {
          selectInputCooldown = maxSelectInputCooldown * 0.25;
        }

        if (!promptOptions) {
          currCardIndex++;
        }
        else {
          optionIndex++;
        }
      }
    }
    else if (Input().Has(InputEvents::pressed_ui_right)) {
      extendedHold = false;

      selectInputCooldown -= elapsed;

      if (selectInputCooldown <= 0) {
        selectInputCooldown = maxSelectInputCooldown;

        if (!promptOptions) {
          currFolderIndex++;
          folderSwitch = true;
        }
      }
    }
    else if (Input().Has(InputEvents::pressed_ui_left)) {
      extendedHold = false;

      selectInputCooldown -= elapsed;

      if (selectInputCooldown <= 0) {
        selectInputCooldown = maxSelectInputCooldown;

        if (!promptOptions) {
          currFolderIndex--;
          folderSwitch = true;
        }
      }
    }
    else {
      selectInputCooldown = 0;
      extendedHold = false;
    }

    currCardIndex = std::max(0, currCardIndex);
    currCardIndex = std::min(std::max(0, numOfCards - 1), currCardIndex);
    currFolderIndex = std::max(0, currFolderIndex);
    currFolderIndex = std::min(std::max(0, static_cast<int>(folderNames.size()) - 1), currFolderIndex);
    optionIndex = std::max(0, optionIndex);

    if (folderNames.size()) {
      optionIndex = std::min(4, optionIndex); // total of 5 options 
    }
    else {
      optionIndex = 0; // "NEW" option only
    }

    if (currCardIndex != lastCardIndex 
      || currFolderIndex != lastFolderIndex 
      || optionIndex != lastOptionIndex) {
      Audio().Play(AudioType::CHIP_SELECT);
    }

#ifdef __ANDROID__
    if(lastFolderIndex != currFolderIndex) {
        folderSwitch = true;
    }
#endif

    if (folderSwitch) {
      if (collection.GetFolderNames().size() > 0 && folderNames.size() > 0) {
        collection.GetFolder(*(folderNames.begin() + currFolderIndex), folder);

        numOfCards = folder->GetSize();
        currCardIndex = 0;
      }

      folderSwitch = false;
    }

    InputEvent cancelButton = InputEvents::released_cancel;

    if (Input().Has(InputEvents::pressed_cancel)) {
      if (!promptOptions) {
        gotoNextScene = true;
        Audio().Play(AudioType::CHIP_DESC_CLOSE);

        using segue = segue<PushIn<direction::right>, milli<500>>;
        getController().pop<segue>();
      } else {
          promptOptions = false;
          Audio().Play(AudioType::CHIP_DESC_CLOSE);
      }
    } else if (Input().Has(InputEvents::pressed_cancel)) {
        if (promptOptions) {
          promptOptions = false;
          Audio().Play(AudioType::CHIP_DESC_CLOSE);
        }
    } else if (Input().Has(InputEvents::pressed_confirm)) {
      if (!promptOptions) {
        promptOptions = true;
        RefreshOptions();
        Audio().Play(AudioType::CHIP_DESC);
      }
      else if(folderNames.size()) {
        switch (optionIndex) {
        case 0: // EDIT
          if (folder && IsFolderAllowed(folder)) {
            Audio().Play(AudioType::CHIP_CONFIRM);

            using effect = segue<BlackWashFade, milli<500>>;
            getController().push<effect::to<FolderEditScene>>(*folder, equipFolderOnReturn);
            gotoNextScene = true;
          }
          else {
            Audio().Play(AudioType::CHIP_ERROR);
          }
          break;
        case 1: // EQUIP
        {
          if (EquipFolder(currFolderIndex)) {
            Audio().Play(AudioType::PA_ADVANCE);
          }
          else {
            Audio().Play(AudioType::CHIP_ERROR);
          }
        }
          break;
        case 2: // CHANGE NAME
          if (folder && IsFolderAllowed(folder)) {
            Audio().Play(AudioType::CHIP_CONFIRM);
            using effect = segue<BlackWashFade, milli<500>>;
            getController().push<effect::to<FolderChangeNameScene>>(folderNames[currFolderIndex]);

            gotoNextScene = true;
          }
          else {
            Audio().Play(AudioType::CHIP_ERROR);
          }
          break;
        case 3: // NEW 
          MakeNewFolder();
          promptOptions = false;
          currFolderIndex = (int)folderNames.size() - 1;
          folderSwitch = true;

          break;
        case 4: // DELETE 
          DeleteFolder([this]() {
            promptOptions = false;
            currFolderIndex = std::max(0, currFolderIndex-1);
            folderSwitch = true;
            });
          break;
        }
      }
      else {
        MakeNewFolder();
        promptOptions = false;
        currFolderIndex = std::max(0, static_cast<int>(folderNames.size() - 1));
        folderSwitch = true;
      }
    }
  }

#ifdef __ANDROID__
  if(canSwipe) {
      Logger::Log("touch is down");

      if (sf::Touch::isDown(0)) {

      sf::Vector2i touchPosition = sf::Touch::getPosition(0, *ENGINE.GetWindow());
      sf::Vector2f coords = ENGINE.GetWindow()->mapPixelToCoords(touchPosition,
                                                                 ENGINE.GetDefaultView());
      sf::Vector2i iCoords = sf::Vector2i((int) coords.x, (int) coords.y);
      touchPosition = iCoords;

      canSwipe = false;

      if(touchPosition.y < 100) {
          if (!touchStart) {
              touchStart = true;
              touchPosStartX = touchPosition.x;
          }

          touchPosX = touchPosition.x;
          folderOffsetX = (touchPosStartX - touchPosX);
          Logger::Log("folderOffsetX: " + std::to_string(folderOffsetX));

          canSwipe = true;
      } else if(folderOffsetX > 100){
          currFolderIndex++;
          canSwipe = true;
      } else if(folderOffsetX < -100) {
          currFolderIndex--;
          canSwipe = true;
      }
    } else {
      canSwipe = true;
      touchStart = false;
    }
  }
#endif
}

void FolderScene::onLeave() {
#ifdef __ANDROID__
    ShutdownTouchControls();
#endif
}

void FolderScene::onExit()
{
}

void FolderScene::onEnter()
{
}

void FolderScene::onResume() {
#ifdef __ANDROID__
  StartupTouchControls();
#endif

  gotoNextScene = false;

  // Save any edits
  collection.SetFolderName(folderNames[currFolderIndex], folder);
  folderSwitch = true;

  // Equip if prompted
  if (equipFolderOnReturn) {
    equipFolderOnReturn = false; // reset flag
    EquipFolder(currFolderIndex);
  }
}

void FolderScene::onDraw(IRenderer& renderer) {
  CardPackageManager& packageManager = getController().CardPackagePartitioner().GetPartition(Game::LocalPartition);

  renderer.submit(&bg);
  renderer.submit(&menuLabel);

  if (folderNames.size() > 0) {
    for (int i = 0; i < folderNames.size(); i++) {
      CardFolder* folder{ nullptr };
      collection.GetFolder(folderNames[i], folder);
      if (folder == nullptr) {
          continue;
      }
      bool allowed = IsFolderAllowed(folder);

      float folderLeft = 26.0f + (i*144.0f) - (float)folderOffsetX;

      sf::Sprite& drawFolder = allowed ? folderBox : folderDisabled;
      drawFolder.setPosition(folderLeft, 34.0f);
      renderer.submit(UI{ &drawFolder });

      sf::Color color = allowed ? sf::Color::White : sf::Color(70, 70, 70); 
      cardLabel.SetColor(color);
      cardLabel.SetString(folderNames[i]);
      cardLabel.setOrigin(0.0f, 0.0f);
      cardLabel.setPosition(folderLeft + 12.0f, 54.0f);
      renderer.submit(UI{ &cardLabel });

      if (i == selectedFolderIndex) {
        folderEquip.setPosition(folderLeft - 2.0f, 30.0f);
        renderer.submit(UI{ &folderEquip });
      }
    }

#ifdef __ANDROID__
    if(!canSwipe) {
      float x = swoosh::ease::interpolate((float) frameElapsed * 7.f, folderCursor.getPosition().x,
                                         98.0f + (std::min(2, currFolderIndex) * 144.0f));
      folderCursor.setPosition(x, 68.0f);

      if (currFolderIndex > 2) {
        float before = folderOffsetX;
        folderOffsetX = swoosh::ease::interpolate(frameElapsed * 7.0, folderOffsetX,
                                                  (double) (((currFolderIndex - 2) * 144.0f)));

        if(int(before) == int(folderOffsetX)) {
            canSwipe = true;
            touchStart = false;
        }
      } else {
        float before = folderOffsetX;
        folderOffsetX = swoosh::ease::interpolate(frameElapsed * 7.0, folderOffsetX, 0.0);

        if(int(before) == int(folderOffsetX)) {
            canSwipe = true;
            touchStart = false;
        }
      }
    }
#else 
    float x = swoosh::ease::interpolate(
      (float)frameElapsed * 7.f, folderCursor.getPosition().x,
      98.0f + (std::min(2, currFolderIndex) * 144.0f)
    );

    folderCursor.setPosition(x, 68.0f);

    if (currFolderIndex > 2) {
      double before = folderOffsetX;
      double after = (double(currFolderIndex) - 2.0) * 144.0;
      folderOffsetX = swoosh::ease::interpolate(frameElapsed * 7.0, folderOffsetX, after);
    }
    else {
      double before = folderOffsetX;
      folderOffsetX = swoosh::ease::interpolate(frameElapsed * 7.0, folderOffsetX, 0.0);
    }
#endif

    renderer.submit(UI{ &folderCursor });
  }

  // ScrollBar limits: Top to bottom screen position when selecting first and last card respectively
  float top = 120.0f; float bottom = 170.0f;
  float depth = ((float)(currCardIndex) / (float)numOfCards)*bottom;
  scrollbar.setPosition(436.f, top + depth);

  renderer.submit(UI{ &scrollbar });
  renderer.submit(UI{ &folderOptions });

  float scale = 0.0f;

  if (promptOptions) {
    scale = swoosh::ease::interpolate((float)frameElapsed*4.0f, 2.0f, folderOptions.getScale().y);
    renderer.submit(UI{ &cursor });
  }
  else {
    scale = swoosh::ease::interpolate((float)frameElapsed*4.0f, 0.0f, folderOptions.getScale().y);
  }

  folderOptions.setScale(2.0f, scale);


  // swoosh::ease::interpolate((float)frameElapsed * 7.0f, cursor.getPosition().y, 138.0f + ((optionIndex) * 32.0f));
  float y = 138.0f + ((optionIndex) * 32.0f); 
  cursor.setPosition(2.0, y);

  if (!folder) return;
  //Once the folder is confirmed to exist, update the size.
  //This avoids a crash when updating folders.
  numOfCards = folder->GetSize();
  //And since size was just called, we don't need to call it again anymore.
  //Editing this to use a call to the variable.
  if (numOfCards != 0) {
    // Move the card library iterator to the current highlighted card
    CardFolder::Iter iter = folder->Begin();

    for (int j = 0; j < currCardIndex; j++) {
      iter++;
    }

    if (!IsFolderAllowed(folder)) {
      cardLabel.SetColor(sf::Color(16,136,232));
    }
    else {
      cardLabel.SetColor(sf::Color::White);
    }

    cardLabel.SetString(folderNames[currFolderIndex]);
    cardLabel.setOrigin(0.f, 0.f);
    cardLabel.setPosition(195.0f, 102.0f);
    renderer.submit(UI{ &cardLabel });

    // Now that we are at the viewing range, draw each card in the list
    for (int i = 0; i < maxCardsOnScreen && currCardIndex + i < numOfCards; i++) {
      std::string id = (*iter)->GetUUID();
      if (id.empty()) continue;

      bool hasID = packageManager.HasPackage(id);

      float cardIconY = 132.0f + (32.f * i);

      if (hasID) {
        cardIcon.setTexture(*packageManager.FindPackageByID(id).GetIconTexture());        
        // make the cardLabel white
        cardLabel.SetColor(sf::Color::White);
      }
      else {
        cardIcon.setTexture(*noIcon);

        // make the cardLabel red
        cardLabel.SetColor(sf::Color::Red);
      }

      cardIcon.setPosition(2.f * 99.f, cardIconY);
      renderer.submit(UI{ &cardIcon });

      cardLabel.setPosition(2.f*115.f, cardIconY + 4.0f);
      cardLabel.SetString((*iter)->GetShortName());
      renderer.submit(UI{ &cardLabel });

      int offset = (int)((*iter)->GetElement());
      element.setTextureRect(sf::IntRect(14 * offset, 0, 14, 14));
      element.setPosition(2.f*173.f, cardIconY);
      renderer.submit(UI{ &element });

      cardLabel.setPosition(2.f*190.f, cardIconY + 4.0f);
      cardLabel.SetString(std::string() + (*iter)->GetCode());
      renderer.submit(UI{ &cardLabel });

      mbPlaceholder.setPosition(2.f*200.f, cardIconY + 2.0f);
      renderer.submit(UI{ &mbPlaceholder });
      iter++;
    }
  }

  renderer.submit(UI{ &textbox });
}

const bool FolderScene::IsFolderAllowed(CardFolder* folder)
{
  return getController().Session().IsFolderAllowed(folder);
}

void FolderScene::MakeNewFolder() {
  Audio().Play(AudioType::CHIP_CONFIRM); 

  std::string name = "NewFldr";
  int i = 0;

  while (!collection.MakeFolder(name)) {
    i++;
    name = "NewFldr" + std::to_string(i);
  }

  folderNames = collection.GetFolderNames();
}

void FolderScene::DeleteFolder(std::function<void()> onSuccess)
{
  if (!folderNames.size()) {
    Audio().Play(AudioType::CHIP_ERROR);
    return;
  }

  auto onYes = [onSuccess, this]() {
    if (collection.DeleteFolder(folderNames[currFolderIndex])) {
      onSuccess();
      folderNames = collection.GetFolderNames();

      if (folderNames.empty()) {
        folder = nullptr;
      }
      else {
        while (!collection.GetFolder(currFolderIndex, folder)) {
          if (currFolderIndex == 0) break;
          currFolderIndex = currFolderIndex - 1;
        }
      }
    }

    textbox.Close();
    Audio().Play(AudioType::CHIP_DESC_CLOSE);
  };

  auto onNo = [this]() {
    textbox.Close();
    Audio().Play(AudioType::CHIP_DESC_CLOSE);
  };

  //if (questionInterface) delete questionInterface;
  questionInterface = new Question("Delete this folder?", onYes, onNo);

  textbox.EnqueMessage(
    sf::Sprite(*Textures().LoadFromFile(TexturePaths::MUG_NAVIGATOR)),
    "resources/ui/navigator.animation", 
    questionInterface);

  textbox.Open();
}

bool FolderScene::EquipFolder(size_t index)
{
  CardFolder* folder{ nullptr };
  GameSession& session = getController().Session();

  if (collection.GetFolder(index, folder) && IsFolderAllowed(folder)) {
    selectedFolderIndex = index;
    collection.SwapOrder(0, selectedFolderIndex);

    // Save this session data
    std::string folderStr = collection.GetFolderNames()[0];
    std::string naviSelectedStr = session.GetKeyValue("SelectedNavi");

    if (naviSelectedStr.empty()) {
      naviSelectedStr = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition).FirstValidPackage();
    }

    session.SetKeyValue("FolderFor:" + naviSelectedStr, folderStr);

    return true;
  }

  return false;
}

void FolderScene::RefreshOptions()
{
  const bool emptyCollection = collection.GetFolderNames().empty();
  const std::shared_ptr<sf::Texture> folderOptionsTex = emptyCollection ? Textures().LoadFromFile(TexturePaths::FOLDER_OPTIONS_NEW) : Textures().LoadFromFile(TexturePaths::FOLDER_OPTIONS);
  folderOptions = sf::Sprite(*folderOptionsTex);
  folderOptions.setOrigin(folderOptions.getGlobalBounds().width / 2.0f, folderOptions.getGlobalBounds().height / 2.0f);

  if (emptyCollection) {
    folderOptions.setPosition(98.0f, 145.0f);
}
  else {
    folderOptions.setPosition(98.0f, 210.0f);
  }
}

void FolderScene::onEnd() {
#ifdef __ANDROID__
    ShutdownTouchControls();
#endif
}

#ifdef __ANDROID__
void FolderScene::StartupTouchControls() {
    /* Android touch areas*/
    TouchArea& rightSide = TouchArea::create(sf::IntRect(240, 0, 240, 320));

    rightSide.enableExtendedRelease(true);

    rightSide.onTouch([]() {
        Input().VirtualKeyEvent(InputEvent::RELEASED_A);
    });

    rightSide.onRelease([this](sf::Vector2i delta) {
        if(!releasedB) {
            Input().VirtualKeyEvent(InputEvent::PRESSED_A);
        }

        releasedB = false;
    });

    rightSide.onDrag([this](sf::Vector2i delta){
        if(delta.x < -25 && !releasedB && !touchStart) {
            Input().VirtualKeyEvent(InputEvent::PRESSED_B);
            Input().VirtualKeyEvent(InputEvent::RELEASED_B);
            releasedB = true;
        }
    });
}

void FolderScene::ShutdownTouchControls() {
    TouchArea::free();
}
#endif
