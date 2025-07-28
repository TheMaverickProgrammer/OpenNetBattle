#include "bnCardSelectionCust.h"
#include "bnResourceHandle.h"
#include "bnTextureResourceManager.h"
#include "bnShaderResourceManager.h"
#include "bnInputManager.h"
#include "bnCardPackageManager.h"

#define WILDCARD '*'

CardSelectionCust::CardSelectionCust(CardSelectionCust::Props _props) :
  props(std::move(_props)),
  greyscale(Shaders().GetShader(ShaderType::GREYSCALE)),
  textbox({ 4, 255 }),
  isInView(false),
  isInFormSelect(false),
  playFormSound(false),
  canInteract(true),
  isDarkCardSelected(false),
  darkCardShadowAlpha(0),
  labelFont(Font::Style::thick),
  codeFont(Font::Style::wide),
  codeFont2(Font::Style::small),
  label("", labelFont),
  damageLabel("", labelFont),
  smCodeLabel("?", codeFont)
{
  this->props.cap = std::min(this->props.cap, 8);

  frameElapsed = 1;
  queue = std::vector<Bucket>(this->props.cap);
  selectQueue = std::vector<Bucket*>(this->props.cap);
  newSelectQueue = std::vector<Bucket*>(this->props.cap);

  cardCount = selectCount = newSelectCount = cursorPos = cursorRow = 0;

  emblem.setScale(2.f, 2.f);
  emblem.setPosition(194.0f, 14.0f);

  custSprite = sf::Sprite(*Textures().LoadFromFile(TexturePaths::CHIP_SELECT_MENU));
  custSprite.setScale(2.f, 2.f);
  custSprite.setPosition(-custSprite.getTextureRect().width*2.f, 0);

  custDarkCardOverlay = sf::Sprite(*Textures().LoadFromFile(TexturePaths::CHIP_SELECT_DARK_OVERLAY));
  custDarkCardOverlay.setScale(2.f, 2.f);
  custDarkCardOverlay.setPosition(custSprite.getPosition());

  custMegaCardOverlay = custDarkCardOverlay;
  custMegaCardOverlay.setTexture(*Textures().LoadFromFile(TexturePaths::CHIP_SELECT_MEGA_OVERLAY));

  custGigaCardOverlay = custDarkCardOverlay;
  custGigaCardOverlay.setTexture(*Textures().LoadFromFile(TexturePaths::CHIP_SELECT_GIGA_OVERLAY));

  // TODO: fully use scene nodes on all card slots and the GUI sprite
  // AddSprite(custSprite);

  sf::IntRect iconSize = sf::IntRect{ 0,0,14,14 };
  sf::Vector2f iconScale = sf::Vector2f(2.f, 2.f);
  icon.setTextureRect(iconSize);
  icon.setScale(iconScale);

  // used when card is missing data
  noIcon = Textures().LoadFromFile(TexturePaths::CHIP_ICON_MISSINGDATA);
  noCard = Textures().LoadFromFile(TexturePaths::CHIP_MISSINGDATA);

  element.setTexture(Textures().LoadFromFile(TexturePaths::ELEMENT_ICON));
  element.setScale(2.f, 2.f);
  element.setPosition(2.f*25.f, 146.f);

  cursorSmall = sf::Sprite(*Textures().LoadFromFile(TexturePaths::CHIP_CURSOR_SMALL));
  cursorSmall.setScale(sf::Vector2f(2.f, 2.f));

  cursorBig = sf::Sprite(*Textures().LoadFromFile(TexturePaths::CHIP_CURSOR_BIG));
  cursorBig.setScale(sf::Vector2f(2.f, 2.f));

  // never moves
  cursorBig.setPosition(sf::Vector2f(2.f*104.f, 2.f*122.f));

  cardLock = sf::Sprite(*Textures().LoadFromFile(TexturePaths::CHIP_LOCK));
  cardLock.setScale(sf::Vector2f(2.f, 2.f));

  cardCard.setScale(2.f, 2.f);
  cardCard.setPosition(2.f*16.f, 48.f);
  cardCard.setTextureRect(sf::IntRect{0, 0, 56, 48});

  cardNoData.setTexture(Textures().LoadFromFile(TexturePaths::CHIP_NODATA));
  cardNoData.setScale(2.f, 2.f);
  cardNoData.setPosition(2.f*16.f, 48.f);

  cardSendData.setTexture(Textures().LoadFromFile(TexturePaths::CHIP_SENDDATA));
  cardSendData.setScale(2.f, 2.f);
  cardSendData.setPosition(2.f*16.f, 48.f);

  smCodeLabel.SetColor(sf::Color::Yellow);

  cursorSmallAnimator = Animation("resources/ui/cursor_small.animation");
  cursorSmallAnimator.Reload();
  cursorSmallAnimator.SetAnimation("BLINK");
  cursorSmallAnimator << Animator::Mode::Loop;

  cursorBigAnimator = Animation("resources/ui/cursor_big.animation");
  cursorBigAnimator.Reload();
  cursorBigAnimator.SetAnimation("BLINK");
  cursorBigAnimator << Animator::Mode::Loop;

  formSelectAnimator = Animation("resources/ui/form_select.animation");
  formSelectAnimator.Reload();
  formSelectAnimator.SetAnimation("CLOSED");

  formCursorAnimator = Animation("resources/ui/form_cursor.animation");
  formCursorAnimator.Reload();
  formCursorAnimator.SetAnimation("BLINK");
  formCursorAnimator << Animator::Mode::Loop;

  formSelectQuitTimer = 0.f; // used to time out the activation
  selectedFormRow = lockedInFormIndex = -1;
  SetSelectedFormIndex(selectedFormRow);

  formSelectQuitTimer = 0.f; // used to time out the activation

  formItemBG.setTexture(*Textures().LoadFromFile(TexturePaths::CUST_FORM_ITEM_BG));
  formItemBG.setScale(2.f, 2.f);

  formSelect.setTexture(Textures().LoadFromFile(TexturePaths::CUST_FORM_SELECT));
  formCursor.setTexture(Textures().LoadFromFile(TexturePaths::CUST_FORM_CURSOR));

  formSelect.setScale(2.f, 2.f);
  formCursor.setScale(2.f, 2.f);

  //setScale(0.5f, 0.5); // testing transforms
  label.setScale(2.f, 2.f);
  damageLabel.setScale(2.1, 2.1);
  smCodeLabel.setScale(2.f, 2.f);
}


