#pragma once
#include "bnElements.h"
#include "bnDirection.h"

// forward declare
using EntityID_t = long;

namespace Hit {
  using Flags = uint32_t;

  const Flags none       = 0x00000000;
  const Flags retangible = 0x00000001;
  const Flags freeze     = 0x00000002;
  const Flags pierce     = 0x00000004;
  const Flags flinch     = 0x00000008;
  const Flags shake      = 0x00000010;
  const Flags stun       = 0x00000020;
  const Flags flash      = 0x00000040;
  const Flags breaking   = 0x00000080; // NOTE: this is what we refer to as "true breaking"
  const Flags impact     = 0x00000100;
  const Flags drag       = 0x00000200;
  const Flags bubble     = 0x00000400;
  const Flags no_counter = 0x00000800;
  const Flags root       = 0x00001000;
  const Flags blind      = 0x00002000;

  struct Drag {
    Direction dir{ Direction::none };
    unsigned count{ 0 };
  };

  /**
   * @brief Context for creation, carries properties that Hit::Properties will automatically copy
   */
  struct Context {
    EntityID_t aggressor{};
    Flags flags{};
  };

  /**
   * @struct Properties
   * @author mav
   * @date 05/05/19
   * @brief Hit box information
   */
  struct Properties {
    int damage{};
    Flags flags{ Hit::none };
    Element element{ Element::none };
    EntityID_t aggressor{};
    Drag drag{ }; // Used by Hit::drag flag
    Context context{};
  };

  const constexpr Hit::Properties DefaultProperties = { 
    0, 
    Flags(Hit::flinch | Hit::impact), 
    Element::none, 
    0, 
    Direction::none,
    true
  };
}