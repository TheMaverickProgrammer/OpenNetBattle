#include "bnConfigSettings.h"
#include "bnLogger.h"
#include "bnInputEvent.h"

/**
 * @brief If config file is ok
 * @return true if wellformed, false otherwise
 */
const bool ConfigSettings::IsOK() const { return isOK; }

/**
 * @brief Check if Audio() is on or off based on ini file
 * @return true if on, false otherwise
 */
const bool ConfigSettings::IsAudioEnabled() const { return (musicLevel || sfxLevel); }

const bool ConfigSettings::IsFullscreen() const
{
  return fullscreen;
}

const int ConfigSettings::GetMusicLevel() const { return musicLevel; }

const int ConfigSettings::GetSFXLevel() const { return sfxLevel; }

const bool ConfigSettings::TestKeyboard() const {
  static auto exclusiveEvents = std::vector{
    InputEvents::pressed_ui_up,
    InputEvents::pressed_ui_down,
    InputEvents::pressed_ui_left,
    InputEvents::pressed_ui_right,
    InputEvents::pressed_confirm,
    InputEvents::pressed_cancel,
    InputEvents::pressed_pause,
  };

  std::vector<sf::Keyboard::Key> bindedKeys;

  for (auto& event : exclusiveEvents) {
    auto key = GetPairedInput(event.name);

    auto isUnset = key == sf::Keyboard::Unknown;
    auto isDuplicate = std::find(bindedKeys.begin(), bindedKeys.end(), key) != bindedKeys.end();

    if (isUnset || isDuplicate) {
      return false;
    }

    bindedKeys.push_back(key);
  };

  return true;
}

void ConfigSettings::SetMusicLevel(int level) { musicLevel = level; }

void ConfigSettings::SetSFXLevel(int level) { sfxLevel = level; }

int ConfigSettings::GetGamepadIndex() const { return gamepadIndex; }

void ConfigSettings::SetGamepadIndex(int index) { gamepadIndex = index; }

bool ConfigSettings::GetInvertThumbstick() const { return invertThumbstick; }

void ConfigSettings::SetInvertThumbstick(bool invert) { invertThumbstick = invert; }

bool ConfigSettings::GetInvertMinimap() const { return invertMinimap; }

void ConfigSettings::SetInvertMinimap(bool invert) { invertMinimap = invert; }

const std::list<std::string> ConfigSettings::GetPairedActions(const sf::Keyboard::Key& event) const {
  std::list<std::string> list;

  for (auto& [k, v] : keyboard) {
    if (k == event) {
      list.push_back(v);
    }
  }

  return list;
}

const sf::Keyboard::Key ConfigSettings::GetPairedInput(std::string action) const
{
  for (auto& [k, v] : keyboard) {
    std::string value = v;
    std::transform(value.begin(), value.end(), value.begin(), ::toupper);
    std::transform(action.begin(), action.end(), action.begin(), ::toupper);

    if (value == action) {
      return k;
    }
  }

  return sf::Keyboard::Unknown;
}

const Gamepad ConfigSettings::GetPairedGamepadButton(std::string action) const
{
  for (auto& [k, v] : gamepad) {
    std::string value = v;
    std::transform(value.begin(), value.end(), value.begin(), ::toupper);
    std::transform(action.begin(), action.end(), action.begin(), ::toupper);

    if (value == action) {
      return k;
    }
  }

  return Gamepad::BAD_CODE;
}

const std::list<std::string> ConfigSettings::GetPairedActions(const Gamepad& event) const {
  std::list<std::string> list;

  for (auto& [k, v] : gamepad) {
    if (k == event) {
      list.push_back(v);
    }
  }

  return list;
}

ConfigSettings& ConfigSettings::operator=(const ConfigSettings& rhs)
{
  discord = rhs.discord;
  webServer = rhs.webServer;
  gamepad = rhs.gamepad;
  gamepadIndex = rhs.gamepadIndex;
  invertThumbstick = rhs.invertThumbstick;
  musicLevel = rhs.musicLevel;
  sfxLevel = rhs.sfxLevel;
  isOK = rhs.isOK;
  keyboard = rhs.keyboard;
  fullscreen = rhs.fullscreen;
  invertMinimap = rhs.invertMinimap;
  return *this;
}

const DiscordInfo& ConfigSettings::GetDiscordInfo() const
{
  return discord;
}

const WebServerInfo& ConfigSettings::GetWebServerInfo() const
{
  return webServer;
}

const ConfigSettings::KeyboardHash& ConfigSettings::GetKeyboardHash()
{
  return keyboard;
}

const ConfigSettings::GamepadHash& ConfigSettings::GetGamepadHash()
{
  return gamepad;
}

void ConfigSettings::SetKeyboardHash(const KeyboardHash key)
{
  keyboard = key;
}

void ConfigSettings::SetGamepadHash(const GamepadHash gamepad)
{
  ConfigSettings::gamepad = gamepad;
}

void ConfigSettings::SetWebServerInfo(const std::string& url, int port, const std::string& version)
{
  this->webServer.URL = url;
  this->webServer.port = port;
  this->webServer.version = version;
}

ConfigSettings::ConfigSettings(const ConfigSettings& rhs)
{
  this->operator=(rhs);
}

ConfigSettings::ConfigSettings()
{
  isOK = false;
}
