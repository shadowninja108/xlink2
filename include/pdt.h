#pragma once

#include "resource.h"
#include "accessor.h"
#include "util/yaml.h"

#include <set>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace banana {

class Serializer;

enum class ParamType {
    USER, ASSET, TRIGGER,
};

class ParamDefineTable;

class ParamDefine {
public:
    ParamDefine() = default;

    void initialize(const xlink2::ResParamDefine* param, const std::unordered_map<u64, std::string_view>& strings);

    const std::string_view& getName() const {
        return mName;
    }

    xlink2::ParamType getType() const {
        return mType;
    }

    template <xlink2::ParamType T>
    const auto& getValue() const {
        if constexpr (T == xlink2::ParamType::Int) {
            return std::get<s32>(mDefaultValue);
        }
        if constexpr (T == xlink2::ParamType::Float) {
            return std::get<f32>(mDefaultValue);
        }
        if constexpr (T == xlink2::ParamType::Bool) {
            return std::get<bool>(mDefaultValue);
        }
        if constexpr (T == xlink2::ParamType::Enum) {
            return std::get<u32>(mDefaultValue);
        }
        if constexpr (T == xlink2::ParamType::String) {
            return std::get<std::string_view>(mDefaultValue);
        }
        if constexpr (T == xlink2::ParamType::Bitfield) {
            return std::get<u32>(mDefaultValue);
        }
    }

    void print() const;

    void dumpYAML(LibyamlEmitterWithStorage<std::string>&) const;
    void loadYAML(const ryml::ConstNodeRef&, const std::string_view&&, ParamDefineTable&);

    friend class Serializer;

private:
    std::string_view mName;
    xlink2::ParamType mType;
    std::variant<s32, f32, bool, u32, std::string_view> mDefaultValue;
};

class ParamDefineTable {
public:
    ParamDefineTable() = default;

    bool initialize(const ResourceAccessor& accessor);

    static constexpr s32 sSystemELinkUserParamCount = 0;
    static constexpr s32 sSystemSLinkUserParamCount = 8;

    const ParamDefine& getUserParam(s32) const;
    ParamDefine& getUserParam(s32);

    const ParamDefine& getCustomUserParam(s32) const;
    ParamDefine& getCustomUserParam(s32);

    const ParamDefine& getAssetParam(s32) const;
    ParamDefine& getAssetParam(s32);

    const ParamDefine& getUserAssetParam(s32) const;
    ParamDefine& getUserAssetParam(s32);

    const ParamDefine& getTriggerParam(s32) const;
    ParamDefine& getTriggerParam(s32);

    const ParamDefine& getParam(s32 index, ParamType type) const;
    ParamDefine& getParam(s32 index, ParamType type);

    void printParams() const;

    u32 getUserParamCount() const {
        return mUserParams.size();
    }

    u32 getAssetParamCount() const {
        return mAssetParams.size();
    }

    u32 getTriggerParamCount() const {
        return mTriggerParams.size();
    }

    s32 searchParamIndex(const std::string_view, ParamType) const;

    void dumpYAML(LibyamlEmitterWithStorage<std::string>&, bool exportStrings = false) const;
    bool loadYAML(const ryml::ConstNodeRef&);

    const std::string_view addString(const std::string s) {
        return std::move(*mStrings.insert(std::move(s)).first);
    }

    friend class Serializer;

private:
    std::vector<ParamDefine> mUserParams{};
    std::vector<ParamDefine> mAssetParams{};
    std::vector<ParamDefine> mTriggerParams{};
    std::set<std::string> mStrings{};
    s32 mSystemUserParamCount = 0;
    s32 mSystemAssetParamCount = 0;
    bool mInitialized = false;
};

} // namespace banana