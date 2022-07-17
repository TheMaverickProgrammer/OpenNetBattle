#include "bnText.h"
#include <cmath>
#include <cctype> // for control codes

#include <Poco/UTF8Encoding.h>
#include <Poco/TextIterator.h>

void Text::AddLetterQuad(sf::Vector2f position, const sf::Color & color, uint32_t letter) const
{
  font.SetLetter(letter);
  const auto texcoords = font.GetTextureCoords();
  const auto origin = font.GetOrigin();
  const sf::Texture& texture = font.GetTexture();
  float width  = static_cast<float>(texture.getSize().x);
  float height = static_cast<float>(texture.getSize().y);

  float left   = 0;
  float top    = 0;
  float right  = static_cast<float>(texcoords.width);
  float bottom = static_cast<float>(texcoords.height);

  // fit tall letters on the same line
  float offset = texcoords.height - font.GetLineHeight();
  if (offset > 0) offset = 0;
  position.y = position.y + std::fabs(offset);

  // sfml uses screen space dimensions, not GL 0.0 -> 1.0 space...
  float u1 = static_cast<float>(texcoords.left);
  float v1 = static_cast<float>(texcoords.top);
  float u2 = static_cast<float>(texcoords.left + texcoords.width);
  float v2 = static_cast<float>(texcoords.top + texcoords.height);

  position -= origin;

  vertices.append(sf::Vertex(sf::Vector2f(position.x, position.y + bottom), color, sf::Vector2f(u1, v2)));
  vertices.append(sf::Vertex(sf::Vector2f(position.x, position.y), color, sf::Vector2f(u1, v1)));
  vertices.append(sf::Vertex(sf::Vector2f(position.x + right, position.y + bottom), color, sf::Vector2f(u2, v2)));
  vertices.append(sf::Vertex(sf::Vector2f(position.x, position.y), color, sf::Vector2f(u1, v1)));
  vertices.append(sf::Vertex(sf::Vector2f(position.x + right, position.y + bottom), color, sf::Vector2f(u2, v2)));
  vertices.append(sf::Vertex(sf::Vector2f(position.x + right, position.y), color, sf::Vector2f(u2, v1)));
}

void Text::UpdateGeometry() const
{
  if (!geometryDirty) return;

  vertices.clear();
  bounds = sf::FloatRect();

  if (message.empty()) return; // nothing to draw

  // Precompute the variables needed by the algorithm
  float whitespaceWidth = font.GetWhiteSpaceWidth();
  whitespaceWidth += letterSpacing;
  float lineSpacing = font.GetLineHeight() * Text::lineSpacing;
  float x = 0.f;
  float y = 0.f;
  float width = 0.f;

  Poco::UTF8Encoding utf8Encoding;
  Poco::TextIterator begin(message, utf8Encoding);
  Poco::TextIterator end(message);
  Poco::TextIterator it = begin;

  for (; it != end; ++it) {
    uint32_t letter = *it;

    // Handle special characters
    if ((letter == U' ') || (letter == U'\n') || (letter == U'\t'))
    {
      switch (letter)
      {
      case U' ':  x += whitespaceWidth;     break;
      case U'\t': x += whitespaceWidth * 4; break;
      case U'\n': y += lineSpacing; x = 0;  break;
      }
    } else {
      // skip user-defined control codes
      if (letter > 0 && letter <= 0xff && iscntrl(letter)) continue;

      AddLetterQuad(sf::Vector2f(x, y), color, letter);

      if (it != begin) {
        x += letterSpacing;
      }

      x += font.GetLetterWidth();
    }

    width = std::max(x, width);
  }

  // Update the bounding rectangle
  bounds.left = 0;
  bounds.top = 0;
  bounds.width = width;
  bounds.height = y + lineSpacing;

  geometryDirty = false;
}

Text::Text(const Font& font) : font(font), message(""), geometryDirty(true)
{
  letterSpacing = 1.0f;
  lineSpacing = 1.0f;
  color = sf::Color::White;
  vertices.setPrimitiveType(sf::PrimitiveType::Triangles);
}

Text::Text(const std::string& message, const Font& font) : font(font), message(message), geometryDirty(true)
{
  letterSpacing = 1.0f;
  lineSpacing = 1.0f;
  color = sf::Color::White;
  vertices.setPrimitiveType(sf::PrimitiveType::Triangles);
}

Text::Text(const Text& rhs) : font(rhs.font)
{
  letterSpacing = rhs.letterSpacing;
  lineSpacing = rhs.lineSpacing;
  message = rhs.message;
  color = rhs.color;
  bounds = rhs.bounds;
  vertices = rhs.vertices;
  geometryDirty = rhs.geometryDirty;
}

Text::~Text()
{
}

void Text::draw(sf::RenderTarget & target, sf::RenderStates states) const
{
  UpdateGeometry();

  states.transform *= getTransform();
  states.texture = &font.GetTexture();

  target.draw(vertices, states);
}

void Text::SetFont(const Font& font)
{
  geometryDirty |= Text::font.GetStyle() != font.GetStyle();
  Text::font = font;
}

void Text::SetString(const std::string& message)
{
  geometryDirty |= Text::message != message;
  Text::message = message;
}

void Text::SetString(char c)
{
  geometryDirty |= Text::message != std::to_string(c);
  Text::message = std::string(1, c);
}

void Text::SetColor(const sf::Color & color)
{
  geometryDirty |= Text::color == color;

  Text::color = color;

  // Change vertex colors directly, no need to update whole geometry
  // (if geometry is updated anyway, we can skip this step)
  if (!geometryDirty)
  {
    for (std::size_t i = 0; i < vertices.getVertexCount(); ++i)
      vertices[i].color = Text::color;
  }
}

void Text::SetLetterSpacing(float spacing)
{
  geometryDirty |= letterSpacing != spacing;

  letterSpacing = spacing;
}

void Text::SetLineSpacing(float spacing)
{
  geometryDirty |= lineSpacing != spacing;

  lineSpacing = spacing;
}

const std::string& Text::GetString() const
{
  return message;
}

const Font& Text::GetFont() const
{
  return font;
}

const Font::Style& Text::GetStyle() const
{
  return font.GetStyle();
}

float Text::GetLineSpacing() const
{
  return letterSpacing;
}

sf::FloatRect Text::GetLocalBounds() const
{
  UpdateGeometry();

  return bounds;
}

sf::FloatRect Text::GetWorldBounds() const
{
  return getTransform().transformRect(GetLocalBounds());
}
