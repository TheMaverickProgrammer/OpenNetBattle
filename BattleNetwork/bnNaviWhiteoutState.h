
/*! \brief This state can be used by any Entity in the engine.
 *
 *
 * This state spawns a navi shine, turns the entity white,
 * and fades them out just like the player character would experience
 * according to source material
 */

#pragma once
#include "bnShineExplosion.h"
#include "bnAudioResourceManager.h"
#include "bnShaderResourceManager.h"
#include "bnTextureResourceManager.h"

template<typename Any>
class NaviWhiteoutState : public AIState<Any>
{
protected:
    float factor; /*!< Strength of the pixelate effect. Set to 125 */
    std::shared_ptr<ShineExplosion> shine; /*!< Shine X that appears over navi ranked enemies */
    bool fadeout; /*!< If true, begin fading out*/
public:
    inline static const int PriorityLevel = 0; // Highest

    /**
     * @brief Constructor

     */
    NaviWhiteoutState();

    /**
     * @brief deconstructor
     */
    virtual ~NaviWhiteoutState();

    /**
     * @brief Calls when the entity enters this state
     * @param e entity
     *
     * Adds shine effect on field
     */
    void OnEnter(Any& e);

    /**
     * @brief if e is deleted, removes the shine from the field
     * @param _elapsed in seconds
     * @param e entity
     */
    void OnUpdate(double _elapsed, Any& e);

    /**
     * @brief Calls when leaving the state
     * @param e entity
     */
    void OnLeave(Any& e);
};

template<typename Any>
NaviWhiteoutState<Any>::NaviWhiteoutState() : shine(nullptr), factor(125.f), fadeout(false)
{
  this->PriorityLock();
}

template<typename Any>
NaviWhiteoutState<Any>::~NaviWhiteoutState() {
    /* shine artifact is deleted by field */
}

template<typename Any>
void NaviWhiteoutState<Any>::OnEnter(Any& e) {
    e.EnableHitbox(false); // Shoot through dying enemies

    /* Spawn shine artifact */
    Battle::Tile* tile = e.GetTile();
    std::shared_ptr<Field> field = e.GetField();
    shine = std::make_shared<ShineExplosion>();
    shine->Init();

    // ShineExplosion loops, we just want to play once and delete
    auto animComponent = shine->GetFirstComponent<AnimationComponent>();
    std::string animStr = animComponent->GetAnimationString();
    animComponent->SetAnimation(animStr, [this]() {
        shine->Erase();
        fadeout = true;
        ResourceHandle().Audio().Play(AudioType::DELETED);
    });

    field->AddEntity(shine, tile->GetX(), tile->GetY());

    auto animation = e.template GetFirstComponent<AnimationComponent>();

    if (animation) {
        animation->SetPlaybackSpeed(0);
        animation->CancelCallbacks();
    }

    e.Reveal();
}

template<typename Any>
void NaviWhiteoutState<Any>::OnUpdate(double _elapsed, Any& e) {
    float range = factor / 125.f;

    // this sets the alpha channel
    e.setColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(255 * range)));

    // this makes all pixels white
    e.SetShader(e.Shaders().GetShader(ShaderType::WHITE));

    if (!fadeout) return;

    factor -= static_cast<float>(_elapsed) * 180.f;

    if (factor <= 0.f) {
        factor = 0.f;
        e.Erase();
    }
}

template<typename Any>
void NaviWhiteoutState<Any>::OnLeave(Any& e) {
}
