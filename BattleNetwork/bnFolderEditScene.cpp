#include <time.h>
#include <cmath>

#include <Swoosh/Activity.h>
#include <Swoosh/ActivityController.h>
#include <Swoosh/Ease.h>
#include <Swoosh/Timer.h>

#include "bnFolderEditScene.h"
#include "bnGameSession.h"
#include "Segues/BlackWashFade.h"
#include "bnCardFolder.h"
#include "bnCardPackageManager.h"
#include "Android/bnTouchArea.h"
#include "stx/string.h"

#include <SFML/Graphics.hpp>
using sf::RenderWindow;
using sf::VideoMode;
using sf::Clock;
using sf::Event;

FolderEditScene::FolderEditScene(swoosh::ActivityController& controller, CardFolder& folder) :
  Scene(controller),
  camera(sf::View(sf::Vector2f(240, 160), sf::Vector2f(480, 320))),
  folder(folder),
  hasFolderChanged(false),
  card(),
  font(Font::Style::wide),
  limitLabel("NO", Font::Style::tiny),
  limitLabel2("\nMAX", Font::Style::tiny),
  menuLabel("FOLDER EDIT", font),
  cardFont(Font::Style::thick),
  cardLabel("", cardFont),
  cardDescFont(Font::Style::thin),
  cardDesc("", cardDescFont),
  numberFont(Font::Style::thick),
  numberLabel(Font::Style::gradient) {
  // Move card data into their appropriate containers for easier management
  PlaceFolderDataIntoCardSlots();
  PlaceLibraryDataIntoBuckets();

  // We must account for existing card data to accurately represent what's left from our pool
  ExcludeFolderDataFromPool();

  // Add sort options
  ComposeSortOptions();

  // Menu name font
  menuLabel.setPosition(sf::Vector2f(20.f, 8.0f));
  menuLabel.setScale(2.f, 2.f);

  // Selection input delays
  maxSelectInputCooldown = 0.5; // half of a second
  selectInputCooldown = maxSelectInputCooldown;

  // Battle::Card UI font
  cardLabel.setPosition(275.f, 15.f);
  cardLabel.setScale(2.f, 2.f);
  numberLabel.setScale(1.6f, 1.6f);

  limitLabel.setScale(2.f, 2.f);
  limitLabel.SetLetterSpacing(0.f);
  limitLabel.SetLineSpacing(1.2f);

  limitLabel2.setScale(2.f, 2.f);
  limitLabel2.SetLetterSpacing(0.f);
  limitLabel2.SetLineSpacing(1.2f);

  // Battle::Card description font
  cardDesc.SetColor(sf::Color::Black);
  cardDesc.setScale(2.f, 2.f);

  // folder menu graphic
  bg = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_VIEW_BG));
  bg.setScale(2.f, 2.f);

  folderDock = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_DOCK));
  folderDock.setScale(2.f, 2.f);
  folderDock.setPosition(2.f, 30.f);

  packDock = sf::Sprite(*Textures().LoadFromFile(TexturePaths::PACK_DOCK));
  packDock.setScale(2.f, 2.f);
  packDock.setPosition(480.f, 30.f);

  scrollbar = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_SCROLLBAR));
  scrollbar.setScale(2.f, 2.f);

  folderCursor = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_CURSOR));
  folderCursor.setScale(2.f, 2.f);
  folderCursor.setPosition((2.f * 90.f), 64.0f);
  folderSwapCursor = folderCursor;

  packCursor = folderCursor;
  packCursor.setPosition((2.f * 90.f) + 480.0f, 64.0f);
  packSwapCursor = packCursor;

  sortCursor = folderCursor;

  folderNextArrow = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_NEXT_ARROW));
  folderNextArrow.setScale(2.f, 2.f);

  packNextArrow = folderNextArrow;
  packNextArrow.setScale(-2.f, 2.f);

  folderCardCountBox = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_SIZE));
  folderCardCountBox.setPosition(sf::Vector2f(425.f, 10.f + folderCardCountBox.getLocalBounds().height));
  folderCardCountBox.setScale(2.f, 2.f);
  folderCardCountBox.setOrigin(folderCardCountBox.getLocalBounds().width / 2.0f, folderCardCountBox.getLocalBounds().height / 2.0f);

  cardHolder = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_CHIP_HOLDER));
  cardHolder.setPosition(16.f, 35.f);
  cardHolder.setScale(2.f, 2.f);

  packCardHolder = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_CHIP_HOLDER));
  packCardHolder.setPosition(310.f + 480.f, 35.f);
  packCardHolder.setScale(2.f, 2.f);

  folderSort = sf::Sprite(*Textures().LoadFromFile(TexturePaths::FOLDER_SORT));
  folderSort.setScale(2.f, 2.f);
  folderSort.setPosition(cardHolder.getPosition() + sf::Vector2f(11 * 2, 5 * 2));

  element = sf::Sprite(*Textures().LoadFromFile(TexturePaths::ELEMENT_ICON));
  element.setScale(2.f, 2.f);

  // Current card graphic
  card = sf::Sprite();
  card.setTextureRect(sf::IntRect(0, 0, 56, 48));
  card.setScale(2.f, 2.f);
  card.setOrigin(card.getLocalBounds().width / 2.0f, card.getLocalBounds().height / 2.0f);

  cardIcon = sf::Sprite();
  cardIcon.setTextureRect(sf::IntRect(0, 0, 14, 14));
  cardIcon.setScale(2.f, 2.f);

  noPreview = Textures().LoadFromFile(TexturePaths::CHIP_MISSINGDATA);
  noIcon = Textures().LoadFromFile(TexturePaths::CHIP_ICON_MISSINGDATA);

  cardRevealTimer.start();
  easeInTimer.start();

  /* folder view */
  folderView.maxCardsOnScreen = 7;
  folderView.numOfCards = int(folderCardSlots.size());

  /* library view */
  packView.maxCardsOnScreen = 7;
  packView.numOfCards = int(poolCardBuckets.size());

  prevViewMode = currViewMode = ViewMode::folder;

  totalTimeElapsed = frameElapsed = 0.0;

  canInteract = false;

  setView(sf::Vector2u(480, 320));
}

