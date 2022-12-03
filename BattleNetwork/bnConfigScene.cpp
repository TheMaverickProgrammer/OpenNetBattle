#include "bnConfigScene.h"
#include "Segues/WhiteWashFade.h"
#include "bnRobotBackground.h"
#include "bnGameSession.h"
#include "bnMessageInput.h"

constexpr float COL_PADDING = 4.0f;
constexpr float SUBMENU_SPAN = 90.0f;
constexpr float BINDED_VALUE_OFFSET = 240.0f - SUBMENU_SPAN - COL_PADDING;
constexpr float MENU_START_Y = 40.0f;
constexpr float LINE_SPAN = 15.0f;
constexpr float SCROLL_INTERPOLATION_MULTIPLIER = 6.0f;
constexpr float INITIAL_SCROLL_COOLDOWN = 0.5f;
constexpr float SCROLL_COOLDOWN = 0.05f;

const sf::Color DEFAULT_TEXT_COLOR = sf::Color(255, 165, 0);
const sf::Color DISABLED_TEXT_COLOR = sf::Color(255, 0, 0);
const sf::Color NO_BINDING_COLOR = sf::Color(10, 165, 255);

void ConfigScene::MenuItem::draw(sf::RenderTarget& target, sf::RenderStates states) const {
  // SceneNode doesn't apply transform like SpriteProxyNode
  states.transform *= getTransform();
  SceneNode::draw(target, states);
}

ConfigScene::TextItem::TextItem(
  const std::string& text,
  const std::function<void(TextItem&)>& callback,
  const std::function<void(TextItem&)>& secondaryCallback
) :
  MenuItem(
    [this, callback] { callback(*this); },
    [this, secondaryCallback] { secondaryCallback(*this); }
  ),
  label(std::make_shared<Text>(text, Font::Style::wide)),
  color(DEFAULT_TEXT_COLOR)
{
  AddNode(label);
}

const std::string& ConfigScene::TextItem::GetString() {
  return label->GetString();
}

void ConfigScene::TextItem::SetString(const std::string& text) {
  label->SetString(text);
}

void ConfigScene::TextItem::SetColor(sf::Color color) {
  this->color = color;
  label->SetColor(color);
}

void ConfigScene::TextItem::SetAlpha(sf::Uint8 alpha) {
  color.a = alpha;
  label->SetColor(color);
}

ConfigScene::NicknameItem::NicknameItem(const std::function<void()>& callback) : TextItem("Set Nick", [callback](auto&) { callback(); }) {}

void ConfigScene::NicknameItem::Update() {
  if (GetNick().empty()) {
    SetString("Set Nick");
    return;
  }

  SetString("Nick: " + GetNick());
}

void ConfigScene::NicknameItem::SetNick(const std::string& nickname)
{
  this->nickname = nickname;
}

std::string ConfigScene::NicknameItem::GetNick()
{
  return this->nickname;
}

ConfigScene::BindingItem::BindingItem(
  const std::string& inputName,
  std::optional<std::reference_wrapper<std::string>> valueName,
  const std::function<void(BindingItem&)>& callback,
  const std::function<void(BindingItem&)>& secondaryCallback
) :
  TextItem(
    inputName,
    [this, callback](auto&) { callback(*this); },
    [this, secondaryCallback](auto&) { secondaryCallback(*this); }
  ),
  valueText(std::make_shared<Text>(Font::Style::wide))
{
  SetValue(valueName);
  AddNode(valueText);
}

void ConfigScene::BindingItem::SetValue(std::optional<std::reference_wrapper<std::string>> valueName) {
  if (valueName) {
    valueColor = DEFAULT_TEXT_COLOR;
    valueText->SetString(valueName->get());
  }
  else {
    valueColor = NO_BINDING_COLOR;
    valueText->SetString("NO KEY");
  }
}

