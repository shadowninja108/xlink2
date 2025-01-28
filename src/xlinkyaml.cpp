#include "system.h"
#include "util/error.h"
#include "util/crc32.h"

#include "usernames.inc"

#include <bit>
#include <iostream>
#include <format>
#include <variant>

namespace banana {

void System::dumpCurve(LibyamlEmitterWithStorage<std::string>& emitter, const Curve& curve) const {
    LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
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
}

void System::dumpRandom(LibyamlEmitterWithStorage<std::string>& emitter, const Random& random) const {
    LibyamlEmitter::MappingScope scope{emitter, {}, YAML_FLOW_MAPPING_STYLE};
    emitter.EmitString("Min");
    emitter.EmitFloat(random.min);
    emitter.EmitString("Max");
    emitter.EmitFloat(random.max);
}

void System::dumpArrangeGroupParam(LibyamlEmitterWithStorage<std::string>& emitter, const ArrangeGroupParams& groups) const {
    LibyamlEmitter::SequenceScope seqScope{emitter, {}, YAML_BLOCK_SEQUENCE_STYLE};
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
            throw InvalidDataError(std::format("Invalid parameter type {:#x}", static_cast<u32>(type)));
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
                    throw InvalidDataError(std::format("Invalid param type {:#x}", static_cast<u32>(param.type)));
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
            emitter.EmitInt(std::get<u32>(param.value), "!curve");
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
            LibyamlEmitter::MappingScope mapScope{emitter, "!random", YAML_FLOW_MAPPING_STYLE};
            emitter.EmitString("Type");
            emitter.EmitScalar(std::format("{:#010x}", static_cast<u32>(param.type)), false, false, "!u");
            emitter.EmitString("Index");
            emitter.EmitInt(std::get<u32>(param.value));
            break;
        }
        case RefType::ArrangeParam: {
            if (paramType != ValType::Bitfield)
                throw InvalidDataError("ArrangeParam needs to be a bitfield!");
            emitter.EmitInt(std::get<u32>(param.value), "!arrangeGroupParam");
            break;
        }
        case RefType::Bitfield: { // should this just be called immediate? seems to be used just for ints?
            if (paramType != ValType::Int)
                throw InvalidDataError(std::format("Bitfields need to be ints! {:d}", static_cast<u32>(paramType)));
            emitter.EmitScalar(std::format("{:#x}", std::get<u32>(param.value)), false, false, "!bitfield");
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
            LibyamlEmitter::MappingScope scope{emitter, "!switch", YAML_BLOCK_MAPPING_STYLE};
            const auto cond = condition.getAs<Type::Switch>();
            emitter.EmitString("CompareType");
            emitter.EmitString(sCompareTypeStrings[static_cast<u32>(cond->compareType)]);
            emitter.EmitString("IsGlobal");
            emitter.EmitBool(cond->isGlobal);
            emitter.EmitString("Value1"); // action hash or enum value/index
            emitter.EmitScalar(std::format("{:#010x}", cond->actionHash), false, false, "!u");
            emitter.EmitString("Value2");
            switch (cond->propType) {
                case xlink2::PropertyType::S32:
                    emitter.EmitInt(cond->conditionValue.i);
                    break;
                case xlink2::PropertyType::_04:
                    emitter.EmitInt(cond->conditionValue.i, "!i4");
                    break;
                case xlink2::PropertyType::F32:
                    emitter.EmitFloat(cond->conditionValue.f);
                    break;
                case xlink2::PropertyType::_05:
                    emitter.EmitFloat(cond->conditionValue.f, "!f5");
                    break;
                case xlink2::PropertyType::Bool:
                    emitter.EmitBool(cond->conditionValue.b);
                    break;
                case xlink2::PropertyType::Enum: {
                    emitter.EmitScalar(std::format("{:#010x}", std::bit_cast<u32, s32>(cond->conditionValue.i)), false, false, "!u");
                    emitter.EmitString("EnumName");
                    emitter.EmitString(cond->enumName);
                    break;
                }
                default:
                    throw InvalidDataError(std::format("Invalid switch case property type {:#x}", static_cast<u32>(condition.parentContainerType)));
            }
            break;
        }
        case Type::Random: {
            LibyamlEmitter::MappingScope scope{emitter, "!random", YAML_BLOCK_MAPPING_STYLE};
            const auto cond = condition.getAs<Type::Random2>();
            emitter.EmitString("Weight");
            emitter.EmitFloat(cond->weight);
            break;
        }
        case Type::Random2: {
            LibyamlEmitter::MappingScope scope{emitter, "!random2", YAML_BLOCK_MAPPING_STYLE};
            const auto cond = condition.getAs<Type::Random2>();
            emitter.EmitString("Weight");
            emitter.EmitFloat(cond->weight);
            break;
        }
        case Type::Blend: {
            LibyamlEmitter::MappingScope scope{emitter, "!blend", YAML_BLOCK_MAPPING_STYLE};
            const auto cond = condition.getAs<Type::Blend>();
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
            LibyamlEmitter::MappingScope scope{emitter, "!sequence", YAML_BLOCK_MAPPING_STYLE};
            const auto cond = condition.getAs<Type::Sequence>();
            emitter.EmitString("ContinueOnFade");
            emitter.EmitInt(cond->continueOnFade);
            break;
        }
        case Type::Grid: {
            LibyamlEmitter::MappingScope scope{emitter, "!grid", YAML_BLOCK_MAPPING_STYLE};
            break;
        }
        case Type::Jump: {
            LibyamlEmitter::MappingScope scope{emitter, "!jump", YAML_BLOCK_MAPPING_STYLE};
            break;
        }
        default:
            throw InvalidDataError(std::format("Invalid condition type! {:#x}", static_cast<u32>(condition.parentContainerType)));
    }
}