FolderEditScene::~FolderEditScene() { ; }

void FolderEditScene::onStart() {
  canInteract = true;
}

void FolderEditScene::onUpdate(double elapsed) {
  frameElapsed = elapsed;
  totalTimeElapsed += elapsed;

  cardRevealTimer.update(sf::seconds(static_cast<float>(elapsed)));
  easeInTimer.update(sf::seconds(static_cast<float>(elapsed)));

  auto offset = camera.GetView().getCenter().x - 240;
  bg.setPosition(offset, 0.f);
  menuLabel.setPosition(offset + 20.0f, 8.f);

  // We need to move the view based on the camera pos
  camera.Update((float)elapsed);
  setView(camera.GetView());

  // update the folder sort cursor
  sf::Vector2f sortCursorOffset = sf::Vector2f(0, 2.0 * (14.0 + (cursorSortIndex * 16.0)));
  sortCursor.setPosition(folderSort.getPosition() + sortCursorOffset);

  // Scene keyboard controls
  if (canInteract) {
    if (isInSortMenu) {
      ISortOptions<ICardView, 7u>* options = &poolSortOptions;

      if (currViewMode == ViewMode::folder) {
        options = &folderSortOptions;
      }

      if (Input().Has(InputEvents::pressed_ui_up)) {
        if (cursorSortIndex > 0) {
          cursorSortIndex--;
        }
        else {
          cursorSortIndex = options->size() - 1;
        }
      }
      if (Input().Has(InputEvents::pressed_ui_down)) {
        if (cursorSortIndex + 1 < options->size()) {
          cursorSortIndex++;
        }
        else {
          cursorSortIndex = 0;
        }
      }

      if (Input().Has(InputEvents::pressed_confirm)) {
        options->SelectOption(cursorSortIndex);
        Audio().Play(AudioType::CHIP_DESC);
      }

      if (Input().Has(InputEvents::pressed_cancel)) {
        Audio().Play(AudioType::CHIP_DESC_CLOSE);
        isInSortMenu = false;
      }
      return;
    }
    else if (Input().Has(InputEvents::pressed_pause)) {
      Audio().Play(AudioType::CHIP_DESC);
      isInSortMenu = true;
      cursorSortIndex = 0;
      return;
    }

    CardView* view = nullptr;

    if (currViewMode == ViewMode::folder) {
      view = &folderView;
    }
    else if (currViewMode == ViewMode::pool) {
      view = &packView;
    }

    // If CTRL+C is pressed during this scene, copy the folder contents in discord-friendly format
    if (Input().HasSystemCopyEvent()) {
      std::string buffer;
      const std::string& nickname = getController().Session().GetNick();
      const CardPackageManager& manager = getController().CardPackagePartitioner().GetPartition(Game::LocalPartition);

      buffer += "```\n";
      buffer += "# Folder by " + nickname + "\n";

      if (folderView.numOfCards == 0) {
        buffer += "# [NONE] \n";
      }

      for (int i = 0; i < folderView.numOfCards; i++) {
        const Battle::Card& card = folderCardSlots[i].ViewCard();
        const std::string& uuid = card.GetUUID();

        if (!manager.HasPackage(uuid)) continue;

        const CardMeta& meta = manager.FindPackageByID(uuid);
        buffer += uuid + " " + meta.GetPackageFingerprint() + " " + card.GetCode() + "\n";
      }

      buffer += "```";

      if (buffer != Input().GetClipboard()) {
        Input().SetClipboard(buffer);
        Audio().Play(AudioType::NEW_GAME);
      }

      return;
    }

    if (Input().Has(InputEvents::pressed_ui_up) || Input().Has(InputEvents::held_ui_up)) {
      if (lastKey != InputEvents::pressed_ui_up) {
        lastKey = InputEvents::pressed_ui_up;
        extendedHold = false;
      }

      selectInputCooldown -= elapsed;

      if (selectInputCooldown <= 0) {
        if (!extendedHold) {
          selectInputCooldown = maxSelectInputCooldown;
          extendedHold = true;
        }
        //Set the index.
        view->currCardIndex = std::max(0, view->currCardIndex - 1);
        //Check the index's validity. If proper, play the sound and reset the timer.
        if (view->currCardIndex >= 0) {
          Audio().Play(AudioType::CHIP_SELECT);
          cardRevealTimer.reset();
        }
        //Condition: if we're at the top of the screen, decrement the last card on screen.
        if (view->currCardIndex < view->firstCardOnScreen) {
          --view->firstCardOnScreen;
        }
      }
    }
    else if (Input().Has(InputEvents::pressed_ui_down) || Input().Has(InputEvents::held_ui_down)) {
      if (lastKey != InputEvents::pressed_ui_down) {
        lastKey = InputEvents::pressed_ui_down;
        extendedHold = false;
      }

      selectInputCooldown -= elapsed;

      if (selectInputCooldown <= 0) {
        if (!extendedHold) {
          selectInputCooldown = maxSelectInputCooldown;
          extendedHold = true;
        }
        //Adjust the math to use std::min so that the current card index is always set to numOfCards-1 at most.
        //Otherwise, if available, increment the index by 1.
        view->currCardIndex = std::min(view->numOfCards - 1, view->currCardIndex + 1);

        if (view->currCardIndex < view->numOfCards) {
          Audio().Play(AudioType::CHIP_SELECT);
          cardRevealTimer.reset();
        }
        //Condition: If we're at the bottom of the menu, increment the last card on screen.
        if (view->currCardIndex > view->firstCardOnScreen + view->maxCardsOnScreen - 1) {
          ++view->firstCardOnScreen;
        }
      }
    }
    else if (Input().Has(InputEvents::pressed_shoulder_left) || Input().Has(InputEvents::held_shoulder_left)) {
      if (lastKey != InputEvents::pressed_shoulder_left) {
        lastKey = InputEvents::pressed_shoulder_left;
        extendedHold = false;
      }

      selectInputCooldown -= elapsed;

      if (selectInputCooldown <= 0) {
        if (!extendedHold) {
          selectInputCooldown = maxSelectInputCooldown;
          extendedHold = true;
        }
        //Adjust the math to use std::max so that the current card index is always set to 0 at least.
        view->currCardIndex = std::max(view->currCardIndex - view->maxCardsOnScreen, 0);

        if (view->currCardIndex < view->numOfCards) {
          Audio().Play(AudioType::CHIP_SELECT);
          cardRevealTimer.reset();
        }
        //Set last card to either the current last card minus the amount of cards on screen, or the first card in the pool.
        view->firstCardOnScreen = std::max(view->firstCardOnScreen - view->maxCardsOnScreen, 0);
      }
    }
    else if (Input().Has(InputEvents::pressed_shoulder_right) || Input().Has(InputEvents::held_shoulder_right)) {
      if (lastKey != InputEvents::pressed_shoulder_right) {
        lastKey = InputEvents::pressed_shoulder_right;
        extendedHold = false;
      }

      selectInputCooldown -= elapsed;

      if (selectInputCooldown <= 0) {
        if (!extendedHold) {
          selectInputCooldown = maxSelectInputCooldown;
          extendedHold = true;
        }

        Audio().Play(AudioType::CHIP_SELECT);

        //Adjust the math to use std::min so that the current card index is always set to numOfCards-1 at most.
        view->currCardIndex = std::min(view->numOfCards - 1, view->currCardIndex + view->maxCardsOnScreen);

        //Set the last card on screen to be one page down or the true final card in the pack.
        view->firstCardOnScreen = std::min(view->firstCardOnScreen + view->maxCardsOnScreen, view->numOfCards - view->maxCardsOnScreen);

        cardRevealTimer.reset();
      }
    }
    else {
      selectInputCooldown = 0;
      extendedHold = false;
    }

    if (Input().Has(InputEvents::pressed_confirm)) {
      if (currViewMode == ViewMode::folder) {
        if (folderView.swapCardIndex != -1) {
          if (folderView.swapCardIndex == folderView.currCardIndex) {
            Battle::Card copy;
            bool found = false;
            for (size_t i = 0; i < poolCardBuckets.size(); i++) {
              if (poolCardBuckets[i].ViewCard() == folderCardSlots[folderView.currCardIndex].ViewCard()) {
                poolCardBuckets[i].AddCard();
                folderCardSlots[folderView.currCardIndex].GetCard(copy);
                found = true;
                break;
              };
            }
            if (found == false) {
              folderCardSlots[folderView.currCardIndex].GetCard(copy);
              if (copy.GetShortName().length() > 0) {
                auto slot = PoolBucket(1, copy);
                poolCardBuckets.push_back(slot);
                packView.numOfCards++;
              }
            }
            // Unselect the card
            folderView.swapCardIndex = -1;
            Audio().Play(AudioType::CHIP_CANCEL);
          }
          else {
            // swap the card
            FolderEditScene::FolderSlot temp = folderCardSlots[folderView.swapCardIndex];
            folderCardSlots[folderView.swapCardIndex] = folderCardSlots[folderView.currCardIndex];
            folderCardSlots[folderView.currCardIndex] = temp;
            Audio().Play(AudioType::CHIP_CONFIRM);

            folderView.swapCardIndex = -1;
          }
        }
        else {
          // See if we're swapping from a pack
          if (packView.swapCardIndex != -1) {
            // Try to swap the card with the one from the pack
            // The card from the pack is copied and added to the slot
            // The slot card needs to find its corresponding bucket and increment it
            Battle::Card copy;
            Battle::Card copy2 = poolCardBuckets[packView.swapCardIndex].ViewCard();
            Battle::Card checkCard;
            int maxCards = copy2.GetLimit();
            int foundCards = 0;
            for (size_t i = 0; i < folderCardSlots.size(); i++) {
              checkCard = Battle::Card(folderCardSlots[i].ViewCard());
              if (checkCard.GetShortName() == copy2.GetShortName() && checkCard.GetClass() == copy2.GetClass() && checkCard.GetElement() == copy2.GetElement()) {
                foundCards++;
              }
            }
            bool gotCard = false;
            if (foundCards < maxCards || maxCards == 0) {

              // If the pack pointed to is the same as the card in our folder, add the card back into folder
              if (poolCardBuckets[packView.swapCardIndex].ViewCard() == folderCardSlots[folderView.currCardIndex].ViewCard()) {
                poolCardBuckets[packView.swapCardIndex].AddCard();
                folderCardSlots[folderView.currCardIndex].GetCard(copy);

                gotCard = true;
              }
              else if (poolCardBuckets[packView.swapCardIndex].GetCard(copy)) {
                Battle::Card prev;

                bool findBucket = folderCardSlots[folderView.currCardIndex].GetCard(prev);

                folderCardSlots[folderView.currCardIndex].AddCard(copy);

                // If the card slot had a card, find the corresponding bucket to add it back into
                if (findBucket) {
                  auto iter = std::find_if(poolCardBuckets.begin(), poolCardBuckets.end(),
                    [&prev](const PoolBucket& in) { return prev.GetShortName() == in.ViewCard().GetShortName(); }
                  );

                  if (iter != poolCardBuckets.end()) {
                    iter->AddCard();
                  }
                }

                gotCard = true;
              }
            }
            if (gotCard) {
              hasFolderChanged = true;

              packView.swapCardIndex = -1;
              folderView.swapCardIndex = -1;

              Audio().Play(AudioType::CHIP_CONFIRM);
            }
            else {
              Audio().Play(AudioType::CHIP_ERROR);
            }
          }
          else {
            // select the card
            folderView.swapCardIndex = folderView.currCardIndex;
            Audio().Play(AudioType::CHIP_CHOOSE);
          }
        }
      }
      else if (currViewMode == ViewMode::pool) {
        if (packView.swapCardIndex != -1) {
          if (packView.swapCardIndex == packView.currCardIndex) {
            if (folder.GetSize() <= 30) {
              int found = -1;
              for (size_t i = 0; i < 30; i++) {
                if (found == -1 && folderCardSlots[i].IsEmpty()) {
                  found = i;
                }
              }
              Battle::Card copy;
              if (found != -1 && poolCardBuckets[packView.currCardIndex].GetCard(copy)) {
                Battle::Card checkCard;
                int maxCards = copy.GetLimit();
                int foundCards = 0;
                for (size_t i = 0; i < folderCardSlots.size(); i++) {
                  checkCard = Battle::Card(folderCardSlots[i].ViewCard());
                  if (checkCard.GetShortName() == copy.GetShortName() && checkCard.GetClass() == copy.GetClass() && checkCard.GetElement() == copy.GetElement()) {
                    foundCards++;
                  }
                }
                if (foundCards < maxCards || maxCards == 0) {
                  folderCardSlots[found].AddCard(copy);
                  packView.swapCardIndex = -1;
                  folderView.swapCardIndex = -1;
                  hasFolderChanged = true;
                  Audio().Play(AudioType::CHIP_CONFIRM);
                }
                else {
                  if (folderView.swapCardIndex == -1) {
                    Audio().Play(AudioType::CHIP_CANCEL);
                  }
                  else {
                    Audio().Play(AudioType::CHIP_ERROR);
                  }

                  hasFolderChanged = true;
                  poolCardBuckets[packView.currCardIndex].AddCard();
                  packView.swapCardIndex = -1;
                  folderView.swapCardIndex = -1;
                }
              }
            }
            else {
              // Unselect the card
              packView.swapCardIndex = -1;
              Audio().Play(AudioType::CHIP_CANCEL);
            }
          }
          else {
            // swap the pack
            FolderEditScene::PoolBucket temp = poolCardBuckets[packView.swapCardIndex];
            poolCardBuckets[packView.swapCardIndex] = poolCardBuckets[packView.currCardIndex];
            poolCardBuckets[packView.currCardIndex] = temp;
            Audio().Play(AudioType::CHIP_CONFIRM);
            packView.swapCardIndex = -1;
          }
        }
        else {
          // See if we're swapping from our folder
          bool swappingBetween = folderView.swapCardIndex != -1 && packView.currCardIndex != -1;
          swappingBetween = swappingBetween || (folderView.currCardIndex != -1 && packView.swapCardIndex != -1);

          if (swappingBetween && packView.numOfCards > 0) {
            // Try to swap the card with the one from the folder
            // The card from the pack is copied and added to the slot
            // The slot card needs to find its corresponding bucket and increment it
            Battle::Card copy;

            bool gotCard = false;

            // If the pack pointed to is the same as the card in our folder, add the card back into pool
            if (poolCardBuckets[packView.currCardIndex].ViewCard() == folderCardSlots[folderView.swapCardIndex].ViewCard()) {
              poolCardBuckets[packView.currCardIndex].AddCard();
              folderCardSlots[folderView.swapCardIndex].GetCard(copy);

              gotCard = true;
            }
            else if (packView.swapCardIndex > -1 && poolCardBuckets[packView.swapCardIndex].GetCard(copy)) {
              Battle::Card prev;

              bool findBucket = folderCardSlots[folderView.currCardIndex].GetCard(prev);

              folderCardSlots[folderView.currCardIndex].AddCard(copy);

              // If the card slot had a card, find the corresponding bucket to add it back into
              if (findBucket) {
                auto iter = std::find_if(poolCardBuckets.begin(), poolCardBuckets.end(),
                  [&prev](const PoolBucket& in) { return prev.GetShortName() == in.ViewCard().GetShortName(); }
                );

                if (iter != poolCardBuckets.end()) {
                  iter->AddCard();
                }
              }
              gotCard = true;
            }
            else if (folderView.swapCardIndex > -1 && poolCardBuckets[packView.currCardIndex].GetCard(copy)) {
              Battle::Card prev;

              bool findBucket = folderCardSlots[folderView.swapCardIndex].GetCard(prev);

              Battle::Card checkCard;
              int maxCards = copy.GetLimit();
              int foundCards = 0;
              for (size_t i = 0; i < folderCardSlots.size(); i++) {
                checkCard = Battle::Card(folderCardSlots[i].ViewCard());
                if (checkCard.GetShortName() == copy.GetShortName() && checkCard.GetClass() == copy.GetClass() && checkCard.GetElement() == copy.GetElement()) {
                  foundCards++;
                }
              }
              if (foundCards < maxCards || maxCards == 0) {
                folderCardSlots[folderView.swapCardIndex].AddCard(copy);
                packView.swapCardIndex = -1;
                folderView.swapCardIndex = -1;
                hasFolderChanged = true;
                Audio().Play(AudioType::CHIP_CONFIRM);
              }
              else {
                poolCardBuckets[packView.currCardIndex].AddCard();
                hasFolderChanged = true;
                packView.swapCardIndex = -1;
                folderView.swapCardIndex = -1;
                Audio().Play(AudioType::CHIP_ERROR);
              }
            }
          }
          else {
            // select the card
            packView.swapCardIndex = packView.currCardIndex;
            Audio().Play(AudioType::CHIP_CHOOSE);
          }
        }
      }
    }
    else if (Input().Has(InputEvents::pressed_ui_right) && currViewMode == ViewMode::folder) {
      currViewMode = ViewMode::pool;
      canInteract = false;
      Audio().Play(AudioType::CHIP_DESC);
    }
    else if (Input().Has(InputEvents::pressed_ui_left) && currViewMode == ViewMode::pool) {
      currViewMode = ViewMode::folder;
      canInteract = false;
      Audio().Play(AudioType::CHIP_DESC);
    }

    view->currCardIndex = std::max(0, view->currCardIndex);
    view->currCardIndex = std::min(view->numOfCards - 1, view->currCardIndex);

    switch (currViewMode) {
    case ViewMode::folder:
    {
      using SlotType = decltype(folderCardSlots)::value_type;
      RefreshCurrentCardDock<SlotType>(*view, folderCardSlots);
    }
    break;
    case ViewMode::pool:
    {
      using SlotType = decltype(poolCardBuckets)::value_type;
      RefreshCurrentCardDock<SlotType>(*view, poolCardBuckets);
    }
    break;
    default:
      Logger::Logf(LogLevel::critical, "No applicable view mode for folder edit scene: %i", static_cast<int>(currViewMode));
      break;
    }

    view->prevIndex = view->currCardIndex;

    view->firstCardOnScreen = std::max(0, view->firstCardOnScreen);
    view->firstCardOnScreen = std::min(view->numOfCards - 1, view->firstCardOnScreen);

    bool gotoLastScene = false;

    if (Input().Has(InputEvents::pressed_cancel) && canInteract) {
      if (packView.swapCardIndex != -1 || folderView.swapCardIndex != -1) {
        Audio().Play(AudioType::CHIP_DESC_CLOSE);
        packView.swapCardIndex = folderView.swapCardIndex = -1;
      }
      else if (currViewMode == ViewMode::pool) {
        currViewMode = ViewMode::folder;
        canInteract = false;
        Audio().Play(AudioType::CHIP_DESC);
      }
      else {
        WriteNewFolderData();

        gotoLastScene = true;
        canInteract = false;

        Audio().Play(AudioType::CHIP_DESC_CLOSE);
      }
    }

    if (gotoLastScene) {
      canInteract = false;

      using namespace swoosh::types;
      using segue = segue<BlackWashFade, milli<500>>;
      getController().pop<segue>();
    }
  } // end if(gotoLastScene)
  else {
    if (prevViewMode != currViewMode) {
      if (currViewMode == ViewMode::folder) {
        if (camera.GetView().getCenter().x > 240) {
          camera.OffsetCamera(sf::Vector2f(-960.0f * 2.0f * float(elapsed), 0));
        }
        else {
          prevViewMode = currViewMode;
          canInteract = true;
          folderSort.setPosition(cardHolder.getPosition() + sf::Vector2f(11 * 2, 5 * 2));
        }
      }
      else if (currViewMode == ViewMode::pool) {
        if (camera.GetView().getCenter().x < 720) {
          camera.OffsetCamera(sf::Vector2f(960.0f * 2.0f * float(elapsed), 0));
        }
        else {
          prevViewMode = currViewMode;
          canInteract = true;
          folderSort.setPosition(packCardHolder.getPosition() + sf::Vector2f(11 * 2, 5 * 2));
        }
      }
      else {
        prevViewMode = currViewMode;
        canInteract = true;
      }
    }
  }
}