void ConfigScene::BindingItem::SetAlpha(sf::Uint8 alpha) {
  valueColor.a = alpha;
  valueText->SetColor(valueColor);
  TextItem::SetAlpha(alpha);
}

void ConfigScene::BindingItem::Update() {
  valueText->setPosition((BINDED_VALUE_OFFSET * 2.0f) / getScale().x - valueText->GetLocalBounds().width, 0.0f);
}

ConfigScene::NumberItem::NumberItem(
  const std::string& text,
  sf::Color color,
  int value,
  const std::function<void(int, NumberItem&)>& callback) :
  TextItem(text, createCallback(callback), createSecondaryCallback(callback)),
  refreshCallback(callback),
  value(value),
  icon(std::make_shared<SpriteProxyNode>())
{
  this->color = color;
}

std::function<void(ConfigScene::TextItem&)> ConfigScene::NumberItem::createCallback(const std::function<void(int, NumberItem&)>& callback) {
  // increase number
  return [this, callback](auto&) {
    value = value + 1;

    if (value > maxValue) {
      value = minValue;
    }

    animator.SetFrame(value, icon->getSprite());
    callback(value, *this);
  };
}

std::function<void(ConfigScene::TextItem&)> ConfigScene::NumberItem::createSecondaryCallback(const std::function<void(int, NumberItem&)>& callback) {
  // lower number
  return [this, callback](auto&) {
    value = value - 1;

    if (value < minValue) {
      value = maxValue;
    }

    animator.SetFrame(value, icon->getSprite());
    callback(value, *this);
  };
}

void ConfigScene::NumberItem::SetAlpha(sf::Uint8 alpha) {
  TextItem::SetAlpha(alpha);
  icon->setColor(color);
}

void ConfigScene::NumberItem::UseIcon(const std::filesystem::path& image_path, const std::filesystem::path& animation_path, const std::string& state)
{
  AddNode(icon);
  icon->setTexture(Textures().LoadFromFile(image_path));
  icon->setPosition(
    label->GetLocalBounds().width + COL_PADDING,
    label->GetLocalBounds().height / 2.0f - icon->getLocalBounds().height / 2.0f
  );

  animator = Animation(animation_path);
  animator.Load();
  animator.SetAnimation(state);
  animator.SetFrame(value, icon->getSprite());
  animator.Update(0, icon->getSprite());
}

void ConfigScene::NumberItem::SetValueRange(int min, int max)
{
  minValue = min;
  maxValue = max;

  value = std::min(max, std::max(min, value));
  refreshCallback(value, *this);
}

