#include "accessor.h"
#include "util/common.h"

#ifndef NDEBUG
#include <format>
#include <iostream>
#endif

namespace banana {

bool ResourceAccessor::load(void* data) {
    mHeader = reinterpret_cast<xlink2::ResourceHeader*>(data);
    if (mHeader == nullptr || mHeader->magic != xlink2::cResourceMagic || mHeader->fileSize < 0x60) {
        mHeader = nullptr;
        return false;
    }

    mUserHashArray = reinterpret_cast<u32*>(reinterpret_cast<uintptr_t>(data) + sizeof(xlink2::ResourceHeader));
    mUserOffsetArray = reinterpret_cast<TargetPointer*>(
        util::align(reinterpret_cast<uintptr_t>(mUserHashArray + mHeader->numUsers), sizeof(TargetPointer))
    );

    mParamDefineTable = reinterpret_cast<xlink2::ResParamDefineTableHeader*>(
        util::align(reinterpret_cast<uintptr_t>(mUserOffsetArray + mHeader->numUsers), sizeof(TargetPointer))
    );
    mUserParams = reinterpret_cast<xlink2::ResParamDefine*>(mParamDefineTable + 1); // (uintptr_t)pdt + sizeof(pdt)
    mAssetParams = mUserParams + mParamDefineTable->numUserParams;
    mTriggerParams = mAssetParams + mParamDefineTable->numAssetParams;
    mPdtNameTable = reinterpret_cast<char*>(mTriggerParams + mParamDefineTable->numTriggerParams);

    mTriggerOverwriteParamTable = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(data) + mHeader->triggerOverwriteTablePos);

    mLocalPropertyNameRefTable = reinterpret_cast<TargetPointer*>(reinterpret_cast<uintptr_t>(data) + mHeader->localPropertyNameRefTablePos);
    mLocalPropertyEnumNameRefTable = mLocalPropertyNameRefTable + mHeader->numLocalPropertyNameRefs;

    mExRegion = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(data) + mHeader->exRegionPos);
    mNameTable = reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(data) + mHeader->nameTablePos);
    mDirectValueTable = reinterpret_cast<void*>(mLocalPropertyEnumNameRefTable + mHeader->numLocalPropertyEnumNameRefs);

    mRandomCallTable = reinterpret_cast<xlink2::ResRandomCallTable*>(reinterpret_cast<uintptr_t>(mDirectValueTable) + sizeof(u32) * mHeader->numDirectValues);
    mCurveCallTable = reinterpret_cast<xlink2::ResCurveCallTable*>(mRandomCallTable + mHeader->numRandom);
    mCurvePointTable = reinterpret_cast<xlink2::ResCurvePoint*>(mCurveCallTable + mHeader->numCurves);

    mConditionTable = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(data) + mHeader->conditionTablePos);

#ifndef NDEBUG
    std::cout << std::format("Magic: 0x{:08x}, Version: {}\n", mHeader->magic, mHeader->version);
    std::cout << std::format("User Count: {:d}\n", mHeader->numUsers);
#endif

    mLoaded = true;
    return mLoaded;
}

} // namespace banana