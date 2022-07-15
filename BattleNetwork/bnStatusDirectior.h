#pragma once

#include "bnHitProperties.h"
#include "bnFrameTimeUtils.h"
#include "bnSpriteProxyNode.h"
#include "bnShaderResourceManager.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"

class Entity;

struct AppliedStatus {
    Hit::Flags statusFlag;
    frame_time_t remainingTime;
};

class StatusBehaviorDirector {
public:
    StatusBehaviorDirector();
    virtual ~StatusBehaviorDirector();
    void AddStatus(Hit::Flags statusFlag, frame_time_t maxCooldown, bool deffer);
    std::vector<AppliedStatus> GetStatusList(bool isPrevious);
    AppliedStatus HasStatus(Hit::Flags flag, bool isPrevious);
    void Initialize(Entity*);
    void UpdateAnimations(double _elapsed);
private:
    Entity* owner;
    ResourceHandle manager;
    std::vector<AppliedStatus> previousStatus;
    std::vector<AppliedStatus> currentStatus;
    std::shared_ptr<SpriteProxyNode> iceFx{ nullptr };
    std::shared_ptr<SpriteProxyNode> blindFx{ nullptr };
    std::shared_ptr<SpriteProxyNode> confusedFx{ nullptr };
    frame_time_t stunCooldown{ 0 }; /*!< Timer until stun is over */
    frame_time_t rootCooldown{ 0 }; /*!< Timer until root is over */
    frame_time_t freezeCooldown{ 0 }; /*!< Timer until freeze is over */
    frame_time_t blindCooldown{ 0 }; /*!< Timer until blind is over */
    frame_time_t confusedCooldown{ 0 }; /*!< Timer until confusion is over */
    frame_time_t confusedSfxCooldown{ 0 }; /*!< Timer to replay confusion SFX */
    Animation iceFxAnimation, blindFxAnimation, confusedFxAnimation;
};