ConfigScene::ConfigScene(swoosh::ActivityController& controller) :
  textbox(sf::Vector2f(4, 250)),
  Scene(controller)
{
  configSettings = getController().ConfigSettings();
  gamepadWasActive = Input().IsUsingGamepadControls();
  textbox.SetTextSpeed(2.0);
  isSelectingTopMenu = false;

  // Draws the scrolling background
  bg = new RobotBackground();

  // dim
  bg->SetColor(sf::Color(120, 120, 120));

  endBtnAnimator = Animation("resources/ui/config/end_btn.animation");
  endBtnAnimator.Load();

  // end button
  endBtn = sf::Sprite(*Textures().LoadFromFile(TexturePaths::END_BTN));;
  endBtn.setScale(2.f, 2.f);
  endBtnAnimator.SetAnimation("BLINK");
  endBtnAnimator.SetFrame(1, endBtn);
  endBtn.setPosition(2 * 180, 2 * 10);

  // ui sprite maps
  // ascii 58 - 96
  // BGM
  const std::filesystem::path audio_img = "resources/ui/config/audio.png";
  const std::filesystem::path audio_ani = "resources/ui/config/audio.animation";

  musicLevel = configSettings.GetMusicLevel();
  auto bgMusicItem = std::make_shared<NumberItem>(
    "BGM",
    sf::Color(255, 0, 255),
    musicLevel,
    [this](int volumeLevel, NumberItem& item) { UpdateBgmVolume(volumeLevel); });

  bgMusicItem->UseIcon(audio_img, audio_ani);
  bgMusicItem->SetValueRange(1, 4);
  primaryMenu.push_back(std::move(bgMusicItem));

  // SFX
  sfxLevel = configSettings.GetSFXLevel();
  auto sfxItem = std::make_shared<NumberItem>(
    "SFX",
    sf::Color(10, 165, 255),
    sfxLevel,
    [this](int volumeLevel, NumberItem& item) { UpdateSfxVolume(volumeLevel); });

  sfxItem->UseIcon(audio_img, audio_ani);
  sfxItem->SetValueRange(1, 4);
  primaryMenu.push_back(std::move(sfxItem));

  // Shaders
  shaderLevel = configSettings.GetShaderLevel();
  auto shadersItem = std::make_shared<NumberItem>(
    "Shaders",
    sf::Color(10, 165, 255),
    shaderLevel,
    [this](int shaderLevel, NumberItem& item) { UpdateShaderLevel(shaderLevel, item); });
  shadersItem->SetValueRange(0, 1);
  //shadersItem->SetColor(DISABLED_TEXT_COLOR);
  primaryMenu.push_back(shadersItem);

  // Keyboard
  primaryMenu.push_back(std::make_shared<TextItem>(
    "MY KEYBOARD",
    [this](auto&) { ShowKeyboardMenu(); })
  );
  // Gamepad
  primaryMenu.push_back(std::make_shared<TextItem>(
    "MY GAMEPAD",
    [this](auto&) { ShowGamepadMenu(); })
  );
  // Minimap
  invertMinimap = configSettings.GetInvertMinimap();
  primaryMenu.push_back(std::make_shared<TextItem>(
    invertMinimap ? "INVRT MAP: yes" : "INVRT MAP: no",
    [this](TextItem& item) { InvertMinimap(item); },
    [this](TextItem& item) { InvertMinimap(item); }
  ));
  // Nickname
  this->nicknameItem = std::make_shared<NicknameItem>(
    [this] {
      this->inputInterface = new MessageInput(this->nicknameItem->GetNick(), 9u);
      this->inputInterface->SetHint("Type new nickname");
      this->textbox.EnqueMessage(this->inputInterface);
      this->textbox.Open();
    }
  );

  std::string nickname = getController().Session().GetNick();
  if (!nickname.empty()) {
    nicknameItem->SetNick(nickname);
  }

  primaryMenu.push_back(nicknameItem);

  // For keyboard keys
  auto keyCallback = [this](BindingItem& item) { AwaitKeyBinding(item); };
  auto keySecondaryCallback = [this](BindingItem& item) { UnsetKeyBinding(item); };

  for (auto eventName : InputEvents::KEYS) {
    std::optional<std::reference_wrapper<std::string>> value;
    std::string keyStr;

    auto key = configSettings.GetPairedInput(eventName);

    if (Input().ConvertKeyToString(key, keyStr)) {
      keyHash.insert(std::make_pair(key, eventName));
      value = keyStr;
    }

    keyboardMenu.push_back(std::make_shared<BindingItem>(eventName, value, keyCallback, keySecondaryCallback));
  }

  // Adjusting gamepad index (abusing BindingItem for alignment)
  gamepadIndex = configSettings.GetGamepadIndex();
  auto gamepadIndexString = std::to_string(gamepadIndex);
  gamepadMenu.push_back(std::make_shared<BindingItem>(
    "Gamepad Index",
    gamepadIndexString,
    [this](BindingItem& item) { IncrementGamepadIndex(item); },
    [this](BindingItem& item) { DecrementGamepadIndex(item); }
  ));

  invertThumbstick = configSettings.GetInvertThumbstick();
  std::string invertThumbstickString = invertThumbstick ? "yes" : "no";
  gamepadMenu.push_back(std::make_shared<BindingItem>(
    "Invert Thumbstick",
    invertThumbstickString,
    [this](BindingItem& item) { InvertThumbstick(item); },
    [this](BindingItem& item) { InvertThumbstick(item); }
  ));

  // For gamepad keys
  auto gamepadCallback = [this](BindingItem& item) { AwaitGamepadBinding(item); };
  auto gamepadSecondaryCallback = [this](BindingItem& item) { UnsetGamepadBinding(item); };

  for (auto eventName : InputEvents::KEYS) {
    std::optional<std::reference_wrapper<std::string>> value;
    std::string valueString;

    auto gamepadCode = configSettings.GetPairedGamepadButton(eventName);
    gamepadHash.insert(std::make_pair(gamepadCode, eventName));

    if (gamepadCode != Gamepad::BAD_CODE) {
      valueString = "BTN " + std::to_string((int)gamepadCode);

      switch (gamepadCode) {
      case Gamepad::DOWN:
        valueString = "-Y AXIS";
        break;
      case Gamepad::UP:
        valueString = "+Y AXIS";
        break;
      case Gamepad::LEFT:
        valueString = "-X AXIS";
        break;
      case Gamepad::RIGHT:
        valueString = "+X AXIS";
        break;
      case Gamepad::BAD_CODE:
        valueString = "BAD_CODE";
        break;
      }

      value = valueString;
    }

    gamepadMenu.push_back(std::make_shared<BindingItem>(
      eventName,
      value,
      gamepadCallback,
      gamepadSecondaryCallback
      ));
  }

  nextScrollCooldown = INITIAL_SCROLL_COOLDOWN;
  setView(sf::Vector2u(480, 320));
}