void System::dumpContainer(LibyamlEmitterWithStorage<std::string>& emitter, const Container& container) const {
    using Type = xlink2::ContainerType;
    switch (container.type) {
        case Type::Switch: {
            LibyamlEmitter::MappingScope scope{emitter, "!switch", YAML_BLOCK_MAPPING_STYLE};
            const auto param = container.getAs<Type::Switch>();
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
            emitter.EmitString("ChildContainerBaseIndex");
            emitter.EmitInt(container.childContainerStartIdx);
            emitter.EmitString("ChildContainerCount");
            emitter.EmitInt(container.childCount);
            emitter.EmitString("IsNeedObserve");
            emitter.EmitBool(container.isNeedObserve);
            break;
        }
        case Type::Random: {
            LibyamlEmitter::MappingScope scope{emitter, "!random", YAML_BLOCK_MAPPING_STYLE};
            emitter.EmitString("ChildContainerBaseIndex");
            emitter.EmitInt(container.childContainerStartIdx);
            emitter.EmitString("ChildContainerCount");
            emitter.EmitInt(container.childCount);
            emitter.EmitString("IsNeedObserve");
            emitter.EmitBool(container.isNeedObserve);
            break;
        }
        case Type::Random2: {
            LibyamlEmitter::MappingScope scope{emitter, "!random2", YAML_BLOCK_MAPPING_STYLE};
            emitter.EmitString("ChildContainerBaseIndex");
            emitter.EmitInt(container.childContainerStartIdx);
            emitter.EmitString("ChildContainerCount");
            emitter.EmitInt(container.childCount);
            emitter.EmitString("IsNeedObserve");
            emitter.EmitBool(container.isNeedObserve);
            break;
        }
        case Type::Blend: {
            LibyamlEmitter::MappingScope scope{emitter, "!blend", YAML_BLOCK_MAPPING_STYLE};
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
            emitter.EmitString("ChildContainerBaseIndex");
            emitter.EmitInt(container.childContainerStartIdx);
            emitter.EmitString("ChildContainerCount");
            emitter.EmitInt(container.childCount);
            emitter.EmitString("IsNeedObserve");
            emitter.EmitBool(container.isNeedObserve);
            break;
        }
        case Type::Sequence: {
            LibyamlEmitter::MappingScope scope{emitter, "!sequence", YAML_BLOCK_MAPPING_STYLE};
            emitter.EmitString("ChildContainerBaseIndex");
            emitter.EmitInt(container.childContainerStartIdx);
            emitter.EmitString("ChildContainerCount");
            emitter.EmitInt(container.childCount);
            emitter.EmitString("IsNeedObserve");
            emitter.EmitBool(container.isNeedObserve);
            break;
        }
        case Type::Grid: {LibyamlEmitter::MappingScope scope{emitter, "!grid", YAML_BLOCK_MAPPING_STYLE};
            const auto param = container.getAs<Type::Grid>();;
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
            emitter.EmitString("ChildContainerBaseIndex");
            emitter.EmitInt(container.childContainerStartIdx);
            emitter.EmitString("ChildContainerCount");
            emitter.EmitInt(container.childCount);
            emitter.EmitString("IsNeedObserve");
            emitter.EmitBool(container.isNeedObserve);
            break;
        }
        case Type::Jump: {
            LibyamlEmitter::MappingScope scope{emitter, "!jump", YAML_BLOCK_MAPPING_STYLE};
            emitter.EmitString("ChildContainerBaseIndex");
            emitter.EmitInt(container.childContainerStartIdx);
            emitter.EmitString("ChildContainerCount");
            emitter.EmitInt(container.childCount);
            emitter.EmitString("IsNeedObserve");
            emitter.EmitBool(container.isNeedObserve);
            break;
        }
        default:
            throw InvalidDataError(std::format("Invalid condition type! {:#x}", static_cast<u32>(container.type)));
    }
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
    emitter.EmitString("AssetCallTableIndex");
    emitter.EmitInt(trigger.assetCallIdx);
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
        for (const auto& prop : user.mLocalProperties) {
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

std::string System::dumpYAML(bool exportStrings) const {
    LibyamlEmitterWithStorage<std::string> emitter{};
    yaml_event_t event;

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    emitter.Emit(event);

    yaml_document_start_event_initialize(&event, nullptr, nullptr, nullptr, 1);
    emitter.Emit(event);

    {
        LibyamlEmitter::MappingScope scope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};

        emitter.EmitString("Version");
        emitter.EmitInt(mVersion);

        mPDT.dumpYAML(emitter, exportStrings);
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
            emitter.EmitString("Curves");
            LibyamlEmitter::MappingScope mapScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
            for (u32 i = 0; const auto& curve : mCurves) {
                emitter.EmitInt(i);
                dumpCurve(emitter, curve);
                ++i;
            }
        }
        {
            emitter.EmitString("RandomTable");
            LibyamlEmitter::MappingScope mapScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
            for (u32 i = 0; const auto& rand : mRandomCalls) {
                emitter.EmitInt(i);
                dumpRandom(emitter, rand);
                ++i;
            }
        }
        {
            emitter.EmitString("ArrangeGroupParams");
            LibyamlEmitter::MappingScope mapScope{emitter, {}, YAML_BLOCK_MAPPING_STYLE};
            for (u32 i = 0; const auto& group : mArrangeGroupParams) {
                emitter.EmitInt(i);
                dumpArrangeGroupParam(emitter, group);
                ++i;
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
                const auto res = mVersion == 0x24 ? sELinkUserNames.find(hash) : sSLinkUserNames.find(hash);
                if (res == (mVersion == 0x24 ? sELinkUserNames.end() : sSLinkUserNames.end())) {
                    emitter.EmitScalar(std::format("{:#010x}", hash), false, false, "!u");
                } else {
                    emitter.EmitString(res->second);
                }
                dumpUser(emitter, user);
            }
        }
        if (exportStrings) {
            emitter.EmitString("Strings");
            LibyamlEmitter::SequenceScope seqScope{emitter, {}, YAML_BLOCK_SEQUENCE_STYLE};
            for (const auto& string : mStrings) {
                emitter.EmitString(string);
            }
        }
    }

    yaml_document_end_event_initialize(&event, 1);
    emitter.Emit(event);

    yaml_stream_end_event_initialize(&event);
    emitter.Emit(event);

    return std::move(emitter.GetOutput());
}

