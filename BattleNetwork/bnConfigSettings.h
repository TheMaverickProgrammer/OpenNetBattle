#pragma once
#include <string>
#include <map>
#include <list>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Joystick.hpp>

struct DiscordInfo {
  std::string user;
  std::string key;
};

struct WebServerInfo {
  std::string user;
  std::string password;
  std::string URL;
  std::string version;
  int port{};
};

/*! \brief easy to cast in with some special codes for joystick x/y axis */
enum class Gamepad : signed { BAD_CODE = -1, UP = 5555, LEFT = 5556, RIGHT = 5557, DOWN = 5558 };

struct ConfigSettings {
public:
  friend class ConfigReader;

  typedef std::multimap<sf::Keyboard::Key, std::string> KeyboardHash;
  typedef std::multimap<Gamepad, std::string> GamepadHash;

  /**
   * @brief If config file is ok
   * @return true if wellformed, false otherwise
   */
  const bool IsOK() const;

  /**
   * @brief Tests keyboard mappings for issues
   * @return true if keyboard mapping are usable
   */
  const bool TestKeyboard() const;

  /**
   * @brief Check if Audio() is on or off based on ini file
   * @return true if on, false otherwise
   */
  const bool IsAudioEnabled() const;

  /**
   * @brief Get fullscreen mode
   */
  const bool IsFullscreen() const;

  const int GetMusicLevel() const;
  const int GetSFXLevel() const;
  void SetMusicLevel(int level);
  void SetSFXLevel(int level);

  /**
   * @brief Get the active gamepad index
   */
  int GetGamepadIndex() const;

  /**
   * @brief Set the active gamepad index
   */
  void SetGamepadIndex(int index);

  /**
  * @brief Set the keyboard game bindings
  */
  void SetKeyboardHash(const KeyboardHash key);

  /**
  * @brief Set the gamepad game bindings
  */
  void SetGamepadHash(const GamepadHash gamepad);

  /**
  * @brief Set the web server info
  */
  void SetWebServerInfo(const std::string& url, int port, const std::string& version);

  /**
   * @brief Get inverted thumbstick setting
   */
  bool GetInvertThumbstick() const;

  /**
   * @brief Set inverted thumbstick setting
   */
  void SetInvertThumbstick(bool invert);

  /**
   * @brief Get inverted minimap setting
   */
  bool GetInvertMinimap() const;

  /**
   * @brief Set inverted minimap setting
   */
  void SetInvertMinimap(bool invert);

  /**
   * @brief For a keyboard event, return the action string
   * @param event sfml keyboard key
   * @return the mapped input event
   */
  const std::list<std::string> GetPairedActions(const sf::Keyboard::Key& event) const;

  /**
   * @brief For an action string, return the bound keyboard key
   * @param action name
   * @return the bound key
   */
  const sf::Keyboard::Key GetPairedInput(std::string action) const;

  /**
   * @brief For an action string, return the bound gamepad event
   * @param action name
   * @return the bound gamepad event
   */
  const Gamepad GetPairedGamepadButton(std::string action) const;

  /**
   * @brief For a gamepad event, return the action string
   * @param Gamepad button
   * @return list of the mapped input events
   */
  const std::list<std::string> GetPairedActions(const Gamepad& event) const;

  ConfigSettings& operator=(const ConfigSettings& rhs);

  const DiscordInfo& GetDiscordInfo() const;
  const WebServerInfo& GetWebServerInfo() const;

  const KeyboardHash& GetKeyboardHash();
  const GamepadHash& GetGamepadHash();

  ConfigSettings(const ConfigSettings& rhs);
  ConfigSettings();

private:
  // Config values
  // Map keys to actions
  KeyboardHash keyboard; /*!< Keyboard key to event */
  GamepadHash gamepad; /*!< Gamepad button to event */
  DiscordInfo discord; /*!< account info to allow alerts on discord channel */
  WebServerInfo webServer; /*!< account info that hosts cards and folders */

  int musicLevel{ 3 };
  int sfxLevel{ 3 };
  int gamepadIndex{};
  bool invertThumbstick{};

  bool fullscreen{};

  bool invertMinimap{};

  // State flags
  bool isOK{}; /*!< true if the file was ok */
};
