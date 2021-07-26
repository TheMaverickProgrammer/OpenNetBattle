#ifdef BN_MOD_SUPPORT
#include "bnScriptedSpell.h"

ScriptedSpell::ScriptedSpell(Team _team) : 
  Spell(_team) {
  setScale(2.f, 2.f);

  shadow = new SpriteProxyNode();
  shadow->setTexture(LOAD_TEXTURE(MISC_SHADOW));
  shadow->SetLayer(1);
  shadow->Hide(); // default: hidden
  AddNode(shadow);

  animComponent = CreateComponent<AnimationComponent>(this);
}

ScriptedSpell::~ScriptedSpell() {
  delete shadow;
}

bool ScriptedSpell::CanMoveTo(Battle::Tile * next)
{
  return canMoveToCallback? canMoveToCallback(*next) : false;
}

void ScriptedSpell::OnUpdate(double _elapsed) {
  setPosition(tile->getPosition().x + Entity::tileOffset.x + ScriptedSpell::scriptedOffset.x,
    tile->getPosition().y - this->height + Entity::tileOffset.y + ScriptedSpell::scriptedOffset.y);

  //shadow->setPosition(0, +GetHeight()); // counter offset the shadow node
  ScriptedSpell& ss = *this;
  updateCallback ? updateCallback(ss, _elapsed) : (void)0;

}

void ScriptedSpell::OnDelete() {
  ScriptedSpell& ss = *this;
  deleteCallback ? deleteCallback(ss) : (void)0;
  Remove();
}

void ScriptedSpell::OnCollision(const Character* other)
{
  ScriptedSpell& ss = *this;
  collisionCallback ? collisionCallback(ss, const_cast<Character&>(*other)) : (void)0;
}

void ScriptedSpell::Attack(Character* other) {
  other->Hit(GetHitboxProperties());
  ScriptedSpell& ss = *this;
  attackCallback ? attackCallback(ss, *other) : (void)0;
}

void ScriptedSpell::OnSpawn(Battle::Tile& spawn)
{
  ScriptedSpell& ss = *this;
  spawnCallback ? spawnCallback(ss, spawn) : (void)0;
}

const float ScriptedSpell::GetHeight() const
{
  return height;
}

void ScriptedSpell::SetHeight(const float height)
{
  this->height = height;
}

void ScriptedSpell::ShowShadow(const bool show)
{
  if (!show) {
    this->shadow->Hide();
  }
  else {
    this->shadow->Reveal();
  }
}

Animation& ScriptedSpell::GetAnimationObject()
{
  return animComponent->GetAnimationObject();
}

const sf::Vector2f& ScriptedSpell::GetTileOffset() const
{
  return ScriptedSpell::scriptedOffset;
}

void ScriptedSpell::SetTileOffset(float x, float y)
{
  ScriptedSpell::scriptedOffset = { x, y };
}

void ScriptedSpell::ShakeCamera(double power, float duration)
{
  this->EventChannel().Emit(&Camera::ShakeCamera, power, sf::seconds(duration));
}
#endif