template<typename T>
inline std::optional<T> FindParseScalar(const std::string_view key, const c4::yml::ConstNodeRef& node) {
    const auto child = node.find_child(StrViewToRymlSubstr(key));
    if (child.invalid())
        return std::nullopt;
    
    return ParseScalarAs<T>(child);
}

inline void ParseSequence(const c4::yml::ConstNodeRef& node, void* userdata, void(*callback)(void*, const c4::yml::ConstNodeRef&, u32)) {
    for (u32 i = 0; const auto& child : node) {
        callback(userdata, child, i); ++i;
    }
}

void System::loadCurve(Curve& curve, const c4::yml::ConstNodeRef& node) {
    curve.propertyName = addString(*FindParseScalar<std::string>("PropertyName", node));
    curve.propertyIndex = static_cast<s16>(*FindParseScalar<u64>("PropertyIndex", node));
    curve.isGlobal = *FindParseScalar<bool>("IsGlobal", node);
    curve.type = static_cast<u16>(*FindParseScalar<u64>("CurveType", node));
    curve.unk = static_cast<s32>(*FindParseScalar<u64>("Unknown1", node));
    curve.unk2 = static_cast<u16>(*FindParseScalar<u64>("Unknown2", node));
    const auto p = node.find_child("Points");
    curve.points.resize(p.num_children());
    ParseSequence(p, &curve.points, [](void* data, const c4::yml::ConstNodeRef& n, u32 index) -> void {
        auto points = reinterpret_cast<std::vector<CurvePoint>*>(data);
        (*points)[index].x = static_cast<f32>(*FindParseScalar<f64>("x", n));
        (*points)[index].y = static_cast<f32>(*FindParseScalar<f64>("y", n));
    });
}

void System::loadRandom(Random& rand, const c4::yml::ConstNodeRef& node) {
    rand.min = static_cast<f32>(*FindParseScalar<f64>("Min", node));
    rand.max = static_cast<f32>(*FindParseScalar<f64>("Max", node));
}

void System::loadArrangeGroupParams(ArrangeGroupParams& groups, const c4::yml::ConstNodeRef& node) {
    groups.groups.resize(node.num_children());
    using Arg = struct { System* sys; std::vector<ArrangeGroupParam>* groups; };
    Arg arg = { this, &groups.groups };
    ParseSequence(node, &arg, [](void* data, const c4::yml::ConstNodeRef& n, u32 index) -> void {
        auto arg = reinterpret_cast<Arg*>(data);
        (*arg->groups)[index].groupName = arg->sys->addString(*FindParseScalar<std::string>("GroupName", n));
        (*arg->groups)[index].limitType = static_cast<s8>(*FindParseScalar<u64>("LimitType", n));
        (*arg->groups)[index].limitThreshold = static_cast<s8>(*FindParseScalar<u64>("LimitThreshold", n));
        (*arg->groups)[index].unk = static_cast<u8>(*FindParseScalar<u64>("Unknown", n));
    });
}