CardSelectionCust::~CardSelectionCust() {
  ClearCards();
}

bool CardSelectionCust::CursorUp() {
  if (isInFormSelect) {
    if (--formCursorRow < 0) {
      formCursorRow = 0;
      return false;
    }

    return true;
  }
  else {
    if (--cursorRow < 0) {
      cursorRow = 0;

      if (forms.size()) {
        auto onEnd = [this]() {
          formSelectAnimator << "OPEN";
        };

        formSelectAnimator << "OPENING" << onEnd;
        isInFormSelect = true;
        cursorPos = cursorRow = 0;
        Audio().Play(AudioType::CHIP_DESC);
      }
      return false;
    }

    return true;
  }
}

bool CardSelectionCust::CursorDown() {
  if (isInFormSelect) {
    if (forms.size() == 0) return false;

    if (++formCursorRow >= forms.size()) {
      formCursorRow = int(forms.size() - 1);

      return false;
    }

    return true;
  }
  else {
    if (++cursorRow > 1) {
      cursorRow = 1;

      return false;
    }

    if (cursorPos > 2) {
      cursorPos = 0;
    }

    return true;
  }
}

bool CardSelectionCust::CursorRight() {
  if (isInFormSelect) return false;

  if (++cursorPos > 2 && cursorRow == 1) {
    cursorPos = 5;
    cursorRow = 0;
  }
  else if (cursorPos > 5 && cursorRow == 0) {
    cursorPos = 0;
  }

  return true;
}

bool CardSelectionCust::CursorLeft() {
  if (isInFormSelect) return false;

  if (--cursorPos < 0 && cursorRow == 1) {
    cursorPos = 5;
    cursorRow = 0;
  }
  else if (cursorPos < 0 && cursorRow == 0) {
    cursorPos = 5;
  }

  return true;
}

