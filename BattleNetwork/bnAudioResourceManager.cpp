#include "bnAudioResourceManager.h"
#include "bnLogger.h"

AudioResourceManager::AudioResourceManager(){
  midiMusic.loadSoundFontFromFile("resources/midi/soundfont.sf2");

  isEnabled = true;

  channels = new AudioResourceManager::Channel[NUM_OF_CHANNELS];

  for (int i = 0; i < NUM_OF_CHANNELS; i++) {
    channels[i].buffer = sf::Sound();
  }

  sources = new sf::SoundBuffer[static_cast<size_t>(AudioType::AUDIO_TYPE_SIZE)];

  for (int i = 0; i < static_cast<size_t>(AudioType::AUDIO_TYPE_SIZE); i++) {
    sources[i] = sf::SoundBuffer();
  }

  channelVolume = streamVolume = 100; //SFML default
}


AudioResourceManager::~AudioResourceManager() {
  // Stop playing everything 
  stream.stop();

  for (int i = 0; i < NUM_OF_CHANNELS; i++) {
    channels[i].buffer.stop();
  }

  // Free memory
  delete[] channels;
  delete[] sources;
}

void AudioResourceManager::EnableAudio(bool status) {
  isEnabled = status;
}

void AudioResourceManager::Mute(bool status)
{
  muted = status;

  if (!muted) {
    SetStreamVolume(streamVolume);
    SetChannelVolume(channelVolume);
  }
  else {
    float streamBefore = streamVolume;
    float channelBefore = channelVolume;
    SetStreamVolume(0);
    SetChannelVolume(0);
    streamVolume = streamBefore;
    channelVolume = channelBefore;
  }
}

void AudioResourceManager::SetPitch(float pitch)
{
  stream.setPitch(pitch);
}

void AudioResourceManager::LoadAllSources(std::atomic<int> &status) {
  LoadSource(AudioType::COUNTER_BONUS, "resources/sfx/counter_bonus.ogg"); status++;
  LoadSource(AudioType::DIR_TILE, "resources/sfx/dir_tile.ogg"); status++;
  LoadSource(AudioType::FANFARE, "resources/sfx/fanfare.ogg"); status++;
  LoadSource(AudioType::APPEAR, "resources/sfx/appear.ogg"); status++;
  LoadSource(AudioType::AREA_GRAB, "resources/sfx/area_grab.ogg"); status++;
  LoadSource(AudioType::AREA_GRAB_TOUCHDOWN, "resources/sfx/area_grab_touchdown.ogg"); status++;
  LoadSource(AudioType::BUSTER_PEA, "resources/sfx/pew.ogg"); status++;
  LoadSource(AudioType::BUSTER_CHARGED, "resources/sfx/buster_charged.ogg"); status++;
  LoadSource(AudioType::BUSTER_CHARGING, "resources/sfx/buster_charging.ogg"); status++;
  LoadSource(AudioType::BUBBLE_POP, "resources/sfx/bubble_pop.ogg"); status++;
  LoadSource(AudioType::BUBBLE_SPAWN, "resources/sfx/bubble_spawn.ogg"); status++;
  LoadSource(AudioType::GUARD_HIT, "resources/sfx/guard_hit.ogg"); status++;
  LoadSource(AudioType::CANNON, "resources/sfx/cannon.ogg"); status++;
  LoadSource(AudioType::COUNTER, "resources/sfx/counter.ogg"); status++;
  LoadSource(AudioType::WIND, "resources/sfx/wind.ogg"); status++;
  LoadSource(AudioType::CHIP_CANCEL, "resources/sfx/card_cancel.ogg"); status++;
  LoadSource(AudioType::CHIP_CHOOSE, "resources/sfx/card_choose.ogg"); status++;
  LoadSource(AudioType::CHIP_CONFIRM, "resources/sfx/card_confirm.ogg"); status++;
  LoadSource(AudioType::CHIP_DESC, "resources/sfx/card_desc.ogg"); status++;
  LoadSource(AudioType::CHIP_DESC_CLOSE, "resources/sfx/card_desc_close.ogg"); status++;
  LoadSource(AudioType::CHIP_SELECT, "resources/sfx/card_select.ogg"); status++;
  LoadSource(AudioType::CHIP_ERROR, "resources/sfx/card_error.ogg"); status++;
  LoadSource(AudioType::CUSTOM_BAR_FULL, "resources/sfx/custom_bar_full.ogg"); status++;
  LoadSource(AudioType::CUSTOM_SCREEN_OPEN, "resources/sfx/card_screen_open.ogg"); status++;
  LoadSource(AudioType::ITEM_GET, "resources/sfx/item_get.ogg"); status++;
  LoadSource(AudioType::DELETED, "resources/sfx/deleted.ogg"); status++;
  LoadSource(AudioType::EXPLODE, "resources/sfx/explode_once.ogg"); status++;
  LoadSource(AudioType::GUN, "resources/sfx/gun.ogg"); status++;
  LoadSource(AudioType::HURT, "resources/sfx/hurt.ogg"); status++;
  LoadSource(AudioType::PANEL_CRACK, "resources/sfx/panel_crack.ogg"); status++;
  LoadSource(AudioType::PANEL_RETURN, "resources/sfx/panel_return.ogg"); status++;
  LoadSource(AudioType::PAUSE, "resources/sfx/pause.ogg"); status++;
  LoadSource(AudioType::PRE_BATTLE, "resources/sfx/pre_battle.ogg"); status++;
  LoadSource(AudioType::RECOVER, "resources/sfx/recover.ogg"); status++;
  LoadSource(AudioType::SPREADER, "resources/sfx/spreader.ogg"); status++;
  LoadSource(AudioType::SWORD_SWING, "resources/sfx/sword_swing.ogg"); status++;
  LoadSource(AudioType::TOSS_ITEM, "resources/sfx/toss_item.ogg"); status++;
  LoadSource(AudioType::TOSS_ITEM_LITE, "resources/sfx/toss_item_lite.ogg"); status++;
  LoadSource(AudioType::WAVE, "resources/sfx/wave.ogg"); status++;
  LoadSource(AudioType::THUNDER, "resources/sfx/thunder.ogg"); status++;
  LoadSource(AudioType::ELECPULSE, "resources/sfx/elecpulse.ogg"); status++;
  LoadSource(AudioType::INVISIBLE, "resources/sfx/invisible.ogg"); status++;
  LoadSource(AudioType::PA_ADVANCE, "resources/sfx/pa_advance.ogg"); status++;
  LoadSource(AudioType::LOW_HP, "resources/sfx/low_hp.ogg"); status++;
  LoadSource(AudioType::DARK_CARD, "resources/sfx/dark_card.ogg"); status++;
  LoadSource(AudioType::POINT_SFX, "resources/sfx/point.ogg"); status++;
  LoadSource(AudioType::NEW_GAME, "resources/sfx/new_game.ogg"); status++;
  LoadSource(AudioType::TEXT, "resources/sfx/text.ogg"); status++;
  LoadSource(AudioType::SHINE, "resources/sfx/shine.ogg"); status++;
  LoadSource(AudioType::METEOR, "resources/sfx/meteor.ogg"); status++;
  LoadSource(AudioType::TIME_FREEZE, "resources/sfx/timefreeze.ogg"); status++;
  LoadSource(AudioType::TRAP, "resources/sfx/trap.ogg"); status++;
  LoadSource(AudioType::DEFORM, "resources/sfx/deform.ogg"); status++;
}