void System::loadParam(Param& param, const c4::yml::ConstNodeRef& node, ParamType type, std::map<ValKey, s32>& valueMap) {
    param.index = mPDT.searchParamIndex(RymlSubstrToStrView(node.key()), type);
    const std::string_view tag = RymlGetValTag(node);
    const auto define = mPDT.getParam(param.index, type);
    if (tag.empty() || tag == "!u") {
        switch (define.getType()) {
            case xlink2::ParamType::Int: {
                param.type = xlink2::ValueReferenceType::Direct;
                const s32 val = static_cast<s32>(*ParseScalarAs<u64>(node));
                auto res = valueMap.find(ValKey{{.s = val}, {.e = xlink2::ParamType::Int}});
                if (res == valueMap.end()) {
                    mDirectValues.emplace_back(std::bit_cast<u32, s32>(val));
                    param.value = static_cast<u32>(mDirectValues.size() - 1);
                    valueMap.emplace(ValKey{{.s = val}, {.e = xlink2::ParamType::Int}}, static_cast<s32>(mDirectValues.size() - 1));
                } else {
                    param.value = static_cast<u32>(res->second);
                }
                return;
            }
            case xlink2::ParamType::Float: {
                param.type = xlink2::ValueReferenceType::Direct;
                const f32 val = static_cast<f32>(*ParseScalarAs<f64>(node));
                auto res = valueMap.find(ValKey{{.f = val}, {.e = xlink2::ParamType::Float}});
                if (res == valueMap.end()) {
                    mDirectValues.emplace_back(std::bit_cast<u32, f32>(val));
                    param.value = static_cast<u32>(mDirectValues.size() - 1);
                    valueMap.emplace(ValKey{{.f = val}, {.e = xlink2::ParamType::Float}}, static_cast<s32>(mDirectValues.size() - 1));
                } else {
                    param.value = static_cast<u32>(res->second);
                }
                return;
            }
            case xlink2::ParamType::Bool: {
                param.type = xlink2::ValueReferenceType::Direct;
                const bool val = *ParseScalarAs<bool>(node);
                auto res = valueMap.find(ValKey{{.b = val}, {.e = xlink2::ParamType::Bool}});
                if (res == valueMap.end()) {
                    mDirectValues.emplace_back(val ? 1 : 0);
                    param.value = static_cast<u32>(mDirectValues.size() - 1);
                    valueMap.emplace(ValKey{{.b = val}, {.e = xlink2::ParamType::Bool}}, static_cast<s32>(mDirectValues.size() - 1));
                } else {
                    param.value = static_cast<u32>(res->second);
                }
                return;
            }
            case xlink2::ParamType::Enum: {
                param.type = xlink2::ValueReferenceType::Direct;
                const u32 val = static_cast<u32>(*ParseScalarAs<u64>(node));
                auto res = valueMap.find(ValKey{{.u = val}, {.e = xlink2::ParamType::Enum}});
                if (res == valueMap.end()) {
                    mDirectValues.emplace_back(val);
                    param.value = static_cast<u32>(mDirectValues.size() - 1);
                    valueMap.emplace(ValKey{{.u = val}, {.e = xlink2::ParamType::Enum}}, static_cast<s32>(mDirectValues.size() - 1));
                } else {
                    param.value = static_cast<u32>(res->second);
                }
                return;
            }
            case xlink2::ParamType::String: {
                param.type = xlink2::ValueReferenceType::String;
                param.value = addString(*ParseScalarAs<std::string>(node));
                return;
            }
            default:
                throw ParseError(std::format("Invalid param type {:#x}", static_cast<u32>(define.getType())));
        }
    }
    else if (tag == "!curve") {
        if (define.getType() != xlink2::ParamType::Float)
            throw ParseError("Curves must be floats!");
        param.type = xlink2::ValueReferenceType::Curve;
        param.value = static_cast<u32>(*ParseScalarAs<u64>(node));
        return;
    } else if (tag == "!random") {
        if (define.getType() != xlink2::ParamType::Float)
            throw ParseError("Random calls must be floats!");
        param.type = static_cast<xlink2::ValueReferenceType>(*FindParseScalar<u64>("Type", node));
        param.value = static_cast<u32>(*FindParseScalar<u64>("Index", node));
        return;
    } else if (tag == "!bitfield") {
        if (define.getType() != xlink2::ParamType::Int)
            throw ParseError("!bitfield must use Int!");
        param.type = xlink2::ValueReferenceType::Bitfield;
        param.value = static_cast<u32>(*ParseScalarAs<u64>(node));
        return;
    } else if (tag == "!arrangeGroupParam") {
        if (define.getType() != xlink2::ParamType::Bitfield)
            throw ParseError("ArrangeGroupParams must be bitfields!");
        param.type = xlink2::ValueReferenceType::ArrangeParam;
        param.value = static_cast<u32>(*ParseScalarAs<u64>(node));
        return;
    }
    
    throw ParseError(std::format("Invalid tag! {:s}", tag));
}

void System::loadParamSet(ParamSet& params, const c4::yml::ConstNodeRef& node, ParamType type, std::map<ValKey, s32>& valueMap) {
    params.params.resize(node.num_children());
    for (u32 i = 0; const auto& child : node) {
        loadParam(params.params[i], child, type, valueMap);
        ++i;
    }
}

