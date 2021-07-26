#include "bnActionQueue.h"
#include <algorithm>

ActionQueue::~ActionQueue() {
  ClearQueue(ActionQueue::CleanupType::no_interrupts);
}

ActionOrder ActionQueue::ApplyPriorityFilter(const ActionOrder& in) {
  auto iter = priorityFilters.find(in);

  if (iter != priorityFilters.end()) {
    return iter->second;
  }

  return in;
}

ActionQueue::Index ActionQueue::ApplyDiscardFilter(const ActionQueue::Index& in) {
  // Filter types and apply new discard operations
  auto iter = discardFilters.find(in.type);

  Index out = in;

  if (iter != discardFilters.end()) {
      out.discardOp = iter->second;
  }

  return out;
}

bool ActionQueue::IsProcessing(const ActionQueue::Index& in) {
  return in.processing;
}

void ActionQueue::CreatePriorityFilter(const ActionOrder& target, const ActionOrder& newOrder) {
  priorityFilters.insert(std::make_pair(target, newOrder));
}

void ActionQueue::CreateDiscardFilter(const ActionTypes& type, const ActionDiscardOp& newOp) {
  discardFilters.insert(std::make_pair(type, newOp));
}

void ActionQueue::ClearFilters() {
  clearFilters = true;

  if (clearFilters) {
    priorityFilters.clear();
    discardFilters.clear();
    clearFilters = false;
  }
}

void ActionQueue::Sort() {
  // if toggleInterval is true, voluntary preceeds involuntary
  std::sort(indices.begin(), indices.end(),
  [this](const ActionQueue::Index& first, const ActionQueue::Index& second) -> bool {
    ActionOrder first_order = ApplyPriorityFilter(first.order);
    ActionOrder second_order = ApplyPriorityFilter(second.order);

    if (IsProcessing(first) || IsProcessing(second)) {
        // invert true to false so the truthy indices come first in the queue...
        return !IsProcessing(first) < !IsProcessing(second);
    }
    
    // toggleInterval switches between voluntary and involuntary priority
    if (first_order == ActionOrder::voluntary
        && second_order == ActionOrder::involuntary
        && toggleInterval) {
        return true;
    }
    else if (first_order < second_order && !toggleInterval) {
        return true;
    }
    
    // when filters are added, ignore this subtype check
    if (discardFilters.empty()) {
      if (first_order == ActionOrder::voluntary
        && second_order == ActionOrder::voluntary) {
        return first.type < second.type;
      }
    }

    // else
    return false;
  });
}

ActionTypes ActionQueue::TopType() {
  if (indices.empty()) { return ActionTypes::none; }
  return indices[0].type;
}

bool ActionQueue::IsEmpty() const
{
  return indices.empty();
}

void ActionQueue::Process() {
  // Resort and apply any new priority filters to the sort
  Sort();

  ActionTypes top = TopType();

  if (top == ActionTypes::none) {
    if (idleCallback) idleCallback();
    return; // nothing to process. abort
  }

  handlers[top](ExecutionType::process); // invoke handler

  // Remove anything that has a discard op of EOF
  // and didn't get resolved this frame
  auto iter = indices.begin();
  while (iter != indices.end()) {
    if (iter->processing == false) {
      ActionQueue::Index index = ApplyDiscardFilter(*iter);
      if (index.discardOp == ActionDiscardOp::until_eof) {
        poppers[index.type](index.index);
        iter = indices.erase(iter);
        continue;
      }
    }

    iter++;
  }
}

void ActionQueue::Pop() {
  if (indices.empty()) return;

  ActionTypes queue = TopType();
  auto iter = types.find(queue);

  if (iter != types.end()) {
    ActionQueue::Index idx = indices[0];
    size_t index = idx.index;
   
    if (idx.order == ActionOrder::voluntary) {
      toggleInterval = false; // switches to involuntary first
    }
    else if (idx.order == ActionOrder::involuntary) {
      toggleInterval = true; // switches to voluntary first
    }

    poppers[queue](index);
    indices.erase(indices.begin());

    // NOTE: I had this commented but don't know why
    // It might have something to do with directional tiles... idk
    /*
    // below is frame-perfect implementation of the next queue item
    ActionTypes top = TopType();

    if (top == ActionTypes::none) 
      return; // nothing to process. abort

    handlers[top](ExecutionType::reserve);
    */
  }
}

void ActionQueue::ClearQueue(ActionQueue::CleanupType cleanup)
{
  if (indices.empty()) return;

  ActionTypes queue = TopType();
  auto iter = types.find(queue);
  bool interrupt = cleanup == ActionQueue::CleanupType::allow_interrupts && indices[0].processing;

  if (iter != types.end() && interrupt) {
    handlers[queue](ActionQueue::ExecutionType::interrupt);
  }

  while (indices.size()) {
    size_t index = indices[0].index;
    poppers[queue](index);

    indices.erase(indices.begin());
    queue = TopType();
  }

  discardFilters.clear();
  priorityFilters.clear();

  if (cleanup == ActionQueue::CleanupType::clear_and_reset) {
    // The only thing we need to reset at the moment is 
    // the actionable callback as we may be in the deconstructor
    // of an object that owns the callback functor
    idleCallback = nullptr;
  }
}

void ActionQueue::SetIdleCallback(const std::function<void()>& callback)
{
  idleCallback = callback;
}
