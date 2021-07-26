#ifdef BN_MOD_SUPPORT
#include "bnScriptResourceManager.h"
#include "bnAnimator.h"
#include "bnAudioResourceManager.h"
#include "bnTextureResourceManager.h"
#include "bnShaderResourceManager.h"
#include "bnResourceHandle.h"
#include "bnEntity.h"
#include "bnSpell.h"
#include "bnCharacter.h"
#include "bnElements.h"
#include "bnField.h"
#include "bnTile.h"
#include "bnHitbox.h"
#include "bnSharedHitbox.h"
#include "bnDefenseNodrag.h"
#include "bnDefenseVirusBody.h"
#include "bnParticlePoof.h"
#include "bnParticleImpact.h"

#include "bnNaviRegistration.h"
#include "bnMobRegistration.h"
#include "bindings/bnScriptedArtifact.h"
#include "bindings/bnScriptedCardAction.h"
#include "bindings/bnScriptedCharacter.h"
#include "bindings/bnScriptedSpell.h"
#include "bindings/bnScriptedObstacle.h"
#include "bindings/bnScriptedPlayer.h"
#include "bindings/bnScriptedDefenseRule.h"
#include "bindings/bnScriptedMob.h"

// Useful prefabs to use in scripts...
#include "bnExplosion.h"

// temporary proof of concept includes...
#include "bnBusterCardAction.h"
#include "bnSwordCardAction.h"
#include "bnBombCardAction.h"
#include "bnFireBurnCardAction.h"
#include "bnCannonCardAction.h"

