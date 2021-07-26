#pragma once

class Entity;
class BattleSceneBase;

/**
 * @class Component
 * @author mav
 * @date 05/05/19
 * @brief Components need to be genric enough to be used by all in-battle entities
 *
 * Components can be attached to (and owned by) any Entity in the game
 * This allows for custom behavior on pre-existing effects, characters, and attacks
 */
class Component {
public:
  friend class BattleSceneBase;

  using ID_t = long;

  enum class lifetimes {
    local      = 0, // component update tick happens in the attached entitiy 
    battlestep = 1, // component is injected into the scene and update tick happens during battle steps
    ui         = 2  // component is injected into the scene and update tick happens every app tick
  };
private:
  BattleSceneBase* scene{ nullptr };
  static long numOfComponents; /*!< Resource counter to generate new IDs */
  lifetimes lifetime{lifetimes::local};
  Entity* owner; /*!< Who the component is attached to */
  ID_t ID; /*!< ID for quick lookups, resource management, and scripting */

protected:
  /**
 * @brief Update must be implemented by child class
 * @param _elapsed in seconds
 */
  virtual void OnUpdate(double _elapsed) = 0;

public:
  Component() = delete;
  
  /**
   * @brief Sets an owner and ID. Increments numOfComponents beforehand.
   * @param owner the entity to attach to
   */
  Component(Entity* owner, lifetimes lifetime = lifetimes::local);
  virtual ~Component();

  Component(Component&& rhs) = delete;
  Component(const Component& rhs) = delete;

  /**
   * @brief Get the owner as an Entity
   * @return Entity*
   */
  Entity* GetOwner();

  /**
   * @brief Get the owner as an Entity (const. qualified)
   * @return const Entity*
   */
  const Entity* GetOwner() const;

  /**
  * @brief Query if this component has been injected into the battle scene's ui or logic loop 
  */

  const bool Injected();

  BattleSceneBase* Scene();

  /**
  * @brief Return the type of lifetime this component has
  */
  const lifetimes Lifetime();

  /**
   * @brief Get the owner as a special type
   * @return T* if dynamic_cast is successful, otherwise null if type T is incompatible
   */
  template<typename T>
  T* GetOwnerAs() { return dynamic_cast<T*>(owner); }

  /**
   * @brief Releases the pointer and sets it to null
   * 
   * Useful to identify if an entity has been removed from game but a component
   * needs to act accordingly to this information
   */
  void FreeOwner();

  /**
   * @brief Get the Id of the component
   * @return ID
   */
  const ID_t GetID() const;

  void Eject();

  /**
  * @brief Updates the component during its correct lifetime
  * 
  * If a local component, calls onUpdate()
  * If a battle scene component, ensures that it is injected correctly
  */
  void Update(double elapsed);
  
  /**
   * @brief Some components can be injected into the battle routine for custom behavior
   * 
   * Some components modify the battle routine and the battle scene needs to know about their
   * existence.
   * 
   * Some components need to exist in the global update loop outside of the Entity's update loop,
   * and injecting is a great way to ensure the component is updated with the scene.
   * 
   * @warning Components injected into the battle scene are updated and deleted. Free the owner if injecting.
   */
  virtual void Inject(BattleSceneBase&) = 0;
};