bool CardSelectionCust::CursorAction() {
  if (isInFormSelect) {
    SetSelectedFormIndex(static_cast<int>(forms[formCursorRow]->GetFormIndex()));

    if (selectedFormRow != -1 && selectedFormIndex == forms[selectedFormRow]->GetFormIndex()) {
      selectedFormRow = -1; // de-select the form

      if (lockedInFormIndex == -1) {
        currentFormItem = sf::Sprite();
        SetSelectedFormIndex(-1);
      }
      else {
        SetSelectedFormIndex(lockedInFormIndex);
        currentFormItem = lockedInFormItem;
      }

      Audio().Play(AudioType::DEFORM);
    }
    else {
      formSelectQuitTimer = seconds_cast<float>(frames(30));
      selectedFormRow = formCursorRow;
      currentFormItem = formUI[formCursorRow];
    }
    return true;
  }

  // Should never happen but just in case
  if (newSelectCount > 5) {
    return false;
  }

  if (cursorPos == 5 && cursorRow == 0) {
    // End card select
    areCardsReady = true;
  } else {
    // Does card exist
    if (cursorPos + (5 * cursorRow) < cardCount) {
      // Queue this card if not selected
      if (queue[cursorPos + (5 * cursorRow)].state == Bucket::state::staged) {
        newSelectQueue[newSelectCount++] = &queue[cursorPos + (5 * cursorRow)];
        queue[cursorPos + (5 * cursorRow)].state = Bucket::state::queued;
        newHand = true;

        // We can only upload 5 cards to player...
        if (newSelectCount == 5) {
          for (int i = 0; i < cardCount; i++) {
            if (queue[i].state == Bucket::state::queued) continue;

            queue[i].state = Bucket::state::voided;
          }

          emblem.CreateWireEffect();
          return true;
        }
        else {
          RefreshAvailableCards(newSelectCount);

          emblem.CreateWireEffect();
          return true;
        }
      }
    }
  }
  return false;
}

bool CardSelectionCust::CursorCancel() {
  if (isInFormSelect) {
    formSelectAnimator.SetAnimation("CLOSED");
    formCursorRow = 0;
    isInFormSelect = false;
    cursorPos = cursorRow = 0;
    return true;
  }

  if (newSelectCount == 0) {
    if (lockedInFormIndex != GetSelectedFormIndex()) {
      currentFormItem.setTexture(*previousFormItem.getTexture());
      SetSelectedFormIndex(previousFormIndex);
      selectedFormRow = -1;

      // This is also where beastout card would be removed from queue
      // when beastout is available
      return true;
    }
  }

  // Unqueue all cards buckets
  if (newSelectCount > 0) {
    newSelectQueue[--newSelectCount]->state = Bucket::state::staged;

    if (newSelectCount == 0) {
      newHand = false;
    }
  }
  
  newSelectCount = std::max(0, newSelectCount);
  
  // Everything is selectable again
  for (int i = 0; i < cardCount; i++) {
    queue[i].state = Bucket::state::staged;
  }

  for (int i = 0; i < newSelectCount; i++) {
    newSelectQueue[i]->state = Bucket::state::queued;
  }

  /*
    Compatible card states are built upon adding cards from the last available card states.
    The only way to "revert" to the last compatible card states is to step through the already selected
    card queue and build up the state again.
  */

  for(int i = 0; i < newSelectCount; i++) {
    RefreshAvailableCards(i+1);
  }

  emblem.UndoWireEffect();
  return true;
}

// This moves the cursor to OK but does not press it
bool CardSelectionCust::CursorSelectOK()
{
  if (isInFormSelect || (cursorPos == 5 && cursorRow == 0)) return false;

  cursorPos = 5;
  cursorRow = 0;

  return true;
}

bool CardSelectionCust::IsOutOfView() {
  float bounds = 0;

  if (getPosition().x <= bounds) {
    setPosition(bounds, getPosition().y);

    if (RequestedRetreat()) {
      ResetPlayerFormSelection();
      selectedFormRow = -1;
    }
  }

  return (getPosition().x == bounds);
}

const bool CardSelectionCust::IsInView() {
  float bounds = custSprite.getTextureRect().width*2.f;

  // std::cout << "getPosition().x: " << getPosition().x << std::endl;

  if (getPosition().x >= bounds) {
    setPosition(bounds, getPosition().y);
  }

  return (getPosition().x == bounds);
}

bool CardSelectionCust::IsTextBoxOpen()
{
  return textbox.IsOpen();
}

bool CardSelectionCust::IsTextBoxClosed()
{
  return textbox.IsClosed();
}

AnimatedTextBox& CardSelectionCust::GetTextBox()
{
  return textbox;
}

const bool CardSelectionCust::IsDarkCardSelected()
{
  return this->isDarkCardSelected && !this->IsHidden() && this->IsInView();
}

void CardSelectionCust::Move(sf::Vector2f delta) {
  setPosition(getPosition() + delta);
  IsInView();
}

bool CardSelectionCust::OpenTextBox()
{
  if (isInFormSelect) return false;

  int index = cursorPos + (5 * cursorRow);

  if (!IsInView() || textbox.IsOpen() ||
    (cursorPos == 5 && cursorRow == 0) || (index >= cardCount)) return false;

  textbox.DescribeCard(queue[index].data);

  return true;
}

