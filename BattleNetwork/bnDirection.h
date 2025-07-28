#pragma once
#include <Swoosh/Ease.h>
#include <math.h>
#include <string>
#include <string_view>
#include <tuple>
#include <SFML/System/Vector2.hpp>
#include "stx/string.h"

/**
* @brief Used by entity movement
*/
enum class Direction : unsigned char {
  none = 0x00,

  // cardinal
  up = 0x01,
  left = 0x02,
  down = 0x04,
  right = 0x08,

  // diagonal 
  up_left = 0x10,
  up_right = 0x20,
  down_left = 0x40,
  down_right = 0x80,

  size = 9u
};

/**
* @brief Reverses the direction input
*/
inline Direction Reverse(Direction in) {
  switch (in) {
  case Direction::up:
    return Direction::down;
  case Direction::left:
    return Direction::right;
  case Direction::down:
    return Direction::up;
  case Direction::right:
    return Direction::left;
  case Direction::up_left:
    return Direction::down_right;
  case Direction::up_right:
    return Direction::down_left;
  case Direction::down_left:
    return Direction::up_right;
  case Direction::down_right:
    return Direction::up_left;
  }

  return Direction::none;
}

/**
* @brief Flips the direction horizontally
*/
inline Direction FlipHorizontal(Direction in) {
  switch (in) {
  case Direction::up:
  case Direction::down:
    // no change
    return in;
  case Direction::left:
    return Direction::right;
  case Direction::right:
    return Direction::left;
  case Direction::up_left:
    return Direction::up_right;
  case Direction::up_right:
    return Direction::up_left;
  case Direction::down_left:
    return Direction::down_right;
  case Direction::down_right:
    return Direction::down_left;
  }

  return Direction::none;
}

/**
* @brief Flips the direction vertically
*/
inline Direction FlipVertical(Direction in) {
  switch (in) {
  case Direction::left:
  case Direction::right:
    // no change
    return in;
  case Direction::up:
    return Direction::down;
  case Direction::down:
    return Direction::up;
  case Direction::up_left:
    return Direction::down_left;
  case Direction::up_right:
    return Direction::down_right;
  case Direction::down_left:
    return Direction::up_left;
  case Direction::down_right:
    return Direction::up_right;
  }

  return Direction::none;
}

/**
* @brief Splits 2-dimensional direction values into a 1-dimensional direction pair
*/
inline std::pair<Direction, Direction> Split(const Direction dir) {
  Direction first = dir;
  Direction second = Direction::none;

  switch (dir) {
  case Direction::up_left:
    first = Direction::up;
    second = Direction::left;
    break;
  case Direction::up_right:
    first = Direction::up;
    second = Direction::right;
    break;
  case Direction::down_left:
    first = Direction::down;
    second = Direction::left;
    break;
  case Direction::down_right:
    first = Direction::down;
    second = Direction::right;
    break;
  }

  return std::make_pair(first, second);
}

/**
* @brief Joins two 1-dimensional direction values into a potential 2-dimensional direction value
* 
* If the 2 joined directions are polar opposites, Direction::none is returned
* If the 2 joined directions are the same, then only a 1-dimensional direction value is returned
* If the 2 joined directions are 1D and 2D then the polar sum directional value is returned
*/
inline Direction Join(const Direction A, const Direction B) {
  auto [A1, A2] = Split(A);
  auto [B1, B2] = Split(B);

  // local-scope struct to make some vector math easy
  // lightweight on purpose
  struct vec_t {
    int x{}, y{};

    vec_t& operator+(const vec_t& other) {
      this->x = this->x + other.x;
      this->y = this->y + other.y;

      return *this;
    }

    vec_t& unit() {
      if (this->x != 0) {
        this->x = this->x > 0 ? 1 : this->x < 0 ? -1 : 0;
      }

      if (this->y != 0) {
        this->y = this->y > 0 ? 1 : this->y < 0 ? -1 : 0;
      }

      return *this;
    }
  };

  auto dir2v = [](const Direction& dir) -> vec_t {
    if (dir == Direction::up) {
      return { 0, -1 };
    } else if (dir == Direction::down) {
      return { 0, 1 };
    } else if (dir == Direction::left) {
      return { -1, 0 };
    } else if (dir == Direction::right) {
      return { 1, 0 };
    }

    return { 0,0 };
  };

  vec_t res = (dir2v(A1) + dir2v(A2) + dir2v(B1) + dir2v(B2)).unit();

  if (res.x < 0) {
    if (res.y < 0) {
      return Direction::up_left;
    }
    else if (res.y > 0) {
      return Direction::down_left;
    }
    else {
      // res.y == 0
      return Direction::left;
    }
  }
  else if (res.x > 0) {
    if (res.y < 0) {
      return Direction::up_right;
    }
    else if (res.y > 0) {
      return Direction::down_right;
    }
    else {
      // res.y == 0
      return Direction::right;
    }
  }
  else {
    // res.x == 0
    if (res.y < 0) {
      return Direction::up;
    }
    else if (res.y > 0) {
      return Direction::down;
    }
  }

  // x == 0 && y == 0
  return Direction::none;
}