void AudioResourceManager::LoadSource(AudioType type, const std::string& path) {
  if (!sources[static_cast<size_t>(type)].loadFromFile(path)) {
    Logger::Logf(LogLevel::critical, "Failed loading Audio(): %s\n", path.c_str());

  } else {
    Logger::Logf(LogLevel::info, "Loaded Audio(): %s", path.c_str());
  }
}

std::shared_ptr<sf::SoundBuffer> AudioResourceManager::LoadFromFile(const std::string& path)
{
  auto iter = cached.find(path);
  std::shared_ptr<sf::SoundBuffer> loaded;

  if (iter == cached.end()) {
    loaded = std::make_shared<sf::SoundBuffer>();
    loaded->loadFromFile(path);
    cached.insert(std::make_pair(path, CachedResource<sf::SoundBuffer>(loaded)));
  }
  else {
    loaded = iter->second.GetResource();
  }

  return loaded;
}

void AudioResourceManager::HandleExpiredAudioCache()
{
  auto iter = cached.begin();
  while (iter != cached.end()) {
    if (iter->second.GetSecondsSinceLastRequest() > 60.0f) {
      if (iter->second.IsInUse()) {
        iter++; continue;
      }

      // 1 minute is long enough
      Logger::Logf(LogLevel::debug, "Audio data %s expired", iter->first.c_str());
      iter = cached.erase(iter);
      continue;
    }

    iter++;
  }
}

