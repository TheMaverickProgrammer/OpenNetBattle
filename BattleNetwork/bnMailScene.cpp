#include "bnMailScene.h"
#include "bnTextureResourceManager.h"
#include <Segues/BlackWashFade.h>

void MailScene::ResetTextBox()
{
  // greetings
  std::string msg = "You have no mail";

  if (inbox.Size()) {
    msg = "Which mail do you want to read?";
  }

  textbox.SetCharactersPerSecond(40.f);
  textbox.SetText(msg);
  textbox.Mute();
  textbox.CompleteAll();
  textbox.setScale(2.f, 2.f);

  isReading = false;
}

std::string MailScene::GetStringFromIcon(Inbox::Icons icon)
{
  std::string state;

  switch (icon) {
  case Inbox::Icons::announcement:
    state = "announcement";
    break;
  case Inbox::Icons::dm:
    state = "dm";
    break;
  case Inbox::Icons::dm_w_attachment:
    state = "dm_w_attachment";
    break;
  case Inbox::Icons::important:
    state = "important";
    break;
  case Inbox::Icons::mission:
    state = "mission";
    break;
  }

  return state;
}

MailScene::MailScene(swoosh::ActivityController& controller, Inbox& inbox) :
  inbox(inbox),
  label(Font::Style::thin),
  textbox(180,22, Font::Style::thin),
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

  iconTexture = Textures().LoadFromFile("resources/scenes/mail/icons.png");
  iconAnim = Animation("resources/scenes/mail/icons.animation");

  noMug = Textures().LoadFromFile("resources/scenes/mail/nomug.png");

  newSprite.setTexture(*iconTexture);
  newSprite.setScale(2.f, 2.f);
  newAnim = iconAnim;
  newAnim.SetAnimation("new");
  newAnim << Animator::Mode::Loop;

  // Text box navigator
  textbox.setPosition(100, 205);
  textbox.SetTextColor(sf::Color::Black);
  textbox.Mute();

  ResetTextBox();

  scroll.setPosition(450.f, 40.f);
  setView(sf::Vector2u(480, 320));
}

MailScene::~MailScene()
{
}

void MailScene::onLeave()
{
  isInFocus = false;
}

void MailScene::onExit()
{
}

void MailScene::onEnter()
{
  isInFocus = false;
}

void MailScene::onResume()
{
  isInFocus = true;
}

void MailScene::onStart()
{
  isInFocus = true;
}

void MailScene::onUpdate(double elapsed)
{
  textbox.Update(elapsed);
  newAnim.Update(elapsed, newSprite);

  size_t lastRow = row;

  if (textbox.HasMore()) {
    textbox.Play(false); // don't keep typing on the second page
  }

  if (Input().Has(InputEvents::pressed_ui_up) && !isReading) {
    row--;
  }
  else if (Input().Has(InputEvents::pressed_ui_down) && !isReading) {
    row++;
  }
  else if (Input().Has(InputEvents::pressed_confirm)) {
    if (row < inbox.Size()) {
      size_t idx = row + rowOffset;

      if (!isReading) {

        auto callback = [this, idx](const Inbox::Mail& mail) {
          isReading = true;
          reading = idx;
          textbox.SetText(mail.body);
          textbox.Unmute();
          textbox.Play();
          Audio().Play(AudioType::CHIP_CONFIRM, AudioPriority::high);
        };

        inbox.ReadMail(idx, callback);

      }else if (textbox.HasMore() || textbox.IsPlaying()) {
        if (textbox.IsEndOfBlock()) {
          for (size_t i = 0; i < textbox.GetNumberOfFittingLines(); i++) {
            textbox.ShowNextLine();
          }
        }
        else {
          textbox.CompleteCurrentBlock();
        }
        textbox.Play(true); // continue typing...
        Audio().Play(AudioType::CHIP_CHOOSE);
      }
      else {
        Inbox::Mail& mail = inbox.GetAt(idx);
        if (mail.read) {
          mail.onReadCallback(mail);
          mail.onReadCallback.Reset();
        }

        if (isInFocus) {
          Audio().Play(AudioType::CHIP_DESC_CLOSE);
        }

        ResetTextBox();
      }
    }
  }
  else if (Input().Has(InputEvents::pressed_cancel)) {
    using namespace swoosh::types;
    getController().pop<segue<BlackWashFade, milli<500>>>();
    Audio().Play(AudioType::CHIP_DESC_CLOSE);
  }

  size_t lastRowOffset = rowOffset;

  if (row >= maxRows) {
    rowOffset++;
    row--;
  }

  if (row < 0) {
    rowOffset--;
    row++;
  }

  size_t maxRowOffset = inbox.Size();

  row = std::max(size_t(0), row);

  if (maxRowOffset > 1) {
    row = std::min(maxRowOffset - 1, row);
  }

  size_t index = row;

  rowOffset = std::max(size_t(0), rowOffset);
  rowOffset = std::min(maxRowOffset, rowOffset);

  if (lastRow != row || lastRowOffset != rowOffset) {
    Audio().Play(AudioType::CHIP_SELECT);
  }

  index += rowOffset;

  // Now that we've forced the change of values into a restricted range,
  // check to see if we actually selected anything new
  if (!(index < inbox.Size() && (rowOffset != lastRowOffset || row != lastRow))) {
    row = lastRow;
    rowOffset = lastRowOffset;
  }

  float startPos = 40.f;
  float endPos = 160.f;
  float rowsPerView = std::max(1.f, static_cast<float>(maxRowOffset - maxRows));
  float delta = static_cast<float>(rowOffset) / rowsPerView;
  float y = (endPos * delta) + (startPos * (1.f - delta));

  scroll.setPosition(450.f, y);

  unsigned bob = from_seconds(this->totalElapsed * 0.20).count() % 5; // 5 pixel bobs
  float bobf = static_cast<float>(bob);
  cursor.setPosition(30 + bobf, 50 + (row * 32) + 1.f);

  float bounce = std::sin((float)totalElapsed * 20.0f);
  moreText.setPosition(sf::Vector2f(225, 140+bounce) * 2.0f);

  totalElapsed += static_cast<float>(elapsed);
}

