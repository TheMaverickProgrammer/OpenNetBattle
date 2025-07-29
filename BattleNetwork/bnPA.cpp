#include "bnPA.h"
#include "bnLogger.h"
#include "bnFileUtil.h"

#include <assert.h>
#include <iostream>

PA::PA()
{
  advanceCardRef = nullptr;
}


PA::~PA()
{
  if (advanceCardRef) {
    // PA manages the memory it creates with `new`
    delete advanceCardRef;
    advanceCardRef = nullptr;
  }

  advances.clear();
}

void PA::RegisterPA(PACard entry)
{
  std::vector<PA::StepData> vec(entry.steps.begin(), entry.steps.begin() + entry.steps.size());

  std::sort(vec.begin(), vec.end(), [](PA::StepData a, PA::StepData b) { return a.name < b.name; });
  vec.erase(unique(vec.begin(), vec.end(), [](PA::StepData a, PA::StepData b) { return a.name == b.name; }), vec.end());

  if (vec.size() == 1u) {
    entry.ruleType = PA::RuleType::dupes;
  }

  advances.push_back(entry);
}

const PA::Steps PA::GetMatchingSteps()
{
  PA::Steps result;

  for (int i = 0; i < iter->steps.size(); i++) {
    result.push_back(iter->steps[i]);
  }

  return result;
}

Battle::Card& PA::GetAdvanceCard()
{
  return *advanceCardRef;
}

const int PA::FindPA(std::vector<Battle::Card>& input)
{
  int startIndex = -1;
  auto size = input.size();

  if (size == 0) {
    return startIndex;
  }

  for (iter = advances.begin(); iter != advances.end(); iter++) {
    bool match = false;
    
    if (iter->steps.size() > size) {
      continue; // try next recipe
    }

    int index = 0;
    while (index <= (int)size - (int)iter->steps.size()) {
      size_t wildCodeCount = 0;
      bool strictWildPattern = iter->ruleType == PA::RuleType::dupes;

      for (unsigned i = 0; i < iter->steps.size(); i++) {
        char code_i = input[i + index].GetCode();
        bool isSameCode = code_i == iter->steps[i].code;
        bool isWildStar = code_i == '*';
        bool isSameCard = iter->steps[i].name == input[i + index].GetShortName();

        if (isWildStar) {
          wildCodeCount++;
        }

        if ((isWildStar || isSameCode) && isSameCard) {
          if (isWildStar && strictWildPattern && wildCodeCount > 1) {
            match = false;
            startIndex = -1;
            break;
          }

          match = true;
          // We do not break here. If it is a match all across the steps, then the for loop ends 
          // and match stays == true

          if (startIndex == -1) {
            startIndex = i + index;
          }
        }
        else {
          match = false;
          startIndex = -1;
          break;
        }
      }

      if (match) break;

      index++;
    }

    if (match) {
      // Load the PA card
      if (advanceCardRef) { delete advanceCardRef; }

      advanceCardRef = new Battle::Card({
        iter->uuid,
        iter->damage,
        0,
        '*',
        iter->canBoost,
        iter->timeFreeze,
        false,
        true,
        iter->name,
        iter->action,
        iter->action,
        "combo",
        iter->primaryElement,
        iter->secondElement,
        Battle::CardClass::standard,
        iter->metaClasses
      });

      return startIndex;
    }

    // else keep looking
    startIndex = -1;
  }

  return startIndex;
}

