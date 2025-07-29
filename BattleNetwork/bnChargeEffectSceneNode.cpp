#include "bnEntity.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnShaderResourceManager.h"
#include "bnChargeEffectSceneNode.h"

ChargeEffectSceneNode::ChargeEffectSceneNode(Entity* _entity) {
  entity = _entity;
  charging = false;
  chargeCounter = frames(0) ;
  setScale(0.f, 0.f);
  setTexture(Textures().LoadFromFile(TexturePaths::SPELL_BUSTER_CHARGE));

  animation = Animation("resources/scenes/battle/spells/spell_buster_charge.animation");

  isCharged = isPartiallyCharged = false;
  chargeColor = sf::Color::Magenta;
}

ChargeEffectSceneNode::~ChargeEffectSceneNode() {
}

void ChargeEffectSceneNode::Update(double _elapsed) {
  if (charging) {
    chargeCounter += from_seconds(_elapsed);
    if (chargeCounter >= maxChargeTime + i10) {
      if (isCharged == false) {
        // We're switching states
        setColor(chargeColor);
        Audio().Play(AudioType::BUSTER_CHARGED);
        animation.SetAnimation("CHARGED");
        animation << Animator::Mode::Loop;
        SetShader(Shaders().GetShader(ShaderType::ADDITIVE));
      }

      isCharged = true;      
    } else if (chargeCounter >= i10) {
      if (isPartiallyCharged == false) {
        // Switching states
        Audio().Play(AudioType::BUSTER_CHARGING);
        animation.SetAnimation("CHARGING");
        animation << Animator::Mode::Loop;
        setColor(sf::Color::White);
        RevokeShader();

      }

      isPartiallyCharged = true;
    }

    if (isPartiallyCharged) {
      setScale(1.0f, 1.0f);
    }
  }

  Animate(_elapsed);
}

void ChargeEffectSceneNode::Animate(double _elapsed) {
  animation.Update(_elapsed, getSprite());
}

void ChargeEffectSceneNode::SetCharging(bool _charging) {
  charging = _charging;

  if (!charging) {
    chargeCounter = frames(0);
    setScale(0.0f, 0.0f);
    isCharged = false;
    isPartiallyCharged = false;
  }
}

void ChargeEffectSceneNode::SetMaxChargeTime(const frame_time_t& max)
{
  this->maxChargeTime = max;
}

frame_time_t ChargeEffectSceneNode::GetChargeTime() const {
  return chargeCounter;
}

const bool ChargeEffectSceneNode::IsFullyCharged() const
{
  return isCharged;
}

void ChargeEffectSceneNode::SetFullyChargedColor(const sf::Color color)
{
  chargeColor = color;
}
