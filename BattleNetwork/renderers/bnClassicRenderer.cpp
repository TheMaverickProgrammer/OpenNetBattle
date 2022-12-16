#include "bnClassicRenderer.h"

bool ClassicRenderer::validLayer(size_t index)
{
  return index >= 0 && index <= layers.size() && (count[index]+1 < layers[0].size());
}

ClassicRenderer::ClassicRenderer(const sf::View view) {
  const unsigned int ux = (unsigned int)view.getSize().x;
  const unsigned int uy = (unsigned int)view.getSize().y;
  ui.create(ux, uy);
  objects.create(ux, uy);
  sprBuffer.resize(200);
  rBuffer.resize(200);
}

void ClassicRenderer::draw() {
  // Draw the objects in order by their layer
  for (size_t i = 0; i < layers.size(); i++) {
    for (size_t j = 0; j < count[i]; j++) {
      const RenderSource& r = *layers[i][j];
      objects.draw(*r.drawable(), r.states());
    }
  }

  ui.display();
  sf::Sprite uiLayer = sf::Sprite(ui.getTexture());
  objects.draw(uiLayer);

  for (size_t& c : count) {
    c = 0;
  }

  sprBuffer.clear();
  rBuffer.clear();

  // NOTE: have to clear because swoosh submits segue output screen contents back to the renderer
  //       before a final `renderer->draw()` call. IMO segues should draw the output onto the screen skipping the last `draw()`...
  //ui.clear(sf::Color::Transparent);
}

sf::RenderTexture& ClassicRenderer::getRenderTextureTarget() {
  return objects;
}

void ClassicRenderer::clear(sf::Color color) {
  ui.clear(sf::Color::Transparent);
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
  size_t index = event.GetLayer();
  if (!validLayer(index)) return;

  auto& L = layers[index];
  rBuffer.insert(rBuffer.end(), new const RenderSource(&event));
  L[count[index]++] = rBuffer.back();
}

void ClassicRenderer::onEvent(const UI& event)
{
  ui.draw(*event.drawable(), event.states());
  /*size_t index = (size_t)LayerID::layer_5;
  if (!validLayer(index)) return;

  auto& L = layers[index];
  rBuffer.insert(rBuffer.end(), new const RenderSource(event.drawable(), event.states()));
  L[count[(size_t)LayerID::layer_5]++] = rBuffer.back();*/
}

void ClassicRenderer::onEvent(const Layered& event)
{
  size_t index = static_cast<size_t>(event.id);
  if (!validLayer(index)) return;

  auto& L = layers[index];
  rBuffer.insert(rBuffer.end(), new const RenderSource(event.drawable(), event.states()));
  L[count[index]++] = rBuffer.back();
}

void ClassicRenderer::onEvent(const LayeredSprite& event)
{
  size_t index = static_cast<size_t>(event.id);
  if (!validLayer(index)) return;

  auto& L = layers[index];
  sprBuffer.push_back(event.spr);
  rBuffer.insert(rBuffer.end(), new const RenderSource(&sprBuffer.back(), event.states()));
  L[count[index]++] = rBuffer.back();
}

void ClassicRenderer::onEvent(const LayeredNode& event)
{
  size_t index = static_cast<size_t>(event.id);
  if (!validLayer(index)) return;

  auto& L = layers[index];
  rBuffer.insert(rBuffer.end(), new const RenderSource(event.drawable(), event.states()));
  L[count[index]++] = rBuffer.back();
}