const bool CardSelectionCust::HasQuestion() const
{
  return textbox.HasQuestion();
}

bool CardSelectionCust::ContinueTextBox() {
  if (isInFormSelect) return false;
  if (!IsInView() || textbox.IsClosed()) return false;

  // textbox.Continue();
  return false;
}

bool CardSelectionCust::FastForwardTextBox(double factor) {
  if (isInFormSelect) return false;
  if (!IsInView() || textbox.IsClosed()) return false;

  textbox.SetTextSpeed(factor);

  return true;
}

bool CardSelectionCust::CloseTextBox() {
  if (isInFormSelect) return false;
  if (!IsInView() || textbox.IsClosed()) return false;

  textbox.Close();

  return true;
}

void CardSelectionCust::SetSpeaker(const sf::Sprite& mug, const Animation& anim)
{
  textbox.SetSpeaker(mug, anim);
}

void CardSelectionCust::PromptRetreat()
{
  if (!IsInView() || textbox.IsOpen() || isInFormSelect || !retreatAllowed) return;

  textbox.PromptRetreat();
}

bool CardSelectionCust::TextBoxSelectYes() {
  if (isInFormSelect) return false;
  if (!IsInView() || textbox.IsClosed()) return false;

  return textbox.SelectYes();
}

bool CardSelectionCust::TextBoxSelectNo() {
  if (!IsInView() || textbox.IsClosed()) return false;

  return textbox.SelectNo();
}


bool CardSelectionCust::TextBoxConfirmQuestion() {
  if (isInFormSelect) return false;
  if (!IsInView() || textbox.IsClosed()) return false;

  textbox.ConfirmSelection();
  return true; // play audio
}

void CardSelectionCust::GetNextCards() {
  emblem.Reset();

  bool selectFirstDarkCard = true;

  for (int i = cardCount; i < props.cap; i++) {
    queue[i].data = props._folder->Next();

    if (!queue[i].data) {
      // nullptr is end of list
      return;
    }

    bool isDarkCard = queue[i].data->GetClass() == Battle::CardClass::dark;

    // This auto-selects the first dark card if visible
    if (selectFirstDarkCard && isDarkCard) {
      cursorPos = i % 5;
      cursorRow = i / 5;
      selectFirstDarkCard = false;
    }

    queue[i].state = Bucket::state::staged;

    cardCount++;
    props.perTurn--;

   if (props.perTurn == 0) return;
  }
}

void CardSelectionCust::SetPlayerFormOptions(const std::vector<PlayerFormMeta*> forms)
{
  for (PlayerFormMeta* f : forms) {
    this->forms.push_back(f);
    sf::Sprite ui;
    ui.setTexture(*Textures().LoadFromFile(f->GetUIPath()));
    ui.setScale(2.f, 2.f);
    formUI.push_back(ui);
  }
}

void CardSelectionCust::ResetPlayerFormSelection()
{
  SetSelectedFormIndex(-1);
  selectedFormRow = -1;
  lockedInFormIndex = selectedFormIndex;
  lockedInFormItem = currentFormItem = sf::Sprite();
}

void CardSelectionCust::LockInPlayerFormSelection()
{
  lockedInFormIndex = selectedFormIndex;
  lockedInFormItem = currentFormItem;
}

void CardSelectionCust::ErasePlayerFormOption(size_t index)
{
  for (int i = 0; i < forms.size() && formUI.size(); i++) {
    if (forms[i]->GetFormIndex() == index) {
      forms.erase(forms.begin() + i);
      formUI.erase(formUI.begin() + i);
    }
  }

  // reset the row selection pos but keep the form index selection active
  selectedFormRow = -1;
}

