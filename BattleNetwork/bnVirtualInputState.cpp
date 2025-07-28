#include "bnVirtualInputState.h"
#include "bnLogger.h"
#include <algorithm>

void VirtualInputState::Process()
{
  // Prioritize press events only (discard release events)
  for (auto iter = queuedState.begin(); iter != queuedState.end(); /* skip */) {
    auto& event = *iter;

    if (event.second == InputState::pressed) {
      auto query = [event](const std::pair<std::string, InputState>& pair) {
        return pair.first == event.first && pair.second == InputState::released;
      };

      auto iterFind = std::find_if(queuedState.begin(), queuedState.end(), query);

      if (iterFind != queuedState.end()) {
        iter = queuedState.erase(iterFind);
        continue;
      }
    }

    iter++;
  }

  std::swap(stateLastFrame, state);

  state.clear();

  // Finally, merge the continuous InputState::held key events into the final event buffer
  for (auto& [name, value] : queuedState) {
    auto previouslyHeld = (value == InputState::pressed && stateLastFrame[name] == InputState::pressed) || stateLastFrame[name] == InputState::held;

    if (previouslyHeld) {
      state[name] = InputState::held;
    }
    else {
      state[name] = value;
    }
  }

  for (auto& [name, value] : stateLastFrame) {
    if (queuedState[name] == InputState::none && value != InputState::released) {
      state[name] = InputState::released;
    }
  }

  queuedState.clear();
}

const std::unordered_map<std::string, InputState> VirtualInputState::ToHash() const
{
  return state;
}

bool VirtualInputState::Has(InputEvent event) const
{
  auto it = state.find(event.name);

  if (it == state.end()) {
    return false;
  }

  return it->second == event.state;
}

bool VirtualInputState::Empty() const
{
  return state.empty();
}

void VirtualInputState::VirtualKeyEvent(InputEvent event)
{
  queuedState[event.name] = event.state;
}

void VirtualInputState::Flush()
{
  for (auto& [name, previousState] : state) {
    // Search for press keys that have been InputState::held and transform them
    if (previousState == InputState::pressed || previousState == InputState::held) {
      queuedState[name] = InputState::released;
    }
  }
}

void VirtualInputState::DebugPrint()
{
  Logger::Logf(LogLevel::debug, "========Begin VirtualInputState::DebugPrint()========");
  for (auto& [name, state] : stateLastFrame) {
    std::string stateName = "NONE";

    switch (state) {
    case InputState::held:
      stateName = "InputState::held";
      break;
    case InputState::released:
      stateName = "InputState::released";
      break;
    case InputState::pressed:
      stateName = "InputState::pressed";
      break;
    }
    Logger::Logf(LogLevel::debug, "input event %s, %s", name.c_str(), stateName.c_str());
  }
  Logger::Logf(LogLevel::debug, "========End VirtualInputState::DebugPrint()========");
}