void ScriptResourceManager::ConfigureEnvironment(sol::state& state) {
  state.open_libraries(sol::lib::base, sol::lib::math);

        // create_table returns non reference to global table? internal tracking?
    sol::table battle_namespace = state.create_table("Battle");
    sol::table overworld_namespace = state.create_table("Overworld");
    sol::table engine_namespace = state.create_table("Engine");

    const auto& textureresource_record = engine_namespace.new_usertype<TextureResourceManager>("TextureResourceManager",
        "LoadFile", &TextureResourceManager::LoadTextureFromFile
        );

    const auto& audioresource_record = engine_namespace.new_usertype<AudioResourceManager>("AudioResourceMananger",
        "LoadFile", &AudioResourceManager::LoadFromFile,
        "Play", sol::overload(
            sol::resolve<int(AudioType, AudioPriority)>(&AudioResourceManager::Play),
            sol::resolve<int(std::shared_ptr<sf::SoundBuffer>, AudioPriority)>(&AudioResourceManager::Play)
        ),
        "Stream", sol::resolve<int(std::string, bool)>(&AudioResourceManager::Stream)
        );

    const auto& shaderresource_record = engine_namespace.new_usertype<ShaderResourceManager>("ShaderResourceManager",
        "LoadFile", &ShaderResourceManager::LoadShaderFromFile
        );

    // make resource handle metatable
    const auto& resourcehandle_record = engine_namespace.new_usertype<ResourceHandle>("ResourceHandle",
        sol::constructors<ResourceHandle()>(),
        "Textures", sol::property(sol::resolve<TextureResourceManager & ()>(&ResourceHandle::Textures)),
        "Audio", sol::property(sol::resolve<AudioResourceManager & ()>(&ResourceHandle::Audio)),
        "Shaders", sol::property(sol::resolve<ShaderResourceManager & ()>(&ResourceHandle::Shaders))
        );

    // make input handle metatable
    const auto& input_record = engine_namespace.new_usertype<InputManager>("Input",
        sol::factories([]() -> InputManager&
            {
                static InputHandle handle;
                return handle.Input();
            }),
        "Has", &InputManager::Has
    );

    // TODO: Perhaps see if there's a way to get /readonly/ access to the X/Y value?
    // The function calls in Lua for what is normally treated like a member variable seem a little bit wonky
    const auto& tile_record = state.new_usertype<Battle::Tile>("Tile",
        "X", &Battle::Tile::GetX,
        "Y", &Battle::Tile::GetY,
        "Width", &Battle::Tile::GetWidth,
        "Height", &Battle::Tile::GetHeight,
        "GetState", &Battle::Tile::GetState,
        "SetState", &Battle::Tile::SetState,
        "IsEdge", &Battle::Tile::IsEdgeTile,
        "IsCracked", &Battle::Tile::IsCracked,
        "IsHole", &Battle::Tile::IsHole,
        "IsWalkable", &Battle::Tile::IsWalkable,
        "IsReserved", &Battle::Tile::IsReservedByCharacter,
        "Team", &Battle::Tile::GetTeam,
        "AttackEntities", &Battle::Tile::AffectEntities
        );

    // Exposed "GetCharacter" so that there's a way to maintain a reference to other actors without hanging onto pointers.
    // If you hold onto their ID, and use that through Field::GetCharacter,
    // instead you will get passed a nullptr/nil in Lua after they've been effectively removed,
    // rather than potentially holding onto a hanging pointer to deleted data.
    const auto& field_record = battle_namespace.new_usertype<Field>("Field",
        "TileAt", &Field::GetAt,
        "Width", &Field::GetWidth,
        "Height", &Field::GetHeight,
        "Spawn", sol::overload(
            sol::resolve<Field::AddEntityStatus(std::unique_ptr<ScriptedObstacle>&, int, int)>(&Field::AddEntity),
            sol::resolve<Field::AddEntityStatus(std::unique_ptr<ScriptedArtifact>&, int, int)>(&Field::AddEntity),
            sol::resolve<Field::AddEntityStatus(std::unique_ptr<ScriptedSpell>&, int, int)>(&Field::AddEntity),
            sol::resolve<Field::AddEntityStatus(Artifact&, int, int)>(&Field::AddEntity),
            sol::resolve<Field::AddEntityStatus(Spell&, int, int)>(&Field::AddEntity)
        ),
        "GetCharacter", &Field::GetCharacter,
        "GetEntity", &Field::GetEntity,
        "NotifyOnDelete", &Field::NotifyOnDelete,
        "CallbackOnDelete", &Field::CallbackOnDelete
    );

    const auto& animation_record = engine_namespace.new_usertype<Animation>("Animation",
        sol::constructors<Animation(const std::string&), Animation(const Animation&)>(),
        "Load", &Animation::Load,
        "Update", &Animation::Update,
        "Refresh", &Animation::Refresh,
        "SetState", &Animation::SetAnimation,
        "CopyFrom", &Animation::CopyFrom,
        "State", &Animation::GetAnimationString,
        "Point", &Animation::GetPoint,
        "SetPlayback", sol::resolve<Animation& (char)>(&Animation::operator<<),
        "OnComplete", sol::resolve<void(const FrameCallback&)>(&Animation::operator<<),
        "OnFrame", &Animation::AddCallback,
        "OnInterrupt", &Animation::SetInterruptCallback
    );

    const auto& node_record = engine_namespace.new_usertype<SpriteProxyNode>("SpriteNode",
        sol::constructors<SpriteProxyNode()>(),
        "SetTexture", &SpriteProxyNode::setTexture,
        "SetLayer", &SpriteProxyNode::SetLayer,
        "Show", &SpriteProxyNode::Reveal,
        "Hide", &SpriteProxyNode::Hide,
        "SetPosition", sol::resolve<void(float, float)>(&SpriteProxyNode::setPosition),
        "GetPosition", &SpriteProxyNode::getPosition,
        "Sprite", &SpriteProxyNode::getSprite,
        "EnableParentShader", &SpriteProxyNode::EnableParentShader,
        sol::base_classes, sol::bases<SceneNode>()
    );

    const auto& scriptedspell_record = battle_namespace.new_usertype<ScriptedSpell>( "Spell",
        sol::factories([](Team team) -> std::unique_ptr<ScriptedSpell> {
            return std::make_unique<ScriptedSpell>(team);
        }),
            sol::meta_function::index, &dynamic_object::dynamic_get,
            sol::meta_function::new_index, &dynamic_object::dynamic_set,
            sol::meta_function::length, [](dynamic_object& d) { return d.entries.size(); },
            "GetID", &ScriptedSpell::GetID,
            "GetTile", & ScriptedSpell::GetTile,
            "CurrentTile", & ScriptedSpell::GetCurrentTile,
            "Field", & ScriptedSpell::GetField,
            "Slide", & ScriptedSpell::Slide,
            "Jump", & ScriptedSpell::Jump,
            "Teleport", & ScriptedSpell::Teleport,
            "RawMoveEvent", & ScriptedSpell::RawMoveEvent,
            "IsSliding", & ScriptedSpell::IsSliding,
            "IsJumping", & ScriptedSpell::IsJumping,
            "IsTeleporting", & ScriptedSpell::IsTeleporting,
            "IsMoving", & ScriptedSpell::IsMoving,
            "Team", & ScriptedSpell::GetTeam,
            "IsTeammate", & ScriptedSpell::Teammate,
            "Remove", & ScriptedSpell::Remove,
            "Delete", & ScriptedSpell::Delete,

            "SetTexture", &ScriptedSpell::setTexture,
            "SetLayer", &ScriptedSpell::SetLayer,
            "AddNode", &ScriptedSpell::AddNode,

            "HighlightTile", &ScriptedSpell::HighlightTile,
            "GetHitProps", &ScriptedSpell::GetHitboxProperties,
            "SetHitProps", &ScriptedSpell::SetHitboxProperties,

            "GetAnimation", &ScriptedSpell::GetAnimationObject,
            "ShakeCamera", &ScriptedSpell::ShakeCamera,
            "SetHeight", &ScriptedSpell::SetHeight,
            "SetPosition", &ScriptedSpell::SetTileOffset,
            "GetPosition", &ScriptedSpell::GetTileOffset,
            "ShowShadow", & ScriptedSpell::ShowShadow,
            "attackFunc", &ScriptedSpell::attackCallback,
            "deleteFunc", &ScriptedSpell::deleteCallback,
            "updateFunc", &ScriptedSpell::updateCallback,
            "collisionFunc", &ScriptedSpell::collisionCallback,
            "canMoveToFunc", &ScriptedSpell::canMoveToCallback,
            "onSpawnFunc", &ScriptedSpell::spawnCallback,
            sol::base_classes, sol::bases<Spell>()
    );

    const auto& scriptedobstacle_record = battle_namespace.new_usertype<ScriptedObstacle>("Obstacle",
        sol::factories([](Team team) -> std::unique_ptr<ScriptedObstacle> {
            return std::make_unique<ScriptedObstacle>(team);
        }),
        sol::base_classes, sol::bases<Obstacle, Spell, Character>(),
        sol::meta_function::index, &dynamic_object::dynamic_get,
        sol::meta_function::new_index, &dynamic_object::dynamic_set,
        sol::meta_function::length, [](dynamic_object& d) { return d.entries.size(); },
        "GetID", &ScriptedObstacle::GetID,
        "GetElement", &ScriptedObstacle::GetElement,
        "SetElement", &ScriptedObstacle::SetElement,
        "GetTile", & ScriptedObstacle::GetTile,
        "CurrentTile", & ScriptedObstacle::GetCurrentTile,
        "Field", & ScriptedObstacle::GetField,
        "Facing", &ScriptedObstacle::GetFacing,
        "Slide", & ScriptedObstacle::Slide,
        "Jump", & ScriptedObstacle::Jump,
        "Teleport", & ScriptedObstacle::Teleport,
        "RawMoveEvent", & ScriptedObstacle::RawMoveEvent,
        "IsSliding", & ScriptedObstacle::IsSliding,
        "IsJumping", & ScriptedObstacle::IsJumping,
        "IsTeleporting", & ScriptedObstacle::IsTeleporting,
        "IsMoving", & ScriptedObstacle::IsMoving,
        "IsTeammate", & ScriptedObstacle::Teammate,
        "Team", & ScriptedObstacle::GetTeam,
        "Remove", & ScriptedObstacle::Remove,
        "Delete", & ScriptedObstacle::Delete,

        "GetName", &ScriptedObstacle::GetName,
        "GetHealth", &ScriptedObstacle::GetHealth,
        "GetMaxHealth", &ScriptedObstacle::GetMaxHealth,
        "SetName", &ScriptedObstacle::SetName,
        "SetHealth", &ScriptedObstacle::SetHealth,
        "ShareTile", & ScriptedObstacle::ShareTileSpace,
        "AddDefenseRule", & ScriptedObstacle::AddDefenseRule,

        "SetTexture", & ScriptedObstacle::setTexture,
        "SetLayer", &ScriptedObstacle::SetLayer,
        "GetAnimation", &ScriptedObstacle::GetAnimationObject,
        "AddNode", &ScriptedObstacle::AddNode,

        "HighlightTile", &ScriptedObstacle::HighlightTile,
        "GetHitProps", &ScriptedObstacle::GetHitboxProperties,
        "SetHitProps", &ScriptedObstacle::SetHitboxProperties,

        "IgnoreCommonAggressor", &ScriptedObstacle::IgnoreCommonAggressor,

        "SetHeight", & ScriptedObstacle::SetHeight,
        "ShowShadow", & ScriptedObstacle::ShowShadow,
        "ShakeCamera", &ScriptedObstacle::ShakeCamera,
        "SetPosition", & ScriptedObstacle::SetTileOffset,
        "GetPosition", & ScriptedObstacle::GetTileOffset,
        "attackFunc", &ScriptedObstacle::attackCallback,
        "deleteFunc", &ScriptedObstacle::deleteCallback,
        "updateFunc", &ScriptedObstacle::updateCallback,
        "collisionFunc", &ScriptedObstacle::collisionCallback,
        "canMoveToFunc", &ScriptedObstacle::canMoveToCallback,
        "onSpawnFunc", &ScriptedObstacle::spawnCallback
        );

    // Adding in bindings for Character* type objects to sol.
    // Without adding these in, it has no idea what to do with Character* objects passed up to it,
    // even though there's bindings for ScriptedCharacter already done.
    const auto& basic_character_record = battle_namespace.new_usertype<Character>( "BasicCharacter",
        "GetID", &Character::GetID,
        "GetElement", &Character::GetElement,
        "GetTile", &Character::GetTile,
        "CurrentTile", &Character::GetCurrentTile,
        "Field", &Character::GetField,
        "Facing", &Character::GetFacing,
        "IsSliding", &Character::IsSliding,
        "IsJumping", &Character::IsJumping,
        "IsTeleporting", &Character::IsTeleporting,
        "IsMoving", &Character::IsMoving,
        "IsTeammate", &Character::Teammate,
        "Team", &Character::GetTeam,

        "GetName", &Character::GetName,
        "GetRank", &Character::GetRank,
        "GetHealth", &Character::GetHealth,
        "GetMaxHealth", &Character::GetMaxHealth,
        "SetHealth", &Character::SetHealth
    );

    const auto& scriptedcharacter_record = battle_namespace.new_usertype<ScriptedCharacter>("Character",
        sol::meta_function::index, &dynamic_object::dynamic_get,
        sol::meta_function::new_index, &dynamic_object::dynamic_set,
        sol::meta_function::length, [](dynamic_object& d) { return d.entries.size(); },
        sol::base_classes, sol::bases<Character>(),
        "GetID", &ScriptedCharacter::GetID,
        "GetElement", &ScriptedCharacter::GetElement,
        "SetElement", &ScriptedCharacter::SetElement,
        "GetTile", &ScriptedCharacter::GetTile,
        "CurrentTile", &ScriptedCharacter::GetCurrentTile,
        "Field", &ScriptedCharacter::GetField,
        "Facing", &ScriptedCharacter::GetFacing,
        "Target", &ScriptedCharacter::GetTarget,
        "Slide", &ScriptedCharacter::Slide,
        "Jump", &ScriptedCharacter::Jump,
        "Teleport", &ScriptedCharacter::Teleport,
        "RawMoveEvent", &ScriptedCharacter::RawMoveEvent,
        "IsSliding", &ScriptedCharacter::IsSliding,
        "IsJumping", &ScriptedCharacter::IsJumping,
        "IsTeleporting", &ScriptedCharacter::IsTeleporting,
        "IsMoving", &ScriptedCharacter::IsMoving,
        "Team", &ScriptedCharacter::GetTeam,
        "IsTeammate", &ScriptedCharacter::Teammate,

        "SetTexture", &ScriptedCharacter::setTexture,
        "AddNode", &ScriptedCharacter::AddNode,

        "GetName", &ScriptedCharacter::GetName,
        "GetHealth", &ScriptedCharacter::GetHealth,
        "GetMaxHealth", &ScriptedCharacter::GetMaxHealth,
        "SetName", &ScriptedCharacter::SetName,
        "SetHealth", &ScriptedCharacter::SetHealth,
        "GetRank", &ScriptedCharacter::GetRank,
        "SetRank", &ScriptedCharacter::SetRank,
        "ShareTile", &ScriptedCharacter::ShareTileSpace,
        "AddDefenseRule", &ScriptedCharacter::AddDefenseRule,

        "SetPosition", &ScriptedCharacter::SetTileOffset,
        "GetPosition", &ScriptedCharacter::GetTileOffset,
        "SetHeight", &ScriptedCharacter::SetHeight,
        "GetAnimation", &ScriptedCharacter::GetAnimationObject,
        "ShakeCamera", &ScriptedCharacter::ShakeCamera,
        "ToggleCounter", &ScriptedCharacter::ToggleCounter,
        "RegisterStatusCallback", &ScriptedCharacter::RegisterStatusCallback
    );

    const auto& player_record = battle_namespace.new_usertype<Player>("BasicPlayer",
        sol::base_classes, sol::bases<Character>(),
        "GetID", &Player::GetID,
        "GetTile", &Player::GetTile,
        "CurrentTile", &Player::GetCurrentTile,
        "Field", &Player::GetField,
        "Facing", &Player::GetFacing,
        "IsSliding", &Player::IsSliding,
        "IsJumping", &Player::IsJumping,
        "IsTeleporting", &Player::IsTeleporting,
        "IsMoving", &Player::IsMoving,
        "Team", &Player::GetTeam,

        "GetName", &Player::GetName,
        "GetHealth", &Player::GetHealth,
        "GetMaxHealth", &Player::GetMaxHealth
    );

    const auto& scriptedplayer_record = battle_namespace.new_usertype<ScriptedPlayer>("Player",
        sol::base_classes, sol::bases<Player>(),
        "GetID", &ScriptedPlayer::GetID,
        "GetElement", &ScriptedPlayer::GetElement,
        "SetElement", &ScriptedPlayer::SetElement,
        "GetTile", &ScriptedPlayer::GetTile,
        "CurrentTile", &ScriptedPlayer::GetCurrentTile,
        "Field", &ScriptedPlayer::GetField,
        "Facing", &ScriptedPlayer::GetFacing,
        "Slide", &ScriptedPlayer::Slide,
        "Jump", &ScriptedPlayer::Jump,
        "Teleport", &ScriptedPlayer::Teleport,
        "RawMoveEvent", &ScriptedPlayer::RawMoveEvent,
        "IsSliding", &ScriptedPlayer::IsSliding,
        "IsJumping", &ScriptedPlayer::IsJumping,
        "IsTeleporting", &ScriptedPlayer::IsTeleporting,
        "IsMoving", &ScriptedPlayer::IsMoving,
        "Team", &ScriptedPlayer::GetTeam,

        "GetName", &ScriptedPlayer::GetName,
        "GetHealth", &ScriptedPlayer::GetHealth,
        "GetMaxHealth", &ScriptedPlayer::GetMaxHealth,
        "SetName", &ScriptedPlayer::SetName,
        "SetHealth", &ScriptedPlayer::SetHealth,
        "SetTexture", &ScriptedPlayer::setTexture,

        "SetAnimation", & ScriptedPlayer::SetAnimation,
        "SetHeight", &ScriptedPlayer::SetHeight,
        "SetFullyChargeColor", &ScriptedPlayer::SetFullyChargeColor,
        "SetChargePosition", &ScriptedPlayer::SetChargePosition,
        "GetAnimation", &ScriptedPlayer::GetAnimationObject,
        "SetFloatShoe", &ScriptedPlayer::SetFloatShoe,
        "SetAirShoe", &ScriptedPlayer::SetAirShoe,
        "SlideWhenMoving", &ScriptedPlayer::SlideWhenMoving
    );

    const auto& scripted_artifact_record = engine_namespace.new_usertype<ScriptedArtifact>("Artifact",
        sol::factories([]() -> std::unique_ptr<ScriptedArtifact> {
            return std::make_unique<ScriptedArtifact>();
            }),
        sol::meta_function::index, &dynamic_object::dynamic_get,
        sol::meta_function::new_index, &dynamic_object::dynamic_set,
        sol::meta_function::length, [](dynamic_object& d) { return d.entries.size(); },
        "GetID", &ScriptedArtifact::GetID,
        "GetTile", &ScriptedArtifact::GetTile,
        "Field", &ScriptedArtifact::GetField,
        "Slide", &ScriptedArtifact::Slide,
        "Teleport", &ScriptedArtifact::Teleport,

        "SetTexture", & ScriptedArtifact::setTexture,
        "SetLayer", &ScriptedArtifact::SetLayer,

        "SetAnimation", &ScriptedArtifact::SetAnimation,
        "SetPath", &ScriptedArtifact::SetPath,
        "Flip", & ScriptedArtifact::Flip,
        "updateFunc", &ScriptedArtifact::onUpdate
    );

    // Had it crash when using ScriptedPlayer* values so had to expose other versions for it to cooperate.
    // Many things use Character* references but we will maybe have to consolidate all the interfaces for characters into one type.
    const auto& scripted_card_action_record = battle_namespace.new_usertype<ScriptedCardAction>("CardAction",
        sol::factories(
        [](Character* character, const std::string& state)-> std::unique_ptr<ScriptedCardAction> {
                return std::make_unique<ScriptedCardAction>(*character, state);
        },
        [](ScriptedPlayer* character, const std::string& state) -> std::unique_ptr<ScriptedCardAction> {
            return std::make_unique<ScriptedCardAction>(*character, state);
        }, 
        [](ScriptedCharacter* character, const std::string& state) -> std::unique_ptr<ScriptedCardAction> {
            return std::make_unique<ScriptedCardAction>(*character, state);
        }
        ),
        sol::meta_function::index,
        &dynamic_object::dynamic_get,
        sol::meta_function::new_index,
        &dynamic_object::dynamic_set,
        sol::meta_function::length,
        [](dynamic_object& d) { return d.entries.size(); },
        "SetLockout", &ScriptedCardAction::SetLockout,
        "SetLockoutGroup", &ScriptedCardAction::SetLockoutGroup,
        "OverrideAnimationFrames", &ScriptedCardAction::OverrideAnimationFrames,
        "AddAttachment", sol::overload(
            sol::resolve<CardAction::Attachment & (Character*, const std::string&, SpriteProxyNode&)>(&ScriptedCardAction::AddAttachment),
            sol::resolve<CardAction::Attachment & (Animation&, const std::string&, SpriteProxyNode&)>(&ScriptedCardAction::CardAction::AddAttachment)
        ),
        "AddAnimAction", &ScriptedCardAction::AddAnimAction,
        "AddStep", &ScriptedCardAction::AddStep,
        "EndAction", &ScriptedCardAction::EndAction,
        "GetActor", &ScriptedCardAction::GetActor,
        "actionEndFunc", &ScriptedCardAction::onActionEnd,
        "animationEndFunc", &ScriptedCardAction::onAnimationEnd,
        "executeFunc", &ScriptedCardAction::onExecute,
        "updateFunc", &ScriptedCardAction::onUpdate,
        sol::base_classes, sol::bases<CardAction>()
    );

    state.set_function("MakeActionLockout",
        [](CardAction::LockoutType type, double cooldown, CardAction::LockoutGroup group)
            { return CardAction::LockoutProperties{ type, cooldown, group }; }
    );

    const auto& lockout_record = state.new_usertype<CardAction::LockoutProperties>("LockoutProps",
        "Type", &CardAction::LockoutProperties::type,
        "Cooldown", &CardAction::LockoutProperties::cooldown,
        "Group", &CardAction::LockoutProperties::group
    );

    const auto& lockout_type_record = state.new_enum("LockType",
        "Animation", CardAction::LockoutType::animation,
        "Async", CardAction::LockoutType::async,
        "Sequence", CardAction::LockoutType::sequence
    );

    const auto& lockout_group_record = state.new_enum("Lockout",
        "Weapons", CardAction::LockoutGroup::weapon,
        "Cards", CardAction::LockoutGroup::card,
        "Abilities", CardAction::LockoutGroup::ability
    );

    const auto& defense_frame_state_judge_record = state.new_usertype<DefenseFrameStateJudge>("DefenseFrameStateJudge",
        "BlockDamage", &DefenseFrameStateJudge::BlockDamage,
        "BlockImpact", &DefenseFrameStateJudge::BlockImpact,
        "IsDamageBlocked", &DefenseFrameStateJudge::IsDamageBlocked,
        "IsImpactBlocked", &DefenseFrameStateJudge::IsImpactBlocked,
        /*"AddTrigger", &DefenseFrameStateJudge::AddTrigger,*/
        "SignalDefenseWasPierced", &DefenseFrameStateJudge::SignalDefenseWasPierced
        );

    const auto& defense_rule_record = battle_namespace.new_usertype<ScriptedDefenseRule>("DefenseRule",
        sol::factories(
            [](int priority, const DefenseOrder& order) -> std::unique_ptr<ScriptedDefenseRule>
            { return std::make_unique<ScriptedDefenseRule>(Priority(priority), order); }
        ),
        "IsReplaced", &ScriptedDefenseRule::IsReplaced,
        "canBlockFunc", &ScriptedDefenseRule::canBlockCallback,
        "filterStatusesFunc", &ScriptedDefenseRule::filterStatusesCallback,
        sol::base_classes, sol::bases<DefenseRule>()
        );

    const auto& defense_rule_nodrag = battle_namespace.new_usertype<DefenseNodrag>("DefenseNoDrag",
        sol::factories(
            [] { return std::make_unique<DefenseNodrag>(); }
        ),
        sol::base_classes, sol::bases<DefenseRule>()
        );

    const auto& defense_rule_virus_body = battle_namespace.new_usertype<DefenseVirusBody>("DefenseVirusBody",
        sol::factories(
            [] { return std::make_unique<DefenseVirusBody>(); }
        ),
        sol::base_classes, sol::bases<DefenseRule>()
        );

    const auto& attachment_record = battle_namespace.new_usertype<CardAction::Attachment>("Attachment",
        sol::constructors<CardAction::Attachment(Animation&, const std::string&, SpriteProxyNode&)>(),
        "UseAnimation", &CardAction::Attachment::UseAnimation,
        "AddAttachment", &CardAction::Attachment::AddAttachment
    );

    const auto& hitbox_record = battle_namespace.new_usertype<Hitbox>("Hitbox",
        sol::factories([](Team team)
            { return new Hitbox(team); }
        ),
        "OnAttack", &Hitbox::AddCallback,
        "SetHitProps", &Hitbox::SetHitboxProperties,
        "GetHitProps", &Hitbox::GetHitboxProperties,
        sol::base_classes, sol::bases<Spell>()
    );

    const auto& shared_hitbox_record = battle_namespace.new_usertype<SharedHitbox>("SharedHitbox",
        sol::constructors<SharedHitbox(Spell*, float)>(),
        sol::base_classes, sol::bases<Spell>()
    );

    const auto& busteraction_record = battle_namespace.new_usertype<BusterCardAction>("Buster",
        sol::factories(
            [](Character* character, bool charged, int dmg) -> std::unique_ptr<CardAction>
                { return std::make_unique<BusterCardAction>(*character, charged, dmg); },
            [](ScriptedPlayer* character, bool charged, int dmg) -> std::unique_ptr<CardAction>
                { return std::make_unique<BusterCardAction>(*character, charged, dmg); },
            [](ScriptedCharacter* character, bool charged, int dmg) -> std::unique_ptr<CardAction>
                { return std::make_unique<BusterCardAction>(*character, charged, dmg); }
        ),
        sol::base_classes, sol::bases<CardAction>()
    );

    const auto& swordaction_record = battle_namespace.new_usertype<SwordCardAction>("Sword",
        sol::factories(
            [](Character* character, int dmg) -> std::unique_ptr<CardAction>
                { return std::make_unique<SwordCardAction>(*character, dmg); },
            [](ScriptedPlayer* character, int dmg) -> std::unique_ptr<CardAction>
                { return std::make_unique<SwordCardAction>(*character, dmg); },
            [](ScriptedCharacter* character, int dmg) -> std::unique_ptr<CardAction>
                { return std::make_unique<SwordCardAction>(*character, dmg); }
        ),
        sol::base_classes, sol::bases<CardAction>()
    );

    const auto& bombaction_record = battle_namespace.new_usertype<BombCardAction>("Bomb",
        sol::factories(
            [](Character* character, int dmg) -> std::unique_ptr<CardAction>
                { return std::make_unique<BombCardAction>(*character, dmg); },
            [](ScriptedPlayer* character, int dmg) -> std::unique_ptr<CardAction>
                { return std::make_unique<BombCardAction>(*character, dmg); },
            [](ScriptedCharacter* character, int dmg) -> std::unique_ptr<CardAction>
                { return std::make_unique<BombCardAction>(*character, dmg); }
        ),
        sol::base_classes, sol::bases<CardAction>()
    );

    const auto& fireburn_record = battle_namespace.new_usertype<FireBurnCardAction>("FireBurn",
        sol::factories(
            [](Character* character, FireBurn::Type type, int dmg) -> std::unique_ptr<CardAction>
                { return std::make_unique<FireBurnCardAction>(*character, type, dmg); },
            [](ScriptedPlayer* character, FireBurn::Type type, int dmg) -> std::unique_ptr<CardAction>
                { return std::make_unique<FireBurnCardAction>(*character, type, dmg); },
            [](ScriptedCharacter* character, FireBurn::Type type, int dmg) -> std::unique_ptr<CardAction>
                { return std::make_unique<FireBurnCardAction>(*character, type, dmg); }
        ),
        sol::base_classes, sol::bases<CardAction>()
    );

    const auto& flame_cannon_type_record = battle_namespace.new_enum("FlameCannon",
        "_1", FireBurn::Type::_1,
        "_2", FireBurn::Type::_2,
        "_3", FireBurn::Type::_3
    );

    const auto& cannon_record = battle_namespace.new_usertype<CannonCardAction>("Cannon",
        sol::factories(
            [](Character* character, CannonCardAction::Type type, int dmg) -> std::unique_ptr<CardAction> {
                return std::make_unique<CannonCardAction>(*character, type, dmg); }, 
            [](ScriptedPlayer* character, CannonCardAction::Type type, int dmg) -> std::unique_ptr<CardAction> {
                return std::make_unique<CannonCardAction>(*character, type, dmg); }, 
            [](ScriptedCharacter* character, CannonCardAction::Type type, int dmg) -> std::unique_ptr<CardAction> {
                return std::make_unique<CannonCardAction>(*character, type, dmg); }
        ),
        sol::base_classes, sol::bases<CardAction>()
    );

    const auto& cannon_type_record = battle_namespace.new_enum("CannonType",
        "Green", CannonCardAction::Type::green,
        "Blue", CannonCardAction::Type::blue,
        "Red", CannonCardAction::Type::red
    );

  // make loading resources easier
  // DOESNT WORK??
  /*state.script(
    "-- Shorthand load texture"
    "function LoadTexture(path)"
    "  return Engine.ResourceHandle.new().Textures:LoadFile(path)"
    "end"

    "-- Shorthand load audio"
    "function LoadAudio(path)"
    "  return Engine.ResourceHandle.new().Audio:LoadFile(path)"
    "end"

    "-- Shorthand load shader"
    "function LoadShader(path)"
    "  return Engine.ResourceHandle.new().Shaders:LoadFile(path)"
    "end"
  );*/

  // make meta object info metatable
    const auto& navimeta_table = engine_namespace.new_usertype<NaviRegistration::NaviMeta>("NaviMeta",
        "SetSpecialDescription", &NaviRegistration::NaviMeta::SetSpecialDescription,
        "SetAttack", &NaviRegistration::NaviMeta::SetAttack,
        "SetChargedAttack", &NaviRegistration::NaviMeta::SetChargedAttack,
        "SetSpeed", &NaviRegistration::NaviMeta::SetSpeed,
        "SetHP", &NaviRegistration::NaviMeta::SetHP,
        "SetIsSword", &NaviRegistration::NaviMeta::SetIsSword,
        "SetOverworldAnimationPath", &NaviRegistration::NaviMeta::SetOverworldAnimationPath,
        "SetOverworldTexturePath", &NaviRegistration::NaviMeta::SetOverworldTexturePath,
        "SetMugshotTexturePath", &NaviRegistration::NaviMeta::SetMugshotTexturePath,
        "SetMugshotAnimationPath", &NaviRegistration::NaviMeta::SetMugshotAnimationPath,
        "SetEmotionsTexturePath", &NaviRegistration::NaviMeta::SetEmotionsTexturePath,
        "SetPreviewTexture", &NaviRegistration::NaviMeta::SetPreviewTexture,
        "SetIconTexture", &NaviRegistration::NaviMeta::SetIconTexture
    );

    const auto& mobmeta_table = engine_namespace.new_usertype<MobRegistration::MobMeta>("MobInfo",
        "SetDescription", &MobRegistration::MobMeta::SetDescription,
        "SetName", &MobRegistration::MobMeta::SetName,
        "SetPreviewTexturePath", &MobRegistration::MobMeta::SetPlaceholderTexturePath,
        "SetSpeed", &MobRegistration::MobMeta::SetSpeed,
        "SetAttack", &MobRegistration::MobMeta::SetAttack,
        "SetHP", &MobRegistration::MobMeta::SetHP
    );

    const auto& scriptedmob_table = engine_namespace.new_usertype<ScriptedMob>("Mob",
        "CreateSpawner", &ScriptedMob::CreateSpawner,
        "SetBackground", &ScriptedMob::SetBackground,
        "StreamMusic", &ScriptedMob::StreamMusic,
        "Field", &ScriptedMob::GetField
    );

    const auto& scriptedspawner_table = engine_namespace.new_usertype<ScriptedMob::ScriptedSpawner>("Spawner",
        "SpawnAt", &ScriptedMob::ScriptedSpawner::SpawnAt
    );

    engine_namespace.set_function("DefineCharacter",
        [this](const std::string& fqn, const std::string& path) {
            this->DefineCharacter(fqn, path);
        }
    );

    engine_namespace.set_function("RequiresCharacter",
        [this](const std::string& fqn)
        {
            // Handle built-ins...
            auto builtins = { "BuiltIns.Canodumb", "BuiltIns.Mettaur" };
            for (auto&& match : builtins) {
                if (fqn == match) return;
            }

            if (this->FetchCharacter(fqn) == nullptr) {
                std::string msg = "Failed to Require character with FQN " + fqn;
                Logger::Log(msg);
                throw std::runtime_error(msg);
            }
        }
    );

    const auto& tile_state_table = state.new_enum("TileState",
        "Broken", TileState::broken,
        "Cracked", TileState::cracked,
        "DirectionDown", TileState::directionDown,
        "DirectionLevel", TileState::directionLeft,
        "DirectionRight", TileState::directionRight,
        "DirectionUp", TileState::directionUp,
        "Empty", TileState::empty,
        "Grass", TileState::grass,
        "Hidden", TileState::hidden,
        "Holy", TileState::holy,
        "Ice", TileState::ice,
        "Lava", TileState::lava,
        "Normal", TileState::normal,
        "Poison", TileState::poison,
        "Volcano", TileState::volcano
    );

    const auto& defense_order_table = state.new_enum("DefenseOrder",
        "Always", DefenseOrder::always,
        "CollisionOnly", DefenseOrder::collisionOnly
    );

    const auto& particle_impact_type_table = state.new_enum("ParticleType",
        "Blue", ParticleImpact::Type::blue,
        "Fire", ParticleImpact::Type::fire,
        "Green", ParticleImpact::Type::green,
        "Thin", ParticleImpact::Type::thin,
        "Volcano", ParticleImpact::Type::volcano,
        "Vulcan", ParticleImpact::Type::vulcan,
        "Wind", ParticleImpact::Type::wind,
        "Yellow", ParticleImpact::Type::yellow
    );

    const auto& elements_table = state.new_enum("Element",
        "Fire", Element::fire,
        "Aqua", Element::aqua,
        "Elec", Element::elec,
        "Wood", Element::wood,
        "Sword", Element::sword,
        "Wind", Element::wind,
        "Cursor", Element::cursor,
        "Summon", Element::summon,
        "Plus", Element::plus,
        "Break", Element::breaker,
        "None", Element::none,
        "Ice", Element::ice
    );

    const auto& direction_table = state.new_enum("Direction",
        "None", Direction::none,
        "Up", Direction::up,
        "Down", Direction::down,
        "Left", Direction::left,
        "Right", Direction::right,
        "UpLeft", Direction::up_left,
        "UpRight", Direction::up_right,
        "DownLeft", Direction::down_left,
        "DownRight", Direction::down_right
    );

    const auto& animation_mode_record = state.new_enum("Playback",
        "Once", Animator::Mode::NoEffect,
        "Loop", Animator::Mode::Loop,
        "Bounce", Animator::Mode::Bounce,
        "Reverse", Animator::Mode::Reverse
    );

    const auto& team_record = state.new_enum("Team",
        "Red", Team::red,
        "Blue", Team::blue,
        "Other", Team::unknown
    );

    const auto& highlight_record = state.new_enum("Highlight",
        "Solid", Battle::Tile::Highlight::solid,
        "Flash", Battle::Tile::Highlight::flash,
        "None", Battle::Tile::Highlight::none
    );

    const auto& add_status_record = state.new_enum("EntityStatus",
        "Queued", Field::AddEntityStatus::queued,
        "Added", Field::AddEntityStatus::added,
        "Failed", Field::AddEntityStatus::deleted
    );

    const auto& action_order_record = state.new_enum("ActionOrder",
        "Involuntary", ActionOrder::involuntary,
        "Voluntary", ActionOrder::voluntary,
        "Immediate", ActionOrder::immediate
    );

    const auto& input_event_record = state.new_enum("InputEvent",
        "pressed_move_up", InputEvents::pressed_move_up,
        "pressed_move_left", InputEvents::pressed_move_left,
        "pressed_move_right", InputEvents::pressed_move_right,
        "pressed_move_down", InputEvents::pressed_move_down,
        "pressed_use_chip", InputEvents::pressed_use_chip,
        "pressed_special", InputEvents::pressed_special,
        "pressed_shoot", InputEvents::pressed_shoot,
        "released_move_up", InputEvents::released_move_up,
        "released_move_left", InputEvents::released_move_left,
        "released_move_right", InputEvents::released_move_right,
        "released_move_down", InputEvents::released_move_down,
        "released_use_chip", InputEvents::released_use_chip,
        "released_special", InputEvents::released_special,
        "released_shoot", InputEvents::released_shoot
    );

    const auto& hitbox_flags_record = state.new_enum("Hit",
        "None", Hit::none,
        "Flinch", Hit::flinch,
        "Flash", Hit::flash,
        "Stun", Hit::stun,
        "Impact", Hit::impact,
        "Shake", Hit::shake,
        "Pierce", Hit::pierce,
        "Retangible", Hit::retangible,
        "Breaking", Hit::breaking,
        "Bubble", Hit::bubble,
        "Freeze", Hit::freeze,
        "Drag", Hit::drag
    );

    const auto& character_rank_record = state.new_enum("Rank",
        "V1", Character::Rank::_1,
        "V2", Character::Rank::_2,
        "V3", Character::Rank::_3,
        "SP", Character::Rank::SP,
        "EX", Character::Rank::EX,
        "Rare1", Character::Rank::Rare1,
        "Rare2", Character::Rank::Rare2
    );

    const auto& audio_type_record = state.new_enum("AudioType", 
        "CounterBonus", AudioType::COUNTER_BONUS,
        "DirTile", AudioType::DIR_TILE,
        "Fanfare", AudioType::FANFARE,
        "Appear", AudioType::APPEAR,
        "AreaGrab", AudioType::AREA_GRAB,
        "AreaGrabTouchdown", AudioType::AREA_GRAB_TOUCHDOWN,
        "BusterPea", AudioType::BUSTER_PEA,
        "BusterCharged", AudioType::BUSTER_CHARGED,
        "BusterCharging", AudioType::BUSTER_CHARGING,
        "BubblePop", AudioType::BUBBLE_POP,
        "BubbleSpawn", AudioType::BUBBLE_SPAWN,
        "GuardHit", AudioType::GUARD_HIT,
        "Cannon", AudioType::CANNON,
        "Counter", AudioType::COUNTER,
        "Wind", AudioType::WIND,
        "ChipCancel", AudioType::CHIP_CANCEL,
        "ChipChoose", AudioType::CHIP_CHOOSE,
        "ChipConfirm", AudioType::CHIP_CONFIRM,
        "ChipDesc", AudioType::CHIP_DESC,
        "ChipDescClose", AudioType::CHIP_DESC_CLOSE,
        "ChipError", AudioType::CHIP_ERROR,
        "CustomBarFull", AudioType::CUSTOM_BAR_FULL,
        "CustomScreenOpen", AudioType::CUSTOM_SCREEN_OPEN,
        "ItemGet", AudioType::ITEM_GET,
        "Deleted", AudioType::DELETED,
        "Explode", AudioType::EXPLODE,
        "Gun", AudioType::GUN,
        "Hurt", AudioType::HURT,
        "PanelCrack", AudioType::PANEL_CRACK,
        "PanelReturn", AudioType::PANEL_RETURN,
        "Pause", AudioType::PAUSE,
        "PreBattle", AudioType::PRE_BATTLE,
        "Recover", AudioType::RECOVER,
        "Spreader", AudioType::SPREADER,
        "SwordSwing", AudioType::SWORD_SWING,
        "TossItem", AudioType::TOSS_ITEM,
        "TossItemLite", AudioType::TOSS_ITEM_LITE,
        "Wave", AudioType::WAVE,
        "Thunder", AudioType::THUNDER,
        "ElecPulse", AudioType::ELECPULSE,
        "Invisible", AudioType::INVISIBLE,
        "ProgramAdvance", AudioType::PA_ADVANCE,
        "LowHP", AudioType::LOW_HP,
        "DarkCard", AudioType::DARK_CARD,
        "PointSFX", AudioType::POINT_SFX,
        "NewGame", AudioType::NEW_GAME,
        "Text", AudioType::TEXT,
        "Shine", AudioType::SHINE,
        "TimeFreeze", AudioType::TIME_FREEZE,
        "Meteor", AudioType::METEOR,
        "Deform", AudioType::DEFORM );

    const auto& audio_priority_record = state.new_enum("AudioPriority",
        "Lowest", AudioPriority::lowest,
        "Low", AudioPriority::low,
        "High", AudioPriority::high,
        "Highest", AudioPriority::highest);

    const auto& hitbox_props_record = state.new_usertype<Hit::Properties>("HitProps",
        "aggressor", &Hit::Properties::aggressor,
        "damage", &Hit::Properties::damage,
        "drag", &Hit::Properties::drag,
        "element", &Hit::Properties::element,
        "flags", &Hit::Properties::flags
    );

    const auto& hitbox_drag_prop_record = state.new_usertype<Hit::Drag>("Drag",
        "direction", &Hit::Drag::dir,
        "count", &Hit::Drag::count
        );

    state.set_function("Drag",
        [](Direction dir, unsigned count)
        { return Hit::Drag{ dir, count }; }
    );

    state.set_function("MakeHitProps",
        [](int damage, Hit::Flags flags, Element element, Entity::ID_t aggressor, Hit::Drag drag) {
            return Hit::Properties{ damage, flags, element, aggressor, drag };
        }
    );

    // auto& frame_time_record = state.new_usertype<frame_time_t>("Frame");
    const auto& move_event_record = state.new_usertype<MoveEvent>("MoveEvent");

    const auto& explosion_record = battle_namespace.new_usertype<Explosion>("Explosion",
        sol::factories([](int count, double speed)
            { return new Explosion(count, speed); }),
        sol::base_classes, sol::bases<Artifact>()
        );

    const auto& particle_poof = battle_namespace.new_usertype<ParticlePoof>("ParticlePoof",
        sol::constructors<ParticlePoof()>(),
        sol::base_classes, sol::bases<Artifact>()
        );

    const auto& particle_impact = battle_namespace.new_usertype<ParticleImpact>("ParticleImpact",
        sol::constructors<ParticleImpact(ParticleImpact::Type)>(),
        sol::base_classes, sol::bases<Artifact>()
        );

    const auto& color_record = state.new_usertype<sf::Color>("Color",
        sol::constructors<sf::Color(sf::Uint8, sf::Uint8, sf::Uint8, sf::Uint8)>()
    );

    const auto& vector_record = state.new_usertype<sf::Vector2f>("Vector2",
        "x", &sf::Vector2f::x,
        "y", &sf::Vector2f::y
    );

    state.set_function("frames",
        [](unsigned num)
            { return frames(num); }
    );

    state.set_function("fdata",
        [](unsigned index, double sec)
            { return OverrideFrame{ index, sec };  }
    );
}

