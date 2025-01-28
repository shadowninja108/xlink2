#include "serializer.h"
#include "util/error.h"
#include "util/crc32.h"

#include <algorithm>
#include <iostream>
#include <format>

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

const xlink2::ResourceHeader Serializer::calcOffsets() {
    // they seem to not care about alignment of u64s to 8 bytes much
    u64 nameTableSize = 0;
    for (const auto& string : mSystem->mStrings) {
        mStringOffsets.emplace(string, nameTableSize);
        nameTableSize += string.size() + 1;
    }
    mPDTNameTableSize = 0;
    for (const auto& string : mSystem->mPDT.mStrings) {
        mPDTStringOffsets.emplace(string, mPDTNameTableSize);
        mPDTNameTableSize += string.size() + 1;
    }
    u64 conditionTableSize = 0;
    for (const auto& condition : mSystem->mConditions) {
        mConditionOffsets.emplace_back(conditionTableSize);
        switch (condition.parentContainerType) {
            case xlink2::ContainerType::Switch:
                // varies in size based on if it's an enum property or not
                conditionTableSize += sizeof(xlink2::ResSwitchCondition) - sizeof(u64) * (condition.getAs<xlink2::ContainerType::Switch>()->propType != xlink2::PropertyType::Enum);
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
            case xlink2::ContainerType::Grid:
                conditionTableSize += sizeof(xlink2::ResGridCondition);
                break;
            case xlink2::ContainerType::Jump:
                conditionTableSize += sizeof(xlink2::ResJumpCondition);
                break;
            default:
                throw InvalidDataError("Invalid condition type");
        }
    }
    u64 triggerOverwriteParamTableSize = 0;
    for (const auto& param : mSystem->mTriggerOverwriteParams) {
        mTriggerParamOffsets.emplace_back(triggerOverwriteParamTableSize);
        triggerOverwriteParamTableSize += sizeof(xlink2::ResTriggerOverwriteParam) + param.params.size() * sizeof(xlink2::ResParam);
    }
    s32 numParams = 0;
    u64 assetParamTableSize = 0;
    for (const auto& param : mSystem->mAssetParams) {
        mAssetParamOffsets.emplace_back(assetParamTableSize);
        assetParamTableSize += sizeof(xlink2::ResAssetParam) + param.params.size() * sizeof(xlink2::ResParam);
        numParams += param.params.size();
    }

    u64 triggerOverwriteTableOffset =
            sizeof(xlink2::ResourceHeader)
            + util::align(sizeof(u32) * mSystem->mUsers.size() + sizeof(u64) * mSystem->mUsers.size(), 8)
            + sizeof(xlink2::ResParamDefineTableHeader)
            + (mSystem->mPDT.getUserParamCount() + mSystem->mPDT.getAssetParamCount() + mSystem->mPDT.getTriggerParamCount()) * sizeof(xlink2::ResParamDefine)
            + util::align(mPDTNameTableSize, 8) + assetParamTableSize;
    u64 localPropertyNameRefTableOffset = triggerOverwriteTableOffset + triggerOverwriteParamTableSize;

    u32 curvePointCount = 0;
    for (const auto& curve : mSystem->mCurves) {
        curvePointCount += curve.points.size();
    }

    u64 exRegionOffset = 
        localPropertyNameRefTableOffset + sizeof(u64) * (mSystem->mLocalProperties.size() + mSystem->mLocalPropertyEnumStrings.size())
        + sizeof(u32) * mSystem->mDirectValues.size() + sizeof(xlink2::ResRandomCallTable) * mSystem->mRandomCalls.size()
        + sizeof(xlink2::ResCurveCallTable) * mSystem->mCurves.size() + sizeof(xlink2::ResCurvePoint) * curvePointCount;
    
    auto calcSize = [](const User& user) {
        UserInfo info{};
        u64 triggerTableOffset = sizeof(xlink2::ResUserHeader) + sizeof(u64) * user.mLocalProperties.size()
            + sizeof(xlink2::ResParam) * user.mUserParams.size() + sizeof(u16) * (user.mAssetCallTables.size() + user.mAssetCallTables.size() % 2)
            + sizeof(xlink2::ResAssetCallTable) * user.mAssetCallTables.size();
        s32 assetCount = 0;
        for (u16 i = 0; const auto& act : user.mAssetCallTables) {
            if (!act.isContainer())
                ++assetCount;
            info.assetIdMap.emplace(std::move(AssetKey{act.keyName, i}), i);
            ++i;
        }
        s32 randomCount = 0;
        const u64 baseContainerOffset = triggerTableOffset;
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
                case xlink2::ContainerType::Blend:
                    if (!container.isNotBlendAll) {
                        triggerTableOffset += sizeof(xlink2::ResBlendContainerParam);
                    } else {
                        triggerTableOffset += sizeof(xlink2::ResBlendContainerParam2);
                    }
                    break;
                case xlink2::ContainerType::Sequence:
                    triggerTableOffset += sizeof(xlink2::ResSequenceContainerParam);
                    break;
                case xlink2::ContainerType::Grid: {
                    auto grid = container.getAs<xlink2::ContainerType::Grid>();
                    triggerTableOffset += sizeof(xlink2::ResGridContainerParam)
                                        + sizeof(u32) * (grid->values1.size() + grid->values2.size())
                                        + sizeof(s32) * grid->indices.size();
                    break;
                }
                case xlink2::ContainerType::Jump:
                    triggerTableOffset += sizeof(xlink2::ResJumpContainerParam);
                    break;
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

    u64 conditionTableOffset = exRegionOffset;

    for (const auto& params : mSystem->mArrangeGroupParams) {
        mArrangeGroupParamOffsets.emplace_back(conditionTableOffset - exRegionOffset);
        conditionTableOffset += sizeof(u32) + sizeof(xlink2::ArrangeGroupParam) * params.groups.size();
    }

    u32 userParamCount = 0;
    for (const auto& [hash, user] : mSystem->mUsers) {
        mUserOffsets.emplace(hash, conditionTableOffset);
        auto res = mUserInfo.emplace(hash, calcSize(user));
        conditionTableOffset += (*res.first).second.totalSize;
        userParamCount += user.mUserParams.size() + (*res.first).second.assetCount;
    }

    u64 nameTableOffset = conditionTableOffset + conditionTableSize;

    const xlink2::ResourceHeader header {
        .magic = xlink2::cResourceMagic,
        .fileSize = static_cast<u32>(util::align(nameTableOffset + nameTableSize, 0x8)),
        .version = mSystem->mVersion,
        .numParams = numParams,
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
        .padding = {},
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
            value = def.getValue<xlink2::ParamType::Bool>() ? 1 : 0;
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

    const xlink2::ResParamDefine res = {
        .nameOffset = mPDTStringOffsets.at(def.getName()),
        .type = static_cast<u32>(def.getType()),
        .padding = {},
        .defaultValue = value,
    };

    write(res);
}

void Serializer::writePDT() {
    auto pdt = mSystem->mPDT;
    const xlink2::ResParamDefineTableHeader header {
        .size = static_cast<s32>(util::align(sizeof(xlink2::ResParamDefineTableHeader) + sizeof(xlink2::ResParamDefine) * (pdt.getUserParamCount() + pdt.getAssetParamCount() + pdt.getTriggerParamCount()) + mPDTNameTableSize, 0x8)),
        .numUserParams = static_cast<s32>(pdt.getUserParamCount()),
        .numAssetParams = static_cast<s32>(pdt.getAssetParamCount()),
        .numUserAssetParams = static_cast<s32>(pdt.getAssetParamCount() - pdt.mSystemAssetParamCount),
        .numTriggerParams = static_cast<s32>(pdt.getTriggerParamCount()),
        .padding = {},
    };

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
    const auto info = mUserInfo.at(hash);
    
    const xlink2::ResUserHeader header = {
        .isSetup = 0,
        .localPropertyCount = static_cast<s16>(user.mLocalProperties.size()),
        .unk = user.mUnknown,
        .callCount = static_cast<s32>(user.mAssetCallTables.size()),
        .assetCount = info.assetCount,
        .randomContainerCount = info.randomContainerCount,
        .actionSlotCount = static_cast<s32>(user.mActionSlots.size()),
        .actionCount = static_cast<s32>(user.mActions.size()),
        .actionTriggerCount = static_cast<s32>(user.mActionTriggers.size()),
        .propertyCount = static_cast<s32>(user.mProperties.size()),
        .propertyTriggerCount = static_cast<s32>(user.mPropertyTriggers.size()),
        .alwaysTriggerCount = static_cast<s32>(user.mAlwaysTriggers.size()),
        .padding = {},
        .triggerTableOffset = info.triggerTableOffset,
    };
    write(header);

    for (const auto& prop : user.mLocalProperties) {
        write(mStringOffsets.at(prop));
    }

    for (const auto& param : user.mUserParams) {
        writeParam(param);
    }

    // I cannot seem to figure out how entries with the same asset key are ordered...
    // sorting by index is kinda close but not quite
    for (const auto& [act, idx] : info.assetIdMap) {
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
        const xlink2::ResAssetCallTable res = {
            .keyNameOffset = mStringOffsets.at(act.keyName),
            .assetIndex = static_cast<s16>(act.isContainer() ? -1 : assetIndex++),
            .flag = act.flag,
            .duration = act.duration,
            .parentIndex = act.parentIndex,
            .guid = act.guid,
            .keyNameHash = util::calcCRC32(act.keyName),
            .padding = {},
            .paramOffset = (act.isContainer() ? info.containerOffsets[act.containerParamIdx] : mAssetParamOffsets[act.assetParamIdx]),
            .conditionOffset = (act.conditionIdx == -1 ? 0xffffffff : mConditionOffsets[act.conditionIdx]),
        };
        write(res);
    }

    for (const auto& container : user.mContainers) {
        switch (container.type) {
            case xlink2::ContainerType::Switch: {
                const auto param = container.getAs<xlink2::ContainerType::Switch>();
                const xlink2::ResSwitchContainerParam res = {
                    {
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
                    param->isActionTrigger,
                };
                write(res);
                break;
            }
            case xlink2::ContainerType::Random: {
                // const auto param = container.getAs<xlink2::ContainerType::Random>();
                const xlink2::ResRandomContainerParam res = {
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
            case xlink2::ContainerType::Random2: {
                // const auto param = container.getAs<xlink2::ContainerType::Random2>();
                const xlink2::ResRandomContainerParam2 res = {
                    {{
                        .type = static_cast<u8>(container.type),
                        .isNotBlendAll = container.isNotBlendAll,
                        .isNeedObserve = container.isNeedObserve,
                        .unk = 0,
                        .childStartIdx = container.childContainerStartIdx,
                        .childEndIdx = container.childContainerStartIdx + container.childCount,
                        .padding = {},
                    }},
                };
                write(res);
                break;
            }
            case xlink2::ContainerType::Blend: {
                if (!container.isNotBlendAll) {
                    // const auto param = container.getAs<xlink2::ContainerType::Blend>();
                    const xlink2::ResBlendContainerParam res = {
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
                break;
            }
            case xlink2::ContainerType::Sequence: {
                // const auto param = container.getAs<xlink2::ContainerType::Sequence>();
                const xlink2::ResSequenceContainerParam res = {
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
            default:
                throw InvalidDataError(std::format("Invalid container type {:d}", static_cast<u32>(container.type)));
        }
    }

    for (const auto& slot : user.mActionSlots) {
        const xlink2::ResActionSlot res = {
            .nameOffset = mStringOffsets.at(slot.actionSlotName),
            .actionStartIdx = slot.actionStartIdx,
            .actionEndIdx = static_cast<s16>(slot.actionStartIdx + slot.actionCount),
            ._padding = {},
        };
        write(res);
    }
    for (const auto& action : user.mActions) {
        const xlink2::ResAction res = {
            .nameOffset = mStringOffsets.at(action.actionName),
            .triggerStartIdx = action.actionTriggerStartIdx,
            .enableMatchStart = action.enableMatchStart,
            .padding = {},
            .triggerEndIdx = static_cast<u32>(action.actionTriggerStartIdx + action.actionTriggerCount),
        };
        write(res);
    }
    for (const auto& trigger : user.mActionTriggers) {
        const xlink2::ResActionTrigger res = {
            .guid = trigger.guid,
            .unk = trigger.unk,
            .assetCallTableOffset = trigger.assetCallIdx * sizeof(xlink2::ResAssetCallTable),
            .previousActionNameOffset = trigger.nameMatch ? mStringOffsets.at(trigger.previousActionName) : std::bit_cast<u32, s32>(trigger.startFrame),
            .endFrame = trigger.endFrame,
            .flag = static_cast<u16>(trigger.triggerOnce | (trigger.fade << 2) | (trigger.alwaysTrigger << 3) | (trigger.nameMatch << 4)),
            .overwriteHash = trigger.overwriteHash,
            .overwriteParamOffset = (trigger.triggerOverwriteIdx == -1 ? 0xffffffff : mTriggerParamOffsets[trigger.triggerOverwriteIdx]),
        };
        write(res);
    }

    for (const auto& prop : user.mProperties) {
        const xlink2::ResProperty res = {
            .nameOffset = mStringOffsets.at(prop.propertyName),
            .isGlobal = prop.isGlobal,
            .triggerStartIdx = prop.propTriggerStartIdx,
            .triggerEndIdx = prop.propTriggerStartIdx + prop.propTriggerCount,
            .padding = {},
        };
        write(res);
    }
    for (const auto& trigger : user.mPropertyTriggers) {
        const xlink2::ResPropertyTrigger res = {
            .guid = trigger.guid,
            .flag = trigger.flag,
            .overwriteHash = trigger.overwriteHash,
            .assetCallTableOffset = trigger.assetCallTableIdx * sizeof(xlink2::ResAssetCallTable),
            .conditionOffset = (trigger.conditionIdx == -1 ? 0xffffffff : mConditionOffsets[trigger.conditionIdx]),
            .overwriteParamOffset = (trigger.triggerOverwriteIdx == -1 ? 0xffffffff : mTriggerParamOffsets[trigger.triggerOverwriteIdx]),
        };
        write(res);
    }

    for (const auto& trigger : user.mAlwaysTriggers) {
        const xlink2::ResAlwaysTrigger res = {
            .guid = trigger.guid,
            .flag = trigger.flag,
            .overwriteHash = trigger.overwriteHash,
            .assetCallTableOffset = trigger.assetCallIdx * sizeof(xlink2::ResAssetCallTable),
            .overwriteParamOffset = (trigger.triggerOverwriteIdx == -1 ? 0xffffffff : mTriggerParamOffsets[trigger.triggerOverwriteIdx]),
        };
        write(res);
    }
}

void Serializer::serialize() {
    if (mSystem == nullptr)
        return;
    
    auto header = calcOffsets();
    expand(header.fileSize);
    write(header);

    for (const auto& [hash, user] : mSystem->mUsers) {
        write(hash);
    }

    for (const auto& [hash, offset] : mUserOffsets) {
        write(offset);
    }

    align(0x8);

    writePDT();

    align(0x8);

    for (auto& assetParam : mSystem->mAssetParams) {
        size_t pos = tell();
        write<u64>(0);
        u64 values = 0;
        std::sort(assetParam.params.begin(), assetParam.params.end(), [](const Param& lhs, const Param& rhs) { return lhs.index < rhs.index; });
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
        std::sort(triggerParam.params.begin(), triggerParam.params.end(), [](const Param& lhs, const Param& rhs) { return lhs.index < rhs.index; });
        for (const auto& param : triggerParam.params) {
            values |= 1u << param.index;
            writeParam(param);
        }
        writeAt(values, pos);
    }

    for (const auto& prop : mSystem->mLocalProperties) {
        write<u64>(mStringOffsets.at(prop));
    }

    for (const auto& value : mSystem->mLocalPropertyEnumStrings) {
        write<u64>(mStringOffsets.at(value));
    }

    for (const auto& value : mSystem->mDirectValues) {
        write(value);
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
                .unk = param.unk,
                .padding = {},
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
                const xlink2::ResSwitchCondition res = {
                    {
                        .type = static_cast<u32>(condition.parentContainerType),
                    },
                    static_cast<u8>(param->propType),
                    static_cast<u8>(param->compareType),
                    false,
                    param->isGlobal,
                    param->actionHash,
                    param->conditionValue.i,
                    (param->propType == xlink2::PropertyType::Enum ? mStringOffsets.at(param->enumName) : 0),
                };
                if (param->propType == xlink2::PropertyType::Enum) {
                    write(res);
                } else {
                    write({reinterpret_cast<const u8*>(&res), sizeof(xlink2::ResSwitchCondition) - sizeof(u64)});
                }
                break;
            }
            case xlink2::ContainerType::Random: {
                const auto param = condition.getAs<xlink2::ContainerType::Random>();
                const xlink2::ResRandomCondition res = {
                    {
                        .type = static_cast<u32>(condition.parentContainerType),
                    },
                    param->weight,
                };
                write(res);
                break;
            }
            case xlink2::ContainerType::Random2: {
                const auto param = condition.getAs<xlink2::ContainerType::Random2>();
                const xlink2::ResRandomCondition2 res = {
                    {{
                        .type = static_cast<u32>(condition.parentContainerType),
                    },
                    param->weight,}
                };
                write(res);
                break;
            }
            case xlink2::ContainerType::Blend: {
                const auto param = condition.getAs<xlink2::ContainerType::Blend>();
                const xlink2::ResBlendCondition res = {
                    {
                        .type = static_cast<u32>(condition.parentContainerType),
                    },
                    param->min,
                    param->max,
                    static_cast<u8>(param->blendTypeToMax),
                    static_cast<u8>(param->blendTypeToMin),
                    {},
                };
                write(res);
                break;
            }
            case xlink2::ContainerType::Sequence: {
                const auto param = condition.getAs<xlink2::ContainerType::Sequence>();
                const xlink2::ResSequenceCondition res = {
                    {
                        .type = static_cast<u32>(condition.parentContainerType),
                    },
                    param->continueOnFade,
                };
                write(res);
                break;
            }
            case xlink2::ContainerType::Grid: {
                // const auto param = condition.getAs<xlink2::ContainerType::Grid>();
                const xlink2::ResGridCondition res = {
                    {
                        .type = static_cast<u32>(condition.parentContainerType),
                    },
                };
                write(res);
                break;
            }
            case xlink2::ContainerType::Jump: {
                // const auto param = condition.getAs<xlink2::ContainerType::Jump>();
                const xlink2::ResJumpCondition res = {
                    {
                        .type = static_cast<u32>(condition.parentContainerType),
                    },
                };
                write(res);
                break;
            }
            default:
                throw InvalidDataError("Invalid condition type");
        }
    }

    for (const auto& str : mSystem->mStrings) {
        writeString(str);
    }
}

} // namespace banana