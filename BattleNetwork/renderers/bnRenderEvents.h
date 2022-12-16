#pragma once
#include <Swoosh/Renderers/Renderer.h>
#include <Swoosh/Shaders.h>
#include <SFML/Graphics/RenderTexture.hpp>
#include "../bnSpriteProxyNode.h"
using swoosh::Renderer;
using swoosh::RenderSource;
using swoosh::glsl::deferred::MeshData;
using swoosh::glsl::deferred::MeshPass;
using swoosh::glsl::deferred::LightPass;
using swoosh::glsl::deferred::PositionPass;
using swoosh::glsl::deferred::EmissivePass;

/**
  @class Draw3D
  @brief A render event with material data that can render 2D graphics as psuedo-3D
*/
struct Draw3D : RenderSource {
  MeshData data; // !< aggregate data for the 2D model
  SpriteProxyNode* spr{ nullptr };
  explicit Draw3D(SpriteProxyNode* spr,
    sf::Texture* normal,
    sf::Texture* emissive = nullptr,
    float metallic = 0,
    float specular = 0) :
    spr(spr),
    RenderSource(spr),
    data({ &spr->getSprite(), normal, emissive, metallic, specular })
  {}

  Draw3D WithZ(float z) {
    data.WithZ(z);
    return *this;
  }
};

/**
  @class Light
  @brief A render event with lighting data for rendering lights
  If used in a SimpleRenderer, will draw the light as a circle
*/
struct Light : RenderSource {
  float radius{};
  sf::Vector3f position{};
  sf::Color color{ sf::Color::White };
  sf::CircleShape circle{};
  float specular{};
  float cutoff{};

  explicit Light(float radius,
    sf::Vector3f position,
    sf::Color color,
    float specular = 0.0f,
    float cutoff = 0.0f) :
    RenderSource(&circle),
    radius(radius),
    position(position),
    color(color),
    specular(specular),
    cutoff(cutoff)
  {
    circle.setPointCount(360);
    circle.setRadius(radius);
    circle.setFillColor(color);
    circle.setPosition({ position.x, position.y });

    sf::FloatRect bounds = circle.getLocalBounds();
    circle.setOrigin(bounds.width / 2, bounds.height / 2);
  };
};

/**
  @brief Transform 2D vector into a 3D vector
  @param xy a 2D vector
  @param z a float representing the 3rd dimension
  @return sf::Vector3f with a z value
*/
static sf::Vector3f WithZ(const sf::Vector2f xy, float z) {
  return sf::Vector3f(xy.x, xy.y, z);
}

struct UI : RenderSource {
  UI(const sf::Drawable* dptr, const sf::RenderStates states = sf::RenderStates::Default) : RenderSource(dptr, states) {}
  ~UI() {}
};

enum class LayerID : size_t {
  bg = 0,

  // used for battle
  floor  = 1,
  objs   = 2,
  spells = 3,
  fx     = 4,
  xtra   = 5, // attachments

  // everything else
  layer_1 = 1,
  layer_2 = 2,
  layer_3 = 3,
  layer_4 = 4,
  layer_5 = 5,

  // denotes how many logical layers there are
  sz = 6
};

struct Layered : RenderSource {
  const LayerID id{};
  const sf::Drawable* dptr{ nullptr };
  Layered(LayerID id, const sf::Drawable* dptr, const sf::RenderStates states = sf::RenderStates::Default) : id(id), RenderSource(dptr, states), dptr(dptr) {}
  ~Layered() {}
};

struct LayeredNode : RenderSource {
  const LayerID id{};
  const SpriteProxyNode* sNodePtr{ nullptr };
  const sf::Vector2f pos;
  LayeredNode(LayerID id, const SpriteProxyNode* ptr, const sf::RenderStates states = sf::RenderStates::Default) 
    : id(id), 
    RenderSource(ptr, states), 
    pos(ptr->getPosition()),
    sNodePtr(ptr) {}
  ~LayeredNode() {}
};

struct LayeredSprite : RenderSource {
  const LayerID id{};
  const sf::Sprite spr;
  LayeredSprite(LayerID id, sf::Sprite og, const sf::RenderStates states = sf::RenderStates::Default) : id(id), spr(og), RenderSource(&spr, states) {}
  ~LayeredSprite() {}
};