ConfigScene::~ConfigScene() {
  delete bg;
}

void ConfigScene::UpdateBgmVolume(int volumeLevel) {
  musicLevel = volumeLevel;
  Audio().SetStreamVolume(((musicLevel-1) / 3.0f) * 100.0f);
}

void ConfigScene::UpdateSfxVolume(int volumeLevel) {
  sfxLevel = volumeLevel;
  Audio().SetChannelVolume(((sfxLevel-1) / 3.0f) * 100.0f);
  Audio().Play(AudioType::BUSTER_PEA);
}

void ConfigScene::UpdateShaderLevel(int shaderLevel, NumberItem& item) {
  this->shaderLevel = shaderLevel;

  if (shaderLevel == 0) {
    item.SetString("Shaders: OFF");
    item.SetColor(sf::Color::Red);
  }
  else {
    item.SetString("Shaders: ON");
    item.SetColor(sf::Color(10, 165, 255));
  }

  Audio().Play(AudioType::CHIP_SELECT);
}

void ConfigScene::ShowKeyboardMenu() {
  activeSubmenu = keyboardMenu;
  Audio().Play(AudioType::CHIP_SELECT);
}

void ConfigScene::ShowGamepadMenu() {
  activeSubmenu = gamepadMenu;
  Audio().Play(AudioType::CHIP_SELECT);
}

void ConfigScene::AwaitKeyBinding(BindingItem& item) {
  pendingKeyBinding = item;
}

void ConfigScene::UnsetKeyBinding(BindingItem& item) {
  auto& eventName = item.GetString();
  auto iter = keyHash.begin();

  while (iter != keyHash.end()) {
    if (iter->second == eventName) break;
    iter++;
  }

  if (iter != keyHash.end()) {
    keyHash.erase(iter);
  }

  item.SetValue({});
}

void ConfigScene::AwaitGamepadBinding(BindingItem& item) {
  pendingGamepadBinding = item;

  // disable gamepad so we can escape binding in case the gamepad is not working or not plugged in
  Input().UseGamepadControls(false);
}

