#include "serializer.h"
#include "util/error.h"
#include "util/crc32.h"

#include <algorithm>
#include <iostream>
#include <format>
#include <ranges>

namespace banana {

/*
File Section Order:
- Header
- User Hashes
- User Offsets
- Param Define Table (aligned 8)
  - Header
  - User Param Defines
  - Asset Param Defines
  - Trigger Param Defines
  - PDT Name Table
- Asset Params (aligned 8)
- Trigger Overwrite Params //
- Local Property Name Refs // 
- Local Property Enum Name Refs
- Direct Value Table
- Random Call Table
- Curve Call Table
- Curve Point Table
- ExRegion (unaligned) // 
  - ArrangeGroupParams
  - Users (idk if this is considered part of ExRegion or just comes after it but w/e)
    - Header
    - Local Properties
    - User Params
    - Sorted Asset Id Table
    - Asset Call Tables (aligned 4)
    - Containers
    - Action Slots (trigger table offset refers to this point)
    - Actions
    - Action Triggers
    - Properties
    - Property Triggers
    - Always Triggers
- Condition Table // 
- Name Table // 
*/
const TargetPointer negativeOne = static_cast<u32>(-1);

const xlink2::ResourceHeader Serializer::calcOffsets() {
    // they seem to not care about alignment of u64s to 8 bytes much
    size_t nameTableSize = 0;
    for (const auto& string : mSystem->mStrings) {
        mStringOffsets.emplace(string, nameTableSize);
        nameTableSize += string.size() + 1;
    }
    mPDTNameTableSize = 0;
    for (const auto& string : mSystem->mPDT.mStrings) {
        mPDTStringOffsets.emplace(string, mPDTNameTableSize);
        mPDTNameTableSize += string.size() + 1;
    }
    size_t conditionTableSize = 0;
    for (const auto& condition : mSystem->mConditions) {
        mConditionOffsets.emplace_back(conditionTableSize);
        switch (condition.parentContainerType) {
            case xlink2::ContainerType::Switch:
                // varies in size based on if it's an enum property or not
                conditionTableSize += sizeof(xlink2::ResSwitchCondition)
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                    - sizeof(u64) * (condition.getAs<xlink2::ContainerType::Switch>()->propType != xlink2::PropertyType::Enum)
#endif
                ;
                break;
            case xlink2::ContainerType::Random:
                conditionTableSize += sizeof(xlink2::ResRandomCondition);
                break;
            case xlink2::ContainerType::Random2:
                conditionTableSize += sizeof(xlink2::ResRandomCondition2);
                break;
            case xlink2::ContainerType::Blend:
                conditionTableSize += sizeof(xlink2::ResBlendCondition);
                break;
            case xlink2::ContainerType::Sequence:
                conditionTableSize += sizeof(xlink2::ResSequenceCondition);
                break;
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
            case xlink2::ContainerType::Grid:
                conditionTableSize += sizeof(xlink2::ResGridCondition);
                break;
#endif
#if XLINK_TARGET_IS_TOTK
            case xlink2::ContainerType::Jump:
                conditionTableSize += sizeof(xlink2::ResJumpCondition);
                break;
#endif
            default:
                throw InvalidDataError("Invalid condition type");
        }
    }
    size_t triggerOverwriteParamTableSize = 0;
    for (const auto& param : mSystem->mTriggerOverwriteParams) {
        mTriggerParamOffsets.emplace_back(triggerOverwriteParamTableSize);
        triggerOverwriteParamTableSize += sizeof(xlink2::ResTriggerOverwriteParam) + param.params.size() * sizeof(xlink2::ResParam);
    }
    size_t numParams = 0;
    size_t assetParamTableSize = 0;
    for (const auto& param : mSystem->mAssetParams) {
        mAssetParamOffsets.emplace_back(assetParamTableSize);
        assetParamTableSize += sizeof(xlink2::ResAssetParam) + param.params.size() * sizeof(xlink2::ResParam);
        numParams += param.params.size();
    }

    constexpr TargetPointer userHashesStart = sizeof(xlink2::ResourceHeader);
    const TargetPointer userPositionsStart = userHashesStart + (sizeof(u32) * mSystem->mUsers.size());
    const TargetPointer pdtStart = userPositionsStart + (sizeof(TargetPointer) * mSystem->mUsers.size());
    const TargetPointer pdtDefinesStart = util::align(pdtStart + sizeof(xlink2::ResParamDefineTableHeader), sizeof(TargetPointer));
    const size_t paramCount = mSystem->mPDT.getUserParamCount() + mSystem->mPDT.getAssetParamCount() + mSystem->mPDT.getTriggerParamCount() ;
    const TargetPointer pdtStringTableStart = pdtDefinesStart + (sizeof(xlink2::ResParamDefine) * paramCount);

    const TargetPointer triggerOverwriteTableOffset = pdtStringTableStart + util::align(mPDTNameTableSize, sizeof(TargetPointer)) + assetParamTableSize;
    const TargetPointer localPropertyNameRefTableOffset = triggerOverwriteTableOffset + triggerOverwriteParamTableSize;

    u32 curvePointCount = 0;
    for (const auto& curve : mSystem->mCurves) {
        curvePointCount += curve.points.size();
    }

    const TargetPointer localPropertyEnumNameRefOffset = localPropertyNameRefTableOffset + (sizeof(TargetPointer) * mSystem->mLocalProperties.size());
    const TargetPointer directValueOffset = localPropertyEnumNameRefOffset + (sizeof(TargetPointer) * mSystem->mLocalPropertyEnumStrings.size());
    const TargetPointer randomOffset  = directValueOffset + (sizeof(u32) * mSystem->mDirectValues.size());
    const TargetPointer curveOffset = randomOffset + (sizeof(xlink2::ResRandomCallTable) * mSystem->mRandomCalls.size());
    const TargetPointer curvePointOffset = curveOffset + (sizeof(xlink2::ResCurveCallTable) * mSystem->mCurves.size());
    const TargetPointer exRegionOffset = curvePointOffset + (sizeof(xlink2::ResCurvePoint) * curvePointCount);
    
    auto calcSize = [](const User& user) {
        UserInfo info{};
        constexpr TargetPointer userStart = 0;
        constexpr TargetPointer localPropertyRefOffset = userStart + sizeof(xlink2::ResUserHeader);
        const TargetPointer sortedAssetIdOffset = localPropertyRefOffset + (sizeof(TargetPointer) * user.mLocalProperties.size());
        const TargetPointer userParamOffset = sortedAssetIdOffset + (sizeof(u16) * user.mAssetCallTables.size());
        TargetPointer assetCtbOffset = userParamOffset + (sizeof(xlink2::ResParam)  * user.mUserParams.size());
        if(user.mAssetCallTables.size() % 2 != 0)
            assetCtbOffset += sizeof(u16);
        TargetPointer triggerTableOffset = assetCtbOffset + (sizeof(xlink2::ResAssetCallTable) * user.mAssetCallTables.size());

        s32 assetCount = 0;
        for (u16 i = 0; const auto& act : user.mAssetCallTables) {
            if (!act.isContainer())
                ++assetCount;
            info.assetIdMap.emplace(std::move(AssetKey{act.keyName, act.conditionIdx, i}), i);
            ++i;
        }
        s32 randomCount = 0;
        const TargetPointer baseContainerOffset = triggerTableOffset;
        for (const auto& container : user.mContainers) {
            info.containerOffsets.emplace_back(triggerTableOffset - baseContainerOffset);
            switch (container.type) {
                case xlink2::ContainerType::Switch:
                    triggerTableOffset += sizeof(xlink2::ResSwitchContainerParam);
                    break;
                case xlink2::ContainerType::Random:
                    triggerTableOffset += sizeof(xlink2::ResRandomContainerParam);
                    break;
                case xlink2::ContainerType::Random2: {
                    triggerTableOffset += sizeof(xlink2::ResRandomContainerParam);
                    ++randomCount;
                    break;
                }
                case xlink2::ContainerType::Sequence:
                    triggerTableOffset += sizeof(xlink2::ResSequenceContainerParam);
                    break;
                case xlink2::ContainerType::Blend:
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                    if (!container.isNotBlendAll) {
#endif
                        triggerTableOffset += sizeof(xlink2::ResBlendContainerParam);
                        
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                    } else {
                        triggerTableOffset += sizeof(xlink2::ResBlendContainerParam2);
                    }
#endif
                    break;
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                case xlink2::ContainerType::Grid: {
                    auto grid = container.getAs<xlink2::ContainerType::Grid>();
                    triggerTableOffset += sizeof(xlink2::ResGridContainerParam)
                                        + sizeof(u32) * (grid->values1.size() + grid->values2.size())
                                        + sizeof(s32) * grid->indices.size();
                    break;
                }
#endif
#if XLINK_TARGET_IS_TOTK
                case xlink2::ContainerType::Jump:
                    triggerTableOffset += sizeof(xlink2::ResJumpContainerParam);
                    break;
#endif
                default:
                    throw InvalidDataError("Invalid container type");
            }
        }
        info.totalSize = triggerTableOffset + sizeof(xlink2::ResActionSlot) * user.mActionSlots.size()
                        + sizeof(xlink2::ResAction) * user.mActions.size() + sizeof(xlink2::ResActionTrigger) * user.mActionTriggers.size()
                        + sizeof(xlink2::ResProperty) * user.mProperties.size() + sizeof(xlink2::ResPropertyTrigger) * user.mPropertyTriggers.size()
                        + sizeof(xlink2::ResAlwaysTrigger) * user.mAlwaysTriggers.size();
        info.triggerTableOffset = triggerTableOffset;
        info.assetCount = assetCount;
        info.randomContainerCount = randomCount;
        return info;
    };

    TargetPointer conditionTableOffset = exRegionOffset;

    for (const auto& params : mSystem->mArrangeGroupParams) {
        mArrangeGroupParamOffsets.emplace_back(conditionTableOffset - exRegionOffset);
        conditionTableOffset += sizeof(xlink2::ArrangeGroupParams) + (sizeof(xlink2::ArrangeGroupParam) * params.groups.size());
    }

    u32 userParamCount = 0;
    for (const auto& [hash, user] : mSystem->mUsers) {
        mUserOffsets.emplace(hash, conditionTableOffset);
        auto res = mUserInfo.emplace(hash, calcSize(user));
        conditionTableOffset += res.first->second.totalSize;
        userParamCount += user.mUserParams.size() + res.first->second.assetCount;
    }

    TargetPointer nameTableOffset = conditionTableOffset + conditionTableSize;

    const xlink2::ResourceHeader header {
        .magic = xlink2::cResourceMagic,
        .fileSize = static_cast<u32>(util::align(nameTableOffset + nameTableSize, sizeof(TargetPointer))),
        .version = mSystem->mVersion,
        .numParams = static_cast<s32>(numParams),
        .numAssetParams = static_cast<s32>(mSystem->mAssetParams.size()),
        .numTriggerOverwriteParams = static_cast<s32>(mSystem->mTriggerOverwriteParams.size()),
        .triggerOverwriteTablePos = triggerOverwriteTableOffset,
        .localPropertyNameRefTablePos = localPropertyNameRefTableOffset,
        .numLocalPropertyNameRefs = static_cast<s32>(mSystem->mLocalProperties.size()),
        .numLocalPropertyEnumNameRefs = static_cast<s32>(mSystem->mLocalPropertyEnumStrings.size()),
        .numDirectValues = static_cast<s32>(mSystem->mDirectValues.size()),
        .numRandom = static_cast<s32>(mSystem->mRandomCalls.size()),
        .numCurves = static_cast<s32>(mSystem->mCurves.size()),
        .numCurvePoints = static_cast<s32>(curvePointCount),
        .exRegionPos = exRegionOffset,
        .numUsers = static_cast<s32>(mSystem->mUsers.size()),
        .conditionTablePos = conditionTableOffset,
        .nameTablePos = nameTableOffset,
    };

    return header;
}

void Serializer::writeParamDefine(const ParamDefine& def) {
    u64 value;
    switch (def.getType()) {
        case xlink2::ParamType::Int:
            value = std::bit_cast<u64, s64>(static_cast<s64>(def.getValue<xlink2::ParamType::Int>()));
            break;
        case xlink2::ParamType::Float:
            value = std::bit_cast<u64, f64>(static_cast<f64>(def.getValue<xlink2::ParamType::Float>()));
            break;
        case xlink2::ParamType::Bool:
            value = def.getValue<xlink2::ParamType::Bool>() != 0;
            break;
        case xlink2::ParamType::Enum:
            value = def.getValue<xlink2::ParamType::Enum>();
            break;
        case xlink2::ParamType::String:
            value = mPDTStringOffsets.at(def.getValue<xlink2::ParamType::String>());
            break;
        case xlink2::ParamType::Bitfield:
            value = def.getValue<xlink2::ParamType::Bitfield>();
            break;
        default:
            throw InvalidDataError("Invalid param define type");
    }
    xlink2::ResParamDefine res = {};
    res.nameOffset = mPDTStringOffsets.at(def.getName());
    res.type = static_cast<u32>(def.getType());
    res.defaultValue = static_cast<TargetPointer>(value);

    write(res);
}

void Serializer::writePDT() {
    auto pdt = mSystem->mPDT;
    xlink2::ResParamDefineTableHeader header {};
    header.size = static_cast<s32>(util::align(sizeof(xlink2::ResParamDefineTableHeader) + sizeof(xlink2::ResParamDefine) * (pdt.getUserParamCount() + pdt.getAssetParamCount() + pdt.getTriggerParamCount()) + mPDTNameTableSize, sizeof(TargetPointer)));
    header.numUserParams = static_cast<s32>(pdt.getUserParamCount());
    header.numAssetParams = static_cast<s32>(pdt.getAssetParamCount());
    header.numUserAssetParams = static_cast<s32>(pdt.getAssetParamCount() - pdt.mSystemAssetParamCount);
    header.numTriggerParams = static_cast<s32>(pdt.getTriggerParamCount());

    write(header);

    for (const auto& param : mSystem->mPDT.mUserParams) {
        writeParamDefine(param);
    }

    for (const auto& param : mSystem->mPDT.mAssetParams) {
        writeParamDefine(param);
    }
    
    for (const auto& param : mSystem->mPDT.mTriggerParams) {
        writeParamDefine(param);
    }

    for (const auto& str : mSystem->mPDT.mStrings) {
        writeString(str);
    }
}

void Serializer::writeParam(const Param& param) {
    u32 val;
    if (param.type == xlink2::ValueReferenceType::ArrangeParam) {
        val = static_cast<u32>(mArrangeGroupParamOffsets.at(std::get<u32>(param.value)));
    } else if (param.type == xlink2::ValueReferenceType::String) {
        val = static_cast<u32>(mStringOffsets.at(std::get<std::string_view>(param.value)));
    } else {
        val = std::get<u32>(param.value);
    }
    write(static_cast<u32>(param.type) << 0x18 | (val & 0xffffff));
}

void Serializer::writeUser(const User& user, const u32 hash) {
    const auto& info = mUserInfo.at(hash);
    
    xlink2::ResUserHeader header = { };
    header.isSetup = 0;
#if XLINK_TARGET_IS_TOTK
    header.localPropertyCount = static_cast<s16>(user.mLocalProperties.size());
    header.unk = user.mUnknown;
#else
    header.localPropertyCount = static_cast<s32>(user.mLocalProperties.size());
#endif
    header.callCount = static_cast<s32>(user.mAssetCallTables.size());
    header.assetCount = info.assetCount;
    header.randomContainerCount = info.randomContainerCount;
    header.actionSlotCount = static_cast<s32>(user.mActionSlots.size());
    header.actionCount = static_cast<s32>(user.mActions.size());
    header.actionTriggerCount = static_cast<s32>(user.mActionTriggers.size());
    header.propertyCount = static_cast<s32>(user.mProperties.size());
    header.propertyTriggerCount = static_cast<s32>(user.mPropertyTriggers.size());
    header.alwaysTriggerCount = static_cast<s32>(user.mAlwaysTriggers.size());
    header.triggerTableOffset = info.triggerTableOffset;
    write(header);

    for (const auto& prop : user.mLocalProperties) {
        write(mStringOffsets.at(prop));
    }

    for (const auto& param : user.mUserParams) {
        writeParam(param);
    }

    // I cannot seem to figure out how entries with the same asset key are ordered...
    // sorting by index is kinda close but not quite
    for (const auto &idx: info.assetIdMap | std::views::values) {
        write(idx);
    }
    // if we're not doing any edits, we can get a byte perfect reserialization by doing this instead
    // however I think the above works fine in game
    // for (const auto& idx : user.mSortedAssetIds) {
    //     write(idx);
    // }

    align(0x4);
    
    s32 assetIndex = 0;
    for (const auto& act : user.mAssetCallTables) {
        xlink2::ResAssetCallTable res = {};
        res.keyNameOffset = mStringOffsets.at(act.keyName);
        res.assetIndex = static_cast<s16>(act.isContainer() ? negativeOne : assetIndex++);
        res.flag = act.flag;
        res.duration = act.duration;
        res.parentIndex = act.parentIndex;
        res.guid = act.guid;
        res.keyNameHash = util::calcCRC32(act.keyName);
        res.paramOffset = (act.isContainer() ? info.containerOffsets[static_cast<u32>(act.containerParamIdx)] : mAssetParamOffsets[static_cast<u32>(act.assetParamIdx)]);
        res.conditionOffset = (act.conditionIdx == -1 ? negativeOne : mConditionOffsets[static_cast<u32>(act.conditionIdx)]);
        write(res);
    }

    for (const auto& container : user.mContainers) {
        switch (container.type) {
            case xlink2::ContainerType::Switch: {
                const auto param = container.getAs<xlink2::ContainerType::Switch>();
                xlink2::ResSwitchContainerParam res = { };
                res.type = static_cast<xlink2::ResContainerParam::ContainerTypePrimitive>(container.type);
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                res.isNotBlendAll = container.isNotBlendAll;
                res.isNeedObserve = container.isNeedObserve;
                res.unk = 0;
                res.padding = {};
#endif
                res.childStartIdx = container.childContainerStartIdx;
                res.childEndIdx = container.childContainerStartIdx + container.childCount;

                res.actionSlotNameOffset = mStringOffsets.at(param->actionSlotName);
                res.propertyIndex = param->propertyIndex;
                res.isGlobal = param->isGlobal;

#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                res._00 = param->unk,
                res.isActionTrigger = param->isActionTrigger,
#else
                res.watchPropertyId = param->watchPropertyId;
#endif
                write(res);
                break;
            }
            case xlink2::ContainerType::Random: {
                // const auto param = container.getAs<xlink2::ContainerType::Random>();
                xlink2::ResRandomContainerParam res = {};
                res.type = static_cast<xlink2::ResContainerParam::ContainerTypePrimitive>(container.type);
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                .isNotBlendAll = container.isNotBlendAll,
                .isNeedObserve = container.isNeedObserve,
                .unk = 0,
                .padding = {},
#endif
                res.childStartIdx = container.childContainerStartIdx;
                res.childEndIdx = container.childContainerStartIdx + container.childCount;
                write(res);
                break;
            }
            case xlink2::ContainerType::Random2: {
                // const auto param = container.getAs<xlink2::ContainerType::Random2>();
                xlink2::ResRandomContainerParam2 res = {};
                res.type = static_cast<xlink2::ResContainerParam::ContainerTypePrimitive>(container.type);
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                res.isNotBlendAll = container.isNotBlendAll;
                res.isNeedObserve = container.isNeedObserve;
                res.unk = 0;
                res.padding = {};
#endif
                res.childStartIdx = container.childContainerStartIdx;
                res.childEndIdx = container.childContainerStartIdx + container.childCount;
                write(res);
                break;
            }
            case xlink2::ContainerType::Blend: {
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                if (!container.isNotBlendAll) {
#endif
                    // const auto param = container.getAs<xlink2::ContainerType::Blend>();
                    xlink2::ResBlendContainerParam res = {};
                    res.type = static_cast<xlink2::ResContainerParam::ContainerTypePrimitive>(container.type);
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                    res.isNotBlendAll = container.isNotBlendAll;
                    res.isNeedObserve = container.isNeedObserve;
                    res.unk = 0;
                    res.padding = {};
#endif
                    res.childStartIdx = container.childContainerStartIdx;
                    res.childEndIdx = container.childContainerStartIdx + container.childCount;
                    write(res);
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                } else {
                    const auto param = container.getAs<xlink2::ContainerType::Blend, true>();
                    const xlink2::ResBlendContainerParam2 res = {
                        {{
                            .type = static_cast<u8>(container.type),
                            .isNotBlendAll = container.isNotBlendAll,
                            .isNeedObserve = container.isNeedObserve,
                            .unk = 0,
                            .childStartIdx = container.childContainerStartIdx,
                            .childEndIdx = container.childContainerStartIdx + container.childCount,
                            .padding = {},
                        },
                        mStringOffsets.at(param->actionSlotName),
                        param->unk,
                        param->propertyIndex,
                        param->isGlobal,
                        param->isActionTrigger,}
                    };
                    write(res);
                }
#endif
                break;
            }
            case xlink2::ContainerType::Sequence: {
                // const auto param = container.getAs<xlink2::ContainerType::Sequence>();
                xlink2::ResSequenceContainerParam res = {};
                res.type = static_cast<xlink2::ResContainerParam::ContainerTypePrimitive>(container.type);
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                res.isNotBlendAll = container.isNotBlendAll;
                res.isNeedObserve = container.isNeedObserve;
                res.unk = 0;
                res.padding = {};
#endif
                res.childStartIdx = container.childContainerStartIdx;
                res.childEndIdx = container.childContainerStartIdx + container.childCount;
                write(res);
                break;
            }
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
            case xlink2::ContainerType::Grid: {
                const auto param = container.getAs<xlink2::ContainerType::Grid>();
                const xlink2::ResGridContainerParam res = {
                    {
                        .type = static_cast<u8>(container.type),
                        .isNotBlendAll = container.isNotBlendAll,
                        .isNeedObserve = container.isNeedObserve,
                        .unk = 0,
                        .childStartIdx = container.childContainerStartIdx,
                        .childEndIdx = container.childContainerStartIdx + container.childCount,
                        .padding = {},
                    },
                    mStringOffsets.at(param->propertyName1),
                    mStringOffsets.at(param->propertyName2),
                    param->propertyIndex1,
                    param->propertyIndex2,
                    static_cast<u16>(param->isGlobal1 | (param->isGlobal2 << 1)),
                    static_cast<u8>(param->values1.size()),
                    static_cast<u8>(param->values2.size()),
                };
                write(res);
                for (const auto& val : param->values1) {
                    write(val);
                }
                for (const auto& val : param->values2) {
                    write(val);
                }
                for (const auto& idx : param->indices) {
                    write(idx);
                }
                break;
            }
#endif
#if XLINK_TARGET_IS_TOTK
            case xlink2::ContainerType::Jump: {
                // const auto param = container.getAs<xlink2::ContainerType::Jump>();
                const xlink2::ResJumpContainerParam res = {
                    {
                        .type = static_cast<u8>(container.type),
                        .isNotBlendAll = container.isNotBlendAll,
                        .isNeedObserve = container.isNeedObserve,
                        .unk = 0,
                        .childStartIdx = container.childContainerStartIdx,
                        .childEndIdx = container.childContainerStartIdx + container.childCount,
                        .padding = {},
                    },
                };
                write(res);
                break;
            }
#endif
            default:
                throw InvalidDataError(std::format("Invalid container type {:d}", static_cast<u32>(container.type)));
        }
    }

    for (const auto& slot : user.mActionSlots) {
        xlink2::ResActionSlot res = {};
        res.nameOffset = mStringOffsets.at(slot.actionSlotName),
        res.actionStartIdx = slot.actionStartIdx,
        res.actionEndIdx = static_cast<s16>(slot.actionStartIdx + slot.actionCount),
        write(res);
    }
    for (const auto& action : user.mActions) {
        xlink2::ResAction res = {};
        res.nameOffset = mStringOffsets.at(action.actionName);
        res.triggerStartIdx = action.actionTriggerStartIdx;
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
        .enableMatchStart = action.enableMatchStart,
        .padding = {},
#endif
        res .triggerEndIdx = static_cast<u32>(action.actionTriggerStartIdx + action.actionTriggerCount);
        write(res);
    }
    for (const auto& trigger : user.mActionTriggers) {
        xlink2::ResActionTrigger res = {};
        res.guid = trigger.guid;
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
        res.unk = trigger.unk;
#endif
        res.assetCallTableOffset = trigger.assetCallIdx * sizeof(xlink2::ResAssetCallTable);
        res.previousActionNameOffset = trigger.nameMatch ? mStringOffsets.at(trigger.previousActionName) : std::bit_cast<u32, s32>(trigger.startFrame);
        res.endFrame = trigger.endFrame;
        res.flag = static_cast<u16>(static_cast<u16>(trigger.triggerOnce) | (trigger.fade << 2) | (trigger.alwaysTrigger << 3) | (trigger.nameMatch << 4));
        res.overwriteHash = trigger.overwriteHash;
        res.overwriteParamOffset = (trigger.triggerOverwriteIdx == -1 ? negativeOne : mTriggerParamOffsets[static_cast<u32>(trigger.triggerOverwriteIdx)]);
        write(res);
    }

    for (const auto& prop : user.mProperties) {
        xlink2::ResProperty res = {};
        res.nameOffset = mStringOffsets.at(prop.propertyName);
        res.isGlobal = prop.isGlobal;
        res.triggerStartIdx = prop.propTriggerStartIdx;
        res.triggerEndIdx = prop.propTriggerStartIdx + prop.propTriggerCount;
        write(res);
    }
    for (const auto& trigger : user.mPropertyTriggers) {
        xlink2::ResPropertyTrigger res = {};
        res.guid = trigger.guid;
        res.flag = trigger.flag;
        res.overwriteHash = trigger.overwriteHash;
        res.assetCallTableOffset = trigger.assetCallTableIdx * sizeof(xlink2::ResAssetCallTable);
        res.conditionOffset = (trigger.conditionIdx == -1 ? negativeOne : mConditionOffsets[static_cast<u32>(trigger.conditionIdx)]);
        res.overwriteParamOffset = (trigger.triggerOverwriteIdx == -1 ? negativeOne : mTriggerParamOffsets[static_cast<u32>(trigger.triggerOverwriteIdx)]);
        write(res);
    }

    for (const auto& trigger : user.mAlwaysTriggers) {
        xlink2::ResAlwaysTrigger res = {};
        res.guid = trigger.guid;
        res.flag = trigger.flag;
        res.overwriteHash = trigger.overwriteHash;
        res.assetCallTableOffset = trigger.assetCallIdx * sizeof(xlink2::ResAssetCallTable);
        res.overwriteParamOffset = (trigger.triggerOverwriteIdx == -1 ? negativeOne : mTriggerParamOffsets[static_cast<u32>(trigger.triggerOverwriteIdx)]);
        write(res);
    }
}

void Serializer::serialize() {
    if (mSystem == nullptr)
        return;
    
    auto header = calcOffsets();
    expand(header.fileSize);
    write(header);

    for (const auto &hash: mSystem->mUsers | std::views::keys) {
        write(hash);
    }

    align(sizeof(TargetPointer));

    for (const auto &offset: mUserOffsets | std::views::values) {
        write(offset);
    }

    align(sizeof(TargetPointer));

    writePDT();

    align(sizeof(TargetPointer));

    for (auto& assetParam : mSystem->mAssetParams) {
        size_t pos = tell();
        write<u64>(0);
        u64 values = 0;
        std::ranges::sort(assetParam.params, [](const Param& lhs, const Param& rhs) { return lhs.index < rhs.index; });
        for (const auto& param : assetParam.params) {
            values |= 1ull << param.index;
            writeParam(param);
        }
        writeAt(values, pos);
    }

    for (auto& triggerParam : mSystem->mTriggerOverwriteParams) {
        size_t pos = tell();
        write<u32>(0);
        u32 values = 0;
        std::ranges::sort(triggerParam.params, [](const Param& lhs, const Param& rhs) { return lhs.index < rhs.index; });
        for (const auto& param : triggerParam.params) {
            values |= 1u << param.index;
            writeParam(param);
        }
        writeAt(values, pos);
    }

    for (const auto& prop : mSystem->mLocalProperties) {
        write<TargetPointer>(mStringOffsets.at(prop));
    }

    for (const auto& value : mSystem->mLocalPropertyEnumStrings) {
        write<TargetPointer>(mStringOffsets.at(value));
    }

    for (const auto& value : mSystem->mDirectValues) {
        write(value.value.u);
    }

    for (const auto& random : mSystem->mRandomCalls) {
        const xlink2::ResRandomCallTable res = {
            .minVal = random.min,
            .maxVal = random.max,
        };
        write(res);
    }

    std::vector<std::reference_wrapper<const CurvePoint>> curvePoints;
    u32 points = 0;
    for (const auto& curve : mSystem->mCurves) {
        for (const auto& point : curve.points) {
            curvePoints.emplace_back(point);
        }
        const xlink2::ResCurveCallTable res = {
            .curvePointBaseIdx = static_cast<u16>(points),
            .numCurvePoint = static_cast<u16>(curve.points.size()),
            .curveType = curve.type,
            .isGlobal = static_cast<u16>(curve.isGlobal ? 1 : 0),
            .propNameOffset = mStringOffsets.at(curve.propertyName),
            .unk = curve.unk,
            .propertyIndex = curve.propertyIndex,
            .unk2 = curve.unk2,
        };
        write(res);
        points += res.numCurvePoint;
    }

    for (const auto& point : curvePoints) {
        const xlink2::ResCurvePoint res = {
            .x = point.get().x,
            .y = point.get().y,
        };
        write(res);
    }

    for (const auto& arrangeGroup : mSystem->mArrangeGroupParams) {
        write<u32>(arrangeGroup.groups.size());
        for (const auto& param : arrangeGroup.groups) {
            const xlink2::ArrangeGroupParam res = {
                .groupNameOffset = mStringOffsets.at(param.groupName),
                .limitType = param.limitType,
                .limitThreshold = param.limitThreshold,
                .unk = param.unk
            };
            write(res);
        }
    }

    for (const auto& [hash, user] : mSystem->mUsers) {
        writeUser(user, hash);
    }

    for (const auto& condition : mSystem->mConditions) {
        switch (condition.parentContainerType) {
            case xlink2::ContainerType::Switch: {
                const auto param = condition.getAs<xlink2::ContainerType::Switch>();
                xlink2::ResSwitchCondition res = {};
                res.type = static_cast<u32>(condition.parentContainerType);
                res.propertyType = static_cast<xlink2::PropertyTypePrimitive>(param->propType);
                res.compareType = static_cast<xlink2::CompareTypePrimitive>(param->compareType);
                res.solved = false;
                res.isGlobal = param->isGlobal;
                /* TODO: something aint right here */
                res.actionHash = param->actionHash;
                res.value.i = param->conditionValue.i;
                if (param->propType == xlink2::PropertyType::Enum) {
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                    res.enumNameOffset = mStringOffsets.at(param->enumName);
#else
                    res.value.u = mStringOffsets.at(param->enumName);
                    write(res);
#endif
                } else {
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
                    // omit enumNameOffset
                    write({reinterpret_cast<const u8*>(&res), sizeof(xlink2::ResSwitchCondition) - sizeof(TargetPointer)});
#else
                    write(res);
#endif
                }
                break;
            }
            case xlink2::ContainerType::Random: {
                const auto param = condition.getAs<xlink2::ContainerType::Random>();
                xlink2::ResRandomCondition res = {};
                res.type = static_cast<u32>(condition.parentContainerType);
                res.weight = param->weight;
                write(res);
                break;
            }
            case xlink2::ContainerType::Random2: {
                const auto param = condition.getAs<xlink2::ContainerType::Random2>();
                xlink2::ResRandomCondition2 res = {};
                res.type = static_cast<u32>(condition.parentContainerType);
                res.weight = param->weight;
                write(res);
                break;
            }
            case xlink2::ContainerType::Blend: {
                const auto param = condition.getAs<xlink2::ContainerType::Blend>();
                xlink2::ResBlendCondition res = {};
                res.type = static_cast<u32>(condition.parentContainerType);
                res.min = param->min;
                res.max = param->max;
                res.blendTypeToMax = static_cast<u8>(param->blendTypeToMax);
                res.blendTypeToMin = static_cast<u8>(param->blendTypeToMin);
                write(res);
                break;
            }
            case xlink2::ContainerType::Sequence: {
                const auto param = condition.getAs<xlink2::ContainerType::Sequence>();
                xlink2::ResSequenceCondition res = {};
                res.type = static_cast<u32>(condition.parentContainerType);
                res.isContinueOnFade = param->continueOnFade;
                write(res);
                break;
            }
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
            case xlink2::ContainerType::Grid: {
                // const auto param = condition.getAs<xlink2::ContainerType::Grid>();
                const xlink2::ResGridCondition res = {};
                res.type = static_cast<u32>(condition.parentContainerType);
                write(res);
                break;
            }
#endif
#if XLINK_TARGET_IS_TOTK
            case xlink2::ContainerType::Jump: {
                // const auto param = condition.getAs<xlink2::ContainerType::Jump>();
                const xlink2::ResJumpCondition res = {};
                res.type = static_cast<u32>(condition.parentContainerType);
                write(res);
                break;
            }
#endif
            default:
                throw InvalidDataError("Invalid condition type");
        }
    }

    for (const auto& str : mSystem->mStrings) {
        writeString(str);
    }
}

} // namespace banana