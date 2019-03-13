#pragma once

#include "bnEntity.h"
#include "bnMeta.h"

template<typename T>
class AI;

template<class T>
class AIState
{
  friend class AI<T>;

private:
  AIState<T>* nextState;

  /* Transfer ownership */
  AIState<T>* GetNextState() {
    AIState<T>* out = nextState;
    nextState = nullptr;
    return out;
  }


public:
  AIState() { nextState = nullptr; }
  AIState(const AIState<T>& rhs) = default;
  AIState(AIState<T>&& ref) = default;

  template<class U, typename ...Args>
  void ChangeState(Args... args) {
    if (nextState) { delete nextState; }

    nextState = new U(args...);
  }

  template<class U>
  void ChangeState() {
    if (nextState) { delete nextState; }

    nextState = new U();
  }

  AIState<T>* Update(float _elapsed, T& context) {
    // nextState could be non-null after update
    OnUpdate(_elapsed, context);

    return nextState;
  }

  virtual ~AIState() { ; }

  virtual void OnEnter(T& context) = 0;
  virtual void OnUpdate(float _elapsed, T& context) = 0;
  virtual void OnLeave(T& context) = 0;
};