void ConfigScene::UnsetGamepadBinding(BindingItem& item) {
  auto& eventName = item.GetString();
  auto iter = gamepadHash.begin();

  while (iter != gamepadHash.end()) {
    if (iter->second == eventName) break;
    iter++;
  }

  if (iter != gamepadHash.end()) {
    gamepadHash.erase(iter);
  }

  item.SetValue({});
}

void ConfigScene::IncrementGamepadIndex(BindingItem& item) {
  if (++gamepadIndex >= Input().GetGamepadCount()) {
    gamepadIndex = 0;
  }

  // set gamepad now to allow binding it's input
  Input().UseGamepad(gamepadIndex);

  auto indexString = std::to_string(gamepadIndex);
  item.SetValue(indexString);
}

void ConfigScene::DecrementGamepadIndex(BindingItem& item) {
  if (--gamepadIndex < 0) {
    gamepadIndex = Input().GetGamepadCount() > 0 ? int(Input().GetGamepadCount()) - 1 : 0;
  }

  // set gamepad now to allow binding it's input
  Input().UseGamepad(gamepadIndex);

  auto indexString = std::to_string(gamepadIndex);
  item.SetValue(indexString);
}

void ConfigScene::InvertThumbstick(BindingItem& item) {
  invertThumbstick = !invertThumbstick;
  Input().SetInvertThumbstick(invertThumbstick);
  std::string valueText = invertThumbstick ? "yes" : "no";
  item.SetValue(valueText);
}

void ConfigScene::InvertMinimap(TextItem& item) {
  invertMinimap = !invertMinimap;
  item.SetString(invertMinimap ? "INVRT MAP: yes" : "INVRT MAP: no");
}

bool ConfigScene::IsInSubmenu() {
  return activeSubmenu.has_value();
}

ConfigScene::Menu& ConfigScene::GetActiveMenu() {
  return IsInSubmenu() ? activeSubmenu->get() : primaryMenu;
}

int& ConfigScene::GetActiveIndex() {
  return IsInSubmenu() ? submenuIndex : primaryIndex;
}

