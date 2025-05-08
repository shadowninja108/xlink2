#pragma once

#include "resource.h"

namespace banana {

class ResourceAccessor {
public:
    ResourceAccessor() = default;

    bool load(void* res);

    bool isLoaded() const {
        return mLoaded;
    }

    bool isELink() const {
        return mLoaded && mHeader->version == sELinkResourceVersion;
    }
    bool isSLink() const {
        return mLoaded && mHeader->version == sSLinkResourceVersion;
    }

    const xlink2::ResourceHeader* getResourceHeader() const {
        return mHeader;
    }

    u32 getUserHash(size_t index) const {
        if (index >= static_cast<size_t>(mHeader->numUsers)) {
            return 0;
        }
        return mUserHashArray[index];
    }

    const xlink2::ResUserHeader* getResUserHeader(size_t index) const {
        if (index >= static_cast<size_t>(mHeader->numUsers)) {
            return nullptr;
        }
        return reinterpret_cast<xlink2::ResUserHeader*>(reinterpret_cast<uintptr_t>(mHeader) + mUserOffsetArray[index]);
    }

    const xlink2::ResParamDefineTableHeader* getParamDefineTable() const {
        return mParamDefineTable;
    }

    const xlink2::ResParamDefine* getUserParam(size_t index) const {
        if (index >= static_cast<size_t>(mParamDefineTable->numUserParams)) {
            return nullptr;
        }
        return mUserParams + index;
    }
    const xlink2::ResParamDefine* getAssetParam(size_t index) const {
        if (index >= static_cast<size_t>(mParamDefineTable->numAssetParams)) {
            return nullptr;
        }
        return mAssetParams + index;
    }
    const xlink2::ResParamDefine* getTriggerParam(size_t index) const {
        if (index >= static_cast<size_t>(mParamDefineTable->numTriggerParams)) {
            return nullptr;
        }
        return mTriggerParams + index;
    }

    const char* getParamDefineName(TargetPointer offset) const {
        return mPdtNameTable + offset;
    }

    TargetPointer getLocalPropertyOffset(size_t index) const {
        if (index >= static_cast<size_t>(mHeader->numLocalPropertyNameRefs)) {
            return 0;
        }
        return mLocalPropertyNameRefTable[index];
    }
    TargetPointer getLocalPropertyEnumOffset(size_t index) const {
        if (index >= static_cast<size_t>(mHeader->numLocalPropertyEnumNameRefs)) {
            return 0;
        }
        return mLocalPropertyEnumNameRefTable[index]; 
    }

    const xlink2::ResTriggerOverwriteParam* getTriggerOverwriteParam(TargetPointer offset) const {
        return reinterpret_cast<const xlink2::ResTriggerOverwriteParam*>(reinterpret_cast<uintptr_t>(mTriggerOverwriteParamTable) + offset);
    }

    const xlink2::ResRandomCallTable* getRandomCall(size_t index) const {
        if (index >= static_cast<size_t>(mHeader->numRandom)) {
            return nullptr;
        }
        return mRandomCallTable + index;
    }

    const xlink2::ResCurveCallTable* getCurve(size_t index) const {
        if (index >= static_cast<size_t>(mHeader->numCurves)) {
            return nullptr;
        }
        return mCurveCallTable + index;
    }
    const xlink2::ResCurvePoint* getCurvePoint(size_t index) const {
        if (index >= static_cast<size_t>(mHeader->numCurvePoints)) {
            return nullptr;
        }
        return mCurvePointTable + index;
    }

    const char* getString(TargetPointer offset) const {
        return mNameTable + offset;
    }

    const xlink2::ResCondition* getCondition(TargetPointer offset) const {
        return reinterpret_cast<const xlink2::ResCondition*>(reinterpret_cast<uintptr_t>(mConditionTable) + offset);
    }

    const void* getAssetParamTable() const {
        return reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(mParamDefineTable) + util::align(static_cast<uintptr_t>(mParamDefineTable->size), sizeof(uintptr_t)));
    }

    s32 getDirectValueS32(size_t index) const {
        if (index >= static_cast<size_t>(mHeader->numDirectValues)) {
            return 0;
        }
        return reinterpret_cast<s32*>(mDirectValueTable)[index];
    }
    u32 getDirectValueU32(size_t index) const {
        if (index >= static_cast<size_t>(mHeader->numDirectValues)) {
            return 0;
        }
        return reinterpret_cast<u32*>(mDirectValueTable)[index];
    }
    f32 getDirectValueF32(size_t index) const {
        if (index >= static_cast<size_t>(mHeader->numDirectValues)) {
            return 0.f;
        }
        return reinterpret_cast<f32*>(mDirectValueTable)[index];
    }

    uintptr_t getExRegion() const {
        return reinterpret_cast<uintptr_t>(mExRegion);
    }

#if XLINK_TARGET_IS_TOTK
    static constexpr u32 sELinkResourceVersion = 0x24;
    static constexpr u32 sSLinkResourceVersion = 0x21;
#elif XLINK_TARGET_IS_THUNDER
    static constexpr u32 sELinkResourceVersion = 0x22;
    static constexpr u32 sSLinkResourceVersion = 0x1f;
#elif XLINK_TARGET_IS_BLITZ
    static constexpr u32 sELinkResourceVersion = 0x1e;
    static constexpr u32 sSLinkResourceVersion = 0x1c;
#else
    static_assert(false, "Invalid XLINK_TARGET!");
#endif

private:
    xlink2::ResourceHeader* mHeader;
    // Users
    u32* mUserHashArray;
    TargetPointer* mUserOffsetArray;
    // ParamDefineTable
    xlink2::ResParamDefineTableHeader* mParamDefineTable;
    xlink2::ResParamDefine* mUserParams;
    xlink2::ResParamDefine* mAssetParams;
    xlink2::ResParamDefine* mTriggerParams;
    char* mPdtNameTable;
    // LocalPropertyTable
    TargetPointer* mLocalPropertyNameRefTable;
    TargetPointer* mLocalPropertyEnumNameRefTable;
    // TriggerOverwriteParamTable
    void /*xlink2::ResTriggerOverwriteParam*/ * mTriggerOverwriteParamTable;
    // Special Value Sources
    xlink2::ResRandomCallTable* mRandomCallTable;
    xlink2::ResCurveCallTable* mCurveCallTable;
    xlink2::ResCurvePoint* mCurvePointTable;
    // Conditions
    void* mConditionTable;
    // Common
    void* mExRegion;
    char* mNameTable;
    void* mDirectValueTable;
    // Internal
    bool mLoaded = false;
};

} // namespace banana