#include "bnCardSelectBattleState.h"

#include "../bnBattleSceneBase.h"
#include "../../bnCamera.h"
#include "../../bnCard.h"
#include "../../bnPlayer.h"
#include "../../bnText.h"
#include "../../bnPlayerSelectedCardsUI.h"
#include "../../bnInputManager.h"

// modals like card cust and battle reward slide in 12px per frame for 10 frames. 60 frames = 1 sec
// modal slide moves 120px in 1/6th of a second
// Per 1 second that is 6*120px in 6*1/6 of a sec = 720px in 1 sec
#define MODAL_SLIDE_PX_PER_SEC 720.0f

CardSelectBattleState::CardSelectBattleState(std::vector<Player*>& tracked, std::vector<std::shared_ptr<TrackedFormData>>& forms) : 
  tracked(tracked), 
  forms(forms),
  font(Font::Style::thick)
{
  // Selection input delays
  heldCardSelectInputCooldown = 0.35f; // 21 frames @ 60fps = 0.35 second
  maxCardSelectInputCooldown = 1 / 12.f; // 5 frames @ 60fps = 1/12th second
  cardSelectInputCooldown = maxCardSelectInputCooldown;

  // Load assets
  mobBackdropSprite = sf::Sprite(*LOAD_TEXTURE(MOB_NAME_BACKDROP));
  mobEdgeSprite = sf::Sprite(*LOAD_TEXTURE(MOB_NAME_EDGE));

  mobBackdropSprite.setScale(2.f, 2.f);
  mobEdgeSprite.setScale(2.f, 2.f);
}

void CardSelectBattleState::CheckFormChanges()
{
  CardSelectionCust& cardCust = GetScene().GetCardSelectWidget();

  // Check the form transition values
  if (formSelected) {
    int newFormIndex = cardCust.GetSelectedFormIndex();
    // Check what form we switched to
    if (currForm == newFormIndex) {
      // no change
      formSelected = false;
      forms[0]->animationComplete = true;
    }
    else {
      // else, update our record and proceed to the animate state
      currForm = newFormIndex;
      forms[0]->animationComplete = false;
      forms[0]->selectedForm = currForm;
    }
  }
}

Battle::Card**& CardSelectBattleState::GetCardPtrList()
{
  return cards;
}

int& CardSelectBattleState::GetCardListLengthAddr()
{
  return cardCount;
}

void CardSelectBattleState::onStart(const BattleSceneState*)
{
  CardSelectionCust& cardCust = GetScene().GetCardSelectWidget();

  Audio().Play(AudioType::CUSTOM_SCREEN_OPEN);

  // Load the next cards
  cardCust.ResetState();
  cardCust.GetNextCards();

  // Reset state flags
  currState = state::slidein;
  formSelected = false;
  hasNewChips = false;
}