void ConfigScene::onUpdate(double elapsed)
{
  textbox.Update(elapsed);
  bg->Update((float)elapsed);

  bool hasConfirmed = Input().Has(InputEvents::pressed_confirm);
  bool hasSecondary = Input().Has(InputEvents::pressed_option);

  if (hasConfirmed && isSelectingTopMenu && !leave) {
    if (textbox.IsClosed()) {
      // Save data
      auto onYes = [this]() {
        // backup keyboard hash in case the current hash is invalid
        auto oldKeyboardHash = configSettings.GetKeyboardHash();

        // Save before leaving
        configSettings.SetKeyboardHash(keyHash);

        if (!configSettings.TestKeyboard()) {
          // revert
          configSettings.SetKeyboardHash(oldKeyboardHash);

          // block exit with warning
          messageInterface = new Message("Keyboard has overlapping or unset UI bindings.");
          textbox.EnqueMessage(sf::Sprite(), "", messageInterface);
          Audio().Play(AudioType::CHIP_ERROR, AudioPriority::high);
          return;
        }

        configSettings.SetGamepadIndex(gamepadIndex);
        configSettings.SetGamepadHash(gamepadHash);
        configSettings.SetInvertThumbstick(invertThumbstick);
        configSettings.SetMusicLevel(musicLevel);
        configSettings.SetSFXLevel(sfxLevel);
        configSettings.SetShaderLevel(shaderLevel);
        configSettings.SetInvertMinimap(invertMinimap);

        ConfigWriter writer(configSettings);
        writer.Write("config.ini");
        ConfigReader reader("config.ini");
        getController().UpdateConfigSettings(reader.GetConfigSettings());

        std::string nickname = "Anon";
        if (!nicknameItem->GetNick().empty()) {
          nickname = nicknameItem->GetNick();
        }

        getController().Session().SetNick(nickname);

        // transition to the next screen
        using namespace swoosh::types;
        using effect = segue<WhiteWashFade, milliseconds<300>>;
        getController().pop<effect>();

        Audio().Play(AudioType::NEW_GAME);
        leave = true;
      };

      auto onNo = [this]() {
        // Revert gamepad settings
        Input().UseGamepad(configSettings.GetGamepadIndex());
        Input().SetInvertThumbstick(configSettings.GetInvertThumbstick());

        // Just close and leave
        using namespace swoosh::types;
        using effect = segue<WhiteWashFade, milliseconds<300>>;
        getController().pop<effect>();
        leave = true;
      };
      questionInterface = new Question("Overwrite your config settings?", onYes, onNo);
      textbox.EnqueMessage(sf::Sprite(), "", questionInterface);
      textbox.Open();
      Audio().Play(AudioType::CHIP_DESC);
    }
  }

  auto& activeIndex = GetActiveIndex();
  auto initialIndex = activeIndex;
  bool hasCanceled = false;
  bool hasUp = false;
  bool hasDown = false;
  bool hasLeft = false;
  bool hasRight = false;

  if (!leave) {
    hasCanceled = Input().Has(InputEvents::pressed_cancel);
    hasUp = Input().Has(InputEvents::held_ui_up);
    hasDown = Input().Has(InputEvents::held_ui_down);
    hasLeft = Input().Has(InputEvents::pressed_ui_left);
    hasRight = Input().Has(InputEvents::pressed_ui_right);

    if (!hasUp && !hasDown) {
      nextScrollCooldown = INITIAL_SCROLL_COOLDOWN;
      scrollCooldown = 0.0f;
    }

    if (!pendingGamepadBinding && !gamepadButtonHeld) {
      // re-enable gamepad if it was on, and only if the gamepad does not have a button held down
      // this is to prevent effecting the ui the frame after binding a previous ui binding
      Input().UseGamepadControls(gamepadWasActive);
    }

    if (textbox.IsOpen()) {
      if (messageInterface) {
        if (hasConfirmed) {
          // continue the conversation if the text is complete
          if (textbox.IsEndOfMessage()) {
            textbox.DequeMessage();
            messageInterface = nullptr;
          }
          else if (textbox.IsEndOfBlock()) {
            textbox.ShowNextLines();
          }
          else {
            // double tapping talk will complete the block
            textbox.CompleteCurrentBlock();
          }
        }
      }
      else if (questionInterface) {
        if (textbox.IsEndOfMessage()) {
          if (hasLeft) {
            questionInterface->SelectYes();
          }
          else if (hasRight) {
            questionInterface->SelectNo();
          }
          else if (hasCanceled) {
            questionInterface->Cancel();
            questionInterface = nullptr;
          }
          else if (hasConfirmed) {
            questionInterface->ConfirmSelection();
            questionInterface = nullptr;
          }
        }
        else if (hasConfirmed) {
          if (!textbox.IsPlaying()) {
            questionInterface->Continue();
          }
          else {
            textbox.CompleteCurrentBlock();
          }
        }
      }
      else if (inputInterface) {
        if (inputInterface->IsDone()) {
          // only inputInterface used is for nicknames at this time
          const std::string name = inputInterface->Submit();
          nicknameItem->SetNick(name);
          nicknameItem->SetString(name);
          inputInterface = nullptr;
        }
      }

      if (textbox.IsEndOfMessage() && !textbox.HasMessage()) {
        textbox.Close();
      }
    }
    else if (pendingKeyBinding || pendingGamepadBinding) {
      if (pendingKeyBinding) {
        auto key = Input().GetAnyKey();

        if (key != sf::Keyboard::Unknown) {
          std::string boundKey = "";

          if (Input().ConvertKeyToString(key, boundKey)) {
            auto& menuItem = pendingKeyBinding->get();
            auto& eventName = menuItem.GetString();

            UnsetKeyBinding(menuItem);

            keyHash.insert(std::make_pair(key, eventName));

            std::transform(boundKey.begin(), boundKey.end(), boundKey.begin(), ::toupper);

            menuItem.SetValue(boundKey);

            Audio().Play(AudioType::CHIP_DESC_CLOSE);

            pendingKeyBinding = {};

            // add a scroll cooldown to prevent the ui from moving next frame
            scrollCooldown = INITIAL_SCROLL_COOLDOWN;
          }
        }
      }

      if (hasCanceled) {
        // gamepad input is off, this runs if you hit the keyboard binded cancel button
        pendingGamepadBinding = {};
      }
      else if (pendingGamepadBinding) {
        // GAMEPAD
        auto gamepad = Input().GetAnyGamepadButton();

        if (gamepad != (Gamepad)-1 && !gamepadButtonHeld) {
          auto& menuItem = pendingGamepadBinding->get();
          auto& eventName = menuItem.GetString();

          UnsetGamepadBinding(menuItem);

          gamepadHash.insert(std::make_pair(gamepad, eventName));

          std::string label = "BTN " + std::to_string((int)gamepad);

          switch (gamepad) {
          case Gamepad::DOWN:
            label = "-Y AXIS";
            break;
          case Gamepad::UP:
            label = "+Y AXIS";
            break;
          case Gamepad::LEFT:
            label = "-X AXIS";
            break;
          case Gamepad::RIGHT:
            label = "+X AXIS";
            break;
          case Gamepad::BAD_CODE:
            label = "BAD_CODE";
            break;
          }

          menuItem.SetValue(label);

          Audio().Play(AudioType::CHIP_DESC_CLOSE);

          pendingGamepadBinding = {};
          // we dont need a scroll cooldown
          // the gamepad will be disabled until we release all buttons
        }
      }
    }
    else if (hasCanceled || (IsInSubmenu() && hasLeft)) {
      if (IsInSubmenu()) {
        activeSubmenu = {};
        submenuIndex = 0;
      }
      else {
        isSelectingTopMenu = true;
        primaryIndex = 0;
      }

      Audio().Play(AudioType::CHIP_DESC_CLOSE);
    }
    else if (hasUp && scrollCooldown <= 0.0f) {
      if (activeIndex == 0 && !isSelectingTopMenu) {
        isSelectingTopMenu = true;
      }
      else {
        activeIndex--;
      }
    }
    else if (hasDown && scrollCooldown <= 0.0f) {
      if (isSelectingTopMenu) {
        isSelectingTopMenu = false;
      }
      else {
        activeIndex++;
      }
    }
    else if ((hasConfirmed || (!IsInSubmenu() && hasRight)) && !isSelectingTopMenu) {
      auto& activeMenu = GetActiveMenu();
      activeMenu[GetActiveIndex()]->Select();
    }
    else if ((hasSecondary || (!IsInSubmenu() && hasLeft)) && !isSelectingTopMenu) {
      auto& activeMenu = GetActiveMenu();
      activeMenu[GetActiveIndex()]->SecondarySelect();
    }
  }

  if (scrollCooldown > 0.0f) {
    scrollCooldown -= float(elapsed);
  }

  primaryIndex = std::clamp(primaryIndex, 0, int(primaryMenu.size()) - 1);


  UpdateMenu(primaryMenu, !IsInSubmenu(), primaryIndex, 0, float(elapsed));

  // display submenu, currently just keyboard/gamepad binding
  if (IsInSubmenu()) {
    auto& submenu = activeSubmenu->get();
    submenuIndex = std::clamp(submenuIndex, 0, int(submenu.size()) - 1);

    UpdateMenu(submenu, true, submenuIndex, SUBMENU_SPAN, float(elapsed));
  }

  if (activeIndex != initialIndex) {
    scrollCooldown = nextScrollCooldown;
    nextScrollCooldown = SCROLL_COOLDOWN;

    if (!hasCanceled) {
      Audio().Play(AudioType::CHIP_SELECT);
    }
  }

  // Make endBtn stick at the top of the screen
  endBtn.setPosition(endBtn.getPosition().x, primaryMenu[0]->getPosition().y - 60);

  if (isSelectingTopMenu) {
    endBtnAnimator.SetFrame(2, endBtn);
  }
  else {
    endBtnAnimator.SetFrame(1, endBtn);
  }

  // move view based on selection (keep selection in view)
  auto menuSize = GetActiveMenu().size();
  auto selectionIndex = GetActiveIndex();

  auto scrollEnd = std::max(0.0f, float(menuSize + 1) * LINE_SPAN - getView().getSize().y / 2.0f + MENU_START_Y);
  auto scrollIncrement = scrollEnd / float(menuSize);
  auto newScrollOffset = -float(selectionIndex) * scrollIncrement;
  scrollOffset = swoosh::ease::interpolate(float(elapsed) * SCROLL_INTERPOLATION_MULTIPLIER, scrollOffset, newScrollOffset);

  gamepadButtonHeld = Input().GetAnyGamepadButton() != (Gamepad)-1;
}

