#pragma once
#ifdef BN_MOD_SUPPORT

#include <sol/sol.hpp>
#include "bnWeakWrapper.h"
#include "bnUserTypeAnimation.h"
#include "bnScriptedComponent.h"
#include "../bnEntity.h"
#include "../bnAnimationComponent.h"
#include "../bnSolHelpers.h"
#include "../bnTile.h"

void DefineEntityUserType(sol::state& state, sol::table& battle_namespace);

template<typename E>
void DefineEntityFunctionsOn(sol::basic_usertype<WeakWrapper<E>, sol::basic_reference<false> >& entity_table) {
  entity_table["input_has"] = [](WeakWrapper<E>& entity, const InputEvent& event) -> bool {
    return entity.Unwrap()->InputState().Has(event);
  },
  entity_table["get_id"] = [](WeakWrapper<E>& entity) -> Entity::ID_t {
    return entity.Unwrap()->GetID();
  };
  entity_table["get_name"] = [](WeakWrapper<E>& entity) -> std::string {
    return entity.Unwrap()->GetName();
  };
  entity_table["set_name"] = [](WeakWrapper<E>& entity, const std::string& name) {
    return entity.Unwrap()->SetName(name);
  };
  entity_table["get_element"] = [](WeakWrapper<E>& entity) -> Element {
    return entity.Unwrap()->GetElement();
  };
  entity_table["set_element"] = [](WeakWrapper<E>& entity, Element element) {
    entity.Unwrap()->SetElement(element);
  };
  entity_table["get_tile"] = sol::overload(
    [](WeakWrapper<E>& entity, Direction dir, unsigned count) -> Battle::Tile* {
      return entity.Unwrap()->GetTile(dir, count);
    },
    [](WeakWrapper<E>& entity) -> Battle::Tile* {
      return entity.Unwrap()->GetTile();
    }
  );
  entity_table["get_current_tile"] = [](WeakWrapper<E>& entity) -> Battle::Tile* {
    return entity.Unwrap()->GetCurrentTile();
  };
  entity_table["share_tile"] = [](WeakWrapper<E>& entity, bool share) {
    entity.Unwrap()->ShareTileSpace(share);
  };
  entity_table["get_field"] = [](WeakWrapper<E>& entity) -> WeakWrapper<Field> {
    return WeakWrapper(entity.Unwrap()->GetField());
  };
  entity_table["get_facing"] = [](WeakWrapper<E>& entity) -> Direction {
    return entity.Unwrap()->GetFacing();
  };
  entity_table["get_facing_away"] = [](WeakWrapper<E>& entity) -> Direction {
    return entity.Unwrap()->GetFacingAway();
  };
  entity_table["set_facing"] = [](WeakWrapper<E>& entity, Direction direction) {
    entity.Unwrap()->SetFacing(direction);
  };
  entity_table["get_move_direction"] = [](WeakWrapper<E>& entity)->Direction {
    return entity.Unwrap()->GetMoveDirection();
  };
  entity_table["set_move_direction"] = [](WeakWrapper<E>& entity, Direction direction) {
    entity.Unwrap()->SetMoveDirection(direction);
  };
  entity_table["get_color"] = [](WeakWrapper<E>& entity) -> sf::Color {
    return entity.Unwrap()->getColor();
  };
  entity_table["set_color"] = [](WeakWrapper<E>& entity, sf::Color color) {
    entity.Unwrap()->setColor(color);
  };
  entity_table["set_elevation"] = [](WeakWrapper<E>& entity, float elevation) {
    entity.Unwrap()->SetElevation(elevation);
  };
  entity_table["get_elevation"] = [](WeakWrapper<E>& entity) -> float {
    return entity.Unwrap()->GetElevation();
  };
  entity_table["sprite"] = [](WeakWrapper<E>& entity) -> WeakWrapper<SpriteProxyNode> {
    return WeakWrapper(std::static_pointer_cast<SpriteProxyNode>(entity.Unwrap()));
  };
  entity_table["hide"] = [](WeakWrapper<E>& entity) {
    entity.Unwrap()->Hide();
  };
  entity_table["reveal"] = [](WeakWrapper<E>& entity) {
    entity.Unwrap()->Reveal();
  };
  entity_table["teleport"] = sol::overload(
    [](
      WeakWrapper<E>& entity,
      Battle::Tile* dest,
      ActionOrder order,
      sol::object onBeginObject
    ) -> bool {
      return entity.Unwrap()->Teleport(dest, order, [onBeginObject] {
        sol::protected_function onBegin = onBeginObject;

        if (!onBegin.valid()) {
          return;
        }

        auto result = onBegin();

        if (!result.valid()) {
          sol::error error = result;
          Logger::Log(LogLevel::critical, error.what());
        }
      });
    },
    // repeating instead of using std::optional to get sol to provide type errors
    [](WeakWrapper<E>& entity, Battle::Tile* dest, ActionOrder order) -> bool {
      return entity.Unwrap()->Teleport(dest, order);
    },
    [](WeakWrapper<E>& entity, Battle::Tile* dest) -> bool {
      return entity.Unwrap()->Teleport(dest);
    }
  );
  entity_table["slide"] = sol::overload(
    [](
      WeakWrapper<E>& entity,
      Battle::Tile* dest,
      frame_time_t slideTime,
      frame_time_t endlag,
      ActionOrder order,
      sol::object onBeginObject
    ) -> bool {
      return entity.Unwrap()->Slide(dest, slideTime, endlag, order, [onBeginObject] {
        sol::protected_function onBegin = onBeginObject;

        if (!onBegin.valid()) {
          return;
        }

        auto result = onBegin();

        if (!result.valid()) {
          sol::error error = result;
          Logger::Log(LogLevel::critical, error.what());
        }
      });
    },
    // repeating instead of using std::optional to get sol to provide type errors
    [](WeakWrapper<E>& entity, Battle::Tile* dest, frame_time_t slideTime, frame_time_t endlag, ActionOrder order) -> bool {
      return entity.Unwrap()->Slide(dest, slideTime, endlag, order);
    },
    [](WeakWrapper<E>& entity, Battle::Tile* dest, frame_time_t slideTime, frame_time_t endlag) -> bool {
      return entity.Unwrap()->Slide(dest, slideTime, endlag);
    },
    [](WeakWrapper<E>& entity, Battle::Tile* dest, frame_time_t slideTime) -> bool {
      frame_time_t endlag;
      return entity.Unwrap()->Slide(dest, slideTime, endlag);
    }
  );
  entity_table["jump"] = sol::overload(
    [](
      WeakWrapper<E>& entity,
      Battle::Tile* dest,
      float destHeight,
      frame_time_t jumpTime,
      frame_time_t endlag,
      ActionOrder order,
      sol::object onBeginObject
    ) -> bool {
      return entity.Unwrap()->Jump(dest, destHeight, jumpTime, endlag, order, [onBeginObject] {
        sol::protected_function onBegin = onBeginObject;

        if (!onBegin.valid()) {
          return;
        }

        auto result = onBegin();

        if (!result.valid()) {
          sol::error error = result;
          Logger::Log(LogLevel::critical, error.what());
        }
      });
    },
    // repeating instead of using std::optional to get sol to provide type errors
    [](WeakWrapper<E>& entity, Battle::Tile* dest, float destHeight, frame_time_t jumpTime, frame_time_t endlag, ActionOrder order) -> bool {
      return entity.Unwrap()->Jump(dest, destHeight, jumpTime, endlag, order);
    },
    [](WeakWrapper<E>& entity, Battle::Tile* dest, float destHeight, frame_time_t jumpTime, frame_time_t endlag) -> bool {
      return entity.Unwrap()->Jump(dest, destHeight, jumpTime, endlag);
    },
    [](WeakWrapper<E>& entity, Battle::Tile* dest, float destHeight, frame_time_t jumpTime) -> bool {
      frame_time_t endlag;
      return entity.Unwrap()->Jump(dest, destHeight, jumpTime, endlag);
    }
  );
  entity_table["raw_move_event"] = [](WeakWrapper<E>& entity, const MoveEvent& event, ActionOrder order) -> bool {
    return entity.Unwrap()->RawMoveEvent(event, order);
  };
  entity_table["is_sliding"] = [](WeakWrapper<E>& entity) -> bool {
    return entity.Unwrap()->IsSliding();
  };
  entity_table["is_jumping"] = [](WeakWrapper<E>& entity) -> bool {
    return entity.Unwrap()->IsJumping();
  };
  entity_table["is_teleporting"] = [](WeakWrapper<E>& entity) -> bool {
    return entity.Unwrap()->IsTeleporting();
  };
  entity_table["is_moving"] = [](WeakWrapper<E>& entity) -> bool {
    return entity.Unwrap()->IsMoving();
  };
  entity_table["is_passthrough"] = [](WeakWrapper<E>& entity) -> bool {
    return entity.Unwrap()->IsPassthrough();
  };
  entity_table["is_deleted"] = [](WeakWrapper<E>& entity) -> bool {
    auto ptr = entity.Lock();
    return !ptr || ptr->IsDeleted();
  };
  entity_table["will_erase_eof"] = [](WeakWrapper<E>& entity) -> bool {
    auto ptr = entity.Lock();
    return !ptr || ptr->WillEraseEOF();
  };
  entity_table["is_team"] = [](WeakWrapper<E>& entity, Team team) -> bool {
    return entity.Unwrap()->Teammate(team);
  };
  entity_table["set_team"] = [](WeakWrapper<E>& entity, Team team) {
    entity.Unwrap()->SetTeam(team);
  };
  entity_table["get_team"] = [](WeakWrapper<E>& entity) -> Team {
    return entity.Unwrap()->GetTeam();
  };
  entity_table["erase"] = [](WeakWrapper<E>& entity) {
    entity.Unwrap()->Erase();
  };
  entity_table["delete"] = [](WeakWrapper<E>& entity) {
    entity.Unwrap()->Delete();
  };
  entity_table["register_component"] = [](WeakWrapper<E>& entity, WeakWrapper<ScriptedComponent>& component) {
    entity.Unwrap()->RegisterComponent(component.UnwrapAndRelease());
  };
  entity_table["add_defense_rule"] = [](WeakWrapper<E>& entity, DefenseRule& defenseRule) {
    entity.Unwrap()->AddDefenseRule(defenseRule.shared_from_this());
  };
  entity_table["remove_defense_rule"] = [](WeakWrapper<E>& entity, DefenseRule* defenseRule) {
    entity.Unwrap()->RemoveDefenseRule(defenseRule);
  };
  entity_table["ignore_common_aggressor"] = [](WeakWrapper<E>& entity, bool enable) {
    entity.Unwrap()->IgnoreCommonAggressor(enable);
  };
  entity_table["register_status_callback"] = [](WeakWrapper<E>& entity, const Hit::Flags& flag, sol::object callbackObject) {
    ExpectLuaFunction(callbackObject);

    entity.Unwrap()->RegisterStatusCallback(flag, [callbackObject] {
      sol::protected_function callback = callbackObject;
      auto result = callback();

      if (!result.valid()) {
        sol::error error = result;
        Logger::Log(LogLevel::critical, error.what());
      }
    });
  },
  entity_table["get_texture"] = [](WeakWrapper<E>& entity) -> std::shared_ptr<Texture> {
    return entity.Unwrap()->getTexture();
  };
  entity_table["set_texture"] = [](WeakWrapper<E>& entity, std::shared_ptr<Texture> texture) {
    entity.Unwrap()->setTexture(texture);
  };
  entity_table["set_animation"] = [](WeakWrapper<E>& entity, std::string animation) {
    auto animationComponent = entity.Unwrap()->template GetFirstComponent<AnimationComponent>();
    animationComponent->SetPath(animation);
    animationComponent->Reload();
  };
  entity_table["get_animation"] = [](WeakWrapper<E>& entity) -> AnimationWrapper {
    auto animationComponent = entity.Unwrap()->template GetFirstComponent<AnimationComponent>();
    auto& animation = animationComponent->GetAnimationObject();
    return AnimationWrapper(entity.GetWeak(), animation);
  };
  entity_table["create_node"] = [](WeakWrapper<E>& entity) -> WeakWrapper<SpriteProxyNode> {
    auto child = std::make_shared<SpriteProxyNode>();
    entity.Unwrap()->AddNode(child);

    return WeakWrapper(child);
  };
  entity_table["highlight_tile"] = [](WeakWrapper<E>& entity, Battle::TileHighlight mode) {
    entity.Unwrap()->HighlightTile(mode);
  };
  entity_table["get_context"] = [](WeakWrapper<E>& entity) -> Hit::Context {
    return entity.Unwrap()->GetHitboxContext();
  };
  entity_table["copy_hit_props"] = [](WeakWrapper<E>& entity) -> Hit::Properties {
    return entity.Unwrap()->GetHitboxProperties();
  };
  entity_table["set_hit_props"] = [](WeakWrapper<E>& entity, Hit::Properties props) {
    entity.Unwrap()->SetHitboxProperties(props);
  };
  entity_table["set_height"] = [](WeakWrapper<E>& entity, float height) {
    entity.Unwrap()->SetHeight(height);
  };
  entity_table["get_height"] = [](WeakWrapper<E>& entity) -> float {
    return entity.Unwrap()->GetHeight();
  };
  entity_table["set_shadow"] = sol::overload(
    [](WeakWrapper<E>& entity, Entity::Shadow type) {
      entity.Unwrap()->SetShadowSprite(type);
    },
    [](WeakWrapper<E>& entity, std::shared_ptr<sf::Texture> shadow) {
      entity.Unwrap()->SetShadowSprite(shadow);
    }
  );
  entity_table["show_shadow"] = [](WeakWrapper<E>& entity, bool show) {
    entity.Unwrap()->ShowShadow(show);
  };
  entity_table["get_tile_offset"] = [](WeakWrapper<E>& entity) -> sf::Vector2f {
    return entity.Unwrap()->GetTileOffset();
  };
  entity_table["get_offset"] = [](WeakWrapper<E>& entity) -> sf::Vector2f {
    return entity.Unwrap()->GetDrawOffset();
  };
  entity_table["set_offset"] = [](WeakWrapper<E>& entity, float x, float y) {
    entity.Unwrap()->SetDrawOffset(x, y);
  };
  entity_table["never_flip"] = [](WeakWrapper<E>& entity, bool enabled) {
    entity.Unwrap()->NeverFlip(enabled);
  };
  entity_table["get_health"] = [](WeakWrapper<E>& entity) -> int{
    return entity.Unwrap()->GetHealth();
  };
  entity_table["get_max_health"] = [](WeakWrapper<E>& entity) -> int {
    return entity.Unwrap()->GetMaxHealth();
  };
  entity_table["set_health"] = [](WeakWrapper<E>& entity, int health) {
    entity.Unwrap()->SetHealth(health);
  };
  entity_table["set_float_shoe"] = [](WeakWrapper<E>& entity, bool enable) {
    entity.Unwrap()->SetFloatShoe(enable);
  };
  entity_table["set_air_shoe"] = [](WeakWrapper<E>& entity, bool enable) {
    entity.Unwrap()->SetAirShoe(enable);
  };
  entity_table["toggle_hitbox"] = [](WeakWrapper<E>& entity, bool enabled) {
    return entity.Unwrap()->EnableHitbox(enabled);
  };
  entity_table["is_counterable"] = [](WeakWrapper<E>& entity) -> bool {
    return entity.Unwrap()->IsCounterable();
  };
  entity_table["toggle_counter"] = [](WeakWrapper<E>& entity, bool on) {
    entity.Unwrap()->ToggleCounter(on);
  };
  entity_table["get_current_palette"] = [](WeakWrapper<E>& entity) -> std::shared_ptr<Texture> {
    return entity.Unwrap()->GetPalette();
  };
  entity_table["set_palette"] = [](WeakWrapper<E>& entity, std::shared_ptr<Texture>& texture) {
    entity.Unwrap()->SetPalette(texture);
  };
  entity_table["get_base_palette"] = [](WeakWrapper<E>& entity) -> std::shared_ptr<Texture> {
    return entity.Unwrap()->GetBasePalette();
  };
  entity_table["store_base_palette"] = [](WeakWrapper<E>& entity, std::shared_ptr<Texture>& texture) {
    entity.Unwrap()->StoreBasePalette(texture);
  };
  entity_table["shake_camera"] = [](WeakWrapper<E>& entity, double power, float duration) {
    entity.Unwrap()->EventChannel().Emit(&Camera::ShakeCamera, power, sf::seconds(duration));
  };
}

#endif
