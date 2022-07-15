#include "bnStatusDirectior.h"
#include "bnEntity.h"

StatusBehaviorDirector::StatusBehaviorDirector()
{
    // At creation, make sure both statuses are default.
    previousStatus = std::vector<AppliedStatus>{};
    currentStatus = std::vector<AppliedStatus>{};
}

void StatusBehaviorDirector::AddStatus(Hit::Flags statusFlag, frame_time_t maxCooldown, bool deffer) {
    // Check if we already have it. We might be overwriting a cooldown.
    AppliedStatus statusToCheck = HasStatus(statusFlag, deffer);
    if (statusToCheck.statusFlag == Hit::none) {
        // We didn't find the status, so create the new one.
        AppliedStatus statusToSet{ statusFlag, maxCooldown };
        // Add the current status to the vector.
        currentStatus.push_back(statusToSet);
    }
    else {

    }
    if (deffer) {
        stunCooldown = frames(0);
        freezeCooldown = frames(0);
        iceFx->Hide();
        // If necessary, set the previous status to the current status vector.
        previousStatus = currentStatus;
    }
};

std::vector<AppliedStatus> StatusBehaviorDirector::GetStatusList(bool isPrevious) {
    // If the previous status is being called, grab it.
    if (isPrevious) {
        return previousStatus;
    }
    // Otherwise return the current.
    return currentStatus;
}

AppliedStatus StatusBehaviorDirector::HasStatus(Hit::Flags flag, bool isPrevious)
{
   std::vector<AppliedStatus> list = isPrevious ? previousStatus : currentStatus;
   for (int i = 0; list.size(); i++) {
       AppliedStatus found = list[i];
       if (found.statusFlag == flag) {
           return list.at(i);
       }
   }
   return AppliedStatus{ Hit::none, frames(0) };
}

void StatusBehaviorDirector::Initialize(Entity* entityToAffect)
{
    
    if (entityToAffect != nullptr) {
        this->owner = entityToAffect;
        // Ensure statuses do not play out at battle start
        stunCooldown = frames(0);
        rootCooldown = frames(0);

        iceFx = std::make_shared<SpriteProxyNode>();
        iceFx->setTexture(manager.Textures().LoadFromFile(TexturePaths::ICE_FX));
        iceFx->SetLayer(-2);
        iceFx->Hide(); // default: hidden
        entityToAffect->AddNode(iceFx);

        blindFx = std::make_shared<SpriteProxyNode>();
        blindFx->setTexture(manager.Textures().LoadFromFile(TexturePaths::BLIND_FX));
        blindFx->SetLayer(-2);
        blindFx->Hide(); // default: hidden
        entityToAffect->AddNode(blindFx);

        confusedFx = std::make_shared<SpriteProxyNode>();
        confusedFx->setTexture(manager.Textures().LoadFromFile(TexturePaths::CONFUSED_FX));
        confusedFx->SetLayer(-2);
        confusedFx->Hide(); // default: hidden
        entityToAffect->AddNode(confusedFx);

        iceFxAnimation = Animation(AnimationPaths::ICE_FX);
        blindFxAnimation = Animation(AnimationPaths::BLIND_FX);
        confusedFxAnimation = Animation(AnimationPaths::CONFUSED_FX);
    }
}

void StatusBehaviorDirector::UpdateAnimations(double _elapsed)
{
    if (rootCooldown > frames(0)) {
        rootCooldown -= from_seconds(_elapsed);

        // Root is cancelled if these conditions are met
        if (rootCooldown <= frames(0)/* || IsPassthrough() */) {
            rootCooldown = frames(0);
        }
    }

    owner->canUpdateThisFrame = true;

    if (stunCooldown > frames(0)) {
        owner->canUpdateThisFrame = false;
        stunCooldown -= from_seconds(_elapsed);

        if (stunCooldown <= frames(0)) {
            stunCooldown = frames(0);
        }
    }

    // assume this is hidden, will flip to visible if not
    iceFx->Hide();
    if (freezeCooldown > frames(0)) {
        iceFxAnimation.Update(_elapsed, iceFx->getSprite());
        iceFx->Reveal();

        owner->canUpdateThisFrame = false;
        freezeCooldown -= from_seconds(_elapsed);

        if (freezeCooldown <= frames(0)) {
            freezeCooldown = frames(0);
        }
    }

    // assume this is hidden, will flip to visible if not
    blindFx->Hide();
    if (blindCooldown > frames(0)) {
        blindFxAnimation.Update(_elapsed, blindFx->getSprite());
        blindFx->Reveal();

        blindCooldown -= from_seconds(_elapsed);

        if (blindCooldown <= frames(0)) {
            blindCooldown = frames(0);
        }
    }


    // assume this is hidden, will flip to visible if not
    confusedFx->Hide();
    if (confusedCooldown > frames(0)) {
        confusedFxAnimation.Update(_elapsed, confusedFx->getSprite());
        confusedFx->Reveal();

        confusedSfxCooldown -= from_seconds(_elapsed);
        // Unclear if 55i is the correct timing: this seems to be the one used in MMBN6, though, as the confusion SFX only plays twice during a 110i confusion period.
        constexpr frame_time_t CONFUSED_SFX_INTERVAL{ 55 };
        if (confusedSfxCooldown <= frames(0)) {
            static std::shared_ptr<sf::SoundBuffer> confusedsfx = manager.Audio().LoadFromFile(SoundPaths::CONFUSED_FX);
            manager.Audio().Play(confusedsfx, AudioPriority::highest);
            confusedSfxCooldown = CONFUSED_SFX_INTERVAL;
        }

        confusedCooldown -= from_seconds(_elapsed);

        if (confusedCooldown <= frames(0)) {
            confusedCooldown = frames(0);
            confusedSfxCooldown = frames(0);
        }
    }
};

StatusBehaviorDirector::~StatusBehaviorDirector()
{
}