int AudioResourceManager::Play(AudioType type, AudioPriority priority) {
  if (!isEnabled) { return -1; }

  if (type < AudioType(0) || type >= AudioType::AUDIO_TYPE_SIZE) {
    return -1;
  }

  // Annoying sound check. Make sure duplicate sounds are played only by a given amount of offset from the last time it was played.
  // This prevents amplitude stacking when duplicate sounds are played on the same frame...
  // NOTE: an Audio() queue would be a better place for this check. Then play() those sounds that pass the queue filter.
  if (priority < AudioPriority::high) {
    for (int i = 0; i < NUM_OF_CHANNELS; i++) {
      if (channels[i].buffer.getBuffer() == &sources[static_cast<size_t>(type)] && channels[i].buffer.getStatus() == sf::SoundSource::Status::Playing) {
        sf::Int32 howLongPlayed = channels[i].buffer.getPlayingOffset().asMilliseconds();
        if (howLongPlayed <= AUDIO_DUPLICATES_ALLOWED_IN_X_MILLISECONDS) {
          return -1;
        }
      }
    }
  }

  // Priorities are LOWEST  (one at a time, if channel available),
  //                LOW     (any free channels),
  //                HIGH    (force a channel to play sound, but one at a time, and don't interrupt other high priorities),
  //                HIGHEST (force a channel to play sound always)

  // Highest priority plays over anything that isn't like it
  // If it can, it will play in the first free channel but it will overwrite a sound if it cannot find one
  if (priority == AudioPriority::highest) {
    int lastFreeChannel = -1;
    int lastDifferentChannel = -1;

    auto changeSampleThunk = [this, priority, type](int index) {
      channels[index].buffer.stop();
      channels[index].buffer.setBuffer(sources[static_cast<size_t>(type)]);
      channels[index].buffer.play();
      channels[index].priority = priority;
    };

    for (int i = 0; i < NUM_OF_CHANNELS; i++) {
      if (channels[i].buffer.getStatus() != sf::SoundSource::Status::Playing && lastFreeChannel == -1) {
        lastFreeChannel = i;
      }

      if (channels[i].buffer.getBuffer() != &sources[static_cast<size_t>(type)] && lastDifferentChannel == -1) {
        lastDifferentChannel = i;
      }
    }

    if (lastFreeChannel != -1) {
      changeSampleThunk(lastFreeChannel);
      return 0;
    } else if (lastDifferentChannel != -1) {
      changeSampleThunk(lastDifferentChannel);
      return 0;
    }
    else {
      // all channels are currently playing the same sound. That must be annoying to hear.
      return -1;
    }
  }

  // For lowest priority or high priority sounds, scan and see if this sound is already playing...
  // This step is also used as a pre-check for low priority sounds
  if (priority == AudioPriority::lowest || priority == AudioPriority::high) {
    for (int i = 0; i < NUM_OF_CHANNELS; i++) {
      if (channels[i].buffer.getStatus() == sf::SoundSource::Status::Playing) {
        if ((sf::SoundBuffer*)channels[i].buffer.getBuffer() == &sources[static_cast<size_t>(type)]) {
          // Lowest priority or high priority sounds only play once 
          return -1;
        }
      }
    }
  }

  // Play sound based on priortiy rules
  for (int i = 0; i < NUM_OF_CHANNELS; i++) {
    if (priority != AudioPriority::high) {
      if (channels[i].buffer.getStatus() != sf::SoundSource::Status::Playing) {
        channels[i].buffer.setBuffer(sources[static_cast<size_t>(type)]);
        channels[i].buffer.play();
        channels[i].priority = priority;
        return 0;
      }
    }
    else { // HIGH PRIORITY will not overwrite other HIGH priorities unless they have ended
      bool canOverwrite = channels[i].priority < AudioPriority::high 
        ||(channels[i].priority == AudioPriority::high && channels[i].buffer.getStatus() != sf::SoundSource::Status::Playing);
      if (canOverwrite) {
        channels[i].buffer.stop();
        channels[i].buffer.setBuffer(sources[static_cast<size_t>(type)]);
        channels[i].buffer.play();
        channels[i].priority = priority;
        return 0;
      }
    }
  }

  // No free channel? Skip playing this sound.
  return -1;
}