/**
* @brief Returns new direction value from cartesian perspective to an isometric perspective
*/
inline Direction Isometric(const Direction dir) {
  switch (dir) {
  case Direction::up:
    return Direction::up_right;
  case Direction::left:
    return Direction::up_left;
  case Direction::down:
    return Direction::down_left;
  case Direction::right:
    return Direction::down_right;
  case Direction::up_left:
    return Direction::up;
  case Direction::up_right:
    return Direction::right;
  case Direction::down_left:
    return Direction::left;
  case Direction::down_right:
    return Direction::down;
  }

  return Direction::none;
}

/**
* @brief Returns new direction value from an isometric perspective to a cartesian perspective
*/
inline Direction Orthographic(const Direction dir) {
  switch (dir) {
  case Direction::up:
    return Direction::up_left;
  case Direction::left:
    return Direction::down_left;
  case Direction::down:
    return Direction::down_right;
  case Direction::right:
    return Direction::up_right;
  case Direction::up_left:
    return Direction::left;
  case Direction::up_right:
    return Direction::up;
  case Direction::down_left:
    return Direction::down;
  case Direction::down_right:
    return Direction::right;
  }

  return Direction::none;
}

inline sf::Vector2f UnitVector(Direction dir) {
  const float deg45radians = static_cast<float>(std::sin(swoosh::ease::pi / 4.0));

  switch(dir) {
    case Direction::left:
      return sf::Vector2f(-1, 0);
    case Direction::right:
      return sf::Vector2f(1, 0);
    case Direction::up:
      return sf::Vector2f(0, -1);
    case Direction::up_left:
      return sf::Vector2f(-deg45radians, -deg45radians);
    case Direction::up_right:
      return sf::Vector2f(deg45radians, -deg45radians);
    case Direction::down:
      return sf::Vector2f(0, 1);
    case Direction::down_left:
      return sf::Vector2f(-deg45radians, deg45radians);
    case Direction::down_right:
      return sf::Vector2f(deg45radians, deg45radians);
  }

  return sf::Vector2f();
}

inline Direction FromString(const std::string_view& direction) {
  constexpr std::string_view left = "Left";
  constexpr std::string_view right = "Right";
  constexpr std::string_view up = "Up";
  constexpr std::string_view down = "Down";
  constexpr std::string_view up_left = "Up Left";
  constexpr std::string_view up_right = "Up Right";
  constexpr std::string_view down_left = "Down Left";
  constexpr std::string_view down_right = "Down Right";

  if (stx::insensitive_equals(direction, left)) {
    return Direction::left;
  }
  else if (stx::insensitive_equals(direction, right)) {
    return Direction::right;
  }
  else if (stx::insensitive_equals(direction, up)) {
    return Direction::up;
  }
  else if (stx::insensitive_equals(direction, down)) {
    return Direction::down;
  }
  else if (stx::insensitive_equals(direction, up_left)) {
    return Direction::up_left;
  }
  else if (stx::insensitive_equals(direction, up_right)) {
    return Direction::up_right;
  }
  else if (stx::insensitive_equals(direction, down_left)) {
    return Direction::down_left;
  }
  else if (stx::insensitive_equals(direction, down_right)) {
    return Direction::down_right;
  }

  return Direction::none;
}
