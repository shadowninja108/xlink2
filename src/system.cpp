#include "system.h"
#include "serializer.h"
#include "util/error.h"
#include "util/crc32.h"

#include "usernames.inc"

#include <bit>
#include <iostream>
#include <format>
#include <variant>

namespace banana {

bool System::initialize(void* data, size_t size) {
    xlink2::ResourceHeader* header = reinterpret_cast<xlink2::ResourceHeader*>(data);
    if (header == nullptr || size != header->fileSize) {
        throw ResourceError("Invalid input resource");
    }

    ResourceAccessor accessor;
    if (!accessor.load(data)) {
        throw ResourceError("Failed to load input resource");
    }

    if (!mPDT.initialize(accessor)) {
        throw ResourceError("Failed to initialize ParamDefineTable");
    }

    mVersion = accessor.getResourceHeader()->version;

    // each define will just store a string_view of the string while the PDT will store a set of all strings
    const char* nameTable = accessor.getString(0);
    const char* pos = nameTable;
    const char* end = reinterpret_cast<const char*>(reinterpret_cast<uintptr_t>(header) + header->fileSize);

    InitInfo info{};

    do {
        auto res = mStrings.insert(std::string(pos));
#ifdef _MSC_VER
        info.strings.emplace(pos - nameTable, std::string_view(*res.first)); // msvc appears to treat pos - nameTable as an s64?
#else
        info.strings.emplace(reinterpret_cast<ptrdiff_t>(pos - nameTable), std::string_view(*res.first));
#endif
        pos += (*res.first).size() + 1;
    } while (pos < end && *pos);

    mCurves.resize(header->numCurves);
    mRandomCalls.resize(header->numRandom);
    mDirectValues.resize(header->numDirectValues);
    mTriggerOverwriteParams.resize(header->numTriggerOverwriteParams);
    mLocalProperties.resize(header->numLocalPropertyNameRefs);
    mLocalPropertyEnumStrings.resize(header->numLocalPropertyEnumNameRefs);

    for (u32 i = 0; i < mLocalProperties.size(); ++i) {
        mLocalProperties[i] = info.strings.at(accessor.getLocalPropertyOffset(i));
    }

    for (u32 i = 0; i < mLocalPropertyEnumStrings.size(); ++i) {
        mLocalPropertyEnumStrings[i] = info.strings.at(accessor.getLocalPropertyEnumOffset(i));
    }

    for (u32 i = 0; i < mCurves.size(); ++i) {
        auto curve = accessor.getCurve(i);
        mCurves[i].propertyName = info.strings.at(curve->propNameOffset);
        mCurves[i].propertyIndex = curve->propertyIndex;
        mCurves[i].type = curve->curveType;
        mCurves[i].unk = curve->unk;
        mCurves[i].isGlobal = curve->isGlobal;
        mCurves[i].unk2 = curve->unk2;
        mCurves[i].points.resize(curve->numCurvePoint);
        for (u32 j = 0; j < mCurves[i].points.size(); ++j) {
            auto point = accessor.getCurvePoint(curve->curvePointBaseIdx + j);
            mCurves[i].points[j] = { point->x, point->y };
        }
    }

    for (u32 i = 0; i < mRandomCalls.size(); ++i) {
        auto random = accessor.getRandomCall(i);
        mRandomCalls[i] = { random->minVal, random->maxVal };
    }

    for (u32 i = 0; i < mDirectValues.size(); ++i) {
        // we can cast these when we need them
        mDirectValues[i].value.u = accessor.getDirectValueU32(i);
        mDirectValues[i].type.u = static_cast<u32>(-1);
    }

    std::set<u64> arrangeParams{};

    uintptr_t assets = reinterpret_cast<uintptr_t>(accessor.getAssetParamTable());
    uintptr_t start = assets;
    uintptr_t assetsEnd = reinterpret_cast<uintptr_t>(accessor.getTriggerOverwriteParam(0));
    for (u32 i = 0; assets < assetsEnd; ++i) {
        auto param = reinterpret_cast<const xlink2::ResAssetParam*>(assets);
        mAssetParams.emplace_back(ParamSet());
        mAssetParams[i].params.resize(std::popcount(param->values));
        auto params = reinterpret_cast<const xlink2::ResParam*>(param + 1);
        u32 paramIdx = 0;
        for (u32 j = 0; j < mPDT.getAssetParamCount(); ++j) {
            if (paramIdx >= mAssetParams[i].params.size()) {
                break;
            }
            if ((param->values >> j & 1) == 1) {
                mAssetParams[i].params[paramIdx].type = params[paramIdx].getValueReferenceType();
                mAssetParams[i].params[paramIdx].value = params[paramIdx].getValue();
                mAssetParams[i].params[paramIdx].index = j;
                if (params[paramIdx].getValueReferenceType() == xlink2::ValueReferenceType::ArrangeParam) {
                    arrangeParams.insert(params[paramIdx].getValue());
                } else if (params[paramIdx].getValueReferenceType() == xlink2::ValueReferenceType::Direct) {
                    const auto def = mPDT.getAssetParam(j);
                    mDirectValues[params[paramIdx].getValue()].type.e = def.getType();
                }
                ++paramIdx;
            }
        }
        info.assetParams.emplace(assets - start, i);
        assets += sizeof(xlink2::ResAssetParam) + sizeof(xlink2::ResParam) * mAssetParams[i].params.size();
    }

    u64 offset = 0;
    for (u32 i = 0; i < mTriggerOverwriteParams.size(); ++i) {
        auto param = accessor.getTriggerOverwriteParam(offset);
        mTriggerOverwriteParams[i].params.resize(std::popcount(param->values));
        auto params = reinterpret_cast<const xlink2::ResParam*>(param + 1);
        u32 paramIdx = 0;
        // is there a more efficient way of doing this?
        // maybe some bit shifting magic idk
        for (u32 j = 0; j < mPDT.getTriggerParamCount(); ++j) {
            if (paramIdx >= mTriggerOverwriteParams[i].params.size()) {
                break;
            }
            if ((param->values >> j & 1) == 1) {
                mTriggerOverwriteParams[i].params[paramIdx].type = params[paramIdx].getValueReferenceType();
                mTriggerOverwriteParams[i].params[paramIdx].value = params[paramIdx].getValue();
                mTriggerOverwriteParams[i].params[paramIdx].index = j;
                if (params[paramIdx].getValueReferenceType() == xlink2::ValueReferenceType::ArrangeParam) {
                    arrangeParams.insert(params[paramIdx].getValue());
                } else if (params[paramIdx].getValueReferenceType() == xlink2::ValueReferenceType::Direct) {
                    const auto def = mPDT.getTriggerParam(j);
                    mDirectValues[params[paramIdx].getValue()].type.e = def.getType();
                }
                ++paramIdx;
            }
        }
        info.triggerParams.emplace(offset, i);
        offset += sizeof(xlink2::ResTriggerOverwriteParam) + sizeof(xlink2::ResParam) * mTriggerOverwriteParams[i].params.size();
    }

    // FIXME: move this before users to skip having to go back and fix condition offsets later
    std::unordered_map<u64, s32> condIdxMap{};
    u64 condOffset = 0;
    u64 condBase = reinterpret_cast<uintptr_t>(accessor.getCondition(0));
    u64 condEnd = reinterpret_cast<uintptr_t>(accessor.getString(0));
    u32 i = 0;
    while (condBase + condOffset < condEnd) {
        condIdxMap.emplace(condOffset, i);
        Condition cond{};
        auto conditionBase = reinterpret_cast<const xlink2::ResCondition*>(accessor.getCondition(condOffset));
        cond.parentContainerType = conditionBase->getType();
        switch (conditionBase->getType()) {
            case xlink2::ContainerType::Switch: {
                auto resCond = static_cast<const xlink2::ResSwitchCondition*>(conditionBase);
                auto condition = cond.getAs<xlink2::ContainerType::Switch>();
                condition->propType = resCond->getPropType();
                condition->compareType = resCond->getCompareType();
                condition->isGlobal = resCond->isGlobal;
                // just assume this as it'll copy the data the same regardless
                condition->enumValue = resCond->enumValue;
                condition->conditionValue.i = resCond->value.i;
                // this field only conditionally exists
                if (condition->propType == xlink2::PropertyType::Enum) {
                    condition->enumName = info.strings.at(resCond->enumNameOffset);
                    condOffset += sizeof(xlink2::ResSwitchCondition);
                } else {
                    condOffset += sizeof(xlink2::ResSwitchCondition) - 8;
                }
                break;
            }
            case xlink2::ContainerType::Random:
            case xlink2::ContainerType::Random2: {
                auto resCond = static_cast<const xlink2::ResRandomCondition*>(conditionBase);
                auto condition = cond.getAs<xlink2::ContainerType::Random>();
                condition->weight = resCond->weight;
                condOffset += sizeof(xlink2::ResRandomCondition);
                break;
            }
            case xlink2::ContainerType::Blend: {
                auto resCond = static_cast<const xlink2::ResBlendCondition*>(conditionBase);
                auto condition = cond.getAs<xlink2::ContainerType::Blend>();
                condition->min = resCond->min;
                condition->max = resCond->max;
                condition->blendTypeToMax = resCond->getBlendTypeToMax();
                condition->blendTypeToMin = resCond->getBlendTypeToMin();
                condOffset += sizeof(xlink2::ResBlendCondition);
                break;
            }
            case xlink2::ContainerType::Sequence: {
                auto resCond = static_cast<const xlink2::ResSequenceCondition*>(conditionBase);
                auto condition = cond.getAs<xlink2::ContainerType::Sequence>();
                condition->continueOnFade = resCond->isContinueOnFade;
                condOffset += sizeof(xlink2::ResSequenceCondition);
                break;
            }
            case xlink2::ContainerType::Grid: {
                condOffset += sizeof(xlink2::ResGridCondition);
                break;
            }
            case xlink2::ContainerType::Jump: {
                condOffset += sizeof(xlink2::ResJumpCondition);
                break;
            }
            default:
                throw ResourceError(std::format("Invalid condition type {:#x}\n", static_cast<u32>(conditionBase->getType())));
        }
        mConditions.emplace_back(std::move(cond));
        ++i;
    }

    for (s32 i = 0; i < header->numUsers; ++i) {
        auto res = mUsers.emplace(accessor.getUserHash(i), User());
        (*res.first).second.initialize(this, accessor.getResUserHeader(i), info, condIdxMap, arrangeParams);
    }

    mArrangeGroupParams.resize(arrangeParams.size());
    std::unordered_map<u64, s32> paramIdxMap{};
    for (u32 i = 0; const auto offset : arrangeParams) {
        auto count = reinterpret_cast<const u32*>(accessor.getExRegion() + offset);
        mArrangeGroupParams[i].groups.resize(*count);
        auto params = reinterpret_cast<const xlink2::ArrangeGroupParam*>(count + 1);
        for (u32 j = 0; j < *count; ++j) {
            mArrangeGroupParams[i].groups[j].groupName = info.strings.at(params->groupNameOffset);
            mArrangeGroupParams[i].groups[j].limitType = params->limitType;
            mArrangeGroupParams[i].groups[j].limitThreshold = params->limitThreshold;
            mArrangeGroupParams[i].groups[j].unk = params->unk;
            ++params;
        }
        paramIdxMap.emplace(offset, i);
        ++i;
    }

    // fixup condition indices for PropertyTriggers and AssetCallTables
    for (auto& [hash, user] : mUsers) {
        // for (auto& propTrig : user.mPropertyTriggers) {
        //     if (propTrig.conditionIdx != -1) {
        //         std::cout << std::format("{:#x}\n", propTrig.conditionIdx);
        //         propTrig.conditionIdx = condIdxMap.at(propTrig.conditionIdx);
        //     }
        // }
        // for (auto& act : user.mAssetCallTables) {
        //     if (act.conditionIdx != -1) {
        //         std::cout << std::format("{:#x}\n", act.conditionIdx);
        //         act.conditionIdx = condIdxMap.at(act.conditionIdx);
        //     }
        // }

        for (auto& param : user.mUserParams) {
            if (param.type == xlink2::ValueReferenceType::ArrangeParam) {
                param.value = static_cast<u32>(paramIdxMap.at(std::get<u32>(param.value)));
            } else if (param.type == xlink2::ValueReferenceType::String) {
                param.value = info.strings.at(std::get<u32>(param.value));
            } else if (param.type == xlink2::ValueReferenceType::Direct) {
                const auto def = mPDT.getUserParam(param.index);
                mDirectValues[std::get<u32>(param.value)].type.e = def.getType();
            }
        }
    }

    // update arrange params to use indices instead of offsets
    for (auto& param : mAssetParams) {
        for (auto& p : param.params) {
            if (p.type == xlink2::ValueReferenceType::ArrangeParam) {
                p.value = static_cast<u32>(paramIdxMap.at(std::get<u32>(p.value)));
            } else if (p.type == xlink2::ValueReferenceType::String) {
                p.value = info.strings.at(std::get<u32>(p.value));
            }
        }
    }

    for (auto& param : mTriggerOverwriteParams) {
        for (auto& p : param.params) {
            if (p.type == xlink2::ValueReferenceType::ArrangeParam) {
                p.value = static_cast<u32>(paramIdxMap.at(std::get<u32>(p.value)));
            } else if (p.type == xlink2::ValueReferenceType::String) {
                p.value = info.strings.at(std::get<u32>(p.value));
            }
        }
    }

    return true;
}

const Curve& System::getCurve(s32 index) const {
    return mCurves[index];
}
Curve& System::getCurve(s32 index) {
    return mCurves[index];
}

const Random& System::getRandomCall(s32 index) const {
    return mRandomCalls[index];
}
Random& System::getRandomCall(s32 index) {
    return mRandomCalls[index];
}

const ArrangeGroupParams& System::getArrangeGroupParams(s32 index) const {
    return mArrangeGroupParams[index];
}
ArrangeGroupParams& System::getArrangeGroupParams(s32 index) {
    return mArrangeGroupParams[index];
}

const ParamSet& System::getTriggerOverwriteParam(s32 index) const {
    return mTriggerOverwriteParams[index];
}
ParamSet& System::getTriggerOverwriteParam(s32 index) {
    return mTriggerOverwriteParams[index];
}

const ParamSet& System::getAssetParam(s32 index) const {
    return mAssetParams[index];
}
ParamSet& System::getAssetParam(s32 index) {
    return mAssetParams[index];
}

const User& System::getUser(u32 hash) const {
    return mUsers.at(hash);
}
User& System::getUser(u32 hash) {
    return mUsers.at(hash);
}
const User& System::getUser(const std::string_view& key) const {
    return mUsers.at(util::calcCRC32(key));
}
User& System::getUser(const std::string_view& key) {
    return mUsers.at(util::calcCRC32(key));
}

const Condition& System::getCondition(s32 index) const {
    return mConditions[index];
}
Condition& System::getCondition(s32 index) {
    return mConditions[index];
}

u32 System::getDirectValueU32(s32 index) const {
    return mDirectValues[index].value.u;
}
s32 System::getDirectValueS32(s32 index) const {
    return mDirectValues[index].value.s;
}
f32 System::getDirectValueF32(s32 index) const {
    return mDirectValues[index].value.f;
}

bool System::searchUser(const std::string_view& key) const {
    return mUsers.find(util::calcCRC32(key)) != mUsers.end();
}
bool System::searchUser(u32 hash) const {
    return mUsers.find(hash) != mUsers.end();
}

void System::printUser(const std::string_view username) const {
    if (!searchUser(username))
        return;
    
    const auto user = getUser(username);
    std::cout << "Local Properties\n";
    for (const auto& prop : user.mLocalProperties) {
        std::cout << std::format("    {:s}\n", prop);
    }
    std::cout << "User Params\n";
    for (const auto& param : user.mUserParams) {
        printParam(param, ParamType::USER);
    }
}

void System::printParam(const Param& param, ParamType type) const {
    switch (type) {
        case ParamType::USER: {
            auto p = mPDT.getUserParam(param.index);
            std::cout << std::format("    {:s}\n", p.getName());
            break;
        }
        case ParamType::ASSET: {
            auto p = mPDT.getAssetParam(param.index);
            std::cout << std::format("    {:s}\n", p.getName());
            break;
        }
        case ParamType::TRIGGER: {
            auto p = mPDT.getTriggerParam(param.index);
            std::cout << std::format("    {:s}\n", p.getName());
            break;
        }
    }
}

std::vector<u8> System::serialize() {
    Serializer writer(this);
    writer.serialize();
    return writer.flush();
}

bool System::addAssetCall(User& user, const AssetCallTable& act) {
    if (act.isContainer()) {
        if (act.containerParamIdx < 0 || static_cast<u32>(act.containerParamIdx) >= user.mContainers.size()) {
            return false;
        }
    } else {
        if (act.assetParamIdx < 0 || static_cast<u32>(act.assetParamIdx) >= mAssetParams.size()) {
            return false;
        }
    }
    if (act.conditionIdx >= 0 && static_cast<u32>(act.conditionIdx) >= mConditions.size()) {
        return false;
    }

    auto res = mStrings.insert(std::string(act.keyName));
    const_cast<AssetCallTable*>(&act)->keyName = *res.first;

    user.mAssetCallTables.emplace_back(act);

    return true;
}

bool System::addAssetCall(User& user, const std::string_view& key, bool isContainer, s32 paramIdx, s32 conditionIdx) {
    if (isContainer) {
        if (paramIdx < 0 || static_cast<u32>(paramIdx) >= user.mContainers.size()) {
            return false;
        }
    } else {
        if (paramIdx < 0 || static_cast<u32>(paramIdx) >= mAssetParams.size()) {
            return false;
        }
    }

    if (conditionIdx >= 0 && static_cast<u32>(conditionIdx) >= mConditions.size()) {
        return false;
    }

    auto res = mStrings.insert(std::string(key));

    user.mAssetCallTables.emplace_back(
        AssetCallTable{
            .keyName = *res.first,
            .assetIndex = 0, // doesn't matter, it'll be determined when serializing
            .flag = static_cast<u16>(isContainer ? 1 : 0),
            .duration = 1,
            .parentIndex = -1,
            .guid = 0x69420,
            .keyNameHash = util::calcCRC32(*res.first),
            .assetParamIdx = paramIdx,
            .conditionIdx = conditionIdx,
        }
    );

    return true;
}

bool System::addAssetCall(User& user, const std::string_view& key, const ParamSet& assetParam, s32 conditionIdx) {
    if (conditionIdx >= 0 && static_cast<u32>(conditionIdx) >= mConditions.size()) {
        return false;
    }

    auto res = mStrings.insert(std::string(key));

    mAssetParams.emplace_back(assetParam);

    user.mAssetCallTables.emplace_back(
        AssetCallTable{
            .keyName = *res.first,
            .assetIndex = 0, // doesn't matter, it'll be determined when serializing
            .flag = 0,
            .duration = 1,
            .parentIndex = -1,
            .guid = 0x69420,
            .keyNameHash = util::calcCRC32(*res.first),
            .assetParamIdx = static_cast<s32>(mAssetParams.size() - 1),
            .conditionIdx = conditionIdx,
        }
    );

    return true;
}

bool System::addAssetCall(User& user, const std::string_view& key, const Container& container, s32 conditionIdx) {
    if (conditionIdx >= 0 && static_cast<u32>(conditionIdx) >= mConditions.size()) {
        return false;
    }

    auto res = mStrings.insert(std::string(key));

    user.mContainers.emplace_back(container);

    user.mAssetCallTables.emplace_back(
        AssetCallTable{
            .keyName = *res.first,
            .assetIndex = 0, // doesn't matter, it'll be determined when serializing
            .flag = 1,
            .duration = 1,
            .parentIndex = -1,
            .guid = 0x69420,
            .keyNameHash = util::calcCRC32(*res.first),
            .assetParamIdx = static_cast<s32>(user.mContainers.size() - 1),
            .conditionIdx = conditionIdx,
        }
    );

    return true;
}

s32 System::searchParamIndex(const std::string_view& name, ParamType type) const {
    return mPDT.searchParamIndex(name, type);
}

} // namespace banana