void CardSelectBattleState::onUpdate(double elapsed)
{
  CardSelectionCust& cardCust = GetScene().GetCardSelectWidget();

  if (!cardCust.IsInView() && currState == state::slidein) {
    cardCust.Move(sf::Vector2f(MODAL_SLIDE_PX_PER_SEC * (float)elapsed, 0));
    GetScene().GetCamera().MoveCamera(sf::Vector2f(240.f, 140.f), sf::seconds(0.1f));
    return;
  }

  if (!cardCust.IsOutOfView() && currState == state::slideout) {
    cardCust.Move(sf::Vector2f(-MODAL_SLIDE_PX_PER_SEC * (float)elapsed, 0));
    GetScene().GetCamera().MoveCamera(sf::Vector2f(240.f, 160.f), sf::seconds(0.1f));
    return;
  }

  if (cardCust.IsInView()) {
    if (cardCust.RequestedRetreat()) {
      currState = state::slideout;
    }
    else {
      currState = state::select;
    }

    if (Input().Has(InputEvents::pressed_shoulder_right) && !cardCust.HasQuestion()) {
      if (cardCust.IsVisible()) {
        cardCust.Hide();
      }
      else {
        cardCust.Reveal();
      }
    }
  }

  if (cardCust.CanInteract() && currState == state::select) {
    if (cardCust.IsTextBoxOpen()) {
      if (!Input().Has(InputEvents::held_option) && !cardCust.HasQuestion()) {
        if (cardCust.CloseTextBox() && cardCust.RequestedRetreat()) {
          Audio().Play(AudioType::CHIP_DESC_CLOSE, AudioPriority::lowest);
        }
      }
      else if (Input().Has(InputEvents::pressed_confirm)) {

        cardCust.TextBoxConfirmQuestion() ? Audio().Play(AudioType::CHIP_CHOOSE) : 1;
        cardCust.ContinueTextBox();
      }

      if (Input().Has(InputEvents::pressed_ui_left)) {
        cardCust.TextBoxSelectYes()? Audio().Play(AudioType::CHIP_SELECT) : 1;;
      }
      else if (Input().Has(InputEvents::pressed_ui_right)) {
        cardCust.TextBoxSelectNo()? Audio().Play(AudioType::CHIP_SELECT) : 1;;
      }
    } else if(cardCust.IsTextBoxClosed()) {
      // there's a wait time between moving ones and moving repeatedly (Sticky keys)
      bool moveCursor = cardSelectInputCooldown <= 0 || cardSelectInputCooldown == heldCardSelectInputCooldown;

      if (Input().Has(InputEvents::pressed_ui_left) || Input().Has(InputEvents::held_ui_left)) {
        cardSelectInputCooldown -= elapsed;

        if (moveCursor) {
          cardCust.CursorLeft() ? Audio().Play(AudioType::CHIP_SELECT) : 1;

          if (cardSelectInputCooldown <= 0) {
            cardSelectInputCooldown = maxCardSelectInputCooldown; // sticky key time
          }
        }
      }
      else if (Input().Has(InputEvents::pressed_ui_right) || Input().Has(InputEvents::held_ui_right)) {
        cardSelectInputCooldown -= elapsed;

        if (moveCursor) {
          cardCust.CursorRight() ? Audio().Play(AudioType::CHIP_SELECT) : 1;

          if (cardSelectInputCooldown <= 0) {
            cardSelectInputCooldown = maxCardSelectInputCooldown; // sticky key time
          }
        }
      }
      else if (Input().Has(InputEvents::pressed_ui_up) || Input().Has(InputEvents::held_ui_up)) {
        cardSelectInputCooldown -= elapsed;

        if (moveCursor) {
          cardCust.CursorUp() ? Audio().Play(AudioType::CHIP_SELECT) : 1;

          if (cardSelectInputCooldown <= 0) {
            cardSelectInputCooldown = maxCardSelectInputCooldown; // sticky key time
          }
        }
      }
      else if (Input().Has(InputEvents::pressed_ui_down) || Input().Has(InputEvents::held_ui_down)) {
        cardSelectInputCooldown -= elapsed;

        if (moveCursor) {
          cardCust.CursorDown() ? Audio().Play(AudioType::CHIP_SELECT) : 1;

          if (cardSelectInputCooldown <= 0) {
            cardSelectInputCooldown = maxCardSelectInputCooldown; // sticky key time
          }
        }
      }
      else {
        cardSelectInputCooldown = heldCardSelectInputCooldown;
      }

      if (Input().Has(InputEvents::pressed_confirm)) {
        bool performed = cardCust.CursorAction();

        if (cardCust.AreCardsReady()) {
          Audio().Play(AudioType::CHIP_CONFIRM, AudioPriority::high);

          // If the list is untouched, we do not re-assign the cards
          bool hasNewHand = cardCust.HasNewHand();
          auto newCards = cardCust.GetCards();

          Player* player = tracked[0];
          SelectedCardsUI* ui = player->GetFirstComponent<PlayerSelectedCardsUI>();

          if (ui && hasNewHand) {
            cards = newCards;
            cardCount = cardCust.GetCardCount();
            GetScene().FilterSupportCards(cards, cardCount);
            ui->LoadCards(cards, cardCount);
            ui->Hide();
            hasNewChips = true;
          }

          CheckFormChanges();
          currState = state::slideout;
        }
        else if (performed) {
          Audio().Play(AudioType::CHIP_CHOOSE, AudioPriority::highest);

          // Should probably have a cardCust.IsInFormSelect() to flag this but it works as it is...
          // This was really annoying to debug so here's a note:
          // There should be a query for the cardcust if we selected a form, but instead we just set this to true
          // and then call CheckFormChanges() to manage changing form selections or not.
          formSelected = true;
        }
        else {
          Audio().Play(AudioType::CHIP_ERROR, AudioPriority::lowest);
        }
      }
      else if (Input().Has(InputEvents::pressed_cancel) || sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
        cardCust.CursorCancel() ? Audio().Play(AudioType::CHIP_CANCEL, AudioPriority::highest) : 1;
      }
      else if (Input().Has(InputEvents::held_option)) {
        cardCust.OpenTextBox() ? Audio().Play(AudioType::CHIP_DESC, AudioPriority::lowest) : 1;
      }
      else if (Input().Has(InputEvents::pressed_shoulder_left)) {
        cardCust.PromptRetreat();
      }
      else if (Input().Has(InputEvents::held_pause)) {
        cardCust.CursorSelectOK() ? Audio().Play(AudioType::CHIP_CANCEL, AudioPriority::lowest) : 1;;
      }
    }
  }

  // Play the WHRR WHRR sound when dark cards are highlighted
  if (cardCust.IsDarkCardSelected()) {
    if (streamVolume == -1) {
      streamVolume = Audio().GetStreamVolume();
      Audio().SetStreamVolume(streamVolume / 2.f);
    }

    Audio().Play(AudioType::DARK_CARD, AudioPriority::high);
  }
  else {
    if (streamVolume != -1) {
      Audio().SetStreamVolume(streamVolume);
      streamVolume = -1.f;
    }
  }
}

