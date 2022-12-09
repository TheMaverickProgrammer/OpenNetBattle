#pragma once
#include <functional>
#include <array>
#include <optional>
#include "bnRenderEvents.h"

/**
  @class DeferredRenderer
  @brief A custom renderer example that uses swoosh shaders to compose a 3D scene
  Uses `Draw3D` render event as a tag to draw the sprite multiple times with different G-buffer texture values
  Uses `Light` render event as a tag to calculate the final lighting in the scene
  The end result is a partial implementation of a deferred renderer commonly used in advanced 3D applications
*/
class DeferredRenderer : public Renderer<Draw3D, Light> {
  sf::RenderTexture back, position, diffuse, normal, esm, out;
  PositionPass positionShader;
  LightPass lightShader;
  EmissivePass emissiveShader;
  MeshPass meshShader;
  std::list<RenderSource> memForward;
  std::list<Draw3D> mem3D;
  std::list<Light> memLight;

public:
  DeferredRenderer(const sf::View view);

  void draw() override;

  void clear(sf::Color color) override;

  void setView(const sf::View& view) override;

  sf::RenderTexture& getRenderTextureTarget() override;

  void onEvent(const RenderSource& event) override;

  void onEvent(const Draw3D& event) override;

  void onEvent(const Light& event) override;
};