void FolderEditScene::onLeave() {

}

void FolderEditScene::onExit() {
}

void FolderEditScene::onEnter() {
  folderView.currCardIndex = 0;
  RefreshCurrentCardDock(folderView, folderCardSlots);
}

void FolderEditScene::onResume() {

}

void FolderEditScene::onDraw(sf::RenderTexture& surface) {
  surface.draw(bg);
  surface.draw(menuLabel);

  float scale = 0.0f;

  surface.draw(folderCardCountBox);

  if (int(0.5 + folderCardCountBox.getScale().y) == 2) {
    std::vector<FolderEditScene::FolderSlot> nonempty = (decltype(folderCardSlots))(folderCardSlots.size());
    auto iter = std::copy_if(folderCardSlots.begin(), folderCardSlots.end(), nonempty.begin(), [](const auto& in) { return !in.IsEmpty(); });
    nonempty.resize(std::distance(nonempty.begin(), iter));  // shrink container to new size

    std::string str = std::to_string(nonempty.size());

    // Draw number of cards in this folder
    numberLabel.SetString(str);
    numberLabel.setOrigin(cardLabel.GetLocalBounds().width, 0);
    if (str.length() > 1) {
      numberLabel.setPosition(400.f, 14.f);
    }
    else {
      numberLabel.setPosition(414.f, 14.f);
    }
    if (nonempty.size() == 30) {
      numberLabel.SetFont(Font::Style::gradient_green);
    }
    else {
      numberLabel.SetFont(Font::Style::gradient);
    }

    surface.draw(numberLabel);

    numberLabel.SetFont(numberFont);
    numberLabel.SetString("/");
    numberLabel.setOrigin(0, 0);
    numberLabel.SetColor(sf::Color(200, 224, 248, 255));
    numberLabel.setPosition(418.f, 14.f);
    surface.draw(numberLabel);
    numberLabel.SetColor(sf::Color::White);
    if (nonempty.size() == 30) {
      numberLabel.SetFont(Font::Style::gradient_green);
    }
    else {
      numberLabel.SetFont(Font::Style::gradient);
    }

    // Draw max
    numberLabel.SetString(std::string("  30")); // will print "# / 30"
    numberLabel.setOrigin(0, 0);
    numberLabel.setPosition(414.f, 14.f);

    surface.draw(numberLabel);
  }

  // folder card count opens on FOLDER view mode only
  if (currViewMode == ViewMode::folder) {
    if (prevViewMode == currViewMode) { // camera pan finished
      scale = swoosh::ease::interpolate((float)frameElapsed * 8.f, 2.0f, folderCardCountBox.getScale().y);
    }
  }
  else {
    scale = swoosh::ease::interpolate((float)frameElapsed * 8.f, 0.0f, folderCardCountBox.getScale().y);
  }

  folderCardCountBox.setScale(2.0f, scale);

  DrawFolder(surface);
  DrawPool(surface);

  if (isInSortMenu) {
    surface.draw(folderSort);
    surface.draw(sortCursor);
  }
}