void ConfigScene::UpdateMenu(Menu& menu, bool menuHasFocus, int selectionIndex, float offsetX, float elapsed) {
  if (isSelectingTopMenu && GetActiveMenu() == menu) {
    selectionIndex = -1;
  }

  for (int index = 0; index < menu.size(); index++) {
    auto& menuItem = *menu[index];
    auto w = 0.3f;
    auto diff = index - selectionIndex;
    float scale = 1.0f - (w * abs(diff));
    scale = std::max(scale, 0.8f);
    scale = std::min(scale, 1.0f);

    auto delta = 48.0f * elapsed;

    auto s = sf::Vector2f(2.f * scale, 2.f * scale);
    auto menuItemScale = menuItem.getScale().x;
    auto slerp = sf::Vector2f(
      swoosh::ease::interpolate(delta, s.x, menuItemScale),
      swoosh::ease::interpolate(delta, s.y, menuItemScale)
    );
    menuItem.setScale(slerp);

    menuItem.setPosition(COL_PADDING + 2.f * offsetX, 2.f * (MENU_START_Y + (index * LINE_SPAN)));

    bool awaitingBinding = pendingKeyBinding || pendingGamepadBinding;

    if (menuHasFocus) {
      if (index == selectionIndex) {
        menuItem.SetAlpha(255);
      }
      else {
        menuItem.SetAlpha(awaitingBinding ? 50 : 150);
      }
    }
    else {
      menuItem.SetAlpha(awaitingBinding ? 50 : 100);
    }

    menuItem.Update();
  }
}

