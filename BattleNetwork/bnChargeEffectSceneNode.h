#pragma once
#include "bnResourceHandle.h"
#include "bnAnimation.h"
#include "bnLayered.h"

#include <SFML/Graphics.hpp>

using sf::CircleShape;
using sf::Sprite;
using sf::Drawable;
using sf::Texture;
using sf::IntRect;
class Entity;

/**
 * @class ChargeEffectSceneNode
 * @author mav
 * @date 05/05/19
 * @brief Draws on top of attached entity
 */
class ChargeEffectSceneNode : public SpriteProxyNode, public ResourceHandle {
public:
  ChargeEffectSceneNode(Entity* _entity);
  ~ChargeEffectSceneNode();

  void Update(double _elapsed);
  void Animate(double _elapsed);
  
  /**
   * @brief If true, the component begins to charge. Otherwise, cancels charge
   * @param _charging
   */
  void SetCharging(bool _charging);

  /**
   * @brief change the max charge time (in frames)
   * @param max number of frames that is the max
   */
  void SetMaxChargeTime(const frame_time_t& max);

  /**
   * @brief Query the number of frames this charge has been effect
   * @return frame_time_t number frames
   */
  frame_time_t GetChargeTime() const;
  
  /**
   * @brief Check full charge time
   * @return true if the charge component is at peak charge time
   */
  const bool IsFullyCharged() const;

  void SetFullyChargedColor(const sf::Color color);

private:
  Entity* entity{ nullptr };
  bool charging{};
  bool isCharged{};
  bool isPartiallyCharged{};
  frame_time_t chargeCounter{};
  frame_time_t maxChargeTime{ frames(100) };
  const frame_time_t i10{ frames(10) };
  Animation animation;
  sf::Color chargeColor;
};