void FolderEditScene::DrawFolder(sf::RenderTarget& surface) {
  cardDesc.setPosition(sf::Vector2f(26.f, 175.0f));
  scrollbar.setPosition(410.f, 60.f);
  element.setPosition(2.f * 28.f, 136.f);
  card.setPosition(96.f, 93.f);

  surface.draw(folderDock);
  surface.draw(cardHolder);

  // ScrollBar limits: Top to bottom screen position when selecting first and last card respectively
  float top = 60.0f; float bottom = 260.0f;
  float depth = (bottom - top) * (((float)folderView.firstCardOnScreen) / ((float)folderView.numOfCards - 7));
  scrollbar.setPosition(452.f, top + depth);

  surface.draw(scrollbar);

  // Move the card library iterator to the current highlighted card
  auto iter = folderCardSlots.begin();

  for (int j = 0; j < folderView.firstCardOnScreen; j++) {
    iter++;

    if (iter == folderCardSlots.end()) return;
  }

  // Now that we are at the viewing range, draw each card in the list
  for (int i = 0; i < folderView.maxCardsOnScreen && folderView.firstCardOnScreen + i < folderView.numOfCards; i++) {
    if (!iter->IsEmpty()) {
      const Battle::Card& copy = iter->ViewCard();
      bool hasID = getController().CardPackagePartitioner().GetPartition(Game::LocalPartition).HasPackage(copy.GetUUID());

      cardLabel.SetColor(sf::Color::White);

      if (!hasID) {
        cardLabel.SetColor(sf::Color::Red);
      }

      float cardIconY = 66.0f + (32.f * i);
      cardIcon.setTexture(*GetIconForCard(copy.GetUUID()));
      cardIcon.setPosition(2.f * 104.f, cardIconY);
      surface.draw(cardIcon);

      cardLabel.setPosition(2.f * 120.f, cardIconY + 4.0f);
      cardLabel.SetString(copy.GetShortName());
      cardLabel.setScale(2.f, 2.f);
      surface.draw(cardLabel);

      int offset = (int)(copy.GetElement());
      element.setTextureRect(sf::IntRect(14 * offset, 0, 14, 14));
      element.setPosition(2.f * 183.f, cardIconY);
      element.setScale(2.f, 2.f);
      surface.draw(element);

      cardLabel.setOrigin(0, 0);
      cardLabel.setPosition(2.f * 200.f, cardIconY + 4.0f);
      cardLabel.SetString(std::string() + copy.GetCode());
      surface.draw(cardLabel);
      //Draw Card Limit
      if (copy.GetLimit() > 0) {
        int limit = copy.GetLimit();
        limitLabel.SetColor(sf::Color::White);
        limitLabel.SetString(to_string(limit));
        limitLabel2.SetString("\nMAX");
        limitLabel2.SetColor(sf::Color::Yellow);
      }
      else {
        limitLabel.SetColor(sf::Color::Yellow);
        limitLabel.SetString("NO\nMAX");
      }
      limitLabel.setPosition(2.f * 210.f, cardIconY + 2.f);
      surface.draw(limitLabel);
      limitLabel2.setPosition(2.f * 210.f, cardIconY + 2.f);
      surface.draw(limitLabel2);
    }
    // Draw card at the cursor
    if (folderView.firstCardOnScreen + i == folderView.currCardIndex) {
      float y = swoosh::ease::interpolate((float)frameElapsed * 7.f, folderCursor.getPosition().y, 64.0f + (32.f * i));
      float bounce = std::sin((float)totalTimeElapsed * 10.0f) * 5.0f;
      float scaleFactor = (float)swoosh::ease::linear(cardRevealTimer.getElapsed().asSeconds() + 0.01f, 0.25f, 1.0f); // +0.01 to start partially open 
      float xscale = scaleFactor * 2.f;

      auto interp_position = [scaleFactor, this](sf::Vector2f pos) {
        sf::Vector2f center = card.getPosition();
        pos.x = ((scaleFactor * pos) + ((1.0f - scaleFactor) * center)).x;
        return pos;
      };

      folderCursor.setPosition((2.f * 90.f) + bounce, y);

      if (!isInSortMenu) {
        surface.draw(folderCursor);
      }

      if (!iter->IsEmpty()) {
        const Battle::Card& copy = iter->ViewCard();
        card.setTexture(*GetPreviewForCard(copy.GetUUID()));
        card.setScale(xscale, 2.0f);
        surface.draw(card);

        // This draws the currently highlighted card
        if (copy.GetDamage() > 0) {
          cardLabel.SetColor(sf::Color::White);
          cardLabel.SetString(std::to_string(copy.GetDamage()));
          cardLabel.setOrigin(cardLabel.GetLocalBounds().width + cardLabel.GetLocalBounds().left, 0);
          cardLabel.setScale(xscale, 2.f);
          cardLabel.setPosition(interp_position(sf::Vector2f{ 2.f * 77.f, 145.f }));
          surface.draw(cardLabel);
        }

        cardLabel.setOrigin(0, 0);
        cardLabel.SetColor(sf::Color::Yellow);
        cardLabel.setPosition(interp_position(sf::Vector2f{ 2.f * 20.f, 145.f }));
        cardLabel.SetString(std::string() + copy.GetCode());
        cardLabel.setScale(xscale, 2.f);
        surface.draw(cardLabel);

        std::string formatted = stx::format_to_fit(copy.GetDescription(), 9, 3);
        cardDesc.SetString(formatted);
        surface.draw(cardDesc);

        int offset = (int)(copy.GetElement());
        element.setTextureRect(sf::IntRect(14 * offset, 0, 14, 14));
        element.setPosition(interp_position(sf::Vector2f{ 2.f * 32.f, 142.f }));
        element.setScale(xscale, 2.f);
        surface.draw(element);
      }
    }
    if (folderView.firstCardOnScreen + i == folderView.swapCardIndex && (int(totalTimeElapsed * 1000) % 2 == 0)) {
      float y = 64.0f + (32.f * i);

      folderSwapCursor.setPosition((2.f * 95.f) + 2.0f, y);
      folderSwapCursor.setColor(sf::Color(255, 255, 255, 200));
      surface.draw(folderSwapCursor);
      folderSwapCursor.setColor(sf::Color::White);
    }

    iter++;
    if (iter == folderCardSlots.end()) return;
  }
}

