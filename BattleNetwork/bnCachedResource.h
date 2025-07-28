#pragma once
#include "bnCurrentTime.h"
#include <Swoosh/Timer.h>
#include <memory>

/*! \brief Represents a cached resource of type T. It knows how frequently it is used.
*/
template<typename T>
class CachedResource {
  long long lastRequestTime{}; //!< Used to determine if we should remove this resource
  unsigned int relevanceCount{}; //!< Higher count means this is important
  bool permanent{}; //!< Never delete resource when timer is up
  using SharedPtrType = std::shared_ptr<std::remove_pointer_t<T>>;
  SharedPtrType resource; //!< shared ptr to the actual resource
public:

  /*! \brief Constructor creates a shared resouce, pauses the timer, and sets count to 0*/
  CachedResource(const T& item, bool perm = false) : 
    resource(std::make_shared(item)),
    permanent(perm)
  {
    lastRequestTime = CurrentTime::AsMilli();
    relevanceCount = 0;
  }

  CachedResource(SharedPtrType ref, bool perm = false) : 
    resource(ref),
    permanent(perm)
  {
    lastRequestTime = CurrentTime::AsMilli();
    relevanceCount = 0;
  }

  /*! \brief returns get() counter*/
  const unsigned int GetRelevanceCount() const {
    return relevanceCount;
  }

  /*! \brief checks if this resource is still in use*/
  const bool IsInUse() const {
    return resource.use_count() >= 1 || permanent;
  }

  /*! \brief returns the resource
    This function also increases the use count and resets
    the "last requested" timer.
  */
  SharedPtrType GetResource() {
    relevanceCount++;
    lastRequestTime = CurrentTime::AsMilli();
    return resource;
  }

  /*! \brief return the seconds since this item was last requested */
  const float GetSecondsSinceLastRequest() {
    return (CurrentTime::AsMilli() - lastRequestTime)/1000.0f;
  }
};
