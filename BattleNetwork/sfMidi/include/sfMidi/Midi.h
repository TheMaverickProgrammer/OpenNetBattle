// Modified to expose more FluidSynth settings (reverb)

////////////////////////////////
// sfMidi 1.1.0               //
// Copyright © Kerli Low 2012 //
////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// License:                                                                 //
// sfMidi                                                                   //
// Copyright (c) 2012 Kerli Low                                             //
// kerlilow@gmail.com                                                       //
//                                                                          //
// This software is provided 'as-is', without any express or implied        //
// warranty. In no event will the authors be held liable for any damages    //
// arising from the use of this software.                                   //
//                                                                          //
// Permission is granted to anyone to use this software for any purpose,    //
// including commercial applications, and to alter it and redistribute it   //
// freely, subject to the following restrictions:                           //
//                                                                          //
//    1. The origin of this software must not be misrepresented; you must   //
//    not claim that you wrote the original software. If you use this       //
//    software in a product, an acknowledgment in the product documentation //
//    would be appreciated but is not required.                             //
//                                                                          //
//    2. Altered source versions must be plainly marked as such, and must   //
//    not be misrepresented as being the original software.                 //
//                                                                          //
//    3. This notice may not be removed or altered from any source          //
//    distribution.                                                         //
//////////////////////////////////////////////////////////////////////////////

#ifndef SFMIDI_MIDI_H
#define SFMIDI_MIDI_H

#include <string>
#include <vector>
#include <filesystem>
#include <SFML/Audio.hpp>

#include "sfMidi/Error.h"



const unsigned int SFMIDI_AUDIOFRAMES    = 1024;
const unsigned int SFMIDI_SEEKBUFFERSIZE = SFMIDI_AUDIOFRAMES * 8;


// Forward declarations //
struct _fluid_hashtable_t;
typedef struct _fluid_hashtable_t fluid_settings_t;
struct _fluid_synth_t;
typedef struct _fluid_synth_t fluid_synth_t;
struct _fluid_player_t;
typedef struct _fluid_player_t fluid_player_t;
// //


namespace sfmidi
{
  class Midi;
}


///////////////////////////////////////////////////////////
// Midi class                                            //
///////////////////////////////////////////////////////////
class sfmidi::Midi : public sf::SoundStream, public sfmidi::Error
{
public:
  Midi();
  Midi(const std::filesystem::path& soundFont, const std::filesystem::path& midi);
  Midi(const std::filesystem::path& soundFont,
       const void* midiData, unsigned int midiSize);

  ~Midi();

  void   setLoop(bool loop);
  void   setGain(double gain); // 0.0 - 10.0; Default: 0.2
  void   setReverbActive(bool active); // Default: true
  bool   getLoop() const;
  double getGain() const;

  bool loadSoundFontFromFile(const std::filesystem::path& filename);

  bool loadMidiFromFile  (const std::filesystem::path& filename);
  bool loadMidiFromMemory(const std::string& data);
  bool loadMidiFromMemory(const void* data, unsigned int size);

  void play();
  void stop();


private:
  bool onGetData(sf::SoundStream::Chunk& data);
  void onSeek   (sf::Time timeOffset);


  fluid_settings_t* settings_;
  fluid_synth_t*    synth_;
  fluid_player_t*   player_;

  std::vector<sf::Int16> buffer_;

  std::string midi_;

  bool       loop_;
  sf::Clock* loopClock_;
};



#endif // SFMIDI_MIDI_H
