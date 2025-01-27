#include "system.h"
#include "serializer.h"
#include "util/error.h"
#include "util/crc32.h"

#include <bit>
#include <iostream>
#include <format>

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
        info.strings.emplace(reinterpret_cast<ptrdiff_t>(pos - nameTable), std::string_view(*res.first));
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
        mDirectValues[i] = accessor.getDirectValueU32(i);
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
                if (params[paramIdx].getValueReferenceType() == xlink2::ValueReferenceType::ArrangeParam) {
                    arrangeParams.insert(params[paramIdx].getValue());
                }
                mAssetParams[i].params[paramIdx].index = j;
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
                if (params[paramIdx].getValueReferenceType() == xlink2::ValueReferenceType::ArrangeParam) {
                    arrangeParams.insert(params[paramIdx].getValue());
                }
                mTriggerOverwriteParams[i].params[paramIdx].index = j;
                ++paramIdx;
            }
        }
        info.triggerParams.emplace(offset, i);
        offset += sizeof(xlink2::ResTriggerOverwriteParam) + sizeof(xlink2::ResParam) * mTriggerOverwriteParams[i].params.size();
    }

    std::set<u64> conditionOffsets{};
    for (s32 i = 0; i < header->numUsers; ++i) {
        auto res = mUsers.emplace(accessor.getUserHash(i), User());
        (*res.first).second.initialize(this, accessor.getResUserHeader(i), info, conditionOffsets, arrangeParams);
    }

    mConditions.resize(conditionOffsets.size());
    std::unordered_map<u64, s32> condIdxMap{};
    for (u32 i = 0; const auto offset : conditionOffsets) {
        auto conditionBase = reinterpret_cast<const xlink2::ResCondition*>(accessor.getCondition(offset));
        mConditions[i].parentContainerType = conditionBase->getType();
        switch (conditionBase->getType()) {
            case xlink2::ContainerType::Switch: {
                auto resCond = static_cast<const xlink2::ResSwitchCondition*>(conditionBase);
                auto condition = mConditions[i].getAs<xlink2::ContainerType::Switch>();
                condition->propType = resCond->getPropType();
                condition->compareType = resCond->getCompareType();
                condition->isGlobal = resCond->isGlobal;
                // just assume this as it'll copy the data the same regardless
                condition->enumValue = resCond->enumValue;
                condition->conditionValue.i = resCond->value.i;
                // this field only conditionally exists
                if (condition->propType == xlink2::PropertyType::Enum)
                    condition->enumName = info.strings.at(resCond->enumNameOffset);
                break;
            }
            case xlink2::ContainerType::Random:
            case xlink2::ContainerType::Random2: {
                auto resCond = static_cast<const xlink2::ResRandomCondition*>(conditionBase);
                auto condition = mConditions[i].getAs<xlink2::ContainerType::Random>();
                condition->weight = resCond->weight;
                break;
            }
            case xlink2::ContainerType::Blend: {
                auto resCond = static_cast<const xlink2::ResBlendCondition*>(conditionBase);
                auto condition = mConditions[i].getAs<xlink2::ContainerType::Blend>();
                condition->min = resCond->min;
                condition->max = resCond->max;
                condition->blendTypeToMax = resCond->getBlendTypeToMax();
                condition->blendTypeToMin = resCond->getBlendTypeToMin();
                break;
            }
            case xlink2::ContainerType::Sequence: {
                auto resCond = static_cast<const xlink2::ResSequenceCondition*>(conditionBase);
                auto condition = mConditions[i].getAs<xlink2::ContainerType::Sequence>();
                condition->continueOnFade = resCond->isContinueOnFade;
                break;
            }
            case xlink2::ContainerType::Grid:
            case xlink2::ContainerType::Jump:
                break;
            default:
                throw ResourceError(std::format("Invalid condition type {:#x}\n", static_cast<u32>(conditionBase->getType())));
        }

        condIdxMap.emplace(offset, i);
        ++i;
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
        for (auto& propTrig : user.mPropertyTriggers) {
            if (propTrig.conditionIdx != -1) {
                propTrig.conditionIdx = condIdxMap.at(propTrig.conditionIdx);
            }
        }
        for (auto& act : user.mAssetCallTables) {
            if (act.conditionIdx != -1) {
                act.conditionIdx = condIdxMap.at(act.conditionIdx);
            }
        }

        for (auto& param : user.mUserParams) {
            if (param.type == xlink2::ValueReferenceType::ArrangeParam) {
                param.value = static_cast<u32>(paramIdxMap.at(std::get<u32>(param.value)));
            } else if (param.type == xlink2::ValueReferenceType::String) {
                param.value = info.strings.at(std::get<u32>(param.value));
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
    return mDirectValues[index];
}
s32 System::getDirectValueS32(s32 index) const {
    return std::bit_cast<s32, u32>(mDirectValues[index]);
}
f32 System::getDirectValueF32(s32 index) const {
    return std::bit_cast<f32, u32>(mDirectValues[index]);
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

void System::dumpParam(LibyamlEmitterWithStorage<std::string>& emitter, const Param& param, ParamType type) const {
    xlink2::ParamType paramType = xlink2::ParamType::Int;
    switch (type) {
        case ParamType::USER: {
            auto define = mPDT.getUserParam(param.index);
            emitter.EmitString(define.getName());
            paramType = define.getType();
            break;
        }
        case ParamType::ASSET: {
            auto define = mPDT.getAssetParam(param.index);
            emitter.EmitString(define.getName());
            paramType = define.getType();
            break;
        }
        case ParamType::TRIGGER: {
            auto define = mPDT.getTriggerParam(param.index);
            emitter.EmitString(define.getName());
            paramType = define.getType();
            break;
        }
        default:
            throw InvalidDataError("Invalid parameter type");
    }

    using RefType = xlink2::ValueReferenceType;
    using ValType = xlink2::ParamType;
    switch (param.type) {
        case RefType::Direct: {
            switch (paramType) {
                case ValType::Int: {
                    emitter.EmitInt(getDirectValueS32(std::get<u32>(param.value)));
                    break;
                }
                case ValType::Float: {
                    emitter.EmitFloat(getDirectValueF32(std::get<u32>(param.value)));
                    break;
                }
                case ValType::Bool: {
                    emitter.EmitBool(getDirectValueU32(std::get<u32>(param.value)) != 0);
                    break;
                }
                case ValType::Enum: {
                    emitter.EmitScalar(std::format("{:#010x}", getDirectValueU32(std::get<u32>(param.value))), false, false, "!u");
                    break;
                }
                case ValType::String:
                    throw InvalidDataError("Unreachable case - cannot have direct value strings");
                case ValType::Bitfield:
                    throw InvalidDataError("Unreachable case - bitfields should use the bitfield reference type");
                    break;
                default:
                    throw InvalidDataError("Invalid param type");
            }
            break;
        }
        case RefType::String: {
            if (paramType != ValType::String)
                throw InvalidDataError("String reference type but no string value!");
            emitter.EmitString(std::get<std::string_view>(param.value));
            break;
        }
        case RefType::Curve: {
            if (paramType != ValType::Float)
                throw InvalidDataError("Curves must be floats!");
            const auto curve = mCurves.at(std::get<u32>(param.value));
            LibyamlEmitter::MappingScope scope{emitter, "!curve", YAML_BLOCK_MAPPING_STYLE};
            emitter.EmitString("PropertyName");
            emitter.EmitString(curve.propertyName); // should verify this is matches the corresponding index if the property is local
            emitter.EmitString("PropertyIndex");
            emitter.EmitInt(curve.propertyIndex);
            emitter.EmitString("IsGlobal");
            emitter.EmitBool(curve.isGlobal);
            emitter.EmitString("CurveType");
            emitter.EmitInt(curve.type);
            emitter.EmitString("Unknown1");
            emitter.EmitInt(curve.unk);
            emitter.EmitString("Unknown2");
            emitter.EmitInt(curve.unk2);
            emitter.EmitString("Points");
            {
                LibyamlEmitter::SequenceScope seqScope{emitter, {}, curve.points.size() > 5 ? YAML_BLOCK_SEQUENCE_STYLE
                                                                                            : YAML_FLOW_SEQUENCE_STYLE};
                for (const auto& point : curve.points) {
                    LibyamlEmitter::MappingScope scope{emitter, {}, YAML_FLOW_MAPPING_STYLE};
                    emitter.EmitString("x");
                    emitter.EmitFloat(point.x);
                    emitter.EmitString("y");
                    emitter.EmitFloat(point.y);
                }
            }
            break;
        }
        case RefType::Random:
        case RefType::RandomPowHalf2:
        case RefType::RandomPowHalf3:
        case RefType::RandomPowHalf4:
        case RefType::RandomPowHalf1Point5:
        case RefType::RandomPow2:
        case RefType::RandomPow3:
        case RefType::RandomPow4:
        case RefType::RandomPow1Point5:
        case RefType::RandomPowComplement2:
        case RefType::RandomPowComplement3:
        case RefType::RandomPowComplement4:
        case RefType::RandomPowComplement1Point5: {
            if (paramType != ValType::Float)
                throw InvalidDataError("Random calls must be floats!");
            const auto random = getRandomCall(std::get<u32>(param.value));
            LibyamlEmitter::MappingScope scope{emitter, "!random", YAML_FLOW_MAPPING_STYLE};
            emitter.EmitString("Type");
            emitter.EmitScalar(std::format("{:#010x}", static_cast<u32>(param.type)), false, false, "!u");
            emitter.EmitString("Min");
            emitter.EmitFloat(random.min);
            emitter.EmitString("Max");
            emitter.EmitFloat(random.max);
            break;
        }
        case RefType::ArrangeParam: {
            if (paramType != ValType::Bitfield)
                throw InvalidDataError("ArrangeParam needs to be a bitfield!");
            const auto groups = getArrangeGroupParams(std::get<u32>(param.value));
            LibyamlEmitter::SequenceScope seqScope{emitter, "!arrangeGroupParam", YAML_BLOCK_SEQUENCE_STYLE};
            for (const auto& group : groups.groups) {
                LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
                emitter.EmitString("GroupName");
                emitter.EmitString(group.groupName);
                emitter.EmitString("LimitType");
                emitter.EmitInt(group.limitType);
                emitter.EmitString("LimitThreshold");
                emitter.EmitInt(group.limitThreshold);
                emitter.EmitString("Unknown");
                emitter.EmitInt(group.unk);
            }
            break;
        }
        case RefType::Bitfield: { // should this just be called immediate? seems to be used just for ints?
            if (paramType != ValType::Int)
                throw InvalidDataError(std::format("Bitfields need to be ints! {:d}", static_cast<u32>(paramType)));
            emitter.EmitScalar(std::format("{:#b}", std::get<u32>(param.value)), false, false, "!bitfield");
            break;
        }
        default:
            throw InvalidDataError("Invalid param type!");
    }
}

void System::dumpParamSet(LibyamlEmitterWithStorage<std::string>& emitter, const ParamSet& params, ParamType type) const {
    LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
    for (const auto& param : params.params) {
        dumpParam(emitter, param, type);
    }
}

void System::dumpCondition(LibyamlEmitterWithStorage<std::string>& emitter, const Condition& condition) const {
    LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};

    static constexpr std::string_view sCompareTypeStrings[6] = {
        "Equal", "GreaterThan", "GreaterThanOrEqual",
        "LessThan", "LessThanOrEqual", "NotEqual",
    };

    static constexpr std::string_view sBlendTypeStrings[6] = {
        "None", "Multiply", "SquareRoot", "Sin", "Add", "SetToOne",
    };

    using Type = xlink2::ContainerType;
    switch (condition.parentContainerType) {
        case Type::Switch: {
            const auto cond = condition.getAs<Type::Switch>();
            emitter.EmitString("Type");
            emitter.EmitString("Switch");
            emitter.EmitString("CompareType");
            emitter.EmitString(sCompareTypeStrings[static_cast<u32>(cond->compareType)]);
            emitter.EmitString("IsGlobal");
            emitter.EmitBool(cond->isGlobal);
            emitter.EmitString("Value1"); // action hash or enum value/index
            emitter.EmitScalar(std::format("{:#010x}", cond->actionHash), false, false, "!u");
            emitter.EmitString("Value2");
            switch (cond->propType) {
                case xlink2::PropertyType::S32:
                case xlink2::PropertyType::_04:
                    emitter.EmitInt(cond->conditionValue.i);
                    break;
                case xlink2::PropertyType::F32:
                case xlink2::PropertyType::_05:
                    emitter.EmitFloat(cond->conditionValue.f);
                    break;
                case xlink2::PropertyType::Bool:
                    emitter.EmitBool(cond->conditionValue.b);
                    break;
                case xlink2::PropertyType::Enum:
                    emitter.EmitScalar(std::format("{:#010x}", std::bit_cast<u32, s32>(cond->conditionValue.i)), false, false, "!u");
                    emitter.EmitString("EnumClass");
                    emitter.EmitString(cond->enumName);
                    break;
                default:
                    throw InvalidDataError("Invalid switch case property type");
            }
            break;
        }
        case Type::Random:
        case Type::Random2: {
            const auto cond = condition.getAs<Type::Random2>();
            emitter.EmitString("Type");
            emitter.EmitString(condition.parentContainerType == Type::Random2 ? "Random2" : "Random");
            emitter.EmitString("Weight");
            emitter.EmitFloat(cond->weight);
            break;
        }
        case Type::Blend: {
            const auto cond = condition.getAs<Type::Blend>();
            emitter.EmitString("Type");
            emitter.EmitString("Blend");
            emitter.EmitString("Min");
            emitter.EmitFloat(cond->min);
            emitter.EmitString("Max");
            emitter.EmitFloat(cond->max);
            emitter.EmitString("BlendTypeMin");
            emitter.EmitString(sBlendTypeStrings[static_cast<u32>(cond->blendTypeToMin)]);
            emitter.EmitString("BlendTypeMax");
            emitter.EmitString(sBlendTypeStrings[static_cast<u32>(cond->blendTypeToMax)]);
            break;
        }
        case Type::Sequence: {
            const auto cond = condition.getAs<Type::Sequence>();
            emitter.EmitString("Type");
            emitter.EmitString("Sequence");
            emitter.EmitString("ContinueOnFade");
            emitter.EmitInt(cond->continueOnFade);
            break;
        }
        case Type::Grid: {
            emitter.EmitString("Type");
            emitter.EmitString("Grid");
            break;
        }
        case Type::Jump: {
            emitter.EmitString("Type");
            emitter.EmitString("Jump");
            break;
        }
        default:
            throw InvalidDataError("Invalid condition type!");
    }
}

void System::dumpContainer(LibyamlEmitterWithStorage<std::string>& emitter, const Container& container) const {
    LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};

    using Type = xlink2::ContainerType;
    switch (container.type) {
        case Type::Switch: {
            const auto param = container.getAs<Type::Switch>();
            emitter.EmitString("Type");
            emitter.EmitString("Switch");
            emitter.EmitString("ValueName");
            emitter.EmitString(param->actionSlotName);
            emitter.EmitString("Unknown");
            emitter.EmitInt(param->unk);
            emitter.EmitString("PropertyIndex");
            emitter.EmitInt(param->propertyIndex);
            emitter.EmitString("IsGlobal");
            emitter.EmitBool(param->isGlobal);
            emitter.EmitString("IsActionTrigger");
            emitter.EmitBool(param->isActionTrigger);
            break;
        }
        case Type::Random: {
            emitter.EmitString("Type");
            emitter.EmitString("Random");
            break;
        }
        case Type::Random2: {
            emitter.EmitString("Type");
            emitter.EmitString("Random2");
            break;
        }
        case Type::Blend: {
            emitter.EmitString("Type");
            emitter.EmitString("Blend");
            if (container.isNotBlendAll) {
                const auto param = container.getAs<Type::Blend, true>();
                emitter.EmitString("ValueName");
                emitter.EmitString(param->actionSlotName);
                emitter.EmitString("Unknown");
                emitter.EmitInt(param->unk);
                emitter.EmitString("PropertyIndex");
                emitter.EmitInt(param->propertyIndex);
                emitter.EmitString("IsGlobal");
                emitter.EmitBool(param->isGlobal);
                emitter.EmitString("IsActionTrigger");
                emitter.EmitBool(param->isActionTrigger);
            }
            break;
        }
        case Type::Sequence: {
            emitter.EmitString("Type");
            emitter.EmitString("Sequence");
            break;
        }
        case Type::Grid: {
            const auto param = container.getAs<Type::Grid>();
            emitter.EmitString("Type");
            emitter.EmitString("Grid");
            emitter.EmitString("PropertyName1");
            emitter.EmitString(param->propertyName1);
            emitter.EmitString("PropertyName2");
            emitter.EmitString(param->propertyName2);
            emitter.EmitString("PropertyIndex1");
            emitter.EmitInt(param->propertyIndex1);
            emitter.EmitString("PropertyIndex2");
            emitter.EmitInt(param->propertyIndex2);
            emitter.EmitString("IsProperty1Global");
            emitter.EmitBool(param->isGlobal1);
            emitter.EmitString("IsProperty2Global");
            emitter.EmitBool(param->isGlobal2);
            emitter.EmitString("Property1Values");
            {
                LibyamlEmitter::SequenceScope seqScope{emitter, {}, YAML_FLOW_SEQUENCE_STYLE};
                for (const auto value : param->values1) {
                    emitter.EmitInt(value);
                }
            }
            emitter.EmitString("Property2Values");
            {
                LibyamlEmitter::SequenceScope seqScope{emitter, {}, YAML_FLOW_SEQUENCE_STYLE};
                for (const auto value : param->values2) {
                    emitter.EmitInt(value);
                }
            }
            emitter.EmitString("IndexGridMap");
            {
                LibyamlEmitter::SequenceScope seqScope{emitter, {}, YAML_FLOW_SEQUENCE_STYLE};
                for (const auto index : param->indices) {
                    emitter.EmitInt(index);
                }
            }
            break;
        }
        case Type::Jump: {
            emitter.EmitString("Type");
            emitter.EmitString("Jump");
            break;
        }
        default:
            throw InvalidDataError("Invalid condition type!");
    }

    emitter.EmitString("ChildContainerBaseIndex");
    emitter.EmitInt(container.childContainerStartIdx);
    emitter.EmitString("ChildContainerCount");
    emitter.EmitInt(container.childCount);
    emitter.EmitString("IsNeedObserve");
    emitter.EmitBool(container.isNeedObserve);
}

void System::dumpAssetCallTable(LibyamlEmitterWithStorage<std::string>& emitter, const AssetCallTable& act) const {
    LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};

    emitter.EmitString("KeyName");
    emitter.EmitString(act.keyName);
    emitter.EmitString("AssetIndex");
    emitter.EmitInt(act.assetIndex);
    emitter.EmitString("IsContainer");
    emitter.EmitBool((act.flag & 1) == 1);
    emitter.EmitString("Duration");
    emitter.EmitInt(act.duration);
    emitter.EmitString("ParentIndex");
    emitter.EmitInt(act.parentIndex);
    emitter.EmitString("GUID");
    emitter.EmitScalar(std::format("{:#010x}", act.guid), false, false, "!u");
    emitter.EmitString("KeyNameHash");
    emitter.EmitScalar(std::format("{:#010x}", act.keyNameHash), false, false, "!u");
    emitter.EmitString("AssetParamOrContainerIndex");
    emitter.EmitInt(act.assetParamIdx);
    emitter.EmitString("ConditionIndex");
    emitter.EmitInt(act.conditionIdx);
}

