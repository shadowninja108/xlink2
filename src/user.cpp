#include "user.h"
#include "system.h"
#include "util/error.h"

#include <cstring> // memcpy
#include <format>

namespace banana {

bool User::initialize(System* sys, const xlink2::ResUserHeader* res,
                      const InitInfo& info,
                      std::set<u64>& conditions,
                      std::set<u64>& arrangeParams) {
    if (sys == nullptr || res == nullptr) {
        throw InvalidDataError("User initialize input values were null");
    }

    mLocalProperties.resize(res->localPropertyCount);
    mSortedAssetIds.resize(res->callCount);
    mUserParams.resize(sys->getPDT().getUserParamCount());
    mAssetCallTables.resize(res->callCount);
    mActionSlots.resize(res->actionSlotCount);
    mActions.resize(res->actionCount);
    mActionTriggers.resize(res->actionTriggerCount);
    mProperties.resize(res->propertyCount);
    mPropertyTriggers.resize(res->propertyTriggerCount);
    mAlwaysTriggers.resize(res->alwaysTriggerCount);

    mUnknown = res->unk;

    const u64* locals = reinterpret_cast<const u64*>(res + 1);
    for (u32 i = 0; i < mLocalProperties.size(); ++i) {
        mLocalProperties[i] = info.strings.at(*locals);
        ++locals;
    }

    const xlink2::ResParam* params = reinterpret_cast<const xlink2::ResParam*>(locals);
    for (u32 i = 0; i < mUserParams.size(); ++i) {
        mUserParams[i].type = params->getValueReferenceType();
        mUserParams[i].value = params->getValue();
        if (params->getValueReferenceType() == xlink2::ValueReferenceType::ArrangeParam) {
            arrangeParams.insert(params->getValue());
        }
        mUserParams[i].index = i;
        ++params;
    }

    const u16* id_ptr = reinterpret_cast<const u16*>(params);
    for (u32 i = 0; i < mSortedAssetIds.size(); ++i) {
        mSortedAssetIds[i] = *id_ptr;
        ++id_ptr;
    }

    auto actionSlots = reinterpret_cast<const xlink2::ResActionSlot*>(reinterpret_cast<uintptr_t>(res) + res->triggerTableOffset);
    for (u32 i = 0; i < mActionSlots.size(); ++i) {
        mActionSlots[i].actionSlotName = info.strings.at(actionSlots->nameOffset);
        mActionSlots[i].actionStartIdx = actionSlots->actionStartIdx;
        mActionSlots[i].actionCount = actionSlots->actionEndIdx - actionSlots->actionStartIdx;
        ++actionSlots;
    }

    auto actions = reinterpret_cast<const xlink2::ResAction*>(actionSlots);
    for (u32 i = 0; i < mActions.size(); ++i) {
        mActions[i].actionName = info.strings.at(actions->nameOffset);
        mActions[i].actionTriggerStartIdx = actions->triggerStartIdx;
        mActions[i].actionTriggerCount = static_cast<s16>(actions->triggerEndIdx - actions->triggerStartIdx);
        mActions[i].enableMatchStart = actions->enableMatchStart;
        ++actions;
    }

    auto actionTriggers = reinterpret_cast<const xlink2::ResActionTrigger*>(actions);
    for (u32 i = 0; i < mActionTriggers.size(); ++i) {
        mActionTriggers[i].guid = actionTriggers->guid;
        mActionTriggers[i].unk = actionTriggers->unk;
        mActionTriggers[i].triggerOnce = actionTriggers->isFlagSet(xlink2::ResActionTrigger::Flag::TriggerOnce);
        mActionTriggers[i].fade = actionTriggers->isFlagSet(xlink2::ResActionTrigger::Flag::Fade);
        mActionTriggers[i].alwaysTrigger = actionTriggers->isFlagSet(xlink2::ResActionTrigger::Flag::AlwaysTrigger);
        mActionTriggers[i].nameMatch = actionTriggers->isFlagSet(xlink2::ResActionTrigger::Flag::NameMatch);
        if (mActionTriggers[i].nameMatch) {
            mActionTriggers[i].previousActionName = info.strings.at(actionTriggers->previousActionNameOffset);
        } else {
            mActionTriggers[i].startFrame = actionTriggers->startFrame;
        }
        mActionTriggers[i].endFrame = actionTriggers->endFrame;
        if (actionTriggers->overwriteParamOffset != 0xffffffff) {
            mActionTriggers[i].triggerOverwriteIdx = info.triggerParams.at(actionTriggers->overwriteParamOffset);
        } else {
            mActionTriggers[i].triggerOverwriteIdx = -1;
        }
        mActionTriggers[i].overwriteHash = actionTriggers->overwriteHash;
        mActionTriggers[i].assetCallIdx = actionTriggers->assetCallTableOffset / sizeof(xlink2::ResAssetCallTable);
        ++actionTriggers;
    }

    auto properties = reinterpret_cast<const xlink2::ResProperty*>(actionTriggers);
    for (u32 i = 0; i < mProperties.size(); ++i) {
        mProperties[i].propertyName = info.strings.at(properties->nameOffset);
        mProperties[i].propTriggerStartIdx = properties->triggerStartIdx;
        mProperties[i].propTriggerCount = properties->triggerEndIdx - properties->triggerStartIdx;
        mProperties[i].isGlobal = properties->isGlobal;
        ++properties;
    }

    auto propertyTriggers = reinterpret_cast<const xlink2::ResPropertyTrigger*>(properties);
    for (u32 i = 0; i < mPropertyTriggers.size(); ++i) {
        mPropertyTriggers[i].guid = propertyTriggers->guid;
        mPropertyTriggers[i].flag = propertyTriggers->flag;
        mPropertyTriggers[i].overwriteHash = propertyTriggers->overwriteHash;
        if (propertyTriggers->overwriteParamOffset != 0xffffffff) {
            mPropertyTriggers[i].triggerOverwriteIdx = info.triggerParams.at(propertyTriggers->overwriteParamOffset);
        } else {
            mPropertyTriggers[i].triggerOverwriteIdx = -1;
        }
        mPropertyTriggers[i].assetCallTableIdx = propertyTriggers->assetCallTableOffset / sizeof(xlink2::ResAssetCallTable);
        // write this temporarily which we will come back and fix once all users are parsed
        if (propertyTriggers->conditionOffset != 0xffffffff) {
            mPropertyTriggers[i].conditionIdx = propertyTriggers->conditionOffset;
            conditions.emplace(propertyTriggers->conditionOffset);
        } else {
            mPropertyTriggers[i].conditionIdx = -1;
        }
        ++propertyTriggers;
    }

    auto alwaysTriggers = reinterpret_cast<const xlink2::ResAlwaysTrigger*>(propertyTriggers);
    for (u32 i = 0; i < mAlwaysTriggers.size(); ++i) {
        mAlwaysTriggers[i].guid = alwaysTriggers->guid;
        mAlwaysTriggers[i].flag = alwaysTriggers->flag;
        mAlwaysTriggers[i].overwriteHash = alwaysTriggers->overwriteHash;
        mAlwaysTriggers[i].assetCallIdx = alwaysTriggers->assetCallTableOffset / sizeof(xlink2::ResAssetCallTable);
        if (alwaysTriggers->overwriteParamOffset != 0xffffffff) {
            mAlwaysTriggers[i].triggerOverwriteIdx = info.triggerParams.at(alwaysTriggers->overwriteParamOffset);
        } else {
            mAlwaysTriggers[i].triggerOverwriteIdx = -1;
        }
        ++alwaysTriggers;
    }

    // alignment
    auto act = reinterpret_cast<const xlink2::ResAssetCallTable*>(id_ptr + mSortedAssetIds.size() % 2);

    const uintptr_t containers = reinterpret_cast<uintptr_t>(act + res->callCount);
    
    std::set<u64> containerOffsets{};
    for (u32 i = 0; i < mAssetCallTables.size(); ++i) {
        mAssetCallTables[i].keyName = info.strings.at(act->keyNameOffset);
        mAssetCallTables[i].assetIndex = act->assetIndex;
        mAssetCallTables[i].flag = act->flag;
        mAssetCallTables[i].duration = act->duration;
        mAssetCallTables[i].parentIndex = act->parentIndex;
        mAssetCallTables[i].guid = act->guid;
        mAssetCallTables[i].keyNameHash = act->keyNameHash;
        if (act->isContainer()) {
            containerOffsets.insert(act->paramOffset);
            mAssetCallTables[i].containerParamIdx = act->paramOffset;
        } else {
            mAssetCallTables[i].assetParamIdx = info.assetParams.at(act->paramOffset);
        }
        if (act->conditionOffset != 0xffffffff) {
            mAssetCallTables[i].conditionIdx = act->conditionOffset;
            conditions.emplace(act->conditionOffset);
        } else {
            mAssetCallTables[i].conditionIdx = -1;
        }
        ++act;
    }

    mContainers.resize(containerOffsets.size());
    std::unordered_map<u64, s32> containerIdxMap{};
    for (u32 i = 0; const auto offset : containerOffsets) {
        auto containerBase = reinterpret_cast<const xlink2::ResContainerParam*>(containers + offset);
        mContainers[i].type = containerBase->getType();
        mContainers[i].childContainerStartIdx = containerBase->childStartIdx;
        mContainers[i].childCount = containerBase->childEndIdx - containerBase->childStartIdx;
        mContainers[i].isNotBlendAll = containerBase->isNotBlendAll;
        mContainers[i].isNeedObserve = containerBase->isNeedObserve;
        switch (containerBase->getType()) {
            case xlink2::ContainerType::Switch: {
                auto param = static_cast<const xlink2::ResSwitchContainerParam*>(containerBase);
                auto container = mContainers[i].getAs<xlink2::ContainerType::Switch>();
                container->isGlobal = param->isGlobal;
                container->isActionTrigger = param->isActionTrigger;
                container->actionSlotName = info.strings.at(param->actionSlotNameOffset); // property name if not action trigger
                container->propertyIndex = param->propertyIndex;
                container->unk = param->_00;
                break;
            }
            case xlink2::ContainerType::Random:
            case xlink2::ContainerType::Random2:
                break;
            case xlink2::ContainerType::Blend: {
                if (!mContainers[i].isNotBlendAll)
                    break;
                auto param = static_cast<const xlink2::ResBlendContainerParam2*>(containerBase);
                auto container = mContainers[i].getAs<xlink2::ContainerType::Blend, true>();
                container->isGlobal = param->isGlobal;
                // these can't be action triggers
                container->isActionTrigger = param->isActionTrigger;
                container->actionSlotName = info.strings.at(param->actionSlotNameOffset); // property name if not action trigger
                container->propertyIndex = param->propertyIndex;
                container->unk = param->_00;
                break;
            }
            case xlink2::ContainerType::Sequence:
                break;
            case xlink2::ContainerType::Grid: {
                auto param = static_cast<const xlink2::ResGridContainerParam*>(containerBase);
                auto container = mContainers[i].getAs<xlink2::ContainerType::Grid>();
                container->propertyName1 = info.strings.at(param->propertyNameOffset1);
                container->propertyName2 = info.strings.at(param->propertyNameOffset2);
                container->propertyIndex1 = param->propertyIndex1;
                container->propertyIndex2 = param->propertyIndex2;
                container->isGlobal1 = param->isProperty1Global();
                container->isGlobal2 = param->isProperty2Global();
                const u32* values = reinterpret_cast<const u32*>(param + 1);
                container->values1.resize(param->propertyValueCount1);
                std::memcpy(container->values1.data(), values, sizeof(u32) * param->propertyValueCount1);
                values += param->propertyValueCount1;
                container->values2.resize(param->propertyValueCount2);
                std::memcpy(container->values2.data(), values, sizeof(u32) * param->propertyValueCount2);
                const s32* idx = reinterpret_cast<const s32*>(values + param->propertyValueCount2);
                container->indices.resize(param->propertyValueCount1 * param->propertyValueCount2);
                std::memcpy(container->indices.data(), idx, sizeof(u32) * param->propertyValueCount1 * param->propertyValueCount2);
                break;
            }
            case xlink2::ContainerType::Jump:
                break;
            // Mono is not a valid container type for the resource file
            default:
                throw ResourceError(std::format("Invalid container type: {:#x}", static_cast<u32>(containerBase->getType())));
        }

        containerIdxMap.emplace(offset, i);
        ++i;
    }

    // fixup container indices for each asset call table
    for (auto& act : mAssetCallTables) {
        if (act.isContainer()) {
            act.containerParamIdx = containerIdxMap.at(act.containerParamIdx);
        }
    }

    return true;
}

} // namespace banana