void CardSelectionCust::draw(sf::RenderTarget & target, sf::RenderStates states) const {
  if(IsHidden()) return;

  ResourceHandle handle;

  states.transform = getTransform();

  float offset = -custSprite.getTextureRect().width*2.f; // TODO: this will be uneccessary once we use AddSprite() for all rendered items below
  custSprite.setPosition(-sf::Vector2f(custSprite.getTextureRect().width*2.f, 0));
  target.draw(custSprite, states);

  int index = cursorPos + (5 * cursorRow);

  // Make sure we are not selecting "OK" and we have a valid card highlighted
  if (index < cardCount && !(cursorPos == 5 && cursorRow == 0)) {
    switch (this->queue[index].data->GetClass()) {
    case Battle::CardClass::mega:
      custMegaCardOverlay.setPosition(custSprite.getPosition());
      target.draw(custMegaCardOverlay, states);
      break;
    case Battle::CardClass::giga:
      custGigaCardOverlay.setPosition(custSprite.getPosition());
      target.draw(custGigaCardOverlay, states);
      break;
    case Battle::CardClass::dark:
      custDarkCardOverlay.setPosition(custSprite.getPosition());
      target.draw(custDarkCardOverlay, states);
      break;
    }
  }

  sf::Vector2f lastEmblemPos = emblem.getPosition();
  emblem.setPosition(emblem.getPosition() + sf::Vector2f(offset, 0));
  target.draw(emblem, states);
  emblem.setPosition(lastEmblemPos);

  float x = swoosh::ease::interpolate(static_cast<float>(frameElapsed*25), offset + static_cast<float>((2.f*(16.0f + (cursorPos*16.0f)))), cursorSmall.getPosition().x);
  float y = swoosh::ease::interpolate(static_cast<float>(frameElapsed * 25), static_cast<float>(2.f*(113.f + (cursorRow*24.f))), cursorSmall.getPosition().y);

  cursorSmall.setPosition(x, y); // TODO: Make this relative to cust instead of screen. hint: scene nodes

  int row = 0;
  for (int i = 0; i < cardCount; i++) {
    if (i > 4) {
      row = 1;
    }

    icon.setPosition(offset + 2.f*(9.0f + ((i%5)*16.0f)), 2.f*(105.f + (row*24.0f)) );

    std::shared_ptr<sf::Texture> texture;
    std::string id = queue[i].data->GetUUID();

    if (props.roster->HasPackage(id)) {
      texture = props.roster->FindPackageByID(id).GetIconTexture();
      smCodeLabel.SetColor(sf::Color::Yellow);
    }
    else {
      texture = noIcon;
      smCodeLabel.SetColor(sf::Color::Red);
    }
    
    icon.setTexture(texture);
    icon.SetShader(nullptr);

    if (queue[i].state == Bucket::state::voided) {
      icon.SetShader(greyscale);
      sf::RenderStates statesCopy = states;
      statesCopy.shader = greyscale;

      target.draw(icon,statesCopy);

    } else if (queue[i].state == Bucket::state::staged) {
      target.draw(icon, states);
    }

    smCodeLabel.setPosition(offset + 2.f*(13.0f + ((i % 5)*16.0f)), 2.f*(121.f + (row*24.0f)));

    char code = queue[i].data->GetCode();
    smCodeLabel.SetString(code);
    target.draw(smCodeLabel, states);
  }

  icon.SetShader(nullptr);

  for (int i = 0; i < newSelectCount; i++) {
    icon.setPosition(offset + 2 * 97.f, 2.f*(25.0f + (i*16.0f)));

    // Draw the selected card card
    std::shared_ptr<sf::Texture> texture;
    std::string id = (*newSelectQueue[i]).data->GetUUID();

    if (props.roster->HasPackage(id)) {
      texture = props.roster->FindPackageByID(id).GetIconTexture();
    }
    else {
      texture = noIcon;
    }

    icon.setTexture(texture);

    cardLock.setPosition(offset + 2 * 93.f, 2.f*(23.0f + (i*16.0f)));
    target.draw(cardLock, states);
    target.draw(icon, states);
  }

  if ((cursorPos < 5 && cursorRow == 0) || (cursorPos < 3 && cursorRow == 1)) {

    if (cursorPos + (5 * cursorRow) < cardCount) {
      // Draw the selected card card
      std::shared_ptr<sf::Texture> texture;
      std::string id = queue[cursorPos + (5 * cursorRow)].data->GetUUID();

      if (props.roster->HasPackage(id)) {
        texture = props.roster->FindPackageByID(id).GetPreviewTexture();
      }
      else {
        texture = noCard;
      }
      cardCard.setTexture(texture, false);

      sf::Vector2f lastPos = cardCard.getPosition();
      cardCard.setPosition(sf::Vector2f(offset, 0) + cardCard.getPosition());
      cardCard.SetShader(nullptr);

      if (queue[cursorPos + (5 * cursorRow)].state == Bucket::state::voided) {
        cardCard.SetShader(greyscale);

        sf::RenderStates statesCopy = states;
        statesCopy.shader = greyscale;

        target.draw(cardCard, statesCopy);
      }
      else {
        target.draw(cardCard, states);
      }

      cardCard.setPosition(lastPos);

      // card name font shadow
      const std::string shortname = queue[cursorPos + (5 * cursorRow)].data->GetShortName();
      label.setPosition((offset + 2.f * 16.f) + 2.f, 26.f);
      label.SetString(shortname);
      label.SetColor(sf::Color(80, 75, 80));
      target.draw(label, states);

      // card name font overlay
      label.setPosition(offset + 2.f * 16.f, 24.f);
      label.SetString(shortname);

      if (props.roster->HasPackage(id)) {
        label.SetColor(sf::Color(224, 224, 224, 255));
      }
      else {
        label.SetColor(sf::Color::Red);
      }

      damageLabel.SetColor(sf::Color(224, 224, 224, 255));
      target.draw(label, states);

      // the order here is very important:
      if (queue[cursorPos + (5 * cursorRow)].data->GetDamage() > 0) {
        damageLabel.SetString(std::to_string(queue[cursorPos + (5 * cursorRow)].data->GetDamage()));
        label.setOrigin(label.GetLocalBounds().width + label.GetLocalBounds().left, 0);
        damageLabel.setPosition(((offset-28.f) + 2.f * (70.f)) + 2.f, 150.f);
        target.draw(damageLabel, states);
      }

      label.setOrigin(0, 0);
      label.setPosition(offset + 2.f * 16.f, 150.f);
      label.SetString(std::string() + queue[cursorPos + (5 * cursorRow)].data->GetCode());
      label.SetColor(sf::Color(253, 246, 71));
      target.draw(label, states);

      int elementID = (int)(queue[cursorPos + (5 * cursorRow)].data->GetElement());

      sf::IntRect elementRect = sf::IntRect(14 * elementID, 0, 14, 14);
      element.setTextureRect(elementRect);

      sf::Vector2f elementLastPos = element.getPosition();
      element.setPosition(element.getPosition() + sf::Vector2f(offset, 0));

      target.draw(element, states);

      element.setPosition(elementLastPos);
    }
    else {
      sf::Vector2f cardNoDataLastPos = cardNoData.getPosition();
      cardNoData.setPosition(cardNoData.getPosition() + sf::Vector2f(offset, 0));
      target.draw(cardNoData, states);
      cardNoData.setPosition(cardNoDataLastPos);
    }

    // Draw the small cursor
    if (!isInFormSelect) {
      target.draw(cursorSmall, states);
    }
  }
  else {
    sf::Vector2f cardSendDataLastPos = cardSendData.getPosition();
    cardSendData.setPosition(cardSendData.getPosition() + sf::Vector2f(offset, 0));
    target.draw(cardSendData, states);
    cardSendData.setPosition(cardSendDataLastPos);

    sf::Vector2f cursorBigLastPos = cursorBig.getPosition();
    cursorBig.setPosition(cursorBig.getPosition() + sf::Vector2f(offset, 0));
    target.draw(cursorBig, states);
    cursorBig.setPosition(cursorBigLastPos);
  }

  if (formUI.size()) {
    target.draw(formSelect, states);
  }

  // find out if we're in view (IsInView() is non-const qualified...)
  float bounds = custSprite.getTextureRect().width * 2.f;

  if (getPosition().x == bounds) {
    // Fade in a screen-wide shadow beneath the card if it is a dark card
    sf::RectangleShape screen(target.getView().getSize());
    screen.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(darkCardShadowAlpha * 255)));
    target.draw(screen);
  }

  // draw the dark card in on top of it so it looks brighter
  if (isDarkCardSelected) {
    sf::Vector2f lastPos = cardCard.getPosition();
    cardCard.setPosition(sf::Vector2f(offset, 0) + cardCard.getPosition());
    target.draw(cardCard, states);
    cardCard.setPosition(lastPos);
  }

  if (isInFormSelect) {
    if (formSelectAnimator.GetAnimationString() == "OPEN") {
      int i = 0;
      float offset = -custSprite.getTextureRect().width*2.f; // TODO: this will be uneccessary once we use AddNode() for all rendered items below

      for (sf::Sprite f : formUI) {
        formItemBG.setPosition(offset + 16.f, 16.f + float(i*32.0f));
        target.draw(formItemBG, states);

        f.setPosition(offset + 16.f, 16.f + float(i*32.0f));

        if (i != selectedFormRow && i != formCursorRow && selectedFormRow > -1) {
          sf::RenderStates greyscaleState = states;
          greyscaleState.shader = handle.Shaders().GetShader(ShaderType::GREYSCALE);
          target.draw(f, greyscaleState);
        }
        else if (i == selectedFormRow && selectedFormRow > -1) {
          sf::RenderStates aquaMarineState = states;
          aquaMarineState.shader = handle.Shaders().GetShader(ShaderType::COLORIZE);
          f.setColor(sf::Color(127,255,212));
          target.draw(f, aquaMarineState);
          f.setColor(sf::Color::White); // back to normal after draw to texture
        }
        else {
          // normal color
          target.draw(f, states);
        }

        i++;
      }

      // Draw cursor only if not flashing the screen
      if (formSelectQuitTimer <= 0.f) {
        target.draw(formCursor, states);
      }
    }
  }

  target.draw(textbox);

  SceneNode::draw(target, states);

  // Reveal the new form icon in the HUD after the flash
  if (formSelectQuitTimer <= seconds_cast<float>(frames(10))) {
    sf::IntRect rect = currentFormItem.getTextureRect();
    currentFormItem.setTextureRect(sf::IntRect(rect.left, rect.top, 30, 14));
    currentFormItem.setPosition(sf::Vector2f(4, 36.f));
    target.draw(currentFormItem, states);
    currentFormItem.setTextureRect(rect);
  }

  // draw the white flash
  if (isInFormSelect && formSelectQuitTimer <= seconds_cast<float>(frames(20))) {
    double delta = swoosh::ease::wideParabola(formSelectQuitTimer, seconds_cast<double>(frames(20)), 1.0);
    const sf::View& view = target.getView();
    sf::RectangleShape screen(view.getSize());
    screen.setFillColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(delta * 255.0)));
    target.draw(screen, sf::RenderStates::Default);
  }
}