void System::dumpActionSlot(LibyamlEmitterWithStorage<std::string>& emitter, const ActionSlot& slot) const {
    LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};

    emitter.EmitString("SlotName");
    emitter.EmitString(slot.actionSlotName);
    emitter.EmitString("ActionBaseIndex");
    emitter.EmitInt(slot.actionStartIdx);
    emitter.EmitString("ActionCount");
    emitter.EmitInt(slot.actionCount);
}

void System::dumpAction(LibyamlEmitterWithStorage<std::string>& emitter, const Action& action) const {
    LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};

    emitter.EmitString("ActionName");
    emitter.EmitString(action.actionName);
    emitter.EmitString("TriggerBaseIndex");
    emitter.EmitInt(action.actionTriggerStartIdx);
    emitter.EmitString("TriggerCount");
    emitter.EmitInt(action.actionTriggerCount);
    emitter.EmitString("EnableMatchStart");
    emitter.EmitBool(action.enableMatchStart);
}

void System::dumpActionTrigger(LibyamlEmitterWithStorage<std::string>& emitter, const ActionTrigger& trigger) const {
    LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};

    emitter.EmitString("GUID");
    emitter.EmitScalar(std::format("{:#010x}", trigger.guid), false, false, "!u");
    emitter.EmitString("Unknown");
    emitter.EmitInt(trigger.unk);
    emitter.EmitString("TriggerOnce");
    emitter.EmitBool(trigger.triggerOnce);
    emitter.EmitString("IsFade");
    emitter.EmitBool(trigger.fade);
    emitter.EmitString("AlwaysTrigger");
    emitter.EmitBool(trigger.alwaysTrigger);
    if (trigger.nameMatch) {
        emitter.EmitString("PreviousActionName");
        emitter.EmitString(trigger.previousActionName);
    } else {
        emitter.EmitString("StartFrame");
        emitter.EmitInt(trigger.startFrame);
    }
    emitter.EmitString("EndFrame");
    emitter.EmitInt(trigger.endFrame);
    emitter.EmitString("TriggerOverwriteParamIndex");
    emitter.EmitInt(trigger.triggerOverwriteIdx);
    emitter.EmitString("OverwriteHash");
    emitter.EmitScalar(std::format("{:#06x}", trigger.overwriteHash), false, false, "!u");
}

