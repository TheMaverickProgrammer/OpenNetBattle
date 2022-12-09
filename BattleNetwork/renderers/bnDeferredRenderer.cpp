#include "bnDeferredRenderer.h"

DeferredRenderer::DeferredRenderer(const sf::View view) {
  const unsigned int ux = (unsigned int)view.getSize().x;
  const unsigned int uy = (unsigned int)view.getSize().y;
  const sf::Vector2u size = sf::Vector2u(ux, uy);

  position.create(size.x, size.y);
  diffuse.create(size.x, size.y);
  normal.create(size.x, size.y);
  esm.create(size.x, size.y);
  out.create(size.x, size.y);
  back.create(size.x, size.y);

  meshShader.configure(&diffuse, &normal, &esm);
  positionShader.configure(-100, 100, view, &position);
  lightShader.configure(view, &position, &diffuse, &normal, &esm);
  emissiveShader.configure(&diffuse, &esm);
}

void DeferredRenderer::draw() {
  // draw forward rendered content
  for (RenderSource& source : memForward) {
    back.draw(*source.drawable());
  }

  mem3D.sort([](Draw3D& a, Draw3D& b) { return a.data.getPosition3D().z < b.data.getPosition3D().z; });

  for (Draw3D& source : mem3D) {

    // bake the currect normals
    sf::Vector2f originalPos = source.data.sprite->getPosition();
    sf::Vector2f originalScl = source.data.sprite->getScale();
    source.data.sprite->setPosition(source.spr->getTransform().transformPoint(originalPos));
    source.data.sprite->setScale(source.spr->getScale());
    meshShader.setMeshData(source.data);
    meshShader.apply(*this);

    // bake the position data
    positionShader.setMeshData(source.data);
    positionShader.apply(*this);

    // revert
    source.data.sprite->setPosition(originalPos);
    source.data.sprite->setScale(originalScl);
  }

  lightShader.clearLights();
  for (Light& source : memLight) {
    lightShader.addLight(source.radius,
      source.position,
      source.color,
      source.specular,
      source.cutoff);
  }

  // render light geometry over the scene
  lightShader.apply(*this);

  // draw emissive lighting
  emissiveShader.apply(*this);

  // TODO: I only did this for stream to get 3D lighting render on top of background!
  out.display();
  sf::Sprite spr = sf::Sprite(out.getTexture());
  back.draw(spr);
  back.display();
  spr = sf::Sprite(back.getTexture());
  out.draw(spr);

  // delay clearing so lighting pass doesn't erase our background colors
  back.clear();

  // clear the buffer data
  memForward.clear();
  mem3D.clear();
  memLight.clear();
}

void DeferredRenderer::clear(sf::Color color) {
  // for G-buffers the clear color must always be transparent
  position.clear(sf::Color::Transparent);
  diffuse.clear(sf::Color::Transparent);
  normal.clear(sf::Color::Transparent);
  esm.clear(sf::Color::Transparent);
  out.clear(sf::Color::Transparent);
}

void DeferredRenderer::setView(const sf::View& view) {
  position.setView(view);
  diffuse.setView(view);
  normal.setView(view);
  esm.setView(view);
  out.setView(view);
  back.setView(view);
}

sf::RenderTexture& DeferredRenderer::getRenderTextureTarget() {
  return out;
}

void DeferredRenderer::onEvent(const RenderSource& event) {
  memForward.push_back(event);
}

void DeferredRenderer::onEvent(const Draw3D& event) {
  mem3D.emplace_back(event);
}

void DeferredRenderer::onEvent(const Light& event) {
  memLight.emplace_back(event);
}