void ConfigScene::onDraw(IRenderer& renderer)
{
  renderer.submit(bg);

  // scrolling the view
  auto states = sf::RenderStates::Default;
  states.transform.translate(0, 2.0f * scrollOffset);

  renderer.submit(&endBtn, states);

  for (auto& menuItem : primaryMenu) {
    renderer.submit(menuItem.get(), states);
  }

  if (activeSubmenu) {
    auto& submenu = activeSubmenu->get();
    for (auto& menuItem : submenu) {
      renderer.submit(menuItem.get(), states);
    }
  }

  renderer.submit(&textbox);
}

void ConfigScene::onStart()
{
  Audio().Stream("resources/loops/config.ogg", false);
}

void ConfigScene::onLeave()
{
  Audio().StopStream();
}

void ConfigScene::onExit()
{
  ConfigSettings& config = getController().ConfigSettings();

  const int music = config.GetMusicLevel();
  const int sfx = config.GetSFXLevel();

  Audio().SetStreamVolume(((music - 1) / 3.0f) * 100.0f);
  Audio().SetChannelVolume(((sfxLevel - 1) / 3.0f) * 100.0f);
}

void ConfigScene::onEnter()
{
  Audio().StopStream();
}

void ConfigScene::onResume()
{
}

void ConfigScene::onEnd()
{
}