void MailScene::onDraw(IRenderer& renderer)
{
  renderer.submit(&bg);

  for (size_t i = 0; i < maxRows && i < inbox.Size(); i++) {
    size_t offset = rowOffset;
    size_t index = offset + i;

    if (index >= inbox.Size()) break;

    Inbox::Mail& msg = inbox.GetAt(index);

    // icon
    sf::Sprite spr(*iconTexture);
    iconAnim.SetAnimation(GetStringFromIcon(msg.icon));
    iconAnim.Refresh(spr);
    spr.setPosition(48.f, 44 + (i * 32.f) + 10.f);
    spr.setScale(2.f, 2.f);
    // TODO: remove Clone()
    renderer.submit(Clone(spr));

    // NEW
    if (!msg.read) {
      newSprite.setPosition(spr.getPosition().x - 2.f, spr.getPosition().y - 6.f);
      // TODO: remove Clone()
      renderer.submit(Clone(newSprite));
    }

    label.SetString(msg.title.substr(0, 11));
    label.setPosition(80.f, 42 + (i * 32.f) + 4.f);

    sf::Color read = sf::Color(165, 214, 255);
    sf::Color unread = sf::Color::White;
    sf::Color from = sf::Color(115, 255, 189);

    // TITLE
    if (msg.read) {
      label.SetColor(read);
    }
    else {
      label.SetColor(unread);
    }

    // TODO: remove CLone()
    renderer.submit(Clone(label));

    // FROM
    label.SetString(msg.from.substr(0, 8));
    label.setPosition(label.getPosition().x + 160.f, label.getPosition().y);
    label.SetColor(from);
    // TODO: remove CLone()
    renderer.submit(Clone(label));

    // NUMBER
    size_t num = offset + i + 1;
    std::string num_str = std::to_string(num);

    if (num < 10) {
      num_str = "0" + num_str;
    }

    label.SetString(num_str);
    label.setOrigin(label.GetLocalBounds().width * label.getScale().x, 0.0);
    label.setPosition(label.getPosition().x + 240.f, label.getPosition().y);
    label.SetColor(unread); 
    // TODO: remove CLone()
    renderer.submit(Clone(label));
  
    // reset origin
    label.setOrigin(0, 0);
  }
 
  renderer.submit(&scroll);

  if (isInFocus) {
    renderer.submit(&textbox);
  }

  if (textbox.HasMore()) {
    renderer.submit(&moreText);
  }

  // mugshot
  if (isReading) {
    Inbox::Mail& msg = inbox.GetAt(this->reading);
    
    if (msg.mugshot.getNativeHandle()) {
      sf::Sprite mug(msg.mugshot, sf::IntRect(0, 0, 40, 48));
      mug.setScale(2.f, 2.f);
      mug.setPosition(12.f, 208.f);
      sf::RenderStates states;
      states.shader = Shaders().GetShader(ShaderType::GREYSCALE);
      renderer.submit(&mug, states);
    }
    else {
      sf::Sprite mug(*noMug, sf::IntRect(0, 0, 40, 48));
      mug.setScale(2.f, 2.f);
      mug.setPosition(12.f, 208.f);
      renderer.submit(&mug);
    }
  }
  /*
  else {
    // TODO: Player mug?
  }
  */

  renderer.submit(&cursor);
}

void MailScene::onEnd()
{
}
