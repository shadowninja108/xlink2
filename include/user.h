#pragma once

#include "resource.h"
#include "container.h"
#include "param.h"
#include "property.h"
#include "action.h"
#include "trigger.h"
#include "act.h"
#include "accessor.h"

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace banana {

class Serializer;
class System;

struct InitInfo {
    std::unordered_map<ptrdiff_t, std::string_view> strings{};
    std::unordered_map<u64, s32> triggerParams{};
    std::unordered_map<u64, s32> assetParams{};
};

class User {
public:
    bool initialize(System* sys, const xlink2::ResUserHeader* res,
                    const InitInfo& info,
                    std::set<u64>& conditions,
                    std::set<u64>& arrangeParams);

    friend class Serializer;
    friend class System;

private:
    std::vector<std::string_view> mLocalProperties{};
    std::vector<u16> mSortedAssetIds{}; // asset call table indices sorted by key name
    std::vector<Param> mUserParams{};
    std::vector<Container> mContainers{};
    std::vector<AssetCallTable> mAssetCallTables{};
    std::vector<ActionSlot> mActionSlots{};
    std::vector<Action> mActions{};
    std::vector<ActionTrigger> mActionTriggers{};
    std::vector<Property> mProperties{};
    std::vector<PropertyTrigger> mPropertyTriggers{};
    std::vector<AlwaysTrigger> mAlwaysTriggers{};
    u16 mUnknown;
};

} // namespace banana