ScriptResourceManager& ScriptResourceManager::GetInstance()
{
    static ScriptResourceManager instance;
    return instance;
}

ScriptResourceManager::~ScriptResourceManager()
{
  scriptTableHash.clear();
  for (auto ptr : states) {
    delete ptr;
  }
  states.clear();
}

ScriptResourceManager::LoadScriptResult& ScriptResourceManager::LoadScript(const std::string& path)
{
  auto iter = scriptTableHash.find(path);

  if (iter != scriptTableHash.end()) {
    return iter->second;
  }

  sol::state* lua = new sol::state;
  ConfigureEnvironment(*lua);
  states.push_back(lua);

  auto load_result = lua->safe_script_file(path, sol::script_throw_on_error);
  auto pair = scriptTableHash.emplace(path, LoadScriptResult{std::move(load_result), lua} );
  return pair.first->second;
}

void ScriptResourceManager::DefineCharacter(const std::string& fqn, const std::string& path)
{
  auto iter = characterFQN.find(fqn);

  if (iter == characterFQN.end()) {
    auto& res = LoadScript(path+"/entry.lua");

    if (res.result.valid()) {
      characterFQN[fqn] = path;
    }
    else {
      sol::error error = res.result;
      Logger::Logf("Failed to Require character with FQN %s. Reason: %s", fqn.c_str(), error.what());

      // bubble up to end this scene from loading that needs this character...
      throw std::exception(error);
    }
  }
}

sol::state* ScriptResourceManager::FetchCharacter(const std::string& fqn)
{
  auto iter = characterFQN.find(fqn);

  if (iter != characterFQN.end()) {
    return LoadScript(iter->second+"/entry.lua").state;
  }

  // else miss
  return nullptr;
}

const std::string& ScriptResourceManager::CharacterToModpath(const std::string& fqn) {
  return characterFQN[fqn];
}
#endif