int AudioResourceManager::Play(std::shared_ptr<sf::SoundBuffer> resource, AudioPriority priority)
{
  if (!isEnabled) { return -1; }

  // Annoying sound check. Make sure duplicate sounds are played only by a given amount of offset from the last time it was played.
  // This prevents amplitude stacking when duplicate sounds are played on the same frame...
  // NOTE: an audio queue would be a better place for this check. Then play() those sounds that pass the queue filter.
  if (priority < AudioPriority::high) {
    for (int i = 0; i < NUM_OF_CHANNELS; i++) {
      if (channels[i].buffer.getBuffer() == resource.get() && channels[i].buffer.getStatus() == sf::SoundSource::Status::Playing) {
        sf::Int32 howLongPlayed = channels[i].buffer.getPlayingOffset().asMilliseconds();
        if (howLongPlayed <= AUDIO_DUPLICATES_ALLOWED_IN_X_MILLISECONDS) {
          return -1;
        }
      }
    }
  }

  // Priorities are LOWEST  (one at a time, if channel available),
  //                LOW     (any free channels),
  //                HIGH    (force a channel to play sound, but one at a time, and don't interrupt other high priorities),
  //                HIGHEST (force a channel to play sound always)

  // Highest priority plays over anything that isn't like it
  // If it can, it will play in the first free channel but it will overwrite a sound if it cannot find one
  if (priority == AudioPriority::highest) {
    int lastFreeChannel = -1;
    int lastDifferentChannel = -1;

    auto changeSampleThunk = [this, priority, resource](int index) {
      channels[index].buffer.stop();
      channels[index].buffer.setBuffer(*resource.get());
      channels[index].buffer.play();
      channels[index].priority = priority;
    };

    for (int i = 0; i < NUM_OF_CHANNELS; i++) {
      if (channels[i].buffer.getStatus() != sf::SoundSource::Status::Playing && lastFreeChannel == -1) {
        lastFreeChannel = i;
      }

      if (channels[i].buffer.getBuffer() != resource.get() && lastDifferentChannel == -1) {
        lastDifferentChannel = i;
      }
    }

    if (lastFreeChannel != -1) {
      changeSampleThunk(lastFreeChannel);
      return 0;
    }
    else if (lastDifferentChannel != -1) {
      changeSampleThunk(lastDifferentChannel);
      return 0;
    }
    else {
      // all channels are currently playing the same sound. That must be annoying to hear.
      return -1;
    }
  }

  // For lowest priority or high priority sounds, scan and see if this sound is already playing...
  // This step is also used as a pre-check for low priority sounds
  if (priority == AudioPriority::lowest || priority == AudioPriority::high) {
    for (int i = 0; i < NUM_OF_CHANNELS; i++) {
      if (channels[i].buffer.getStatus() == sf::SoundSource::Status::Playing) {
        if ((sf::SoundBuffer*)channels[i].buffer.getBuffer() == resource.get()) {
          // Lowest priority or high priority sounds only play once 
          return -1;
        }
      }
    }
  }

  // Play sound based on priortiy rules
  for (int i = 0; i < NUM_OF_CHANNELS; i++) {
    if (priority != AudioPriority::high) {
      if (channels[i].buffer.getStatus() != sf::SoundSource::Status::Playing) {
        channels[i].buffer.setBuffer(*resource);
        channels[i].buffer.play();
        channels[i].priority = priority;
        return 0;
      }
    }
    else { // HIGH PRIORITY will not overwrite other HIGH priorities unless they have ended
      bool canOverwrite = channels[i].priority < AudioPriority::high
        || (channels[i].priority == AudioPriority::high && channels[i].buffer.getStatus() != sf::SoundSource::Status::Playing);
      if (canOverwrite) {
        channels[i].buffer.stop();
        channels[i].buffer.setBuffer(*resource);
        channels[i].buffer.play();
        channels[i].priority = priority;
        return 0;
      }
    }
  }

  // No free channel? Skip playing this sound.
  return -1;
}

int AudioResourceManager::Stream(std::string path, bool loop, long long startMs, long long endMs) {
  if (!isEnabled) { return -1; }

  if (path == currStreamPath) { return -1; };

  currStreamPath = path;

  // stop previous stream if any 
  stream.stop();
  midiMusic.stop();

  if (!stream.openFromFile(path)) {
    if (midiMusic.loadMidiFromFile(path)) {
      midiMusic.play();
      midiMusic.setLoop(loop);
      midiMusic.setPitch(1.f);
    }

    return 0;
  }

  stream.play();
  stream.setLoop(loop);
  stream.setPitch(1.f);

  if(loop && startMs > 0 && endMs > startMs) {
    stream.setLoopPoints({ sf::milliseconds(startMs), sf::milliseconds(endMs) });
  }

  return 0;
}

int AudioResourceManager::Stream(std::string path, bool loop) {
  return Stream(path, loop, -1, -1);
}

void AudioResourceManager::StopStream() {
  stream.stop();
  midiMusic.stop();
}

void AudioResourceManager::SetStreamVolume(float volume) {
  stream.setVolume(volume);
  midiMusic.setVolume(volume);
  midiMusic.setGain(1.5); // midi's are quiet, make them louder
  streamVolume = volume;
}

void AudioResourceManager::SetChannelVolume(float volume) {
  for (int i = 0; i < NUM_OF_CHANNELS; i++) {
    channels[i].buffer.setVolume(volume);
  }

  channelVolume = volume;
}

const float AudioResourceManager::GetStreamVolume() const
{
  return this->streamVolume;
}