void System::loadCondition(Condition& condition, const c4::yml::ConstNodeRef& node) {
    const std::string_view tag = RymlGetValTag(node);

    switch (util::calcCRC32(tag)) {
        case 0x2a8d07ed: { // !switch
            static constexpr std::array<std::string_view, 6> sCompareTypes = {
                "Equal", "GreaterThan", "GreaterThanOrEqual", "LessThan", "LessThanOrEqual", "NotEqual",  
            };
            condition.parentContainerType = xlink2::ContainerType::Switch;
            auto c = condition.getAs<xlink2::ContainerType::Switch>();
            c->compareType = util::matchEnum<xlink2::CompareType>(*FindParseScalar<std::string>("CompareType", node), sCompareTypes);
            c->isGlobal = *FindParseScalar<bool>("IsGlobal", node);
            c->actionHash = static_cast<u32>(*FindParseScalar<u64>("Value1", node));
            const auto valNode = RymlGetMapItem(node, "Value2");
            const std::string_view valTag = RymlGetValTag(valNode);
            if (valTag == "!u") {
                c->propType = xlink2::PropertyType::Enum;
                c->conditionValue.i = std::bit_cast<s32, u32>(static_cast<u32>(*ParseScalarAs<u64>(valNode)));
                c->enumName = addString(*FindParseScalar<std::string>("EnumName", node));
            } else if (valTag.empty()) {
                const auto value = ParseScalar(valNode);
                if (const u64* asInt = std::get_if<u64>(&value)) {
                    c->propType = xlink2::PropertyType::S32;
                    c->conditionValue.i = static_cast<s32>(*asInt);
                } else if (const f64* asFloat = std::get_if<f64>(&value)) {
                    c->propType = xlink2::PropertyType::F32;
                    c->conditionValue.f = static_cast<f32>(*asFloat);
                } else if (const bool* asBool = std::get_if<bool>(&value)) {
                    c->propType = xlink2::PropertyType::Bool;
                    c->conditionValue.b = *asBool;
                } else {
                    throw ParseError("Failed to parse switch condition property value");
                }
            } else if (valTag == "!i4") {
                c->propType = xlink2::PropertyType::_04;
                c->conditionValue.i = static_cast<s32>(*ParseScalarAs<u64>(valNode));
            } else if (valTag == "!f5") {
                c->propType = xlink2::PropertyType::_05;
                c->conditionValue.f = static_cast<f32>(*ParseScalarAs<f64>(valNode));
            } else {
                throw ParseError("Failed to parse switch condition property value");
            }
            break;
        }
        case 0x535f9620: { // !random
            condition.parentContainerType = xlink2::ContainerType::Random;
            auto c = condition.getAs<xlink2::ContainerType::Random>();
            c->weight = static_cast<f32>(*FindParseScalar<f64>("Weight", node));
            break;
        }
        case 0x21e8c153: { // !random2
            condition.parentContainerType = xlink2::ContainerType::Random2;
            auto c = condition.getAs<xlink2::ContainerType::Random2>();
            c->weight = static_cast<f32>(*FindParseScalar<f64>("Weight", node));
            break;
        }
        case 0x108df105: { // !blend
            static constexpr std::array<std::string_view, 6> sBlendTypes = {
                "None", "Multiply", "SquareRoot", "Sin", "Add", "SetToOne", 
            };
            condition.parentContainerType = xlink2::ContainerType::Blend;
            auto c = condition.getAs<xlink2::ContainerType::Blend>();
            c->min = static_cast<f32>(*FindParseScalar<f64>("Min", node));
            c->max = static_cast<f32>(*FindParseScalar<f64>("Max", node));
            c->blendTypeToMin = util::matchEnum<xlink2::BlendType>(*FindParseScalar<std::string>("BlendTypeMin", node), sBlendTypes);
            c->blendTypeToMax = util::matchEnum<xlink2::BlendType>(*FindParseScalar<std::string>("BlendTypeMax", node), sBlendTypes);
            break;
        }
        case 0x44278a0c: { // !sequence
            condition.parentContainerType = xlink2::ContainerType::Sequence;
            auto c = condition.getAs<xlink2::ContainerType::Sequence>();
            c->continueOnFade = static_cast<s32>(*FindParseScalar<u64>("ContinueOnFade", node));
            break;
        }
        case 0x35e7f782: { // !grid
            condition.parentContainerType = xlink2::ContainerType::Grid;
            break;
        }
        case 0xbc7428a3: { // !jump
            condition.parentContainerType = xlink2::ContainerType::Jump;
            break;
        }
        default: {
            throw ParseError(std::format("Invalid condition tag: {:#010x}", util::calcCRC32(tag)));
        }
    }
}

void System::loadContainer(Container& container, const c4::yml::ConstNodeRef& node) {
    const std::string_view tag = RymlGetValTag(node);

    switch (util::calcCRC32(tag)) {
        case 0x2a8d07ed: { // !switch
            auto c = container.getAs<xlink2::ContainerType::Switch>();
            container.type = xlink2::ContainerType::Switch;
            c->actionSlotName = addString(*FindParseScalar<std::string>("ValueName", node));
            c->unk = static_cast<s32>(*FindParseScalar<u64>("Unknown", node));
            c->propertyIndex = static_cast<s16>(*FindParseScalar<u64>("PropertyIndex", node));
            c->isGlobal = *FindParseScalar<bool>("IsGlobal", node);
            c->isActionTrigger = *FindParseScalar<bool>("IsActionTrigger", node);
            break;
        }
        case 0x535f9620: { // !random
            container.type = xlink2::ContainerType::Random;
            break;
        }
        case 0x21e8c153: { // !random2
            container.type = xlink2::ContainerType::Random2;
            break;
        }
        case 0x108df105: { // !blend
            container.type = xlink2::ContainerType::Blend;
            container.isNotBlendAll = false;
            if (node.num_children() > 3) { // type 2 blend
                container.isNotBlendAll = true;
                auto c = container.getAs<xlink2::ContainerType::Blend, true>();
                c->actionSlotName = addString(*FindParseScalar<std::string>("ValueName", node));
                c->unk = static_cast<s32>(*FindParseScalar<u64>("Unknown", node));
                c->propertyIndex = static_cast<s16>(*FindParseScalar<u64>("PropertyIndex", node));
                c->isGlobal = *FindParseScalar<bool>("IsGlobal", node);
                c->isActionTrigger = *FindParseScalar<bool>("IsActionTrigger", node);
            }
            break;
        }
        case 0x44278a0c: { // !sequence
            container.type = xlink2::ContainerType::Sequence;
            break;
        }
        case 0x35e7f782: { // !grid
            auto c = container.getAs<xlink2::ContainerType::Grid>();
            container.type = xlink2::ContainerType::Grid;
            c->propertyName1 = addString(*FindParseScalar<std::string>("PropertyName1", node));
            c->propertyName2 = addString(*FindParseScalar<std::string>("PropertyName2", node));
            c->propertyIndex1 = static_cast<s16>(*FindParseScalar<u64>("PropertyIndex1", node));
            c->propertyIndex2 = static_cast<s16>(*FindParseScalar<u64>("PropertyIndex2", node));
            c->isGlobal1 = *FindParseScalar<bool>("IsProperty1Global", node);
            c->isGlobal2 = *FindParseScalar<bool>("IsProperty2Global", node);
            const auto vals1 = RymlGetMapItem(node, "Property1Values");
            const auto vals2 = RymlGetMapItem(node, "Property2Values");
            c->values1.resize(vals1.num_children());
            c->values2.resize(vals2.num_children());
            auto parseU32Array = [](void* data, const c4::yml::ConstNodeRef& n, u32 index) -> void {
                std::vector<u32>* values = reinterpret_cast<std::vector<u32>*>(data);
                (*values)[index] = static_cast<u32>(*ParseScalarAs<u64>(n));
            };
            ParseSequence(vals1, &c->values1, parseU32Array);
            ParseSequence(vals2, &c->values2, parseU32Array);
            auto parseS32Array = [](void* data, const c4::yml::ConstNodeRef& n, u32 index) -> void {
                std::vector<s32>* values = reinterpret_cast<std::vector<s32>*>(data);
                (*values)[index] = static_cast<s32>(*ParseScalarAs<u64>(n));
            };
            const auto indices = RymlGetMapItem(node, "IndexGridMap");
            c->indices.resize(indices.num_children());
            if (c->indices.size() != (c->values1.size() * c->values2.size()))
                throw ParseError("Grid has the incorrect number of indices!\n");
            ParseSequence(indices, &c->indices, parseS32Array);
            break;
        }
        case 0xbc7428a3: { // !jump
            container.type = xlink2::ContainerType::Jump;
            break;
        }
        default: {
            throw ParseError(std::format("Invalid container tag: {:#010x}", util::calcCRC32(tag)));
        }
    }

    container.childContainerStartIdx = static_cast<s32>(*FindParseScalar<u64>("ChildContainerBaseIndex", node));
    container.childCount = static_cast<s32>(*FindParseScalar<u64>("ChildContainerCount", node));
    container.isNeedObserve = *FindParseScalar<bool>("IsNeedObserve", node);
}