void FolderEditScene::DrawPool(sf::RenderTarget& surface) {
  cardDesc.setPosition(sf::Vector2f(320.f + 480.f, 175.0f));
  element.setPosition(400.f + 2.f * 20.f + 480.f, 146.f);
  card.setPosition(389.f + 480.f, 93.f);

  surface.draw(packDock);
  surface.draw(packCardHolder);

  if (packView.numOfCards == 0) return;

  // Per BN6, don't draw the scrollbar itself if you can't scroll in the pack.
  if (packView.numOfCards > 7) {
    // ScrollBar limits: Top to bottom screen position when selecting first and last card respectively
    float top = 60.0f; float bottom = 260.0f;
    float depth = (bottom - top) * (((float)packView.firstCardOnScreen) / ((float)packView.numOfCards - 7));
    scrollbar.setPosition(292.f + 480.f, top + depth);
    surface.draw(scrollbar);
  }

  // Move the card library iterator to the current highlighted card
  auto iter = poolCardBuckets.begin();

  for (int j = 0; j < packView.firstCardOnScreen; j++) {
    iter++;
  }

  // Now that we are at the viewing range, draw each card in the list
  for (int i = 0; i < packView.maxCardsOnScreen && packView.firstCardOnScreen + i < packView.numOfCards; i++) {
    int count = iter->GetCount();
    const Battle::Card& copy = iter->ViewCard();

    cardIcon.setTexture(*GetIconForCard(copy.GetUUID()));
    cardIcon.setPosition(16.f + 480.f, 65.0f + (32.f * i));
    cardIcon.setScale(2.f, 2.f);
    surface.draw(cardIcon);

    cardLabel.SetColor(sf::Color::White);
    cardLabel.setPosition(49.f + 480.f, 69.0f + (32.f * i));
    cardLabel.SetString(copy.GetShortName());
    cardLabel.setScale(2.f, 2.f);
    surface.draw(cardLabel);

    int offset = (int)(copy.GetElement());
    element.setTextureRect(sf::IntRect(14 * offset, 0, 14, 14));
    element.setPosition(182.0f + 480.f, 65.0f + (32.f * i));
    element.setScale(2.f, 2.f);
    surface.draw(element);

    cardLabel.setOrigin(0, 0);
    cardLabel.setPosition(216.f + 480.f, 69.0f + (32.f * i));
    cardLabel.SetString(std::string() + copy.GetCode());
    surface.draw(cardLabel);
    //Draw Card Limit
    if (copy.GetLimit() > 0) {
      int limit = copy.GetLimit();
      limitLabel.SetColor(sf::Color::White);
      limitLabel.SetString(to_string(limit));
      limitLabel2.SetString("\nMAX");
      limitLabel2.SetColor(sf::Color::Yellow);
    }
    else {
      limitLabel.SetColor(sf::Color::Yellow);
      limitLabel.SetString("NO\nMAX");
    }
    limitLabel.setPosition(236.f + 480.f, 67.0f + (32.f * i));
    surface.draw(limitLabel);
    limitLabel2.setPosition(236.f + 480.f, 67.0f + (32.f * i));
    surface.draw(limitLabel2);

    // Draw count in pack
    cardLabel.setOrigin(0, 0);
    cardLabel.setPosition(274.f + 480.f, 69.0f + (32.f * i));
    cardLabel.SetString(std::to_string(count));
    surface.draw(cardLabel);

    // This draws the currently highlighted card
    if (packView.firstCardOnScreen + i == packView.currCardIndex) {
      float y = swoosh::ease::interpolate((float)frameElapsed * 7.f, packCursor.getPosition().y, 64.0f + (32.f * i));
      float bounce = std::sin((float)totalTimeElapsed * 10.0f) * 2.0f;
      float scaleFactor = (float)swoosh::ease::linear(cardRevealTimer.getElapsed().asSeconds() + 0.01f, 0.25f, 1.0f); // + 0.01 to start partially open 
      float xscale = scaleFactor * 2.f;

      auto interp_position = [scaleFactor, this](sf::Vector2f pos) {
        sf::Vector2f center = card.getPosition();
        pos.x = ((scaleFactor * pos) + ((1.0f - scaleFactor) * center)).x;
        return pos;
      };

      // draw the cursor where the entry is located and bounce
      packCursor.setPosition(bounce + 480.f + 2.f, y);

      if (!isInSortMenu) {
        surface.draw(packCursor);
      }

      card.setTexture(*GetPreviewForCard(poolCardBuckets[packView.currCardIndex].ViewCard().GetUUID()));
      card.setTextureRect(sf::IntRect{ 0,0,56,48 });
      card.setScale(xscale, 2.0f);
      surface.draw(card);

      if (copy.GetDamage() > 0) {
        cardLabel.SetColor(sf::Color::White);
        cardLabel.SetString(std::to_string(copy.GetDamage()));
        cardLabel.setOrigin(cardLabel.GetLocalBounds().width + cardLabel.GetLocalBounds().left, 0);
        cardLabel.setPosition(interp_position(sf::Vector2f{ 2.f * (223.f) + 480.f, 145.f }));
        cardLabel.setScale(xscale, 2.f);
        surface.draw(cardLabel);
      }

      cardLabel.setOrigin(0, 0);
      cardLabel.SetColor(sf::Color::Yellow);
      cardLabel.setPosition(interp_position(sf::Vector2f{ 2.f * 167.f + 480.f, 145.f }));
      cardLabel.SetString(std::string() + copy.GetCode());
      cardLabel.setScale(xscale, 2.f);
      surface.draw(cardLabel);

      int offset = (int)(copy.GetElement());
      element.setTextureRect(sf::IntRect(14 * offset, 0, 14, 14));
      element.setPosition(interp_position(sf::Vector2f{ 2.f * 179.f + 480.f, 142.f }));
      element.setScale(xscale, 2.f);
      surface.draw(element);

      std::string formatted = stx::format_to_fit(copy.GetDescription(), 9, 3);
      cardDesc.SetString(formatted);
      surface.draw(cardDesc);
    }

    if (packView.firstCardOnScreen + i == packView.swapCardIndex && (int(totalTimeElapsed * 1000) % 2 == 0)) {
      float y = 64.0f + (32.f * i);

      packSwapCursor.setPosition(485.f + 2.f + 2.f, y);
      packSwapCursor.setColor(sf::Color(255, 255, 255, 200));
      surface.draw(packSwapCursor);
      packSwapCursor.setColor(sf::Color::White);
    }

    iter++;
  }
}