void System::dumpProperty(LibyamlEmitterWithStorage<std::string>& emitter, const Property& prop) const {
    LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};

    emitter.EmitString("PropertyName");
    emitter.EmitString(prop.propertyName);
    emitter.EmitString("IsGlobal");
    emitter.EmitBool(prop.isGlobal);
    emitter.EmitString("TriggerBaseIndex");
    emitter.EmitInt(prop.propTriggerStartIdx);
    emitter.EmitString("TriggerCount");
    emitter.EmitInt(prop.propTriggerCount);
}

void System::dumpPropertyTrigger(LibyamlEmitterWithStorage<std::string>& emitter, const PropertyTrigger& trigger) const {
    LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};

    emitter.EmitString("GUID");
    emitter.EmitScalar(std::format("{:#010x}", trigger.guid), false, false, "!u");
    emitter.EmitString("Flag");
    emitter.EmitInt(trigger.flag);
    emitter.EmitString("OverwriteHash");
    emitter.EmitScalar(std::format("{:#06x}", trigger.overwriteHash), false, false, "!u");
    emitter.EmitString("AssetCallTableIndex");
    emitter.EmitInt(trigger.assetCallTableIdx);
    emitter.EmitString("ConditionIndex");
    emitter.EmitInt(trigger.conditionIdx);
    emitter.EmitString("TriggerOverwriteParamIndex");
    emitter.EmitInt(trigger.triggerOverwriteIdx);
}