void System::loadAssetCallTable(AssetCallTable& act, const c4::yml::ConstNodeRef& node) {
    act.keyName = addString(*FindParseScalar<std::string>("KeyName", node));
    act.assetIndex = static_cast<u16>(*FindParseScalar<u64>("AssetIndex", node)); // this field isn't necessary so maybe we should axe it?
    act.flag = *FindParseScalar<bool>("IsContainer", node) ? 1 : 0;
    act.duration = static_cast<s32>(*FindParseScalar<u64>("Duration", node));
    act.parentIndex = static_cast<s32>(*FindParseScalar<u64>("ParentIndex", node));
    act.guid = static_cast<u32>(*FindParseScalar<u64>("GUID", node));
    act.keyNameHash = static_cast<u32>(*FindParseScalar<u64>("KeyNameHash", node)); // this one also is unnecessary
    act.assetParamIdx = static_cast<s32>(*FindParseScalar<u64>("AssetParamOrContainerIndex", node));
    act.conditionIdx = static_cast<s32>(*FindParseScalar<u64>("ConditionIndex", node));
}

void System::loadActionSlot(ActionSlot& slot, const c4::yml::ConstNodeRef& node) {
    slot.actionSlotName = addString(*FindParseScalar<std::string>("SlotName", node));
    slot.actionStartIdx = static_cast<s16>(*FindParseScalar<u64>("ActionBaseIndex", node));
    slot.actionCount = static_cast<s16>(*FindParseScalar<u64>("ActionCount", node));
}

void System::loadAction(Action& action, const c4::yml::ConstNodeRef& node) {
    action.actionName = addString(*FindParseScalar<std::string>("ActionName", node));
    action.actionTriggerStartIdx = static_cast<s16>(*FindParseScalar<u64>("TriggerBaseIndex", node));
    action.actionTriggerCount = static_cast<s16>(*FindParseScalar<u64>("TriggerCount", node));
    action.enableMatchStart = *FindParseScalar<bool>("EnableMatchStart", node);
}

void System::loadActionTrigger(ActionTrigger& trigger, const c4::yml::ConstNodeRef& node) {
    trigger.guid = static_cast<u32>(*FindParseScalar<u64>("GUID", node));
    trigger.unk = static_cast<u32>(*FindParseScalar<u64>("Unknown", node));
    trigger.triggerOnce = *FindParseScalar<bool>("TriggerOnce", node);
    trigger.fade = *FindParseScalar<bool>("IsFade", node);
    trigger.alwaysTrigger = *FindParseScalar<bool>("AlwaysTrigger", node);
    const auto startFrameMaybe = node.find_child("StartFrame");
    if (startFrameMaybe.invalid()) {
        trigger.previousActionName = addString(*FindParseScalar<std::string>("PreviousActionName", node));
        trigger.nameMatch = true;
    } else {
        trigger.startFrame = static_cast<s32>(*ParseScalarAs<u64>(startFrameMaybe));
        trigger.nameMatch = false;
    }
    trigger.assetCallIdx = static_cast<s32>(*FindParseScalar<u64>("AssetCallTableIndex", node));
    trigger.endFrame = static_cast<s32>(*FindParseScalar<u64>("EndFrame", node));
    trigger.triggerOverwriteIdx = static_cast<s32>(*FindParseScalar<u64>("TriggerOverwriteParamIndex", node));
    trigger.overwriteHash = static_cast<u16>(*FindParseScalar<u64>("OverwriteHash", node));
}

