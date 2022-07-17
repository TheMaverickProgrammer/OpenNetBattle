/*! \brief All of the virtual input events to map to joysticks and keyboard commands */
#pragma once

#include <string>
#include "frame_time_t.h"

enum class InputState : unsigned char {
  none = 0,
  pressed,
  held,
  released
};

struct InputEvent {
  std::string name;
  InputState state{};
  unsigned int wait{};

  const bool operator==(const InputEvent& rhs) const {
    return (rhs.name == name && rhs.state == state);
  }

  const bool operator!=(const InputEvent& rhs) const {
    return !(rhs.name == name && rhs.state == state);
  }
};

namespace InputEvents {
  static const InputEvent none{};

  static const InputEvent pressed_move_up        = { "Move Up",    InputState::pressed };
  static const InputEvent pressed_move_down      = { "Move Down",  InputState::pressed };
  static const InputEvent pressed_move_left      = { "Move Left",  InputState::pressed };
  static const InputEvent pressed_move_right     = { "Move Right", InputState::pressed };
  static const InputEvent pressed_shoot          = { "Shoot",      InputState::pressed };
  static const InputEvent pressed_use_chip       = { "Use Card",   InputState::pressed };
  static const InputEvent pressed_special        = { "Special",    InputState::pressed };
  static const InputEvent pressed_cust_menu      = { "Cust Menu",  InputState::pressed };
  static const InputEvent pressed_pause          = { "Pause",      InputState::pressed };
  static const InputEvent pressed_ui_up          = { "UI Up",      InputState::pressed };
  static const InputEvent pressed_ui_down        = { "UI Down",    InputState::pressed };
  static const InputEvent pressed_ui_left        = { "UI Left",    InputState::pressed };
  static const InputEvent pressed_ui_right       = { "UI Right",   InputState::pressed };
  static const InputEvent pressed_confirm        = { "Confirm",    InputState::pressed };
  static const InputEvent pressed_cancel         = { "Cancel",     InputState::pressed };
  static const InputEvent pressed_option         = { "Option",     InputState::pressed };
  static const InputEvent pressed_run            = { "Run",        InputState::pressed };
  static const InputEvent pressed_interact       = { "Interact",   InputState::pressed };
  static const InputEvent pressed_map            = { "Minimap",    InputState::pressed };
  static const InputEvent pressed_shoulder_left  = { "Shoulder L", InputState::pressed };
  static const InputEvent pressed_shoulder_right = { "Shoulder R", InputState::pressed };
  static const InputEvent pressed_advance_frame  = { "Advance Frame", InputState::pressed };
  static const InputEvent pressed_resume_frames  = { "Resume Frames", InputState::pressed };
  static const InputEvent pressed_record_frames  = { "Record Frames", InputState::pressed };
  static const InputEvent pressed_terminal       = { "Terminal",      InputState::pressed };

  static const InputEvent released_move_up        = { "Move Up",    InputState::released };
  static const InputEvent released_move_down      = { "Move Down",  InputState::released };
  static const InputEvent released_move_left      = { "Move Left",  InputState::released };
  static const InputEvent released_move_right     = { "Move Right", InputState::released };
  static const InputEvent released_shoot          = { "Shoot",      InputState::released };
  static const InputEvent released_use_chip       = { "Use Card",   InputState::released };
  static const InputEvent released_special        = { "Special",    InputState::released };
  static const InputEvent released_cust_menu      = { "Cust Menu",  InputState::released };
  static const InputEvent released_pause          = { "Pause",      InputState::released };
  static const InputEvent released_ui_up          = { "UI Up",      InputState::released };
  static const InputEvent released_ui_down        = { "UI Down",    InputState::released };
  static const InputEvent released_ui_left        = { "UI Left",    InputState::released };
  static const InputEvent released_ui_right       = { "UI Right",   InputState::released };
  static const InputEvent released_confirm        = { "Confirm",    InputState::released };
  static const InputEvent released_cancel         = { "Cancel",     InputState::released };
  static const InputEvent released_option         = { "Option",     InputState::released };
  static const InputEvent released_run            = { "Run",        InputState::released };
  static const InputEvent released_interact       = { "Interact",   InputState::released };
  static const InputEvent released_map            = { "Minimap",    InputState::released };
  static const InputEvent released_shoulder_left  = { "Shoulder L", InputState::released };
  static const InputEvent released_shoulder_right = { "Shoulder R", InputState::released };
  static const InputEvent released_advance_frame  = { "Advance Frame", InputState::released };
  static const InputEvent released_resume_frames  = { "Resume Frames", InputState::released };
  static const InputEvent released_record_frames  = { "Record Frames", InputState::released };
  static const InputEvent released_terminal       = { "Terminal",      InputState::released };

  static const InputEvent held_move_up        = { "Move Up",    InputState::held };
  static const InputEvent held_move_down      = { "Move Down",  InputState::held };
  static const InputEvent held_move_left      = { "Move Left",  InputState::held };
  static const InputEvent held_move_right     = { "Move Right", InputState::held };
  static const InputEvent held_shoot          = { "Shoot",      InputState::held };
  static const InputEvent held_use_chip       = { "Use Card",   InputState::held };
  static const InputEvent held_special        = { "Special",    InputState::held };
  static const InputEvent held_cust_menu      = { "Cust Menu",  InputState::held };
  static const InputEvent held_pause          = { "Pause",      InputState::held };
  static const InputEvent held_ui_up          = { "UI Up",      InputState::held };
  static const InputEvent held_ui_down        = { "UI Down",    InputState::held };
  static const InputEvent held_ui_left        = { "UI Left",    InputState::held };
  static const InputEvent held_ui_right       = { "UI Right",   InputState::held };
  static const InputEvent held_confirm        = { "Confirm",    InputState::held };
  static const InputEvent held_cancel         = { "Cancel",     InputState::held };
  static const InputEvent held_option         = { "Option",     InputState::held };
  static const InputEvent held_run            = { "Run",        InputState::held };
  static const InputEvent held_interact       = { "Interact",   InputState::held };
  static const InputEvent held_map            = { "Minimap",    InputState::held };
  static const InputEvent held_shoulder_left  = { "Shoulder L", InputState::held };
  static const InputEvent held_shoulder_right = { "Shoulder R", InputState::held };
  static const InputEvent held_advance_frame  = { "Advance Frame", InputState::held };
  static const InputEvent held_resume_frames  = { "Resume Frames", InputState::held };
  static const InputEvent held_record_frames  = { "Record Frames", InputState::held };
  static const InputEvent held_terminal       = { "Terminal",      InputState::held };

  static const auto ui_up_group    = { pressed_ui_up,    held_ui_up    };
  static const auto ui_down_group  = { pressed_ui_down,  held_ui_down  };
  static const auto ui_left_group  = { pressed_ui_left,  held_ui_left  };
  static const auto ui_right_group = { pressed_ui_right, held_ui_right };

  static const auto shoulder_left_group  = { pressed_shoulder_left,  held_shoulder_left  };
  static const auto shoulder_right_group = { pressed_shoulder_right, held_shoulder_right };

  static const std::string KEYS[] = {
    "Move Up", "Move Down", "Move Left", "Move Right",
    "Shoot", "Use Card", "Special", "Cust Menu", "Pause",
    "UI Up", "UI Down", "UI Left", "UI Right",
    "Confirm", "Cancel", "Option",
    "Run", "Interact", "Shoulder L", "Shoulder R",
    "Minimap",
    "Advance Frame", "Resume Frames", "Record Frames",
    "Terminal"
  };
};