void CardSelectionCust::Update(double elapsed)
{
  if (IsHidden()) {
    canInteract = false;
    isDarkCardSelected = false;
    darkCardShadowAlpha = 0.f;
    return;
  }

  canInteract = true; // assume we can interact unless another flag says otherwise

  frameElapsed = (double)elapsed;

  if (!IsInView()) {
    darkCardShadowAlpha = 0.0f;
    isDarkCardSelected = false;
  }
  else {
    if (isDarkCardSelected) {
      darkCardShadowAlpha = std::min(darkCardShadowAlpha + static_cast<float>(elapsed), 0.3f);
    }
    else {
      darkCardShadowAlpha = std::max(darkCardShadowAlpha - static_cast<float>(elapsed), 0.0f);
    }
  }

  cursorSmallAnimator.Update(elapsed, cursorSmall);
  cursorBigAnimator.Update(elapsed, cursorBig);

  textbox.Update(elapsed);

  emblem.Update(elapsed);

  formCursorAnimator.Update(elapsed, formCursor.getSprite());
  formSelectAnimator.Update(elapsed, formSelect.getSprite());

  float offset = -custSprite.getTextureRect().width*2.f; // TODO: this will be uneccessary once we use AddNode() for all rendered items...

  formCursor.setPosition(offset + 16.f, 12.f + float(formCursorRow*32.f));
  formSelect.setPosition(offset + 88.f, 194.f);

  if (isInFormSelect && formSelectQuitTimer > 0.f) {
    canInteract = false;

    formSelectQuitTimer -= elapsed;

    if (formSelectQuitTimer <= 0.f) {
      canInteract = true;
      isInFormSelect = false;
      playFormSound = false;
    }
    else if (formSelectQuitTimer <= seconds_cast<float>(frames(10))) {
      formSelectAnimator.SetAnimation("CLOSED");
      cursorPos = cursorRow = formCursorRow = 0;
      if (!playFormSound) {
        playFormSound = true;
        Audio().Play(AudioType::PA_ADVANCE, AudioPriority::highest);
      }
    }
  }

  if (cardCount > 0) {
    // If OK button is highlighted, we are not selecting a dark card
    // If we are in form select, we are not selecting a dark card
    if ((cursorRow == 0 && cursorPos == 5) || isInFormSelect) {
      isDarkCardSelected = false;
    }
    else {
      // Otherwise check if we are highlighting a dark card
      int index = cursorPos + (5 * cursorRow);
      isDarkCardSelected = this->queue[index].data && this->queue[index].data->GetClass() == Battle::CardClass::dark;
    }
  }

  isInView = IsInView();
}