void System::loadProperty(Property& prop, const c4::yml::ConstNodeRef& node) {
    prop.propertyName = addString(*FindParseScalar<std::string>("PropertyName", node));
    prop.isGlobal = *FindParseScalar<bool>("IsGlobal", node);
    prop.propTriggerStartIdx = static_cast<s32>(*FindParseScalar<u64>("TriggerBaseIndex", node));
    prop.propTriggerCount = static_cast<s32>(*FindParseScalar<u64>("TriggerCount", node));
}

void System::loadPropertyTrigger(PropertyTrigger& trigger, const c4::yml::ConstNodeRef& node) {
    trigger.guid = static_cast<u32>(*FindParseScalar<u64>("GUID", node));
    trigger.flag = static_cast<u16>(*FindParseScalar<u64>("Flag", node));
    trigger.overwriteHash = static_cast<u16>(*FindParseScalar<u64>("OverwriteHash", node));
    trigger.assetCallTableIdx = static_cast<s32>(*FindParseScalar<u64>("AssetCallTableIndex", node));
    trigger.conditionIdx = static_cast<s32>(*FindParseScalar<u64>("ConditionIndex", node));
    trigger.triggerOverwriteIdx = static_cast<s32>(*FindParseScalar<u64>("TriggerOverwriteParamIndex", node));
}

void System::loadAlwaysTrigger(AlwaysTrigger& trigger, const c4::yml::ConstNodeRef& node) {
    trigger.guid = static_cast<u32>(*FindParseScalar<u64>("GUID", node));
    trigger.flag = static_cast<u16>(*FindParseScalar<u64>("Flag", node));
    trigger.overwriteHash = static_cast<u16>(*FindParseScalar<u64>("OverwriteHash", node));
    trigger.assetCallIdx = static_cast<s32>(*FindParseScalar<u64>("AssetCallTableIndex", node));
    trigger.triggerOverwriteIdx = static_cast<s32>(*FindParseScalar<u64>("TriggerOverwriteParamIndex", node));
}

void System::loadUser(User& user, const c4::yml::ConstNodeRef& node, std::map<ValKey, s32>& valueMap) {
    const auto localProps = node.find_child("LocalProperties");
    if (localProps.invalid() || !localProps.is_seq())
        throw ParseError("Did not find LocalProperties field!\n");

    user.mLocalProperties.resize(localProps.num_children());
    for (u32 i = 0; const auto& prop : localProps) {
        user.mLocalProperties[i] = addString({prop.val().data(), prop.val().size()});
        ++i;
    }

    const auto userParams = node.find_child("UserParams");
    if (userParams.invalid() || !userParams.is_map())
        throw ParseError("Did not find UserParams field!\n");

    user.mUserParams.resize(userParams.num_children());
    for (u32 i = 0; const auto& child : userParams) {
        loadParam(user.mUserParams[i], child, ParamType::USER, valueMap);
        ++i;
    }

    const auto containers = node.find_child("Containers");
    if (containers.invalid() || !containers.is_map())
        throw ParseError("Did not find Containers field!\n");

    user.mContainers.resize(containers.num_children());
    for (u32 i = 0; const auto& child : containers) {
        loadContainer(user.mContainers[i], child);
        ++i;
    }

    const auto acts = node.find_child("AssetCallTables");
    if (acts.invalid() || !acts.is_map())
        throw ParseError("Did not find AssetCallTables field!\n");

    user.mAssetCallTables.resize(acts.num_children());
    for (u32 i = 0; const auto& child : acts) {
        loadAssetCallTable(user.mAssetCallTables[i], child);
        ++i;
    }

    const auto slots = node.find_child("ActionSlots");
    if (slots.invalid() || !slots.is_map())
        throw ParseError("Did not find ActionSlots field!\n");

    user.mActionSlots.resize(slots.num_children());
    for (u32 i = 0; const auto& child : slots) {
        loadActionSlot(user.mActionSlots[i], child);
        ++i;
    }

    const auto actions = node.find_child("Actions");
    if (actions.invalid() || !actions.is_map())
        throw ParseError("Did not find Actions field!\n");

    user.mActions.resize(actions.num_children());
    for (u32 i = 0; const auto& child : actions) {
        loadAction(user.mActions[i], child);
        ++i;
    }

    const auto actionTriggers = node.find_child("ActionTriggers");
    if (actionTriggers.invalid() || !actionTriggers.is_map())
        throw ParseError("Did not find ActionTriggers field!\n");

    user.mActionTriggers.resize(actionTriggers.num_children());
    for (u32 i = 0; const auto& child : actionTriggers) {
        loadActionTrigger(user.mActionTriggers[i], child);
        ++i;
    }

    const auto props = node.find_child("Properties");
    if (props.invalid() || !props.is_map())
        throw ParseError("Did not find Properties field!\n");

    user.mProperties.resize(props.num_children());
    for (u32 i = 0; const auto& child : props) {
        loadProperty(user.mProperties[i], child);
        ++i;
    }

    const auto propTriggers = node.find_child("PropertyTriggers");
    if (propTriggers.invalid() || !propTriggers.is_map())
        throw ParseError("Did not find PropertyTriggers field!\n");

    user.mPropertyTriggers.resize(propTriggers.num_children());
    for (u32 i = 0; const auto& child : propTriggers) {
        loadPropertyTrigger(user.mPropertyTriggers[i], child);
        ++i;
    }

    const auto alwaysTriggers = node.find_child("AlwaysTriggers");
    if (alwaysTriggers.invalid() || !alwaysTriggers.is_map())
        throw ParseError("Did not find AlwaysTriggers field!\n");

    user.mAlwaysTriggers.resize(alwaysTriggers.num_children());
    for (u32 i = 0; const auto& child : alwaysTriggers) {
        loadAlwaysTrigger(user.mAlwaysTriggers[i], child);
        ++i;
    }

    user.mUnknown = static_cast<u32>(*FindParseScalar<u64>("Unknown", node));
}