void FolderEditScene::ComposeSortOptions() {
  auto sortByID = [](const ICardView& first, const ICardView& second) -> bool {
    return first.ViewCard().GetUUID() < second.ViewCard().GetUUID();
  };

  auto sortByAlpha = [](const ICardView& first, const ICardView& second) -> bool {
    return first.ViewCard().GetShortName() < second.ViewCard().GetShortName();
  };

  auto sortByCode = [](const ICardView& first, const ICardView& second) -> bool {
    return first.ViewCard().GetCode() < second.ViewCard().GetCode();
  };

  auto sortByAttack = [](const ICardView& first, const ICardView& second) -> bool {
    return first.ViewCard().GetDamage() < second.ViewCard().GetDamage();
  };

  auto sortByElement = [](const ICardView& first, const ICardView& second) -> bool {
    return first.ViewCard().GetElement() < second.ViewCard().GetElement();
  };

  auto sortByFolderCopies = [this](const ICardView& first, const ICardView& second) -> bool {
    size_t firstCount{}, secondCount{};

    firstCount = std::count_if(folderCardSlots.cbegin(), folderCardSlots.cend(), [&first](auto& entry) {
      return entry.ViewCard().GetUUID() == first.ViewCard().GetUUID();
    });

    secondCount = std::count_if(folderCardSlots.cbegin(), folderCardSlots.cend(), [&second](auto& entry) {
      return entry.ViewCard().GetUUID() == second.ViewCard().GetUUID();
    });

    return firstCount < secondCount;
  };

  auto sortByPoolCopies = [this](const ICardView& first, const ICardView& second) -> bool {
    size_t firstCount{}, secondCount{};

    auto iter = std::find_if(poolCardBuckets.cbegin(), poolCardBuckets.cend(), [&first](auto& entry) {
      return entry.ViewCard().GetUUID() == first.ViewCard().GetUUID();
    });

    auto iter2 = std::find_if(poolCardBuckets.cbegin(), poolCardBuckets.cend(), [&second](auto& entry) {
      return entry.ViewCard().GetUUID() == second.ViewCard().GetUUID();
    });

    if (iter != poolCardBuckets.cend()) {
      firstCount = iter->GetCount();
    }

    if (iter2 != poolCardBuckets.cend()) {
      secondCount = iter2->GetCount();
    }

    return firstCount < secondCount;
  };

  auto sortByMax = [](const ICardView& first, const ICardView& second) -> bool {
    return first.ViewCard().GetLimit() < second.ViewCard().GetLimit();
  };

  folderSortOptions.AddOption(sortByID);
  folderSortOptions.AddOption(sortByAlpha);
  folderSortOptions.AddOption(sortByCode);
  folderSortOptions.AddOption(sortByAttack);
  folderSortOptions.AddOption(sortByElement);
  folderSortOptions.AddOption(sortByFolderCopies);
  folderSortOptions.AddOption(sortByMax);

  poolSortOptions.AddOption(sortByID);
  poolSortOptions.AddOption(sortByAlpha);
  poolSortOptions.AddOption(sortByCode);
  poolSortOptions.AddOption(sortByAttack);
  poolSortOptions.AddOption(sortByElement);
  poolSortOptions.AddOption(sortByPoolCopies);
  poolSortOptions.AddOption(sortByMax);
}

