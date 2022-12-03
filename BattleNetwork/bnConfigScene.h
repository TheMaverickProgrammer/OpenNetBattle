#pragma once
#include "overworld/bnOverworldHomepage.h"
#include "bnCamera.h"
#include "bnInputManager.h"
#include "bnAudioResourceManager.h"
#include "bnShaderResourceManager.h"
#include "bnTextureResourceManager.h"
#include "bnGame.h"
#include "bnAnimation.h"
#include "bnConfigSettings.h"
#include "bnConfigWriter.h"
#include "bnAnimatedTextBox.h"
#include "bnMessageQuestion.h"
#include "bnMessageInput.h"
#include <SFML/Graphics.hpp>
#include <time.h>

class ConfigScene : public Scene {
private:
  ConfigSettings configSettings;
  ConfigSettings::KeyboardHash keyHash;
  ConfigSettings::GamepadHash gamepadHash;
  int gamepadIndex, musicLevel, sfxLevel, shaderLevel;
  bool invertThumbstick, invertMinimap;
  bool leave{};
  bool isSelectingTopMenu{};
  bool inLoginMenu{};
  bool gamepadWasActive{};
  bool gamepadButtonHeld{};
  int primaryIndex{}; /*!< Current selection */
  int submenuIndex{};
  float scrollOffset{};
  float scrollCooldown{};
  float nextScrollCooldown{};
  AnimatedTextBox textbox;

  // ui sprite maps
  Animation endBtnAnimator;
  sf::Sprite endBtn;
  Background* bg{ nullptr };

  class MenuItem : public SceneNode {
  private:
    std::function<void()> action, secondaryAction;
  public:
    MenuItem(const std::function<void()>& callback, const std::function<void()>& secondaryCallback) {
      action = callback;
      secondaryAction = secondaryCallback;
    }

    virtual void SetAlpha(sf::Uint8) = 0;
    void Select() { action(); };
    void SecondarySelect() { secondaryAction(); };
    virtual void Update() {};
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
  };

  class TextItem : public MenuItem {
  protected:
    inline static const std::function<void(TextItem&)>& defaultSecondaryCallback = [](auto&) {};
    std::shared_ptr<Text> label;
    sf::Color color;
  public:
    TextItem(
      const std::string& text,
      const std::function<void(TextItem&)>& callback,
      const std::function<void(TextItem&)>& secondaryCallback = defaultSecondaryCallback
    );
    const std::string& GetString();
    void SetString(const std::string& text);
    void SetColor(sf::Color);
    void SetAlpha(sf::Uint8) override;
  };

  class NicknameItem : public TextItem {
    std::string nickname;

  public:
    NicknameItem(const std::function<void()>& callback);
    void Update() override;
    void SetNick(const std::string& nickname);
    std::string GetNick();
  };

  class BindingItem : public TextItem {
  private:
    std::shared_ptr<Text> valueText;
    sf::Color valueColor;
  public:
    BindingItem(
      const std::string& inputName,
      std::optional<std::reference_wrapper<std::string>> valueName,
      const std::function<void(BindingItem&)>& callback,
      const std::function<void(BindingItem&)>& secondaryCallback
    );
    void SetValue(std::optional<std::reference_wrapper<std::string>> valueName);
    void SetAlpha(sf::Uint8 alpha) override;
    void Update() override;
  };

  class NumberItem : public TextItem, ResourceHandle {
  private:
    std::shared_ptr<SpriteProxyNode> icon;
    Animation animator;
    int value{}, maxValue{}, minValue{};
    std::function<void(int, NumberItem&)> refreshCallback;
    std::function<void(TextItem&)> createCallback(const std::function<void(int, NumberItem&)>&);
    std::function<void(TextItem&)> createSecondaryCallback(const std::function<void(int, NumberItem&)>&);
  public:
    NumberItem(const std::string& text, sf::Color color, int value, const std::function<void(int, NumberItem&)>& callback);
    void SetAlpha(sf::Uint8 alpha) override;
    void UseIcon(const std::filesystem::path& image_path, const std::filesystem::path& animation_path, const std::string& state = "DEFAULT");
    void SetValueRange(int min, int max);
  };

  using Menu = std::vector<std::shared_ptr<MenuItem>>;

  Menu primaryMenu, keyboardMenu, gamepadMenu;
  std::optional<std::reference_wrapper<Menu>> activeSubmenu;
  std::optional<std::reference_wrapper<BindingItem>> pendingKeyBinding;
  std::optional<std::reference_wrapper<BindingItem>> pendingGamepadBinding;

  Question* questionInterface{ nullptr };
  MessageInput* inputInterface{ nullptr };
  Message* messageInterface{ nullptr };
  std::shared_ptr<NicknameItem> nicknameItem;

#ifdef __ANDROID__
  void StartupTouchControls();
  void ShutdownTouchControls();
#endif
  void UpdateBgmVolume(int volumeLevel);
  void UpdateSfxVolume(int volumeLevel);
  void UpdateShaderLevel(int shaderLevel,NumberItem& item);
  void ShowKeyboardMenu();
  void ShowGamepadMenu();
  void AwaitKeyBinding(BindingItem&);
  void UnsetKeyBinding(BindingItem&);
  void AwaitGamepadBinding(BindingItem&);
  void UnsetGamepadBinding(BindingItem&);
  void IncrementGamepadIndex(BindingItem&);
  void DecrementGamepadIndex(BindingItem&);
  void InvertThumbstick(BindingItem&);
  void InvertMinimap(TextItem&);

  bool IsInSubmenu();
  Menu& GetActiveMenu();
  int& GetActiveIndex();
  void UpdateMenu(Menu&, bool menuHasFocus, int selectionIndex, float colSpan, float elapsed);

public:

  /**
   * @brief Load's the joystick config file
   */
  ConfigScene(swoosh::ActivityController&);
  ~ConfigScene();

  /**
   * @brief Checks input events and listens for select buttons. Segues to new screens.
   * @param elapsed in seconds
   */
  void onUpdate(double elapsed) override;

  /**
   * @brief Draws the UI
   * @param surface
   */
  void onDraw(IRenderer& renderer) override;

  /**
   * @brief Stops music, plays new track, reset the camera
   */
  void onStart() override;

  /**
   * @brief Music fades out
   */
  void onLeave() override;

  /**
   * @brief Does nothing
   */
  void onExit() override;

  /**
   * @brief Does nothing
   */
  void onEnter() override;

  /**
   * @brief Does nothing
   */
  void onResume() override;

  /**
   * @brief Stops the music
   */
  void onEnd() override;
};
