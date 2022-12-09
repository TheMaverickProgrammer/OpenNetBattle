#pragma once
#include <array>
#include "bnRenderEvents.h"
#include "../bnEntity.h"

namespace sf {
  class Drawable;
}

/**
@class ClassicRenderer
@brief ONB default renderer
*/
class ClassicRenderer : public Renderer<UI, Layered, LayeredSprite, LayeredNode, Entity> {
  sf::RenderTexture ui, objects;
  std::list<sf::Sprite> sprBuffer;
  std::list<const RenderSource*> rBuffer;
  std::array<std::array<const RenderSource*, 100>, (size_t)LayerID::sz> layers{};
  std::array<size_t, 100> count{};
  bool validLayer(size_t index);
public:
  ClassicRenderer(const sf::View view);

  void draw() override;
  sf::RenderTexture& getRenderTextureTarget() override;
  void clear(sf::Color color) override;
  void setView(const sf::View& view) override;
  void onEvent(const RenderSource& event) override;
  void onEvent(const Entity& event) override;
  void onEvent(const UI& event) override;
  void onEvent(const Layered& event) override;
  void onEvent(const LayeredSprite& event) override;
  void onEvent(const LayeredNode& event) override;
};