void FolderEditScene::onEnd() {
}

void FolderEditScene::ExcludeFolderDataFromPool() {
  Battle::Card mock; // will not be used
  for (auto& f : folderCardSlots) {
    auto iter = std::find_if(poolCardBuckets.begin(), poolCardBuckets.end(), [&f](PoolBucket& pack) { return pack.ViewCard() == f.ViewCard(); });
    if (iter != poolCardBuckets.end()) {
      iter->GetCard(mock);
    }
  }
}

void FolderEditScene::PlaceFolderDataIntoCardSlots() {
  CardFolder::Iter iter = folder.Begin();

  while (iter != folder.End() && folderCardSlots.size() < 30) {
    auto slot = FolderSlot();
    slot.AddCard(Battle::Card(**iter));
    folderCardSlots.push_back(slot);
    iter++;
  }

  while (folderCardSlots.size() < 30) {
    folderCardSlots.push_back({});
  }
}

void FolderEditScene::PlaceLibraryDataIntoBuckets() {
  auto& packageManager = getController().CardPackagePartitioner().GetPartition(Game::LocalPartition);
  std::string packageId = packageManager.FirstValidPackage();

  if (packageId.empty()) return;

  do {
    auto& meta = packageManager.FindPackageByID(packageId);

    for (auto& code : meta.GetCodes()) {
      Battle::Card::Properties props = meta.GetCardProperties();
      props.code = code;
      auto bucket = PoolBucket(5, Battle::Card(props));
      poolCardBuckets.push_back(bucket);
    }

    packageId = packageManager.GetPackageAfter(packageId);

  } while (packageId != packageManager.FirstValidPackage());
}