std::vector<Battle::Card> CardSelectionCust::GetCards() {
  if (newSelectCount != 0) {
    // Allocate selected cards list
    size_t sz = sizeof(Battle::Card*) * newSelectCount;

    selectedCards.clear();

    // Point to selected queue
    for (int i = 0; i < newSelectCount; i++) {
      selectedCards.emplace_back(*newSelectQueue[i]->data);
    }

    selectCount = newSelectCount;

    ClearCards(); // prepare for next selection
  }

  return selectedCards;
}

bool CardSelectionCust::HasNewHand() const
{
    return newHand;
}

void CardSelectionCust::ClearCards() {
  if (areCardsReady) {
    for (int i = 0; i < newSelectCount; i++) {
      newSelectQueue[i]->data = nullptr;
    }
  }

  // Restructure queue
  // Line up all non-null buckets consecutively
  // Reset bucket state to STAGED
  for (int i = 0; i < cardCount; i++) {
    queue[i].state = Bucket::state::staged;
    int next = i;
    while (!queue[i].data && next + 1 < cardCount) {
      queue[i].data = queue[next + 1].data;
      queue[next + 1].data = nullptr;
      next++;
    }
  }

  cardCount = cardCount - newSelectCount;
  newSelectCount = 0;
}

const int CardSelectionCust::GetCardCount() {
  return selectCount;
}

