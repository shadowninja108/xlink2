#include "pdt.h"
#include "util/error.h"

#include <bit>
#include <format>
#include <iostream>

namespace banana {

void ParamDefine::initialize(const xlink2::ResParamDefine* param, const std::unordered_map<TargetPointer, std::string_view>& strings) {
    mName = strings.at(param->nameOffset);
    mType = param->getType();
    switch (mType) {
        case xlink2::ParamType::Int:
            mDefaultValue = std::bit_cast<s32, u32>(static_cast<u32>(param->defaultValue));
            break;
        case xlink2::ParamType::Float:
            mDefaultValue = std::bit_cast<f32, u32>(static_cast<u32>(param->defaultValue));
            break;
        case xlink2::ParamType::Bool:
            mDefaultValue = param->defaultValue != 0;
            break;
        case xlink2::ParamType::Enum:
            mDefaultValue = static_cast<u32>(param->defaultValue);
            break;
        case xlink2::ParamType::String:
            mDefaultValue = strings.at(param->defaultValue);
            break;
        case xlink2::ParamType::Bitfield:
            mDefaultValue = static_cast<u32>(param->defaultValue);
            break;
        default:
            throw ResourceError(std::format("Invalid ParamDefine type: {}", static_cast<u32>(mType)));
    }
}

void ParamDefine::print() const {
    switch (getType()) {
        case xlink2::ParamType::Int:
            std::cout << std::format("    {:s}: {:d}\n", getName(), getValue<xlink2::ParamType::Int>());
            break;
        case xlink2::ParamType::Float:
            std::cout << std::format("    {:s}: {:f}\n", getName(), getValue<xlink2::ParamType::Float>());
            break;
        case xlink2::ParamType::Bool:
            std::cout << std::format("    {:s}: {:s}\n", getName(), getValue<xlink2::ParamType::Bool>());
            break;
        case xlink2::ParamType::Enum:
            std::cout << std::format("    {:s}: {:d}\n", getName(), getValue<xlink2::ParamType::Enum>());
            break;
        case xlink2::ParamType::String:
            std::cout << std::format("    {:s}: {:s}\n", getName(), getValue<xlink2::ParamType::String>());
            break;
        case xlink2::ParamType::Bitfield:
            std::cout << std::format("    {:s}: {:#032b}\n", getName(), getValue<xlink2::ParamType::Bitfield>());
            break;
    }
}

bool ParamDefineTable::initialize(const ResourceAccessor& accessor) {
    if (!accessor.isLoaded()) {
        return false;
    }

    if (accessor.isELink()) {
        mSystemUserParamCount = sSystemELinkUserParamCount;
    } else if (accessor.isSLink()) {
        mSystemUserParamCount = sSystemSLinkUserParamCount;
    } else {
        throw ResourceError(std::format("Invalid resource version (expecting 0x24 or 0x21): {:x}", accessor.getResourceHeader()->version));
    }

    // each define will just store a string_view of the string while the PDT will store a set of all strings
    const char* nameTable = accessor.getParamDefineName(0);
    const char* pos = nameTable;
    const auto end = reinterpret_cast<const char*>(accessor.getTriggerOverwriteParam(0));

    std::unordered_map<TargetPointer, std::string_view> offsetMap{};

    do {
        auto res = mStrings.insert(std::string(pos));
#ifdef _MSC_VER
        offsetMap.emplace(pos - nameTable, std::string_view(*res.first));
#else
        offsetMap.emplace(reinterpret_cast<ptrdiff_t>(pos - nameTable), std::string_view(*res.first));
#endif
        pos += res.first->size() + 1;
    } while (pos < end && *pos);

    const auto pdt = accessor.getParamDefineTable();

    mSystemAssetParamCount = pdt->numAssetParams - pdt->numUserAssetParams;

    mUserParams.resize(pdt->numUserParams);
    mAssetParams.resize(pdt->numAssetParams);
    mTriggerParams.resize(pdt->numTriggerParams);
    
    for (u32 i = 0; i < mUserParams.size(); ++i) {
        auto param = accessor.getUserParam(i);
        mUserParams[i].initialize(param, offsetMap);
    }
    for (u32 i = 0; i < mAssetParams.size(); ++i) {
        auto param = accessor.getAssetParam(i);
        mAssetParams[i].initialize(param, offsetMap);
    }
    for (u32 i = 0; i < mTriggerParams.size(); ++i) {
        auto param = accessor.getTriggerParam(i);
        mTriggerParams[i].initialize(param, offsetMap);
    }

    mInitialized = true;
    return mInitialized;
}

void ParamDefineTable::printParams() const {
    if (!mInitialized) return;

    std::cout << "User Params:\n";
    for (const auto& param : mUserParams) {
        param.print();
    }
    std::cout << "Asset Params:\n";
    for (const auto& param : mAssetParams) {
        param.print();
    }
    std::cout << "Trigger Params:\n";
    for (const auto& param : mTriggerParams) {
        param.print();
    }
}

const ParamDefine& ParamDefineTable::getUserParam(size_t index) const {
    return mUserParams[index];
}
ParamDefine& ParamDefineTable::getUserParam(size_t index) {
    return mUserParams[index];
}

const ParamDefine& ParamDefineTable::getCustomUserParam(size_t index) const {
    return mUserParams[mSystemUserParamCount + index];
}
ParamDefine& ParamDefineTable::getCustomUserParam(size_t index) {
    return mUserParams[mSystemUserParamCount + index];
}

const ParamDefine& ParamDefineTable::getAssetParam(size_t index) const {
    return mAssetParams[index];
}
ParamDefine& ParamDefineTable::getAssetParam(size_t index) {
    return mAssetParams[index];
}

const ParamDefine& ParamDefineTable::getUserAssetParam(size_t index) const {
    return mAssetParams[mSystemAssetParamCount + index];
}
ParamDefine& ParamDefineTable::getUserAssetParam(size_t index) {
    return mAssetParams[mSystemAssetParamCount + index];
}

const ParamDefine& ParamDefineTable::getTriggerParam(size_t index) const {
    return mTriggerParams[index];
}
ParamDefine& ParamDefineTable::getTriggerParam(size_t index) {
    return mTriggerParams[index];
}

const ParamDefine& ParamDefineTable::getParam(size_t index, ParamType type) const {
    return (type == ParamType::USER) ? mUserParams[index] : (type == ParamType::ASSET ? mAssetParams[index] : mTriggerParams[index]);
}
ParamDefine& ParamDefineTable::getParam(size_t index, ParamType type) {
    return (type == ParamType::USER) ? mUserParams[index] : (type == ParamType::ASSET ? mAssetParams[index] : mTriggerParams[index]);
}

s32 ParamDefineTable::searchParamIndex(const std::string_view name, ParamType type) const {
    switch (type) {
        case ParamType::USER: {
            for (s32 i = 0; const auto& define : mUserParams) {
                if (define.getName() == name) {
                    return i;
                }
                ++i;
            }
            return -1;
        }
        case ParamType::ASSET: {
            for (s32 i = 0; const auto& define : mAssetParams) {
                if (define.getName() == name) {
                    return i;
                }
                ++i;
            }
            return -1;
        }
        case ParamType::TRIGGER: {
            for (s32 i = 0; const auto& define : mTriggerParams) {
                if (define.getName() == name) {
                    return i;
                }
                ++i;
            }
            return -1;
        }
        default:
            return -1;

    }
}

void ParamDefine::dumpYAML(LibyamlEmitterWithStorage<std::string>& emitter) const {
    emitter.EmitString(mName);

    switch (mType) {
        case xlink2::ParamType::Int:
            emitter.EmitInt(std::get<int>(mDefaultValue));
            break;
        case xlink2::ParamType::Float:
            emitter.EmitFloat(std::get<float>(mDefaultValue));
            break;
        case xlink2::ParamType::Bool:
            emitter.EmitBool(std::get<bool>(mDefaultValue));
            break;
        case xlink2::ParamType::Enum:
            emitter.EmitScalar(std::format("{:#010x}", std::get<u32>(mDefaultValue)), false, false, "!u");
            break;
        case xlink2::ParamType::String:
            emitter.EmitString(std::get<std::string_view>(mDefaultValue));
            break;
        case xlink2::ParamType::Bitfield:
            emitter.EmitScalar(std::format("{:#x}", std::get<u32>(mDefaultValue)), false, false, "!bitfield");
            break;
        default:
            throw InvalidDataError("Invalid param define type");
    }
}

void ParamDefineTable::dumpYAML(LibyamlEmitterWithStorage<std::string>& emitter, bool exportStrings) const {
    emitter.EmitString("ParamDefineTable");

    LibyamlEmitter::MappingScope scope{emitter, "!pdt", YAML_BLOCK_MAPPING_STYLE};
    emitter.EmitString("SystemUserParamCount");
    emitter.EmitInt(mSystemUserParamCount);
    emitter.EmitString("SystemAssetParamCount");
    emitter.EmitInt(mSystemAssetParamCount);
    {
        emitter.EmitString("UserParamDefines");
        LibyamlEmitter::MappingScope paramScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
        for (const auto& define : mUserParams) {
            define.dumpYAML(emitter);
        }
    }
    {
        emitter.EmitString("AssetParamDefines");
        LibyamlEmitter::MappingScope paramScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
        for (const auto& define : mAssetParams) {
            define.dumpYAML(emitter);
        }
    }
    {
        emitter.EmitString("TriggerParamDefines");
        LibyamlEmitter::MappingScope paramScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
        for (const auto& define : mTriggerParams) {
            define.dumpYAML(emitter);
        }
    }
    if (exportStrings) {
        emitter.EmitString("Strings");
        LibyamlEmitter::SequenceScope paramScope{emitter, {}, YAML_BLOCK_SEQUENCE_STYLE};
        for (const auto& str : mStrings) {
            emitter.EmitString(str);
        }
    }
}

void ParamDefine::loadYAML(const ryml::ConstNodeRef& node, const std::string_view&& name, ParamDefineTable& pdt) {
    mName = std::move(name);

    if (node.has_val_tag()) {
        if (node.val_tag() == "!u") {
            mType = xlink2::ParamType::Enum;
            mDefaultValue = static_cast<u32>(*ParseScalarAs<u64>(node));
            return;
        } else if (node.val_tag() == "!bitfield") {
            mType = xlink2::ParamType::Bitfield;
            mDefaultValue = static_cast<u32>(*ParseScalarAs<u64>(node));
            return;
        } else {
            throw ResourceError("Unknown node tag!");
        }
    }

    const auto value = ParseScalar(node);
    if (const u64* asInt = std::get_if<u64>(&value)) {
        mType = xlink2::ParamType::Int;
        mDefaultValue = static_cast<s32>(*asInt);
        return;
    }
    if (const f64* asFloat = std::get_if<f64>(&value)) {
        mType = xlink2::ParamType::Float;
        mDefaultValue = static_cast<f32>(*asFloat);
        return;
    }
    if (const bool* asBool = std::get_if<bool>(&value)) {
        mType = xlink2::ParamType::Bool;
        mDefaultValue = *asBool;
        return;
    }

    mType = xlink2::ParamType::String;
    mDefaultValue = pdt.addString(std::get<std::string>(value));
}

bool ParamDefineTable::loadYAML(const ryml::ConstNodeRef& node) {
    mSystemUserParamCount = static_cast<s32>(*ParseScalarAs<u64>(RymlGetMapItem(node, "SystemUserParamCount")));
    mSystemAssetParamCount = static_cast<s32>(*ParseScalarAs<u64>(RymlGetMapItem(node, "SystemAssetParamCount")));

    const auto strings = node.find_child("Strings");
    if (!strings.invalid() && strings.is_seq()) {
        for (const auto& child : strings) {
            mStrings.insert({child.val().data(), child.val().size()});
        }
    }

    const auto userParams = node.find_child("UserParamDefines");
    if (userParams.invalid() || !userParams.is_map()) {
        std::cout << "Failed to find UserParamDefines\n";
        return false;
    }

    mUserParams.resize(userParams.num_children());

    for (u32 i = 0; const auto& param : userParams) {
        auto [it, success] = mStrings.insert({param.key().data(), param.key().size()});
        mUserParams[i].loadYAML(param, *it, *this);
        ++i;
    }

    const auto assetParams = node.find_child("AssetParamDefines");
    if (assetParams.invalid() || !assetParams.is_map()) {
        std::cout << "Failed to find AssetParamDefines\n";
        return false;
    }

    mAssetParams.resize(assetParams.num_children());

    for (u32 i = 0; const auto& param : assetParams) {
        auto [it, success] = mStrings.insert({param.key().data(), param.key().size()});
        mAssetParams[i].loadYAML(param, *it, *this);
        ++i;
    }

    const auto triggerParams = node.find_child("TriggerParamDefines");
    if (triggerParams.invalid() || !triggerParams.is_map()) {
        std::cout << "Failed to find TriggerParamDefines\n";
        return false;
    }

    mTriggerParams.resize(triggerParams.num_children());

    for (u32 i = 0; const auto& param : triggerParams) {
        auto [it, success] = mStrings.insert({param.key().data(), param.key().size()});
        mTriggerParams[i].loadYAML(param, *it, *this);
        ++i;
    }

    mInitialized = true;

    return mInitialized;
}

} // namespace banana