#include "bnClassicRenderer.h"

ClassicRenderer::ClassicRenderer(const sf::View view) {
  const unsigned int ux = (unsigned int)view.getSize().x;
  const unsigned int uy = (unsigned int)view.getSize().y;
  ui.create(ux, uy);
  objects.create(ux, uy);
}

void ClassicRenderer::draw() {
  // Draw the entities in order by their layer
  entities.sort([](const Entity* a, const Entity* b) { return a->GetLayer() < b->GetLayer(); });
  for (const Entity* e : entities) {
    objects.draw(*e);
  }

  ui.display();
  sf::Sprite uiLayer = sf::Sprite(ui.getTexture());
  objects.draw(uiLayer);
}

sf::RenderTexture& ClassicRenderer::getRenderTextureTarget() {
  return objects;
}

void ClassicRenderer::clear(sf::Color color) {
  ui.clear(color);
  objects.clear(color);
}

void ClassicRenderer::setView(const sf::View& view) {
  ui.setView(view);
  objects.setView(view);
}

void ClassicRenderer::onEvent(const RenderSource& event) {
  objects.draw(*event.drawable(), event.states());
}

void ClassicRenderer::onEvent(const Entity& event)
{
  entities.insert(entities.begin(), &event);
}

void ClassicRenderer::onEvent(const UI& event)
{
  ui.draw(*event.drawable(), event.states());
}
