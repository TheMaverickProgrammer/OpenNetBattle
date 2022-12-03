#pragma once

#include <functional>
#include <type_traits>
#include <SFML/Graphics/RenderTexture.hpp>
#include <Swoosh/ActivityController.h>

#include "../bnResourceHandle.h"
#include "../bnInputHandle.h"

class BattleSceneBase;

using namespace swoosh;

/*
    Interface for Battle Scene States
*/
struct BattleSceneState : public ResourceHandle, public InputHandle {
    friend class BattleSceneBase;

    BattleSceneBase& GetScene() { return *scene; }
    const BattleSceneBase& GetScene() const { return *scene; }

    ActivityController& GetController() { return *controller; }
    
    virtual void onStart(const BattleSceneState* next=nullptr) = 0;
    virtual void onEnd(const BattleSceneState* last=nullptr) = 0;
    virtual void onUpdate(double elapsed) = 0;
    virtual void onDraw(IRenderer& renderer) = 0;
    virtual ~BattleSceneState(){};

    using ChangeCondition = std::function<bool()>;

protected:
  BattleSceneBase* scene{ nullptr };

private:
  ActivityController* controller{ nullptr };
};
