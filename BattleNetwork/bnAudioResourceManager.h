#pragma once

#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/Music.hpp>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>

#include "sfMidi/include/sfMidi.h"
#include "bnAudioType.h"
#include "bnCachedResource.h"

// For more retro experience, decrease available channels.
#define NUM_OF_CHANNELS 15

// Prevent duplicate sounds from stacking on same frame
// Allows duplicate audio samples to play in X ms apart from eachother
#define AUDIO_DUPLICATES_ALLOWED_IN_X_MILLISECONDS 58 // 58ms = ~3.5 frames @ 60fps

/**
  * @class AudioPriority
  * @brief Each priority describes how or if a playing sample should be interrupted
  * 
  * Priorities are LOWEST  (one at a time, if channel available),
  *                LOW     (any free channels),
  *                HIGH    (force a channel to play sound, but one at a time, and don't interrupt other high priorities),
  *                HIGHEST (force a channel to play sound always)
  */
enum class AudioPriority : int {
  lowest,
  low,
  high,
  highest
};

/**
 * @class AudioResourceManager
 * @author mav
 * @date 06/05/19
 * @brief Manager loads Audio() samples
 */
class AudioResourceManager {
public:
  /**
   * @brief If true, plays Audio(). If false, does not play Audio()
   * @param status
   */
  void EnableAudio(bool status);

  /**
  * @brief If true, all audio plays as normal but the volume is set to 0
  * @param status
  * If toggled, the previous volume settings are persisted
  */
  void Mute(bool status = true);

  /**
  * @brief Raising the pitch will also result in increasing the speed as a side effect
  * @param pitch. A floating-point multiplied by 100 to achieve the pitch %
  */
  void SetPitch(float pitch);
  
  /**
   * @brief Loads all queued resources. Increases status value.
   * @param status thread-safe counter will reach total count of all samples to load when finished.
   */
  void LoadAllSources(std::atomic<int> &status);
  
  /**
   * @brief Loads an Audio() source at path and map it to enum type
   * @param type Audio() enum to map to
   * @param path path to Audio() sample
   */
  void LoadSource(AudioType type, const std::string& path);

  std::shared_ptr<sf::SoundBuffer> LoadFromFile(const std::string& path);
  
  void HandleExpiredAudioCache();

  /**
   * @brief Play a sound with an Audio() priority
   * @param type Audio() to play
   * @param priority describes if and how to interrupt other playing samples
   * @return -1 if could not play, otherwise 0
   */
  int Play(AudioType type, AudioPriority priority = AudioPriority::low);

  int Play(std::shared_ptr<sf::SoundBuffer> resource, AudioPriority priority = AudioPriority::low);

  int Stream(std::string path, bool loop = false);
  int Stream(std::string path, bool loop, long long startMs, long long endMs);
  void StopStream();
  void SetStreamVolume(float volume);
  void SetChannelVolume(float volume);

  const float GetStreamVolume() const;

  AudioResourceManager();
  ~AudioResourceManager();

private:
  struct Channel {
    sf::Sound buffer;
    AudioPriority priority{ AudioPriority::lowest };
  };

  sfmidi::Midi midiMusic;
  std::mutex mutex;
  Channel* channels;
  sf::SoundBuffer* sources;
  std::map<std::string, CachedResource<sf::SoundBuffer>> cached;
  sf::Music stream;
  std::string currStreamPath;
  float channelVolume{};
  float streamVolume{};
  bool isEnabled{true};
  bool muted{false};
};