void FolderEditScene::WriteNewFolderData() {
  folder = CardFolder();

  for (auto iter = folderCardSlots.begin(); iter != folderCardSlots.end(); iter++) {
    if ((*iter).IsEmpty()) continue;

    folder.AddCard((*iter).ViewCard());
  }
}

std::shared_ptr<sf::Texture> FolderEditScene::GetIconForCard(const std::string& uuid) {
  auto& packageManager = getController().CardPackagePartitioner().GetPartition(Game::LocalPartition);

  if (!packageManager.HasPackage(uuid))
    return noIcon;

  auto& meta = packageManager.FindPackageByID(uuid);
  return meta.GetIconTexture();
}
std::shared_ptr<sf::Texture> FolderEditScene::GetPreviewForCard(const std::string& uuid) {
  auto& packageManager = getController().CardPackagePartitioner().GetPartition(Game::LocalPartition);

  if (!packageManager.HasPackage(uuid))
    return noPreview;

  auto& meta = packageManager.FindPackageByID(uuid);
  return meta.GetPreviewTexture();
}

#ifdef __ANDROID__
void FolderEditScene::StartupTouchControls() {
  /* Android touch areas*/
  TouchArea& rightSide = TouchArea::create(sf::IntRect(240, 0, 240, 320));

  rightSide.enableExtendedRelease(true);

  rightSide.onTouch([]() {
    INPUTx.VirtualKeyEvent(InputEvent::RELEASED_A);
  });

  rightSide.onRelease([this](sf::Vector2i delta) {
    if (!releasedB) {
      INPUTx.VirtualKeyEvent(InputEvent::PRESSED_A);
    }
  });

  rightSide.onDrag([this](sf::Vector2i delta) {
    if (delta.x < -25 && !releasedB) {
      INPUTx.VirtualKeyEvent(InputEvent::PRESSED_B);
      INPUTx.VirtualKeyEvent(InputEvent::RELEASED_B);
      releasedB = true;
    }
  });
}

void FolderEditScene::ShutdownTouchControls() {
  TouchArea::free();
}
#endif