void CardSelectBattleState::onDraw(sf::RenderTexture& surface)
{
  float nextLabelHeight = 6.0f; // start 3px from the top (6px/2 upscale = 3px)
  auto mobList = GetScene().MobList();

  for (int i = 0; i < mobList.size(); i++) {
    const Character& mob = mobList[i].get();
    if (mob.IsDeleted())
      continue;

    std::string name = mob.GetName();
    Text mobLabel{ name, font };

    mobLabel.setOrigin(mobLabel.GetLocalBounds().width, 0);
    mobLabel.setPosition(475.0f+2.f, nextLabelHeight+2.f); // shadow over and down 1px
    mobLabel.setScale(2.0f, 2.0f);
    mobLabel.SetColor(sf::Color::Black);

    float labelWidth = mobLabel.GetWorldBounds().width;
    float labelHeight = mobLabel.GetWorldBounds().height;

    mobEdgeSprite.setPosition(470.0f - (labelWidth + 10), nextLabelHeight-2.0);
    auto edgePos = mobEdgeSprite.getPosition();

    mobBackdropSprite.setPosition(edgePos.x + mobEdgeSprite.getGlobalBounds().width, edgePos.y);

    float scalex = GetScene().getController().getVirtualWindowSize().x - mobBackdropSprite.getPosition().x;
    mobBackdropSprite.setScale(scalex, 2.f);

    surface.draw(mobEdgeSprite);
    surface.draw(mobBackdropSprite);

    // draw the mob label shadow
    surface.draw(mobLabel);

    // draw the white text on top
    mobLabel.setOrigin(mobLabel.GetLocalBounds().width, 0);
    mobLabel.setPosition(475.0f, nextLabelHeight);
    mobLabel.SetColor(sf::Color::White);
    surface.draw(mobLabel);

    // make the next label relative to this one and 3px down + the 3px margin from the first label
    nextLabelHeight += mobEdgeSprite.getLocalBounds().height + (6.f*3.f);
  }

  surface.draw(GetScene().GetCardSelectWidget());
}

void CardSelectBattleState::onEnd(const BattleSceneState*)
{ }

void CardSelectBattleState::EnablePVPMode()
{
  pvpMode = true;
}

bool CardSelectBattleState::OKIsPressed() {
  // CardGUI goes out of view when OK is pressed
  // wait for that animation to end before triggering the next state
  return GetScene().GetCardSelectWidget().IsOutOfView();
}

bool CardSelectBattleState::HasForm()
{
  return OKIsPressed() && formSelected;
}

bool CardSelectBattleState::RequestedRetreat()
{
  return OKIsPressed() && GetScene().GetCardSelectWidget().RequestedRetreat();
}

const bool CardSelectBattleState::SelectedNewChips()
{
  return OKIsPressed() && hasNewChips;
}

void CardSelectBattleState::ResetSelectedForm()
{
  currForm = -1;
  GetScene().GetCardSelectWidget().ResetPlayerFormSelection();
}
