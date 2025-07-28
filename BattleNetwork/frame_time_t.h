#pragma once

#include "template_helpers.h"
#include <cstdint>
#include <cmath>

/**
* @class frame_time_t
* @brief struct representing a single frame time. Allows for basic math operations.
*
* This struct helps keep the codebase consistent with nomenclature
* When the game refers to "5 frames of animation" clocked at 60 fps,
* that is to say "1/12th of a second of animation". When referencing material
* it's easier to discuss time in terms of frames than it is to discuss fractions of a second.
* 
* Because time-sensistive components may need millisecond precision, this struct allows
* easily operability between either fidelities of time within the same struct.
*/
struct frame_time_t {
  static const int64_t frames_per_second = 60u;

  struct milliseconds {
    long long value{};
  };

  struct seconds {
    double value{};
  };

  int64_t value{};

  seconds asSeconds() const {
    return seconds{ static_cast<double>(value) / frame_time_t::frames_per_second };
  }

  milliseconds asMilli() const {
    return milliseconds{ static_cast<long long>(value / frame_time_t::frames_per_second) * 1000ll };
  }

  int64_t count() const {
    return value;
  }

  operator seconds() const {
    return asSeconds();
  }

  operator milliseconds() const {
    return asMilli();
  }

  template<typename T>
  operator T() const;

  frame_time_t& operator++(int) {
    value++;
    return *this;
  }

  frame_time_t& operator-=(const frame_time_t& other) {
    value = value - other.value;
    return *this;
  }

  frame_time_t& operator+=(const frame_time_t& other) {
    value = value + other.value;
    return *this;
  }

  frame_time_t operator-(const frame_time_t& other) const {
    return frame_time_t{ value - other.value };
  }

  frame_time_t operator+(const frame_time_t& other) const {
    return frame_time_t{ value + other.value };
  }

  bool operator <(const frame_time_t& other) const {
    return value < other.value;
  }

  bool operator <=(const frame_time_t& other) const {
    return value <= other.value;
  }

  bool operator >(const frame_time_t& other) const {
    return value > other.value;
  }

  bool operator >=(const frame_time_t& other) const {
    return value >= other.value;
  }

  bool operator ==(const frame_time_t& other) const {
    return value == other.value;
  }

  bool operator !=(const frame_time_t& other) const {
    return value != other.value;
  }

  static frame_time_t max(const frame_time_t& lhs, const frame_time_t& rhs) {
    return lhs.operator<(rhs) ? rhs : lhs;
  }
};

template<typename T>
static constexpr frame_time_t from_seconds(T sec) {
  // cast to high precision to type of milliseconds
  return frame_time_t{ static_cast<int64_t>(std::round(sec * frame_time_t::frames_per_second)) };
}

template<typename T>
static constexpr frame_time_t from_milliseconds(T milli) {
  // cast to high precision to type of milliseconds
  return frame_time_t{ static_cast<int64_t>(std::round((milli / 1000.0) * frame_time_t::frames_per_second)) };
}

 //!< frames method shorthand for frame_time_t object
static constexpr frame_time_t frames(int64_t frames) {
  return frame_time_t{ frames };
};

template<typename T>
const T seconds_cast(const frame_time_t& rhs) {
  return static_cast<T>(rhs.asSeconds().value);
}

template<typename T>
const T milli_cast(const frame_time_t& rhs) {
  return static_cast<T>(rhs.asMilli().value);
}

template<typename T>
const T time_cast(const frame_time_t& rhs) {
  static_assert(!is_template_parsed<T>(), "no cast available for frame_time_cast");
}

template<typename T>
frame_time_t::operator T() const {
  return time_cast<T>(*this);
}

/**
* stop watch implementation that uses frame_time_t
*/


class frame_time_stopwatch_t {
private:
  frame_time_t timer{ frames(0) };
  bool paused{ true };

public:
  inline void tick(frame_time_t frames = ::frames(1)) {
    if (paused) return;
    timer += frames;
  }

  inline void start() {
    paused = false;
  }

  inline void pause() {
    paused = true;
  }

  inline void reset() {
    timer = frames(0);
  }

  inline const frame_time_t elapsed() const {
    return timer;
  }

  inline const bool is_paused() const {
    return paused;
  }
};