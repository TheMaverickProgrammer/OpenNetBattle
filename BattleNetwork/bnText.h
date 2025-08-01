#pragma once
#include "bnSceneNode.h"
#include "bnFont.h"

#include <cstdint>

class Text : public SceneNode {
private:
  mutable Font font;
  float letterSpacing, lineSpacing;
  std::string message;
  sf::Color color;
  mutable sf::FloatRect bounds;
  mutable sf::VertexArray vertices;
  mutable bool geometryDirty; //!< Flag if text needs to be recomputed due to a change in properties

  // Add a glyph quad to the vertex array
  void AddLetterQuad(sf::Vector2f position, const sf::Color& color, uint32_t letter) const;

  // Computes geometry before draw
  void UpdateGeometry() const;

public:
  Text(const Font& font);
  Text(const std::string& message, const Font& font);
  Text(const Text& rhs);
  Text& operator=(const Text& rhs) = default;
  virtual ~Text();

  virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
  void SetFont(const Font& font);
  void SetString(const std::string& message);
  void SetString(char c);
  void SetColor(const sf::Color& color);
  void SetLetterSpacing(float spacing);
  void SetLineSpacing(float spacing);
  const std::string& GetString() const;
  const Font& GetFont() const;
  const Font::Style& GetStyle() const;
  sf::FloatRect GetLocalBounds() const;
  sf::FloatRect GetWorldBounds() const;
};