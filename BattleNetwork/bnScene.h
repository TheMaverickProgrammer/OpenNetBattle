#pragma once
#include <Swoosh/Activity.h>
#include <Swoosh/Renderers/Renderer.h>
#include "renderers/bnClassicRenderer.h"
#include "renderers/bnRenderEvents.h"
#include "bnResourceHandle.h"
#include "bnInputHandle.h"
#include "bnNetHandle.h"
#include "bnGame.h"

using swoosh::IRenderer;

class Scene : 
  public swoosh::Activity,
  public ResourceHandle, 
  public InputHandle, 
  public NetHandle {
public:
  Scene(swoosh::ActivityController& controller) : swoosh::Activity(&controller){}
  Scene(const Scene& rhs) = delete;
  virtual ~Scene() = default;
  Game& getController() { return static_cast<Game&>(swoosh::Activity::getController()); }
};