void System::dumpAlwaysTrigger(LibyamlEmitterWithStorage<std::string>& emitter, const AlwaysTrigger& trigger) const {
    LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};

    emitter.EmitString("GUID");
    emitter.EmitScalar(std::format("{:#010x}", trigger.guid), false, false, "!u");
    emitter.EmitString("Flag");
    emitter.EmitInt(trigger.flag);
    emitter.EmitString("OverwriteHash");
    emitter.EmitScalar(std::format("{:#06x}", trigger.overwriteHash), false, false, "!u");
    emitter.EmitString("AssetCallTableIndex");
    emitter.EmitInt(trigger.assetCallIdx);
    emitter.EmitString("TriggerOverwriteParamIndex");
    emitter.EmitInt(trigger.triggerOverwriteIdx);
}

void System::dumpUser(LibyamlEmitterWithStorage<std::string>& emitter, const User& user) const {
    LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};

    emitter.EmitString("LocalProperties");
    {
        LibyamlEmitter::SequenceScope seqScope{emitter, {}, YAML_BLOCK_SEQUENCE_STYLE};
        for (const auto& prop : mLocalProperties) {
            emitter.EmitString(prop);
        }
    }
    
    emitter.EmitString("UserParams");
    {
        LibyamlEmitter::MappingScope mapScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
        for (const auto& param : user.mUserParams) {
            dumpParam(emitter, param, ParamType::USER);
        }
    }

    emitter.EmitString("Containers");
    {
        LibyamlEmitter::MappingScope mapScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
        for (u32 i = 0; const auto& container : user.mContainers) {
            emitter.EmitInt(i);
            dumpContainer(emitter, container);
            ++i;
        }
    }

    emitter.EmitString("AssetCallTables");
    {
        LibyamlEmitter::MappingScope mapScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
        for (u32 i = 0; const auto& act: user.mAssetCallTables) {
            emitter.EmitInt(i);
            dumpAssetCallTable(emitter, act);
            ++i;
        }
    }

    emitter.EmitString("ActionSlots");
    {
        LibyamlEmitter::MappingScope mapScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
        for (u32 i = 0; const auto& slot: user.mActionSlots) {
            emitter.EmitInt(i);
            dumpActionSlot(emitter, slot);
            ++i;
        }
    }

    emitter.EmitString("Actions");
    {
        LibyamlEmitter::MappingScope mapScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
        for (u32 i = 0; const auto& action: user.mActions) {
            emitter.EmitInt(i);
            dumpAction(emitter, action);
            ++i;
        }
    }

    emitter.EmitString("ActionTriggers");
    {
        LibyamlEmitter::MappingScope mapScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
        for (u32 i = 0; const auto& trigger: user.mActionTriggers) {
            emitter.EmitInt(i);
            dumpActionTrigger(emitter, trigger);
            ++i;
        }
    }

    emitter.EmitString("Properties");
    {
        LibyamlEmitter::MappingScope mapScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
        for (u32 i = 0; const auto& prop: user.mProperties) {
            emitter.EmitInt(i);
            dumpProperty(emitter, prop);
            ++i;
        }
    }

    emitter.EmitString("PropertyTriggers");
    {
        LibyamlEmitter::MappingScope mapScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
        for (u32 i = 0; const auto& trigger: user.mPropertyTriggers) {
            emitter.EmitInt(i);
            dumpPropertyTrigger(emitter, trigger);
            ++i;
        }
    }

    emitter.EmitString("AlwaysTriggers");
    {
        LibyamlEmitter::MappingScope mapScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
        for (u32 i = 0; const auto& trigger: user.mAlwaysTriggers) {
            emitter.EmitInt(i);
            dumpAlwaysTrigger(emitter, trigger);
            ++i;
        }
    }

    emitter.EmitString("Unknown");
    emitter.EmitInt(user.mUnknown);
}

