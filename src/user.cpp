#include "user.h"
#include "system.h"
#include "util/error.h"

#include <cstring> // memcpy
#include <format>

namespace banana {

bool User::initialize(System* sys, const xlink2::ResUserHeader* res,
                      const InitInfo& info,
                      const std::unordered_map<TargetPointer, s32>& conditions,
                      std::set<TargetPointer>& arrangeParams) {
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

#if XLINK_TARGET_IS_TOTK
    mUnknown = res->unk;
#endif

    auto locals = reinterpret_cast<const TargetPointer*>(res + 1);
    for (size_t i = 0; i < mLocalProperties.size(); ++i) {
        mLocalProperties[i] = info.strings.at(*locals);
        ++locals;
    }

    const auto* params = reinterpret_cast<const xlink2::ResParam*>(locals);
    for (size_t i = 0; i < mUserParams.size(); ++i) {
        auto& paramModel = mUserParams[i];
        paramModel.type = params->getValueReferenceType();
        paramModel.value = params->getValue();
        if (params->getValueReferenceType() == xlink2::ValueReferenceType::ArrangeParam) {
            arrangeParams.insert(params->getValue());
        }
        paramModel.index = static_cast<s32>(i);
        ++params;
    }

    const u16* id_ptr = reinterpret_cast<const u16*>(params);
    for (u16& mSortedAssetId : mSortedAssetIds) {
        mSortedAssetId = *id_ptr;
        ++id_ptr;
    }

    auto actionSlots = reinterpret_cast<const xlink2::ResActionSlot*>(reinterpret_cast<uintptr_t>(res) + res->triggerTableOffset);
    for (auto& actionSlotModel : mActionSlots) {
        actionSlotModel.actionSlotName = info.strings.at(actionSlots->nameOffset);
        actionSlotModel.actionStartIdx = actionSlots->actionStartIdx;
        actionSlotModel.actionCount = static_cast<s16>(actionSlots->actionEndIdx - actionSlots->actionStartIdx);
        ++actionSlots;
    }

    auto actions = reinterpret_cast<const xlink2::ResAction*>(actionSlots);
    for (auto& actionModel : mActions) {
        actionModel.actionName = info.strings.at(actions->nameOffset);
        actionModel.actionTriggerStartIdx = actions->triggerStartIdx;
        actionModel.actionTriggerCount = static_cast<s16>(actions->triggerEndIdx - actions->triggerStartIdx);
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
        mActions[i].enableMatchStart = actions->enableMatchStart;
#endif
        ++actions;
    }

    auto actionTriggers = reinterpret_cast<const xlink2::ResActionTrigger*>(actions);
    for (auto& triggerModel : mActionTriggers) {
        triggerModel.guid = actionTriggers->guid;
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
        triggerModel.unk = actionTriggers->unk;
#endif
        triggerModel.triggerOnce = actionTriggers->isFlagSet(xlink2::ResActionTrigger::Flag::TriggerOnce);
        triggerModel.fade = actionTriggers->isFlagSet(xlink2::ResActionTrigger::Flag::Fade);
        triggerModel.alwaysTrigger = actionTriggers->isFlagSet(xlink2::ResActionTrigger::Flag::AlwaysTrigger);
        triggerModel.nameMatch = actionTriggers->isFlagSet(xlink2::ResActionTrigger::Flag::NameMatch);
        if (triggerModel.nameMatch) {
            triggerModel.previousActionName = info.strings.at(actionTriggers->previousActionNameOffset);
        } else {
            triggerModel.startFrame = actionTriggers->startFrame;
        }
        triggerModel.endFrame = actionTriggers->endFrame;
        if (static_cast<s32>(actionTriggers->overwriteParamOffset) != -1) {
            triggerModel.triggerOverwriteIdx = info.triggerParams.at(actionTriggers->overwriteParamOffset);
        } else {
            triggerModel.triggerOverwriteIdx = -1;
        }
        triggerModel.overwriteHash = actionTriggers->overwriteHash;
        triggerModel.assetCallIdx = static_cast<s32>(actionTriggers->assetCallTableOffset / sizeof(xlink2::ResAssetCallTable));
        ++actionTriggers;
    }

    auto properties = reinterpret_cast<const xlink2::ResProperty*>(actionTriggers);
    for (auto& propertyModel : mProperties) {
        propertyModel.propertyName = info.strings.at(properties->nameOffset);
        propertyModel.propTriggerStartIdx = properties->triggerStartIdx;
        propertyModel.propTriggerCount = properties->triggerEndIdx - properties->triggerStartIdx;
        propertyModel.isGlobal = properties->isGlobal;
        ++properties;
    }

    auto propertyTriggers = reinterpret_cast<const xlink2::ResPropertyTrigger*>(properties);
    for (auto& triggerModel : mPropertyTriggers) {
        triggerModel.guid = propertyTriggers->guid;
        triggerModel.flag = propertyTriggers->flag;
        triggerModel.overwriteHash = propertyTriggers->overwriteHash;
        if (static_cast<s32>(propertyTriggers->overwriteParamOffset) != -1) {
            triggerModel.triggerOverwriteIdx = info.triggerParams.at(propertyTriggers->overwriteParamOffset);
        } else {
            triggerModel.triggerOverwriteIdx = -1;
        }
        triggerModel.assetCallTableIdx = static_cast<s32>(propertyTriggers->assetCallTableOffset / sizeof(xlink2::ResAssetCallTable));
        // write this temporarily which we will come back and fix once all users are parsed
        if (static_cast<s32>(propertyTriggers->conditionOffset) != -1) {
            const auto foundCond = conditions.find(propertyTriggers->conditionOffset);
            if (foundCond == conditions.end()) {
                triggerModel.conditionIdx = -1;
            } else { 
                triggerModel.conditionIdx = foundCond->second;
            }
        } else {
            triggerModel.conditionIdx = -1;
        }
        ++propertyTriggers;
    }

    auto alwaysTriggers = reinterpret_cast<const xlink2::ResAlwaysTrigger*>(propertyTriggers);
    for (auto& triggerModel : mAlwaysTriggers) {
        triggerModel.guid = alwaysTriggers->guid;
        triggerModel.flag = alwaysTriggers->flag;
        triggerModel.overwriteHash = alwaysTriggers->overwriteHash;
        triggerModel.assetCallIdx = static_cast<s32>(alwaysTriggers->assetCallTableOffset / sizeof(xlink2::ResAssetCallTable));
        if (static_cast<s32>(alwaysTriggers->overwriteParamOffset) != -1) {
            triggerModel.triggerOverwriteIdx = info.triggerParams.at(alwaysTriggers->overwriteParamOffset);
        } else {
            triggerModel.triggerOverwriteIdx = -1;
        }
        ++alwaysTriggers;
    }

    // alignment
    auto actIt = reinterpret_cast<const xlink2::ResAssetCallTable*>(id_ptr + mSortedAssetIds.size() % 2);

    const auto containers = reinterpret_cast<uintptr_t>(actIt + res->callCount);
    
    std::set<TargetPointer> containerOffsets{};
    for (auto& act : mAssetCallTables) {
        act.keyName = info.strings.at(actIt->keyNameOffset);
        act.assetIndex = actIt->assetIndex;
        act.flag = actIt->flag;
        act.duration = actIt->duration;
        act.parentIndex = actIt->parentIndex;
        act.guid = actIt->guid;
        act.keyNameHash = actIt->keyNameHash;
        if (actIt->isContainer()) {
            containerOffsets.insert(actIt->paramOffset);
            act.containerParamIdx = static_cast<s32>(actIt->paramOffset);
        } else {
            act.assetParamIdx = info.assetParams.at(actIt->paramOffset);
        }
        if (static_cast<s32>(actIt->conditionOffset) != -1) {
            const auto foundCond = conditions.find(actIt->conditionOffset);
            if (foundCond == conditions.end()) {
                act.conditionIdx = -1;
            } else { 
                act.conditionIdx = foundCond->second;
            }
        } else {
            act.conditionIdx = -1;
        }
        ++actIt;
    }

    mContainers.resize(containerOffsets.size());
    std::unordered_map<TargetPointer, s32> containerIdxMap{};
    for (size_t i = 0; const auto offset : containerOffsets) {
        auto containerBase = reinterpret_cast<const xlink2::ResContainerParam*>(containers + offset);
        auto& containerModel = mContainers[i];
        containerModel.type = containerBase->getType();
        containerModel.childContainerStartIdx = containerBase->childStartIdx;
        containerModel.childCount = containerBase->childEndIdx - containerBase->childStartIdx;
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
        containerModel.isNotBlendAll = containerBase->isNotBlendAll;
        containerModel.isNeedObserve = containerBase->isNeedObserve;
#endif
        switch (containerBase->getType()) {
            case xlink2::ContainerType::Switch: {
                auto param = static_cast<const xlink2::ResSwitchContainerParam*>(containerBase);
                auto* container = containerModel.getAs<xlink2::ContainerType::Switch>();
                container->isGlobal = param->isGlobal;
                container->actionSlotName = info.strings.at(param->actionSlotNameOffset); // property name if not action trigger
                container->propertyIndex = param->propertyIndex;
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                container->isActionTrigger = param->isActionTrigger;
                container->unk = param->_00;
#else
                container->watchPropertyId = param->watchPropertyId;
#endif
                break;
            }
            case xlink2::ContainerType::Random:
            case xlink2::ContainerType::Random2:
                break;
            case xlink2::ContainerType::Blend: {
                /* TODO: ??? */
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                if (!containerModel.isNotBlendAll)
                    break;
                auto param = static_cast<const xlink2::ResBlendContainerParam2*>(containerBase);
                auto container = containerModel.getAs<xlink2::ContainerType::Blend, true>();
                container->isGlobal = param->isGlobal;
                // these can't be action triggers
                container->isActionTrigger = param->isActionTrigger;
                container->actionSlotName = info.strings.at(param->actionSlotNameOffset); // property name if not action trigger
                container->propertyIndex = param->propertyIndex;
                container->unk = param->_00;
                break;
#endif
            }
            case xlink2::ContainerType::Sequence:
                break;
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
            case xlink2::ContainerType::Grid: {
                auto param = static_cast<const xlink2::ResGridContainerParam*>(containerBase);
                auto container = containerModel.getAs<xlink2::ContainerType::Grid>();
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
#endif
            // Mono is not a valid container type for the resource file
            default:
                throw ResourceError(std::format("Invalid container type: {:#x}", static_cast<u32>(containerBase->getType())));
        }

        containerIdxMap.emplace(offset, i);
        ++i;
    }

    // fixup container indices for each asset call table
    for (auto& act : mAssetCallTables) {
        if (!act.isContainer())
            continue;

        act.containerParamIdx = containerIdxMap.at(act.containerParamIdx);
    }

    return true;
}

} // namespace banana