bool System::loadYAML(std::string_view text) {
    InitRymlIfNeeded();
    ryml::Tree tree = ryml::parse_in_arena(StrViewToRymlSubstr(text));

    const auto node = tree.rootref();
    if (node.invalid() || !node.is_map()) {
        std::cerr << "Not a valid yaml input file!\n";
        return false;
    }

    const auto version = node.find_child("Version");
    if (version.invalid()) {
        std::cerr << "Did not find Version field!\n";
        return false;
    }

    const auto res = ParseScalarAs<u64>(version);
    if (res != std::nullopt) {
        mVersion = *res;
    } else {
        std::cerr << "Failed to parse version!\n";
        return false;
    }

    if (mVersion != 0x24 && mVersion != 0x21) {
        std::cerr << "Invalid version!\n";
        return false;
    }

    const auto pdt = node.find_child("ParamDefineTable");
    if (pdt.invalid() || !pdt.has_val_tag() || pdt.val_tag() != "!pdt" || !pdt.is_map()) {
        std::cerr << "Did not find ParamDefineTable field!\n";
        return false;
    }

    if (!mPDT.loadYAML(pdt)) {
        std::cerr << "Failed to load ParamDefineTable!\n";
        return false;
    }

    const auto strings = node.find_child("Strings");
    if (!strings.invalid() && strings.is_seq()) {
        for (const auto& child : strings) {
            mStrings.insert({child.val().data(), child.val().size()});
        }
    }

    const auto localProps = node.find_child("LocalProperties");
    if (localProps.invalid() || !localProps.is_seq()) {
        std::cerr << "Did not find LocalProperties field!\n";
        return false;
    }

    mLocalProperties.resize(localProps.num_children());
    for (u32 i = 0; const auto& prop : localProps) {
        mLocalProperties[i] = addString({prop.val().data(), prop.val().size()});
        ++i;
    }

    const auto localEnums = node.find_child("LocalPropertyEnumValues");
    if (localEnums.invalid() || !localEnums.is_seq()) {
        std::cerr << "Did not find LocalPropertyEnumValues field!\n";
        return false;
    }

    mLocalPropertyEnumStrings.resize(localEnums.num_children());
    for (u32 i = 0; const auto& prop : localEnums) {
        mLocalPropertyEnumStrings[i] = addString({prop.val().data(), prop.val().size()});
        ++i;
    }

    const auto curves = node.find_child("Curves");
    if (curves.invalid() || !curves.is_map()) {
        std::cerr << "Did not find Curves field!\n";
        return false;
    }

    mCurves.resize(curves.num_children());
    for (u32 i = 0; const auto& curve : curves) {
        loadCurve(mCurves[i], curve);
        ++i;
    }

    const auto random = node.find_child("RandomTable");
    if (random.invalid() || !random.is_map()) {
        std::cerr << "Did not find RandomTable field!\n";
        return false;
    }

    mRandomCalls.resize(random.num_children());
    for (u32 i = 0; const auto& rand : random) {
        loadRandom(mRandomCalls[i], rand);
        ++i;
    }

    const auto groups = node.find_child("ArrangeGroupParams");
    if (groups.invalid() || !groups.is_map()) {
        std::cerr << "Did not find ArrangeGroupParams field!\n";
        return false;
    }

    mArrangeGroupParams.resize(groups.num_children());
    for (u32 i = 0; const auto& group : groups) {
        loadArrangeGroupParams(mArrangeGroupParams[i], group);
        ++i;
    }

    std::map<ValKey, s32> valueMap{};

    const auto users = node.find_child("Users");
    if (users.invalid() || !users.is_map()) {
        std::cerr << "Did not find Users field!\n";
        return false;
    }

    for (const auto& child : users) {
        const std::string_view keyTag = RymlGetKeyTag(child);
        u32 hash;
        if (keyTag == "!u") {
            hash = static_cast<u32>(*ParseScalarKeyAs<u64>(child));
        } else {
            hash = util::calcCRC32(*ParseScalarKeyAs<std::string>(child));
        }
        auto res = mUsers.emplace(hash, User());
        loadUser((*res.first).second, child, valueMap);
    }

    const auto assets = node.find_child("AssetParams");
    if (assets.invalid() || !assets.is_map()) {
        std::cerr << "Did not find AssetParams field!\n";
        return false;
    }

    mAssetParams.resize(assets.num_children());
    for (u32 i = 0; const auto& child : assets) {
        loadParamSet(mAssetParams[i], child, ParamType::ASSET, valueMap);
        ++i;
    }

    const auto triggers = node.find_child("TriggerOverwriteParams");
    if (triggers.invalid() || !triggers.is_map()) {
        std::cerr << "Did not find TriggerOverwriteParams field!\n";
        return false;
    }

    mTriggerOverwriteParams.resize(triggers.num_children());
    for (u32 i = 0; const auto& child : triggers) {
        loadParamSet(mTriggerOverwriteParams[i], child, ParamType::TRIGGER, valueMap);
        ++i;
    }

    const auto conditions = node.find_child("Conditions");
    if (conditions.invalid() || !conditions.is_map()) {
        std::cerr << "Did not find Conditions field!\n";
        return false;
    }

    mConditions.resize(conditions.num_children());
    for (u32 i = 0; const auto& cond : conditions) {
        loadCondition(mConditions[i], cond);
        ++i;
    }


    return true;
}

} // namespace banana