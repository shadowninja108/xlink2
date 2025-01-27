#pragma once

#include "resource.h"
#include "accessor.h"
#include "pdt.h"
#include "user.h"
#include "value.h"
#include "condition.h"
#include "arrange.h"

#include "util/yaml.h"

// this is not xlink2::System so we're clear

namespace banana {

class Serializer;

class System {
public:
    System() = default;

    bool initialize(void* data, size_t size);

    const ParamDefineTable& getPDT() const {
        return mPDT;
    }

    const Curve& getCurve(s32) const;
    Curve& getCurve(s32);

    const Random& getRandomCall(s32) const;
    Random& getRandomCall(s32);

    const ArrangeGroupParams& getArrangeGroupParams(s32) const;
    ArrangeGroupParams& getArrangeGroupParams(s32);

    const ParamSet& getTriggerOverwriteParam(s32) const;
    ParamSet& getTriggerOverwriteParam(s32);

    const ParamSet& getAssetParam(s32) const;
    ParamSet& getAssetParam(s32);

    const User& getUser(u32) const;
    User& getUser(u32);
    const User& getUser(const std::string_view&) const;
    User& getUser(const std::string_view&);

    const Condition& getCondition(s32) const;
    Condition& getCondition(s32);

    u32 getDirectValueU32(s32) const;
    s32 getDirectValueS32(s32) const;
    f32 getDirectValueF32(s32) const;

    bool searchUser(const std::string_view&) const;
    bool searchUser(u32) const;

    void printUser(const std::string_view) const;
    void printParam(const Param&, ParamType) const;

    bool addAssetCall(User&, const AssetCallTable&);
    bool addAssetCall(User&, const std::string_view&, bool, s32, s32 conditionIdx = -1);
    bool addAssetCall(User&, const std::string_view&, const ParamSet&, s32 conditionIdx = -1);
    bool addAssetCall(User&, const std::string_view&, const Container&, s32 conditionIdx = -1);

    s32 searchParamIndex(const std::string_view&, ParamType) const;

    std::vector<u8> serialize();

    std::string dumpYAML() const;
    std::string dumpUser(u32) const;

    friend class Serializer;

private:
    inline void dumpParam(LibyamlEmitterWithStorage<std::string>&, const Param&, ParamType) const;
    inline void dumpParamSet(LibyamlEmitterWithStorage<std::string>&, const ParamSet&, ParamType) const;
    inline void dumpCondition(LibyamlEmitterWithStorage<std::string>&, const Condition&) const;
    inline void dumpContainer(LibyamlEmitterWithStorage<std::string>&, const Container&) const;
    inline void dumpAssetCallTable(LibyamlEmitterWithStorage<std::string>&, const AssetCallTable&) const;
    inline void dumpActionSlot(LibyamlEmitterWithStorage<std::string>&, const ActionSlot&) const;
    inline void dumpAction(LibyamlEmitterWithStorage<std::string>&, const Action&) const;
    inline void dumpActionTrigger(LibyamlEmitterWithStorage<std::string>&, const ActionTrigger&) const;
    inline void dumpProperty(LibyamlEmitterWithStorage<std::string>&, const Property&) const;
    inline void dumpPropertyTrigger(LibyamlEmitterWithStorage<std::string>&, const PropertyTrigger&) const;
    inline void dumpAlwaysTrigger(LibyamlEmitterWithStorage<std::string>&, const AlwaysTrigger&) const;
    inline void dumpUser(LibyamlEmitterWithStorage<std::string>&, const User&) const;

    ParamDefineTable mPDT;
    std::set<std::string> mStrings;
    std::vector<std::string_view> mLocalProperties;
    std::vector<std::string_view> mLocalPropertyEnumStrings;
    std::vector<Curve> mCurves;
    std::vector<Random> mRandomCalls;
    std::vector<u32> mDirectValues;
    std::vector<ParamSet> mTriggerOverwriteParams;
    std::vector<ParamSet> mAssetParams;
    std::map<u32, User> mUsers;
    std::vector<Condition> mConditions;
    std::vector<ArrangeGroupParams> mArrangeGroupParams;
    u32 mVersion;
};

} // namespace banana