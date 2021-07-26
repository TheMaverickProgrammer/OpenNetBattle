#include "bnConfigScene.h"
#include "bnWebClientMananger.h"
#include "Segues/WhiteWashFade.h"
#include "bnRobotBackground.h"

#undef GetUserName

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
  label(text, Font::Style::wide),
  color(DEFAULT_TEXT_COLOR)
{
  AddNode(&label);
}

const std::string& ConfigScene::TextItem::GetString() {
  return label.GetString();
}

void ConfigScene::TextItem::SetString(const std::string& text) {
  label.SetString(text);
}

void ConfigScene::TextItem::SetColor(sf::Color color) {
  this->color = color;
  label.SetColor(color);
}

void ConfigScene::TextItem::SetAlpha(sf::Uint8 alpha) {
  color.a = alpha;
  label.SetColor(color);
}

ConfigScene::LoginItem::LoginItem(const std::function<void()>& callback) : TextItem("Login", [callback](auto&) { callback(); }) {}

void ConfigScene::LoginItem::Update() {
  if (!WEBCLIENT.IsLoggedIn()) {
    SetString("LOGIN");
  }
  else {
    SetString("LOGOUT " + WEBCLIENT.GetUserName());
  }
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
  value(Font::Style::wide)
{
  SetValue(valueName);
  AddNode(&value);
}

void ConfigScene::BindingItem::SetValue(std::optional<std::reference_wrapper<std::string>> valueName) {
  if (valueName) {
    valueColor = DEFAULT_TEXT_COLOR;
    value.SetString(valueName->get());
  }
  else {
    valueColor = NO_BINDING_COLOR;
    value.SetString("NO KEY");
  }
}

void ConfigScene::BindingItem::SetAlpha(sf::Uint8 alpha) {
  valueColor.a = alpha;
  value.SetColor(valueColor);
  TextItem::SetAlpha(alpha);
}

void ConfigScene::BindingItem::Update() {
  value.setPosition((BINDED_VALUE_OFFSET * 2.0f) / getScale().x - value.GetLocalBounds().width, 0.0f);
}

ConfigScene::VolumeItem::VolumeItem(
  const std::string& text,
  sf::Color color,
  int volumeLevel,
  const std::function<void(int)>& callback
) :
  TextItem(text, createCallback(callback), createSecondaryCallback(callback)),
  volumeLevel(volumeLevel)
{
  this->color = color;

  AddNode(&icon);
  icon.setTexture(Textures().LoadTextureFromFile("resources/ui/config/audio.png"));
  icon.setPosition(
    label.GetLocalBounds().width + COL_PADDING,
    label.GetLocalBounds().height / 2.0f - icon.getLocalBounds().height / 2.0f
  );

  animator = Animation("resources/ui/config/audio.animation");
  animator.Load();
  animator.SetAnimation("DEFAULT");
  animator.SetFrame(volumeLevel + 1, icon.getSprite());
  animator.Update(0, icon.getSprite());
}

std::function<void(ConfigScene::TextItem&)> ConfigScene::VolumeItem::createCallback(const std::function<void(int)>& callback) {
  // raise volume
  return [this, callback](auto&) {
    volumeLevel = (volumeLevel + 1) % 4;
    animator.SetFrame(volumeLevel + 1, icon.getSprite());
    callback(volumeLevel);
  };
}

std::function<void(ConfigScene::TextItem&)> ConfigScene::VolumeItem::createSecondaryCallback(const std::function<void(int)>& callback) {
  // lower volume
  return [this, callback](auto&) {
    volumeLevel = volumeLevel - 1;

    if (volumeLevel < 0) {
      volumeLevel = 3;
    }

    animator.SetFrame(volumeLevel + 1, icon.getSprite());
    callback(volumeLevel);
  };
}

void ConfigScene::VolumeItem::SetAlpha(sf::Uint8 alpha) {
  TextItem::SetAlpha(alpha);
  icon.setColor(color);
}

ConfigScene::ConfigScene(swoosh::ActivityController& controller) :
  textbox(sf::Vector2f(4, 250)),
  Scene(controller)
{
  configSettings = Input().GetConfigSettings();
  gamepadWasActive = Input().IsUsingGamepadControls();
  textbox.SetTextSpeed(2.0);
  isSelectingTopMenu = false;

  // Draws the scrolling background
  bg = new RobotBackground();

  // dim
  bg->setColor(sf::Color(120, 120, 120));

  endBtnAnimator = Animation("resources/ui/config/end_btn.animation");
  endBtnAnimator.Load();

  // end button
  endBtn = sf::Sprite(*LOAD_TEXTURE(END_BTN));;
  endBtn.setScale(2.f, 2.f);
  endBtnAnimator.SetAnimation("BLINK");
  endBtnAnimator.SetFrame(1, endBtn);
  endBtn.setPosition(2 * 180, 2 * 10);

  // ui sprite maps
  // ascii 58 - 96
  // BGM
  musicLevel = configSettings.GetMusicLevel();
  primaryMenu.push_back(std::make_unique<VolumeItem>(
    "BGM",
    sf::Color(255, 0, 255),
    musicLevel,
    [this](int volumeLevel) { UpdateBgmVolume(volumeLevel); })
  );
  // SFX
  sfxLevel = configSettings.GetSFXLevel();
  primaryMenu.push_back(std::make_unique<VolumeItem>(
    "SFX",
    sf::Color(10, 165, 255),
    sfxLevel,
    [this](int volumeLevel) { UpdateSfxVolume(volumeLevel); })
  );
  // Shaders
  auto shadersItem = std::make_unique<TextItem>("SHADERS: ON", [this](auto&) { ToggleShaders(); });
  shadersItem->SetColor(DISABLED_TEXT_COLOR);
  primaryMenu.push_back(std::move(shadersItem));
  // Keyboard
  primaryMenu.push_back(std::make_unique<TextItem>(
    "MY KEYBOARD",
    [this](auto&) { ShowKeyboardMenu(); })
  );
  // Gamepad
  primaryMenu.push_back(std::make_unique<TextItem>(
    "MY GAMEPAD",
    [this](auto&) { ShowGamepadMenu(); })
  );
  // Minimap
  invertMinimap = configSettings.GetInvertMinimap();
  primaryMenu.push_back(std::make_unique<TextItem>(
    invertMinimap ? "INVRT MAP: yes" : "INVRT MAP: no",
    [this](TextItem& item) { InvertMinimap(item); },
    [this](TextItem& item) { InvertMinimap(item); }
  ));
  // Login
  primaryMenu.push_back(std::make_unique<LoginItem>(
    [this] { ToggleLogin(); })
  );

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

    keyboardMenu.push_back(std::make_unique<BindingItem>(eventName, value, keyCallback, keySecondaryCallback));
  }

  // Adjusting gamepad index (abusing BindingItem for alignment)
  gamepadIndex = configSettings.GetGamepadIndex();
  auto gamepadIndexString = std::to_string(gamepadIndex);
  gamepadMenu.push_back(std::make_unique<BindingItem>(
    "Gamepad Index",
    gamepadIndexString,
    [this](BindingItem& item) { IncrementGamepadIndex(item); },
    [this](BindingItem& item) { DecrementGamepadIndex(item); }
  ));

  invertThumbstick = configSettings.GetInvertThumbstick();
  std::string invertThumbstickString = invertThumbstick ? "yes" : "no";
  gamepadMenu.push_back(std::make_unique<BindingItem>(
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

    gamepadMenu.push_back(std::make_unique<BindingItem>(
      eventName,
      value,
      gamepadCallback,
      gamepadSecondaryCallback
      ));
  }

  nextScrollCooldown = INITIAL_SCROLL_COOLDOWN;
  setView(sf::Vector2u(480, 320));
}

ConfigScene::~ConfigScene() { }

void ConfigScene::UpdateBgmVolume(int volumeLevel) {
  musicLevel = volumeLevel;
  Audio().SetStreamVolume((volumeLevel / 3.0f) * 100.0f);
}

void ConfigScene::UpdateSfxVolume(int volumeLevel) {
  sfxLevel = volumeLevel;
  Audio().SetChannelVolume((volumeLevel / 3.0f) * 100.0f);
  Audio().Play(AudioType::BUSTER_PEA);
}

void ConfigScene::ToggleShaders() {
  // TODO: Shader Toggle
  Audio().Play(AudioType::CHIP_ERROR);
}

void ConfigScene::ShowKeyboardMenu() {
  activeSubmenu = keyboardMenu;
  Audio().Play(AudioType::CHIP_SELECT);
}

void ConfigScene::ShowGamepadMenu() {
  activeSubmenu = gamepadMenu;
  Audio().Play(AudioType::CHIP_SELECT);
}

void ConfigScene::ToggleLogin() {
  if (WEBCLIENT.IsLoggedIn()) {
    if (textbox.IsClosed()) {
      auto onYes = [this]() {
        Logger::Log("SendLogoutCommand");
        WEBCLIENT.SendLogoutCommand();
      };

      auto onNo = [this]() {
        Audio().Play(AudioType::CHIP_DESC_CLOSE);
      };
      questionInterface = new Question("Are you sure you want to logout?", onYes, onNo);
      textbox.EnqueMessage(questionInterface);
      textbox.Open();
      Audio().Play(AudioType::CHIP_DESC);
    }
  }
  else {
    // Begin login state from the beginning
    LoginStep(UserInfo::states::entering_server);
  }
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

  if (user.currState == UserInfo::states::pending) {
    if (is_ready(user.result)) {
      if (user.result.get()) {
        LoginStep(UserInfo::states::complete);
      }
      else {
        user.currState = UserInfo::states::entering_username;
        user.password.clear();
        user.username.clear();

        Audio().Play(AudioType::CHIP_ERROR);
      }
    }

    return;
  }

  bool hasConfirmed = Input().Has(InputEvents::pressed_confirm);
  bool hasSecondary = Input().Has(InputEvents::pressed_option);

  if (hasConfirmed && isSelectingTopMenu && !leave) {
    if (textbox.IsClosed()) {
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
        configSettings.SetInvertMinimap(invertMinimap);
        configSettings.SetWebServerInfo(user.server_url, user.port, user.version);

        ConfigWriter writer(configSettings);
        writer.Write("config.ini");
        ConfigReader reader("config.ini");
        Input().SupportConfigSettings(reader);

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

  if (!leave) {
    bool hasCanceled = Input().Has(InputEvents::pressed_cancel);

    bool hasUp = Input().Has(InputEvents::held_ui_up);
    bool hasDown = Input().Has(InputEvents::held_ui_down);
    bool hasLeft = Input().Has(InputEvents::pressed_ui_left);
    bool hasRight = Input().Has(InputEvents::pressed_ui_right);

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
          std::string entry = inputInterface->Submit();

          inputInterface = nullptr;

          switch (user.currState) {
          case UserInfo::states::entering_server:
            user.server_url = entry;
            LoginStep(UserInfo::states::entering_port);
            break;
          case UserInfo::states::entering_port:
            user.port = std::atoi(entry.c_str());
            LoginStep(UserInfo::states::entering_version_num);
            break;
          case UserInfo::states::entering_version_num:
            user.version = entry;
            LoginStep(UserInfo::states::entering_username);
            break;
          case UserInfo::states::entering_username:
            user.username = entry;
            LoginStep(UserInfo::states::entering_password);
            break;
          case UserInfo::states::entering_password:
            user.password = entry;
            WEBCLIENT.ConnectToWebServer(user.version.c_str(), user.server_url.c_str(), user.port);
            user.result = WEBCLIENT.SendLoginCommand(user.username, user.password);
            LoginStep(UserInfo::states::pending);
            break;
          }
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
    Audio().Play(AudioType::CHIP_SELECT);
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

void ConfigScene::onDraw(sf::RenderTexture& surface)
{
  surface.draw(*bg);

  // scrolling the view
  auto states = sf::RenderStates::Default;
  states.transform.translate(0, 2.0f * scrollOffset);

  surface.draw(endBtn, states);

  for (auto& menuItem : primaryMenu) {
    surface.draw(*menuItem, states);
  }

  if (activeSubmenu) {
    auto& submenu = activeSubmenu->get();
    for (auto& menuItem : submenu) {
      surface.draw(*menuItem, states);
    }
  }

  surface.draw(textbox);
}

void ConfigScene::LoginStep(UserInfo::states next)
{
  if (next == UserInfo::states::pending) return;

  if (next == UserInfo::states::complete) {
    Audio().Play(AudioType::NEW_GAME);
    return;
  }

  user.currState = next;

  inputInterface = new MessageInput("", 20);

  switch(user.currState) {
    case UserInfo::states::entering_server:
    {
      inputInterface->SetHint("Enter Server URL");
      inputInterface->SetCaptureText(configSettings.GetWebServerInfo().URL);
      break;
    }
    case UserInfo::states::entering_port:
    {
      inputInterface->SetHint("Enter Server Port");
      int port = configSettings.GetWebServerInfo().port;

      if (port > 0) {
        inputInterface->SetCaptureText(std::to_string(port));
      }

      break;
    }
    case UserInfo::states::entering_version_num:
    {
      inputInterface->SetHint("Enter Server Version");
      inputInterface->SetCaptureText(configSettings.GetWebServerInfo().version);
      break;
    }
    case UserInfo::states::entering_username:
    {
      inputInterface->SetHint("Enter Username");
      break;
    }
    case UserInfo::states::entering_password: 
    {
      inputInterface->ProtectPassword(true);
      inputInterface->SetHint("Enter Password");
      break;
    }
  }

  textbox.EnqueMessage(inputInterface);
  textbox.Open();
  Audio().Play(AudioType::CHIP_DESC);
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