std::string System::dumpYAML() const {
    LibyamlEmitterWithStorage<std::string> emitter{};
    yaml_event_t event;

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    emitter.Emit(event);

    yaml_document_start_event_initialize(&event, nullptr, nullptr, nullptr, 1);
    emitter.Emit(event);

    {
        LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
        mPDT.dumpYAML(emitter);
        {
            emitter.EmitString("LocalProperties");
            LibyamlEmitter::SequenceScope seqScope{emitter, {}, YAML_BLOCK_SEQUENCE_STYLE};
            for (const auto& prop : mLocalProperties) {
                emitter.EmitString(prop);
            }
        }
        {
            emitter.EmitString("LocalPropertyEnumValues");
            LibyamlEmitter::SequenceScope seqScope{emitter, {}, YAML_BLOCK_SEQUENCE_STYLE};
            for (const auto& prop : mLocalPropertyEnumStrings) {
                emitter.EmitString(prop);
            }
        }
        {
            emitter.EmitString("AssetParams");
            LibyamlEmitter::MappingScope mapScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
            for (u32 i = 0; const auto& param : mAssetParams) {
                emitter.EmitInt(i);
                dumpParamSet(emitter, param, ParamType::ASSET);
                ++i;
            }
        }
        {
            emitter.EmitString("TriggerOverwriteParams");
            LibyamlEmitter::MappingScope mapScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
            for (u32 i = 0; const auto& param : mTriggerOverwriteParams) {
                emitter.EmitInt(i);
                dumpParamSet(emitter, param, ParamType::TRIGGER);
                ++i;
            }
        }
        {
            emitter.EmitString("Conditions");
            LibyamlEmitter::MappingScope seqScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
            for (u32 i = 0; const auto& condition : mConditions) {
                emitter.EmitInt(i);
                dumpCondition(emitter, condition);
                ++i;
            }
        }
        {
            emitter.EmitString("Users");
            LibyamlEmitter::MappingScope seqScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
            for (const auto& [hash, user] : mUsers) {
                emitter.EmitScalar(std::format("{:#010x}", hash), false, false, "!u");
                dumpUser(emitter, user);
            }
        }
    }

    yaml_document_end_event_initialize(&event, 1);
    emitter.Emit(event);

    yaml_stream_end_event_initialize(&event);
    emitter.Emit(event);

    return std::move(emitter.GetOutput());
}

} // namespace banana