const int CardSelectionCust::GetSelectedFormIndex()
{
  return selectedFormIndex;
}

const bool CardSelectionCust::SelectedNewForm()
{
  return (selectedFormIndex != -1);
}

const bool CardSelectionCust::CanInteract()
{
  return canInteract;
}

const bool CardSelectionCust::RequestedRetreat()
{
  return textbox.RequestedRetreat();
}

void CardSelectionCust::ResetState() {
  cursorPos = formCursorRow = 0;
  areCardsReady = false;
  isInFormSelect = false;
  newHand = false;
  SetSelectedFormIndex(lockedInFormIndex);
  textbox.Reset();
}

void CardSelectionCust::PreventRetreat()
{
  retreatAllowed = false;
}

bool CardSelectionCust::AreCardsReady() {
  return areCardsReady;
}

void CardSelectionCust::RefreshAvailableCards(int handSize)
{
  // Stage valid available cards
  // If other cards are not compatible, set their bucket state flag to voided

  enum class SelectionMode {
    any,
    same_name_or_code,
    same_code,
    same_name
  };

  // get info about the hand
  char resolvedCode = '*';
  std::string resolvedName = "";
  bool codeDiffers = false;
  bool nameDiffers = false;

  for (int i = 0; i < handSize; i++) {
    Battle::Card* selectedCard = newSelectQueue[i]->data;
    const std::string& name = selectedCard->GetShortName();
    char code = selectedCard->GetCode();

    if (i == 0) {
      resolvedName = name;
      resolvedCode = code;
      continue;
    }

    if (code != WILDCARD) {
      if (resolvedCode == WILDCARD) {
        resolvedCode = code;
      }
      else if (resolvedCode != code) {
        codeDiffers = true;
      }
    }

    if (name != resolvedName) {
      nameDiffers = true;
    }
  }

  // resolve mode
  SelectionMode selectionMode = SelectionMode::any;

  if (nameDiffers && resolvedCode != WILDCARD) {
    selectionMode = SelectionMode::same_code;
  }
  else if (codeDiffers) {
    selectionMode = SelectionMode::same_name;
  }
  else if (resolvedCode != WILDCARD) {
    selectionMode = SelectionMode::same_name_or_code;
  }

  // voiding, this could be reduced
  for (int i = 0; i < cardCount; i++) {
    CardSelectionCust::Bucket& bucket = queue[i];
    Battle::Card* card = bucket.data;
    bool matchingName = card->GetShortName() == resolvedName;
    bool matchingCode = card->GetCode() == resolvedCode || card->GetCode() == WILDCARD;
    bool shouldVoid = false;

    switch (selectionMode) {
    case SelectionMode::any:
      // we could prob just skip this loop to begin with for SelectionMode::any
      break;
    case SelectionMode::same_name_or_code:
      shouldVoid = !matchingName && !matchingCode;
      break;
    case SelectionMode::same_code:
      shouldVoid = !matchingCode;
      break;
    case SelectionMode::same_name:
      shouldVoid = !matchingName;
      break;
    }

    if (shouldVoid) {
      bucket.state = CardSelectionCust::Bucket::state::voided;
    }
  }
}

void CardSelectionCust::SetSelectedFormIndex(int index)
{
  if (selectedFormIndex != index)
  {
    previousFormItem = currentFormItem;
    previousFormIndex = selectedFormIndex;
    selectedFormIndex = index;
    Broadcast(index);
  }
}