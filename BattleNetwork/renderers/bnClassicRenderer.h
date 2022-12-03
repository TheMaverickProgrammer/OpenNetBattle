#pragma once
#include <Swoosh/Renderers/Renderer.h>
#include "../bnEntity.h"

using swoosh::Renderer;
using swoosh::RenderSource;

struct UI : RenderSource {
  UI(const sf::Drawable* dptr, const sf::RenderStates states = sf::RenderStates::Default) : RenderSource(dptr, states) {}
  ~UI() {}
};

struct Layered {

};

/**
@class ClassicRenderer
@brief ONB default renderer
*/
class ClassicRenderer : public Renderer<UI, Entity> {
  sf::RenderTexture ui, objects;
  std::list<const Entity*> entities;
public:
  ClassicRenderer(const sf::View view);

  void draw() override;
  sf::RenderTexture& getRenderTextureTarget() override;
  void clear(sf::Color color) override;
  void setView(const sf::View& view) override;
  void onEvent(const RenderSource& event) override;
  void onEvent(const Entity& event) override;
  void onEvent(const UI& event) override;
};