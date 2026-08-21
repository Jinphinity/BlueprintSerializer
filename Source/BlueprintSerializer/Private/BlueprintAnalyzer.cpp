#include "BlueprintAnalyzer.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimCurveMetadata.h"
#include "Animation/AnimData/AnimDataModel.h"
// #include "Animation/AnimData/AnimDataModelInterface.h"  // Not available in UE 5.6
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Engine/TimelineTemplate.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "UObject/Script.h"
#include "Engine/UserDefinedEnum.h"
#include "StructUtils/UserDefinedStruct.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneTrack.h"

#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_Inertialization.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimationStateMachineGraph.h"
#include "AnimStateNode.h"
#include "AnimStateNodeBase.h"
#include "AnimStateEntryNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimStateConduitNode.h"

#if __has_include("AnimGraphNode_LinkedAnimLayer.h")
#include "AnimGraphNode_LinkedAnimLayer.h"
#define UEARATAME_HAS_LINKED_ANIM_LAYER 1
#else
#define UEARATAME_HAS_LINKED_ANIM_LAYER 0
#endif

#if __has_include("AnimStateAliasNode.h")
#include "AnimStateAliasNode.h"
#define UEARATAME_HAS_ANIM_STATE_ALIAS 1
#else
#define UEARATAME_HAS_ANIM_STATE_ALIAS 0
#endif

#if __has_include("ControlRigBlueprint.h")
#include "ControlRigBlueprint.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/RigVMPin.h"
#include "RigVMModel/RigVMLink.h"
#include "Engine/SkeletalMesh.h"
#define UEARATAME_HAS_CONTROL_RIG 1
#else
#define UEARATAME_HAS_CONTROL_RIG 0
#endif

#if __has_include("Misc/EngineVersionComparison.h")
#include "Misc/EngineVersionComparison.h"
#endif

// Blueprint Graph includes for detailed node analysis
#include "K2Node.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_VariableSetRef.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Timeline.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_ClassDynamicCast.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_Literal.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_Select.h"
#include "K2Node_Switch.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchName.h"
#include "K2Node_SwitchString.h"
#include "K2Node_Composite.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_Knot.h"
#include "K2Node_Self.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_StructOperation.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_GetArrayItem.h"
#include "K2Node_MakeContainer.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeMap.h"
#include "K2Node_MakeSet.h"
#include "K2Node_Tunnel.h"
#include "K2Node_EnumEquality.h"
#include "K2Node_EnumInequality.h"
#include "K2Node_SetVariableOnPersistentFrame.h"
// Task 33-50: additional node type handlers
#include "K2Node_CallMaterialParameterCollectionFunction.h"
// Task 51: remaining partially-supported node types
#if __has_include("K2Node_MultiGate.h")
#include "K2Node_MultiGate.h"
#define BPSLZR_HAS_K2NODE_MULTIGATE 1
#else
#define BPSLZR_HAS_K2NODE_MULTIGATE 0
#endif
#include "K2Node_GetSubsystem.h"
#include "K2Node_AddComponent.h"
#include "K2Node_SetFieldsInStruct.h"
#include "K2Node_AsyncAction.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_Message.h"
#include "K2Node_GenericCreateObject.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_FormatText.h"
#include "K2Node_InputKey.h"
#include "K2Node_MathExpression.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_TemporaryVariable.h"
#include "K2Node_GetDataTableRow.h"
#include "K2Node_GetEnumeratorName.h"
#include "K2Node_LatentAbilityCall.h"
#include "K2Node_LatentGameplayTaskCall.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_GetInputActionValue.h"
#include "InputAction.h"       // UInputAction full definition (EnhancedInput module)
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/SoftObjectPtr.h"
#include "KismetCompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "UObject/UnrealType.h"
#include "Engine/Blueprint.h"
class UK2Node_CallFunction;
class UK2Node_VariableGet;
class UK2Node_VariableSet;

#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformFilemanager.h"

namespace
{
    FString GetPropertyValueAsString(UObject* Object, FProperty* Property)
    {
        if (!Object || !Property)
        {
            return FString();
        }

        FString Value;
        const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
        Property->ExportTextItem_Direct(Value, ValuePtr, nullptr, Object, PPF_None);
        return Value;
    }

    void ExtractObjectProperties(UObject* Object, TMap<FString, FString>& OutProperties)
    {
        if (!Object)
        {
            return;
        }

        for (TFieldIterator<FProperty> PropIt(Object->GetClass()); PropIt; ++PropIt)
        {
            FProperty* Prop = *PropIt;
            if (!Prop || Prop->HasAnyPropertyFlags(CPF_Deprecated))
            {
                continue;
            }

            const FString Value = GetPropertyValueAsString(Object, Prop);
            OutProperties.Add(Prop->GetName(), Value);
        }
    }

    void AddSortedStringMap(TSharedPtr<FJsonObject> Target, const FString& FieldName, const TMap<FString, FString>& Map)
    {
        if (!Target.IsValid())
        {
            return;
        }

        TArray<FString> Keys;
        Map.GetKeys(Keys);
        Keys.Sort();

        TSharedPtr<FJsonObject> MapObj = MakeShareable(new FJsonObject);
        for (const FString& Key : Keys)
        {
            const FString* Value = Map.Find(Key);
            MapObj->SetStringField(Key, Value ? *Value : FString());
        }

        Target->SetObjectField(FieldName, MapObj);
    }

    void AddSortedIntMap(TSharedPtr<FJsonObject> Target, const FString& FieldName, const TMap<FString, int32>& Map)
    {
        if (!Target.IsValid())
        {
            return;
        }

        TArray<FString> Keys;
        Map.GetKeys(Keys);
        Keys.Sort();

        TSharedPtr<FJsonObject> MapObj = MakeShareable(new FJsonObject);
        for (const FString& Key : Keys)
        {
            const int32* Value = Map.Find(Key);
            MapObj->SetNumberField(Key, Value ? *Value : 0);
        }

        Target->SetObjectField(FieldName, MapObj);
    }

    void AddSortedBoolMap(TSharedPtr<FJsonObject> Target, const FString& FieldName, const TMap<FString, bool>& Map)
    {
        if (!Target.IsValid())
        {
            return;
        }

        TArray<FString> Keys;
        Map.GetKeys(Keys);
        Keys.Sort();

        TSharedPtr<FJsonObject> MapObj = MakeShareable(new FJsonObject);
        for (const FString& Key : Keys)
        {
            const bool* Value = Map.Find(Key);
            MapObj->SetBoolField(Key, Value ? *Value : false);
        }

        Target->SetObjectField(FieldName, MapObj);
    }

    TArray<TSharedPtr<FJsonValue>> BuildStringArray(const TArray<FString>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(Values.Num());
        for (const FString& Value : Values)
        {
            Result.Add(MakeShareable(new FJsonValueString(Value)));
        }
        return Result;
    }

    TSharedPtr<FJsonObject> BuildRawInMemoryBytecodeDiagnostic(const FString& Md5)
    {
        TSharedPtr<FJsonObject> Diagnostic = MakeShareable(new FJsonObject);
        Diagnostic->SetStringField(TEXT("value"), Md5);
        Diagnostic->SetStringField(TEXT("algorithm"), TEXT("MD5"));
        Diagnostic->SetStringField(TEXT("source"), TEXT("UFunction::Script in-memory bytes"));
        Diagnostic->SetStringField(TEXT("stabilityScope"), TEXT("single_loaded_function_instance"));
        Diagnostic->SetBoolField(TEXT("canonical"), false);
        Diagnostic->SetBoolField(TEXT("semanticEquivalenceProof"), false);
        return Diagnostic;
    }

    TArray<USCS_Node*> GatherAllConstructionScriptNodes(UBlueprint* Blueprint)
    {
        TArray<USCS_Node*> Nodes;
        if (!Blueprint)
        {
            return Nodes;
        }

        TSet<const USCS_Node*> SeenNodes;

        auto AddNodesFromSCS = [&Nodes, &SeenNodes](USimpleConstructionScript* SCS)
        {
            if (!SCS)
            {
                return;
            }

            for (USCS_Node* Node : SCS->GetAllNodes())
            {
                if (!Node || SeenNodes.Contains(Node))
                {
                    continue;
                }

                SeenNodes.Add(Node);
                Nodes.Add(Node);
            }
        };

        if (const UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            TArray<const UBlueprintGeneratedClass*> Hierarchy;
            if (UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(GeneratedClass, Hierarchy))
            {
                for (int32 Index = Hierarchy.Num() - 1; Index >= 0; --Index)
                {
                    const UBlueprintGeneratedClass* HierarchyClass = Hierarchy[Index];
                    if (!HierarchyClass)
                    {
                        continue;
                    }

                    AddNodesFromSCS(HierarchyClass->SimpleConstructionScript);
                }
            }
        }

        AddNodesFromSCS(Blueprint->SimpleConstructionScript);
        return Nodes;
    }

    FString MakeSafeFileComponent(const FString& Input)
    {
        FString Sanitized = Input;
        Sanitized.TrimStartAndEndInline();

        for (TCHAR& Ch : Sanitized)
        {
            if (!FChar::IsAlnum(Ch) && Ch != TEXT('_') && Ch != TEXT('-'))
            {
                Ch = TEXT('_');
            }
        }

        while (Sanitized.Contains(TEXT("__")))
        {
            Sanitized.ReplaceInline(TEXT("__"), TEXT("_"));
        }

        while (Sanitized.StartsWith(TEXT("_")))
        {
            Sanitized.RightChopInline(1, EAllowShrinking::No);
        }

        while (Sanitized.EndsWith(TEXT("_")))
        {
            Sanitized.LeftChopInline(1, EAllowShrinking::No);
        }

        if (Sanitized.IsEmpty())
        {
            Sanitized = TEXT("Blueprint");
        }

        return Sanitized.Left(64);
    }

    FString BuildBlueprintArtifactStem(const FBS_BlueprintData& BlueprintData)
    {
        const FString SafeName = MakeSafeFileComponent(BlueprintData.BlueprintName);
        const FString IdentityPath = BlueprintData.BlueprintPath.IsEmpty() ? BlueprintData.BlueprintName : BlueprintData.BlueprintPath;
        const FString PathHash = FMD5::HashAnsiString(*IdentityPath).Left(8);
        return FString::Printf(TEXT("%s_%s"), *SafeName, *PathHash);
    }

    TArray<TSharedPtr<FJsonValue>> BuildFloatArray(const TArray<float>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(Values.Num());
        for (float Value : Values)
        {
            Result.Add(MakeShareable(new FJsonValueNumber(Value)));
        }
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> BuildVectorArray(const TArray<FVector>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(Values.Num());
        for (const FVector& Value : Values)
        {
            TSharedPtr<FJsonObject> VectorObj = MakeShareable(new FJsonObject);
            VectorObj->SetNumberField(TEXT("x"), Value.X);
            VectorObj->SetNumberField(TEXT("y"), Value.Y);
            VectorObj->SetNumberField(TEXT("z"), Value.Z);
            Result.Add(MakeShareable(new FJsonValueObject(VectorObj)));
        }

        return Result;
    }

    FString BuildTransformString(const FTransform& Value)
    {
        const FVector Translation = Value.GetTranslation();
        const FRotator Rotation = Value.GetRotation().Rotator();
        const FVector Scale = Value.GetScale3D();
        return FString::Printf(
            TEXT("T=(%.6f,%.6f,%.6f);R=(%.6f,%.6f,%.6f);S=(%.6f,%.6f,%.6f)"),
            Translation.X,
            Translation.Y,
            Translation.Z,
            Rotation.Roll,
            Rotation.Pitch,
            Rotation.Yaw,
            Scale.X,
            Scale.Y,
            Scale.Z);
    }

    FString DescribeInterpolationMode(const FRichCurve& Curve)
    {
        if (Curve.Keys.IsEmpty())
        {
            return TEXT("None");
        }

        switch (Curve.Keys[0].InterpMode)
        {
            case RCIM_Constant:
                return TEXT("Constant");
            case RCIM_Linear:
                return TEXT("Linear");
            case RCIM_Cubic:
                return TEXT("Cubic");
            case RCIM_None:
                return TEXT("None");
            default:
                return TEXT("Unknown");
        }
    }

    FString DescribePropertyType(const FProperty* Property)
    {
        if (!Property)
        {
            return FString();
        }

        if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
        {
            return FString::Printf(TEXT("Array<%s>"), *DescribePropertyType(ArrayProp->Inner));
        }

        if (const FSetProperty* SetProp = CastField<FSetProperty>(Property))
        {
            return FString::Printf(TEXT("Set<%s>"), *DescribePropertyType(SetProp->ElementProp));
        }

        if (const FMapProperty* MapProp = CastField<FMapProperty>(Property))
        {
            return FString::Printf(
                TEXT("Map<%s,%s>"),
                *DescribePropertyType(MapProp->KeyProp),
                *DescribePropertyType(MapProp->ValueProp));
        }

        FString Type = Property->GetCPPType();

        if (const FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
        {
            if (ObjectProp->PropertyClass)
            {
                Type += FString::Printf(TEXT(" (%s)"), *ObjectProp->PropertyClass->GetPathName());
            }
            return Type;
        }

        if (const FClassProperty* ClassProp = CastField<FClassProperty>(Property))
        {
            if (ClassProp->MetaClass)
            {
                Type += FString::Printf(TEXT(" (%s)"), *ClassProp->MetaClass->GetPathName());
            }
            return Type;
        }

        if (const FSoftObjectProperty* SoftObjectProp = CastField<FSoftObjectProperty>(Property))
        {
            if (SoftObjectProp->PropertyClass)
            {
                Type += FString::Printf(TEXT(" (%s)"), *SoftObjectProp->PropertyClass->GetPathName());
            }
            return Type;
        }

        if (const FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
        {
            if (SoftClassProp->MetaClass)
            {
                Type += FString::Printf(TEXT(" (%s)"), *SoftClassProp->MetaClass->GetPathName());
            }
            return Type;
        }

        if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
        {
            if (StructProp->Struct)
            {
                Type += FString::Printf(TEXT(" (%s)"), *StructProp->Struct->GetPathName());
            }
            return Type;
        }

        if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
        {
            if (UEnum* Enum = EnumProp->GetEnum())
            {
                Type += FString::Printf(TEXT(" (%s)"), *Enum->GetPathName());
            }
            return Type;
        }

        if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
        {
            if (UEnum* Enum = ByteProp->Enum)
            {
                Type += FString::Printf(TEXT(" (%s)"), *Enum->GetPathName());
            }
            return Type;
        }

        return Type;
    }

    FString GetPropertyObjectTypePath(const FProperty* Property)
    {
        if (!Property)
        {
            return FString();
        }

        if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
        {
            return GetPropertyObjectTypePath(ArrayProp->Inner);
        }

        if (const FSetProperty* SetProp = CastField<FSetProperty>(Property))
        {
            return GetPropertyObjectTypePath(SetProp->ElementProp);
        }

        if (const FMapProperty* MapProp = CastField<FMapProperty>(Property))
        {
            const FString KeyPath = GetPropertyObjectTypePath(MapProp->KeyProp);
            if (!KeyPath.IsEmpty())
            {
                return KeyPath;
            }
            return GetPropertyObjectTypePath(MapProp->ValueProp);
        }

        if (const FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
        {
            return ObjectProp->PropertyClass ? ObjectProp->PropertyClass->GetPathName() : FString();
        }

        if (const FClassProperty* ClassProp = CastField<FClassProperty>(Property))
        {
            return ClassProp->MetaClass ? ClassProp->MetaClass->GetPathName() : FString();
        }

        if (const FSoftObjectProperty* SoftObjectProp = CastField<FSoftObjectProperty>(Property))
        {
            return SoftObjectProp->PropertyClass ? SoftObjectProp->PropertyClass->GetPathName() : FString();
        }

        if (const FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
        {
            return SoftClassProp->MetaClass ? SoftClassProp->MetaClass->GetPathName() : FString();
        }

        if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
        {
            return StructProp->Struct ? StructProp->Struct->GetPathName() : FString();
        }

        if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
        {
            return EnumProp->GetEnum() ? EnumProp->GetEnum()->GetPathName() : FString();
        }

        if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
        {
            return ByteProp->Enum ? ByteProp->Enum->GetPathName() : FString();
        }

        return FString();
    }

    FString DescribePinTypeDetailed(const FEdGraphPinType& PinType)
    {
        FString Type = PinType.PinCategory.ToString();

        if (PinType.PinSubCategory != NAME_None)
        {
            Type += FString::Printf(TEXT(".%s"), *PinType.PinSubCategory.ToString());
        }

        if (PinType.PinSubCategoryObject.IsValid())
        {
            Type += FString::Printf(TEXT(" (%s)"), *PinType.PinSubCategoryObject->GetPathName());
        }

        switch (PinType.ContainerType)
        {
        case EPinContainerType::Array:
            Type = FString::Printf(TEXT("Array<%s>"), *Type);
            break;
        case EPinContainerType::Map:
        {
            FString ValueType = PinType.PinValueType.TerminalCategory.ToString();
            if (PinType.PinValueType.TerminalSubCategory != NAME_None)
            {
                ValueType += FString::Printf(TEXT(".%s"), *PinType.PinValueType.TerminalSubCategory.ToString());
            }
            if (PinType.PinValueType.TerminalSubCategoryObject.IsValid())
            {
                ValueType += FString::Printf(TEXT(" (%s)"), *PinType.PinValueType.TerminalSubCategoryObject->GetPathName());
            }
            Type = FString::Printf(TEXT("Map<%s,%s>"), *Type, *ValueType);
            break;
        }
        case EPinContainerType::Set:
            Type = FString::Printf(TEXT("Set<%s>"), *Type);
            break;
        default:
            break;
        }

        return Type;
    }

    bool IsConstructionScriptGraph(const UEdGraph* Graph)
    {
        if (!Graph)
        {
            return false;
        }

        const FName GraphName = Graph->GetFName();
        return GraphName == UEdGraphSchema_K2::FN_UserConstructionScript
            || GraphName.ToString().Contains(TEXT("Construction"));
    }

    bool IsNodeTypeKnownSupported(const FString& NodeType)
    {
        static const TSet<FString> SupportedTypes = {
            TEXT("K2Node_CallFunction"),
            TEXT("K2Node_VariableGet"),
            TEXT("K2Node_VariableSet"),
            TEXT("K2Node_VariableSetRef"),
            TEXT("K2Node_Branch"),
            TEXT("K2Node_IfThenElse"),
            TEXT("K2Node_DynamicCast"),
            TEXT("K2Node_MacroInstance"),
            TEXT("K2Node_SwitchEnum"),
            TEXT("K2Node_SwitchInteger"),
            TEXT("K2Node_SwitchName"),
            TEXT("K2Node_SwitchString"),
            TEXT("K2Node_ExecutionSequence"),
            TEXT("K2Node_Select"),
            TEXT("K2Node_CommutativeAssociativeBinaryOperator"),
            TEXT("K2Node_MakeStruct"),
            TEXT("K2Node_BreakStruct"),
            TEXT("K2Node_FunctionEntry"),
            TEXT("K2Node_FunctionResult"),
            TEXT("K2Node_CustomEvent"),
            TEXT("K2Node_Event"),
            TEXT("K2Node_Timeline"),
            TEXT("K2Node_CallParentFunction"),
            // Task 27: promote nodes that already have full meta handlers to known-supported
            TEXT("K2Node_Knot"),                 // Task 24: reroute (transparent pass-through)
            TEXT("K2Node_Self"),                 // Task 25: self-reference marker
            TEXT("K2Node_SpawnActorFromClass"),  // Task 26: actor spawn
            TEXT("K2Node_Composite"),            // Task 18: collapsed sub-graph
            TEXT("K2Node_ClassDynamicCast"),     // existing handler
            TEXT("K2Node_Literal"),              // existing handler
            TEXT("K2Node_CallDelegate"),         // existing handler
            TEXT("K2Node_CreateDelegate"),       // existing handler
            TEXT("K2Node_CallArrayFunction"),    // existing handler
            TEXT("K2Node_ComponentBoundEvent"),  // existing handler
            // Task 28-32 new handlers (added below)
            TEXT("K2Node_PromotableOperator"),           // Task 28
            TEXT("K2Node_GetArrayItem"),                 // Task 29
            TEXT("K2Node_MakeArray"),                    // Task 30
            TEXT("K2Node_MakeMap"),                      // Task 30
            TEXT("K2Node_MakeSet"),                      // Task 30
            TEXT("K2Node_Tunnel"),                       // Task 31
            TEXT("K2Node_TunnelBoundary"),               // Task 31 (subtype)
            TEXT("K2Node_EnumEquality"),                 // Task 32
            TEXT("K2Node_EnumInequality"),               // Task 32
            TEXT("K2Node_SetVariableOnPersistentFrame"), // Task 32
            // Task 33-50: remaining partially-supported node handlers
            TEXT("K2Node_CallMaterialParameterCollectionFunction"), // Task 33
            TEXT("K2Node_GetSubsystem"),                            // Task 34
            TEXT("K2Node_GetEngineSubsystem"),                      // Task 34
            TEXT("K2Node_GetSubsystemFromPC"),                      // Task 34
            TEXT("K2Node_LatentAbilityCall"),                       // Task 35
            TEXT("K2Node_LatentGameplayTaskCall"),                   // Task 35
            TEXT("K2Node_AddComponent"),                            // Task 36
            TEXT("K2Node_SetFieldsInStruct"),                       // Task 37
            TEXT("K2Node_AsyncAction"),                             // Task 38
            TEXT("K2Node_AddDelegate"),                             // Task 39
            TEXT("K2Node_AssignDelegate"),                          // Task 39
            TEXT("K2Node_RemoveDelegate"),                          // Task 39
            TEXT("K2Node_Message"),                                 // Task 40
            TEXT("K2Node_GenericCreateObject"),                     // Task 41
            TEXT("K2Node_CreateWidget"),                            // Task 41
            TEXT("K2Node_FormatText"),                              // Task 42
            TEXT("K2Node_InputKey"),                                // Task 43
            TEXT("K2Node_MathExpression"),                          // Task 44
            TEXT("K2Node_EnhancedInputAction"),                     // Task 45
            TEXT("K2Node_AssignmentStatement"),                     // Task 46
            TEXT("K2Node_TemporaryVariable"),                       // Task 47
            TEXT("K2Node_PropertyAccess"),                          // Task 48
            TEXT("K2Node_GetDataTableRow"),                         // Task 49
            TEXT("K2Node_GetEnumeratorName"),                       // Task 50
            // Task 51: remaining partially-supported K2Node types — promote to known-supported
            // Subclasses whose base-class Cast handler already extracts metadata:
            TEXT("K2Node_AsyncAction_ListenForGameplayMessages"), // UK2Node_BaseAsyncTask branch handles via reflection
            TEXT("K2Node_GetEnumeratorNameAsString"),             // UK2Node_GetEnumeratorName cast handles; string vs FName distinction noted in handler
            TEXT("K2Node_ClearDelegate"),                         // UK2Node_BaseMCDelegate cast handles delegate property reference
            // Nodes with new handlers or basic recognition flags:
            TEXT("K2Node_MultiGate"),                             // Task 51: multi-output exec gate (bIsRandom, bLoop, StartIndex)
            TEXT("K2Node_ConvertAsset"),                          // Legacy asset-type conversion marker
            TEXT("K2Node_AnimNodeReference"),                     // AnimBP: returns typed anim node reference
            TEXT("K2Node_InstancedStruct"),                       // UE5 instanced struct make/break operations
            TEXT("K2Node_CastByteToEnum"),                        // Byte-to-enum cast with enum class context
            TEXT("K2Node_PlayMontage"),                           // Latent montage playback (proxy factory pattern)
            TEXT("K2Node_GetInputAxisKeyValue"),                  // Input axis float value by key
            TEXT("K2Node_LoadAssetClass"),                        // Async soft class loader (BaseAsyncTask pattern)
            TEXT("K2Node_MapForEach"),                            // Map loop with key/value/break/completed pins
            TEXT("K2Node_AddComponentByClass"),                   // Runtime component class + attachment/deferred-finish contract
            TEXT("GameplayTagsK2Node_SwitchGameplayTag"),         // Gameplay-tag switch cases and pins
            TEXT("K2Node_GetInputActionValue"),                   // Enhanced Input action-value read
        };

        return SupportedTypes.Contains(NodeType);
    }

    bool IsContentObjectPath(const FString& Path)
    {
        if (!Path.StartsWith(TEXT("/"))
            || Path.Len() < 3
            || Path.StartsWith(TEXT("//"))
            || Path.Contains(TEXT("//"))
            || Path.Contains(TEXT("\\"))
            || Path.EndsWith(TEXT("/"))
            || Path.StartsWith(TEXT("/Script/"))
            || Path.StartsWith(TEXT("/Temp/"))
            || Path.StartsWith(TEXT("/Engine/Transient")))
        {
            return false;
        }

        int32 LastSlashIndex = INDEX_NONE;
        Path.FindLastChar(TEXT('/'), LastSlashIndex);
        const int32 ObjectDelimiterIndex = Path.Find(
            TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart,
            FMath::Max(LastSlashIndex + 1, 0));

        if (ObjectDelimiterIndex != INDEX_NONE)
        {
            return FPackageName::IsValidObjectPath(Path);
        }

        return FPackageName::IsValidLongPackageName(Path, true);
    }

    bool IsValidMountPointRoot(const FString& Root)
    {
        if (Root.IsEmpty())
        {
            return false;
        }

        for (const TCHAR Ch : Root)
        {
            if (!FChar::IsAlnum(Ch) && Ch != TEXT('_'))
            {
                return false;
            }
        }

        return true;
    }

    bool IsPathTerminator(const TCHAR Ch)
    {
        switch (Ch)
        {
        case TEXT(' '):
        case TEXT('\t'):
        case TEXT('\r'):
        case TEXT('\n'):
        case TEXT('"'):
        case TEXT('\''):
        case TEXT(','):
        case TEXT('('):
        case TEXT(')'):
        case TEXT('['):
        case TEXT(']'):
        case TEXT('{'):
        case TEXT('}'):
        case TEXT('='):
            return true;
        default:
            return false;
        }
    }

    FString NormalizeObjectPath(FString Path, const bool bStripGeneratedClassSuffix)
    {
        Path.TrimStartAndEndInline();
        if (Path.IsEmpty())
        {
            return Path;
        }

        while (!Path.IsEmpty()
            && (Path.StartsWith(TEXT("\""))
                || Path.StartsWith(TEXT("'"))
                || Path.StartsWith(TEXT("("))
                || Path.StartsWith(TEXT(","))))
        {
            Path.RightChopInline(1, EAllowShrinking::No);
        }

        while (!Path.IsEmpty()
            && (Path.EndsWith(TEXT("\""))
                || Path.EndsWith(TEXT("'"))
                || Path.EndsWith(TEXT(")"))
                || Path.EndsWith(TEXT(","))))
        {
            Path.LeftChopInline(1, EAllowShrinking::No);
        }

        int32 ColonIndex = INDEX_NONE;
        if (Path.FindChar(TEXT(':'), ColonIndex))
        {
            Path = Path.Left(ColonIndex);
        }

        if (bStripGeneratedClassSuffix)
        {
            int32 DotIndex = INDEX_NONE;
            if (Path.FindLastChar(TEXT('.'), DotIndex))
            {
                FString ObjectName = Path.Mid(DotIndex + 1);
                if (ObjectName.EndsWith(TEXT("_C")))
                {
                    ObjectName.LeftChopInline(2, EAllowShrinking::No);
                    Path = Path.Left(DotIndex + 1) + ObjectName;
                }
            }
        }

        return Path;
    }

#if UEARATAME_HAS_CONTROL_RIG
    FGuid MakeStableRigVMNodeGuid(const URigVMNode* VMNode)
    {
        if (!VMNode)
        {
            return FGuid();
        }

        const FString NodePath = VMNode->GetNodePath(true);
        const FString NodeTitle = VMNode->GetNodeTitle();
        const FString GraphName = VMNode->GetGraph() ? VMNode->GetGraph()->GetName() : FString();
        const FString NodeClass = VMNode->GetClass() ? VMNode->GetClass()->GetName() : FString();

        return FGuid(
            GetTypeHash(NodePath),
            GetTypeHash(NodeTitle),
            GetTypeHash(GraphName),
            GetTypeHash(NodeClass));
    }
#endif

    void AddCandidatePath(const FString& RawPath, TSet<FString>& OutPaths)
    {
        const FString Normalized = NormalizeObjectPath(RawPath, false);
        if (IsContentObjectPath(Normalized))
        {
            OutPaths.Add(Normalized);
        }

        const FString NormalizedNoClass = NormalizeObjectPath(RawPath, true);
        if (IsContentObjectPath(NormalizedNoClass))
        {
            OutPaths.Add(NormalizedNoClass);
        }
    }

    void CollectAssetPathsFromText(const FString& Text, TSet<FString>& OutPaths)
    {
        if (Text.IsEmpty())
        {
            return;
        }

        // Scan for embedded content-style object paths even when wrapped in class literals,
        // for example: /Script/ControlRig.ControlRigBlueprintGeneratedClass'/Game/.../CR_Name.CR_Name_C'
        const int32 TextLen = Text.Len();
        for (int32 Index = 0; Index < TextLen; ++Index)
        {
            if (Text[Index] != TEXT('/'))
            {
                continue;
            }

            // Only consider slash-prefixed paths that begin at token boundaries.
            // This prevents re-capturing nested segments from a single full path,
            // such as /Characters/... from /Game/Characters/...
            if (Index > 0 && !IsPathTerminator(Text[Index - 1]))
            {
                continue;
            }

            const int32 RootStart = Index + 1;
            int32 RootEnd = RootStart;
            while (RootEnd < TextLen && Text[RootEnd] != TEXT('/'))
            {
                ++RootEnd;
            }

            if (RootEnd >= TextLen)
            {
                continue;
            }

            const FString Root = Text.Mid(RootStart, RootEnd - RootStart);
            if (!IsValidMountPointRoot(Root)
                || Root.Equals(TEXT("Script"), ESearchCase::IgnoreCase)
                || Root.Equals(TEXT("Temp"), ESearchCase::IgnoreCase))
            {
                continue;
            }

            int32 PathEnd = RootEnd + 1;
            while (PathEnd < TextLen && !IsPathTerminator(Text[PathEnd]))
            {
                ++PathEnd;
            }

            const FString EmbeddedPath = Text.Mid(Index, PathEnd - Index);
            AddCandidatePath(EmbeddedPath, OutPaths);
        }

        TArray<FString> SingleQuotedParts;
        Text.ParseIntoArray(SingleQuotedParts, TEXT("'"), true);
        for (const FString& Part : SingleQuotedParts)
        {
            if (Part.Contains(TEXT("/")))
            {
                AddCandidatePath(Part, OutPaths);
            }
        }

        TArray<FString> DoubleQuotedParts;
        Text.ParseIntoArray(DoubleQuotedParts, TEXT("\""), true);
        for (const FString& Part : DoubleQuotedParts)
        {
            if (Part.Contains(TEXT("/")))
            {
                AddCandidatePath(Part, OutPaths);
            }
        }

        TArray<FString> Tokens;
        Text.ParseIntoArrayWS(Tokens);
        for (const FString& Token : Tokens)
        {
            if (Token.Contains(TEXT("/")))
            {
                AddCandidatePath(Token, OutPaths);
            }
        }
    }

    FString FormatPropertyFlags(const EPropertyFlags Flags)
    {
        return FString::Printf(TEXT("0x%016llX"), static_cast<unsigned long long>(Flags));
    }

    FString DescribeLifetimeCondition(const ELifetimeCondition Condition)
    {
        switch (Condition)
        {
        case COND_None: return TEXT("None");
        case COND_InitialOnly: return TEXT("InitialOnly");
        case COND_OwnerOnly: return TEXT("OwnerOnly");
        case COND_SkipOwner: return TEXT("SkipOwner");
        case COND_SimulatedOnly: return TEXT("SimulatedOnly");
        case COND_AutonomousOnly: return TEXT("AutonomousOnly");
        case COND_SimulatedOrPhysics: return TEXT("SimulatedOrPhysics");
        case COND_InitialOrOwner: return TEXT("InitialOrOwner");
        case COND_Custom: return TEXT("Custom");
        case COND_ReplayOrOwner: return TEXT("ReplayOrOwner");
        case COND_ReplayOnly: return TEXT("ReplayOnly");
        case COND_SimulatedOnlyNoReplay: return TEXT("SimulatedOnlyNoReplay");
        case COND_SimulatedOrPhysicsNoReplay: return TEXT("SimulatedOrPhysicsNoReplay");
        case COND_SkipReplay: return TEXT("SkipReplay");
        case COND_Dynamic: return TEXT("Dynamic");
        case COND_Never: return TEXT("Never");
        default: return FString::Printf(TEXT("Unknown(%d)"), static_cast<int32>(Condition));
        }
    }

    void CollectPropertyAssetPaths(
        const TMap<FString, FString>& Properties,
        TSet<FString>& OutPaths)
    {
        for (const TPair<FString, FString>& Pair : Properties)
        {
            CollectAssetPathsFromText(Pair.Value, OutPaths);
        }
    }

    FString FormatFrameRange(const TRange<FFrameNumber>& Range)
    {
        const FString Lower = Range.HasLowerBound()
            ? FString::FromInt(Range.GetLowerBoundValue().Value)
            : TEXT("Open");
        const FString Upper = Range.HasUpperBound()
            ? FString::FromInt(Range.GetUpperBoundValue().Value)
            : TEXT("Open");
        return FString::Printf(TEXT("%s:%s"), *Lower, *Upper);
    }

    FString FormatFrameRate(const FFrameRate& Rate)
    {
        return FString::Printf(TEXT("%d/%d"), Rate.Numerator, Rate.Denominator);
    }

    UObject* TryLoadBestCandidate(const FString& RawPath)
    {
        TSet<FString> CandidateSet;
        AddCandidatePath(RawPath, CandidateSet);
        if (CandidateSet.Num() == 0)
        {
            return nullptr;
        }

        TArray<FString> Candidates = CandidateSet.Array();
        Candidates.Sort();

        for (const FString& Candidate : Candidates)
        {
            FSoftObjectPath SoftPath(Candidate);
            if (UObject* LoadedObject = SoftPath.TryLoad())
            {
                return LoadedObject;
            }
        }

        return nullptr;
    }

    FString ResolveBlueprintVariableDefaultValue(UBlueprint* Blueprint, const FBPVariableDescription& Variable)
    {
        if (!Blueprint)
        {
            return Variable.DefaultValue;
        }

        UClass* SearchClass = Blueprint->GeneratedClass;
        if (!SearchClass)
        {
            SearchClass = Blueprint->SkeletonGeneratedClass;
        }

        if (!SearchClass)
        {
            return Variable.DefaultValue;
        }

        FProperty* Property = FindFProperty<FProperty>(SearchClass, Variable.VarName);
        UObject* CDO = SearchClass->GetDefaultObject();
        if (!Property || !CDO)
        {
            return Variable.DefaultValue;
        }

        const FString ExportedDefault = GetPropertyValueAsString(CDO, Property);
        if (!ExportedDefault.IsEmpty())
        {
            return ExportedDefault;
        }

        return Variable.DefaultValue;
    }

    struct FBS_DependencyClosureData
    {
        TSet<FString> Classes;
        TSet<FString> Structs;
        TSet<FString> Enums;
        TSet<FString> Interfaces;
        TSet<FString> Assets;
        TSet<FString> ControlRigs;
        TSet<FString> MacroGraphs;
        TSet<FString> MacroBlueprints;
        TSet<FString> Modules;
        // CR-036: macro graph path -> list of call-site node GUIDs
        TMap<FString, TArray<FString>> MacroGraphCallSites;
    };

    TArray<FString> ToSortedArray(const TSet<FString>& Values)
    {
        TArray<FString> Result = Values.Array();
        Result.Sort();
        return Result;
    }

    void AddModuleFromPath(const FString& ObjectPath, TSet<FString>& OutModules)
    {
        if (!ObjectPath.StartsWith(TEXT("/Script/")))
        {
            return;
        }

        const FString ScriptPart = ObjectPath.Mid(8);
        FString ModuleName;
        if (ScriptPart.Split(TEXT("."), &ModuleName, nullptr) && !ModuleName.IsEmpty())
        {
            OutModules.Add(ModuleName);
        }
    }

    bool TryParseScriptObjectPath(const FString& ScriptPath, FString& OutModuleName, FString& OutSymbolName)
    {
        OutModuleName.Reset();
        OutSymbolName.Reset();

        if (!ScriptPath.StartsWith(TEXT("/Script/")))
        {
            return false;
        }

        const FString ScriptPart = ScriptPath.Mid(8);
        if (!ScriptPart.Split(TEXT("."), &OutModuleName, &OutSymbolName))
        {
            return false;
        }

        return !OutModuleName.IsEmpty() && !OutSymbolName.IsEmpty();
    }

    FString CanonicalizeScriptClassPath(const FString& ScriptPath)
    {
        FString ModuleName;
        FString SymbolName;
        if (!TryParseScriptObjectPath(ScriptPath, ModuleName, SymbolName))
        {
            return ScriptPath;
        }

        if (SymbolName.StartsWith(TEXT("Default__")))
        {
            FString CanonicalSymbol = SymbolName;
            CanonicalSymbol.RightChopInline(9, EAllowShrinking::No);
            if (!CanonicalSymbol.IsEmpty())
            {
                return FString::Printf(TEXT("/Script/%s.%s"), *ModuleName, *CanonicalSymbol);
            }
        }

        return ScriptPath;
    }

    FString NormalizeIncludeHintPath(FString HeaderPath, const FString& FallbackModuleName)
    {
        HeaderPath.TrimStartAndEndInline();
        HeaderPath.ReplaceInline(TEXT("\\"), TEXT("/"));

        while (!HeaderPath.IsEmpty() && (HeaderPath.StartsWith(TEXT("\"")) || HeaderPath.StartsWith(TEXT("'"))))
        {
            HeaderPath.RightChopInline(1, EAllowShrinking::No);
        }

        while (!HeaderPath.IsEmpty() && (HeaderPath.EndsWith(TEXT("\"")) || HeaderPath.EndsWith(TEXT("'"))))
        {
            HeaderPath.LeftChopInline(1, EAllowShrinking::No);
        }

        if (HeaderPath.IsEmpty() || HeaderPath.EndsWith(TEXT(".generated.h")))
        {
            return FString();
        }

        while (HeaderPath.StartsWith(TEXT("./")))
        {
            HeaderPath.RightChopInline(2, EAllowShrinking::No);
        }

        while (HeaderPath.StartsWith(TEXT("/")))
        {
            HeaderPath.RightChopInline(1, EAllowShrinking::No);
        }

        int32 SourceIndex = HeaderPath.Find(TEXT("/Source/"), ESearchCase::IgnoreCase);
        int32 SourceTokenLen = 8;
        if (SourceIndex == INDEX_NONE && HeaderPath.StartsWith(TEXT("Source/"), ESearchCase::IgnoreCase))
        {
            SourceIndex = 0;
            SourceTokenLen = 7;
        }

        if (SourceIndex != INDEX_NONE)
        {
            FString AfterSource = HeaderPath.Mid(SourceIndex + SourceTokenLen);
            FString SourceModule;
            FString RelativePath;
            if (AfterSource.Split(TEXT("/"), &SourceModule, &RelativePath) && !SourceModule.IsEmpty() && !RelativePath.IsEmpty())
            {
                if (RelativePath.StartsWith(TEXT("Public/")))
                {
                    RelativePath.RightChopInline(7, EAllowShrinking::No);
                }
                else if (RelativePath.StartsWith(TEXT("Classes/")))
                {
                    RelativePath.RightChopInline(8, EAllowShrinking::No);
                }
                else if (RelativePath.StartsWith(TEXT("Private/")))
                {
                    return FString();
                }

                if (!RelativePath.IsEmpty())
                {
                    HeaderPath = SourceModule + TEXT("/") + RelativePath;
                }
            }
        }

        if (!FallbackModuleName.IsEmpty())
        {
            const FString ModuleClassesPrefix = FallbackModuleName + TEXT("/Classes/");
            const FString ModulePublicPrefix = FallbackModuleName + TEXT("/Public/");

            if (HeaderPath.StartsWith(ModuleClassesPrefix))
            {
                HeaderPath = FallbackModuleName + TEXT("/") + HeaderPath.Mid(ModuleClassesPrefix.Len());
            }
            else if (HeaderPath.StartsWith(ModulePublicPrefix))
            {
                HeaderPath = FallbackModuleName + TEXT("/") + HeaderPath.Mid(ModulePublicPrefix.Len());
            }
            else if (HeaderPath.StartsWith(TEXT("Classes/")))
            {
                HeaderPath = FallbackModuleName + TEXT("/") + HeaderPath.Mid(8);
            }
            else if (HeaderPath.StartsWith(TEXT("Public/")))
            {
                HeaderPath = FallbackModuleName + TEXT("/") + HeaderPath.Mid(7);
            }
        }

        if (HeaderPath.StartsWith(TEXT("Private/")) || HeaderPath.Contains(TEXT("/Private/")))
        {
            return FString();
        }

        if (!HeaderPath.EndsWith(TEXT(".h")))
        {
            return FString();
        }

        return HeaderPath;
    }

    FString ResolveMetadataIncludeHint(const FString& ScriptPath)
    {
        FString ModuleName;
        FString SymbolName;
        if (!TryParseScriptObjectPath(ScriptPath, ModuleName, SymbolName))
        {
            return FString();
        }

        UObject* ScriptObject = FindObject<UObject>(nullptr, *ScriptPath);
        if (!ScriptObject)
        {
            return FString();
        }

        const UField* MetadataField = Cast<UField>(ScriptObject);
        if (!MetadataField)
        {
            MetadataField = Cast<UField>(ScriptObject->GetClass());
        }

        if (!MetadataField)
        {
            return FString();
        }

        const FString ModuleRelativePath = MetadataField->GetMetaData(TEXT("ModuleRelativePath"));
        return NormalizeIncludeHintPath(ModuleRelativePath, ModuleName);
    }

    FString BuildFallbackIncludeHint(const FString& ScriptPath)
    {
        FString ModuleName;
        FString SymbolName;
        if (!TryParseScriptObjectPath(ScriptPath, ModuleName, SymbolName))
        {
            return FString();
        }

        // CR-014: Static lookup table for well-known UE/Lyra/plugin classes whose
        // ModuleRelativePath metadata may be absent or generic at runtime.
        // Key = "/Script/Module.ClassName", value = precise include path.
        static const TMap<FString, FString> KnownIncludeHints = {
            // Engine — GameFramework
            { TEXT("/Script/Engine.Actor"),                          TEXT("GameFramework/Actor.h") },
            { TEXT("/Script/Engine.Pawn"),                           TEXT("GameFramework/Pawn.h") },
            { TEXT("/Script/Engine.Character"),                      TEXT("GameFramework/Character.h") },
            { TEXT("/Script/Engine.PlayerController"),               TEXT("GameFramework/PlayerController.h") },
            { TEXT("/Script/Engine.GameMode"),                       TEXT("GameFramework/GameMode.h") },
            { TEXT("/Script/Engine.GameModeBase"),                   TEXT("GameFramework/GameModeBase.h") },
            { TEXT("/Script/Engine.GameState"),                      TEXT("GameFramework/GameState.h") },
            { TEXT("/Script/Engine.GameStateBase"),                  TEXT("GameFramework/GameStateBase.h") },
            { TEXT("/Script/Engine.PlayerState"),                    TEXT("GameFramework/PlayerState.h") },
            { TEXT("/Script/Engine.HUD"),                            TEXT("GameFramework/HUD.h") },
            { TEXT("/Script/Engine.GameInstance"),                   TEXT("Engine/GameInstance.h") },
            { TEXT("/Script/Engine.CharacterMovementComponent"),     TEXT("GameFramework/CharacterMovementComponent.h") },
            { TEXT("/Script/Engine.FloatingPawnMovement"),           TEXT("GameFramework/FloatingPawnMovement.h") },
            // Engine — Components
            { TEXT("/Script/Engine.ActorComponent"),                 TEXT("Components/ActorComponent.h") },
            { TEXT("/Script/Engine.SceneComponent"),                 TEXT("Components/SceneComponent.h") },
            { TEXT("/Script/Engine.StaticMeshComponent"),            TEXT("Components/StaticMeshComponent.h") },
            { TEXT("/Script/Engine.SkeletalMeshComponent"),          TEXT("Components/SkeletalMeshComponent.h") },
            { TEXT("/Script/Engine.PrimitiveComponent"),             TEXT("Components/PrimitiveComponent.h") },
            { TEXT("/Script/Engine.MeshComponent"),                  TEXT("Components/MeshComponent.h") },
            { TEXT("/Script/Engine.CapsuleComponent"),               TEXT("Components/CapsuleComponent.h") },
            { TEXT("/Script/Engine.SphereComponent"),                TEXT("Components/SphereComponent.h") },
            { TEXT("/Script/Engine.BoxComponent"),                   TEXT("Components/BoxComponent.h") },
            { TEXT("/Script/Engine.AudioComponent"),                 TEXT("Components/AudioComponent.h") },
            { TEXT("/Script/Engine.SpotLightComponent"),             TEXT("Components/SpotLightComponent.h") },
            { TEXT("/Script/Engine.PointLightComponent"),            TEXT("Components/PointLightComponent.h") },
            { TEXT("/Script/Engine.DirectionalLightComponent"),      TEXT("Components/DirectionalLightComponent.h") },
            { TEXT("/Script/Engine.LightComponent"),                 TEXT("Components/LightComponent.h") },
            { TEXT("/Script/Engine.ArrowComponent"),                 TEXT("Components/ArrowComponent.h") },
            { TEXT("/Script/Engine.TimelineComponent"),              TEXT("Components/TimelineComponent.h") },
            { TEXT("/Script/Engine.WidgetComponent"),                TEXT("Components/WidgetComponent.h") },
            // Engine — Core
            { TEXT("/Script/Engine.World"),                          TEXT("Engine/World.h") },
            { TEXT("/Script/Engine.Level"),                          TEXT("Engine/Level.h") },
            { TEXT("/Script/Engine.Engine"),                         TEXT("Engine/Engine.h") },
            { TEXT("/Script/Engine.Texture2D"),                      TEXT("Engine/Texture2D.h") },
            { TEXT("/Script/Engine.StaticMesh"),                     TEXT("Engine/StaticMesh.h") },
            { TEXT("/Script/Engine.SkeletalMesh"),                   TEXT("Engine/SkeletalMesh.h") },
            { TEXT("/Script/Engine.AnimSequence"),                   TEXT("Animation/AnimSequence.h") },
            { TEXT("/Script/Engine.AnimBlueprint"),                  TEXT("Animation/AnimBlueprint.h") },
            { TEXT("/Script/Engine.AnimInstance"),                   TEXT("Animation/AnimInstance.h") },
            { TEXT("/Script/Engine.AnimMontage"),                    TEXT("Animation/AnimMontage.h") },
            { TEXT("/Script/Engine.DataTable"),                      TEXT("Engine/DataTable.h") },
            { TEXT("/Script/Engine.DataAsset"),                      TEXT("Engine/DataAsset.h") },
            { TEXT("/Script/Engine.CurveFloat"),                     TEXT("Curves/CurveFloat.h") },
            { TEXT("/Script/Engine.PhysicsAsset"),                   TEXT("PhysicsEngine/PhysicsAsset.h") },
            { TEXT("/Script/Engine.UserDefinedEnum"),                TEXT("Engine/UserDefinedEnum.h") },
            // GameplayAbilities
            { TEXT("/Script/GameplayAbilities.AbilitySystemComponent"),     TEXT("AbilitySystemComponent.h") },
            { TEXT("/Script/GameplayAbilities.GameplayAbility"),            TEXT("Abilities/GameplayAbility.h") },
            { TEXT("/Script/GameplayAbilities.GameplayEffect"),             TEXT("GameplayEffect.h") },
            { TEXT("/Script/GameplayAbilities.AttributeSet"),               TEXT("AttributeSet.h") },
            { TEXT("/Script/GameplayAbilities.AbilitySystemGlobals"),       TEXT("AbilitySystemGlobals.h") },
            { TEXT("/Script/GameplayAbilities.GameplayAbilitySpec"),        TEXT("GameplayAbilitySpec.h") },
            // EnhancedInput
            { TEXT("/Script/EnhancedInput.InputAction"),                    TEXT("InputAction.h") },
            { TEXT("/Script/EnhancedInput.InputMappingContext"),            TEXT("InputMappingContext.h") },
            { TEXT("/Script/EnhancedInput.EnhancedInputComponent"),        TEXT("EnhancedInputComponent.h") },
            { TEXT("/Script/EnhancedInput.EnhancedPlayerInput"),           TEXT("EnhancedPlayerInput.h") },
            // CommonUI
            { TEXT("/Script/CommonUI.CommonUserWidget"),                    TEXT("CommonUserWidget.h") },
            { TEXT("/Script/CommonUI.CommonActivatableWidget"),             TEXT("CommonActivatableWidget.h") },
            { TEXT("/Script/CommonUI.CommonButtonBase"),                    TEXT("CommonButtonBase.h") },
            // GameFeatures
            { TEXT("/Script/GameFeatures.GameFeatureAction"),               TEXT("GameFeatureAction.h") },
            { TEXT("/Script/GameFeatures.GameFeaturesSubsystem"),           TEXT("GameFeaturesSubsystem.h") },
        };

        if (const FString* KnownHint = KnownIncludeHints.Find(ScriptPath))
        {
            return *KnownHint;
        }

        // CR-014: LyraGame module — all classes live under LyraGame/ prefix
        if (ModuleName.Equals(TEXT("LyraGame"), ESearchCase::IgnoreCase))
        {
            if (!SymbolName.StartsWith(TEXT("Default__")))
            {
                return FString::Printf(TEXT("LyraGame/%s.h"), *SymbolName);
            }
        }

        if (SymbolName.StartsWith(TEXT("Default__")))
        {
            SymbolName.RightChopInline(9, EAllowShrinking::No);
            if (SymbolName.IsEmpty())
            {
                return FString();
            }
        }

        SymbolName = SymbolName.Replace(TEXT("::"), TEXT("_"));
        return FString::Printf(TEXT("%s/%s.h"), *ModuleName, *SymbolName);
    }

    void AddDependencyPath(const FString& RawPath, FBS_DependencyClosureData& OutClosure)
    {
        TArray<FString> Candidates;
        auto AddCandidate = [&Candidates](const FString& Candidate)
        {
            if (!Candidate.IsEmpty() && !Candidates.Contains(Candidate))
            {
                Candidates.Add(Candidate);
            }
        };

        AddCandidate(NormalizeObjectPath(RawPath, false));
        AddCandidate(NormalizeObjectPath(RawPath, true));

        for (const FString& Candidate : Candidates)
        {
            if (Candidate.IsEmpty())
            {
                continue;
            }

            if (Candidate.StartsWith(TEXT("/Script/")))
            {
                AddModuleFromPath(Candidate, OutClosure.Modules);

                if (UObject* ScriptObject = FindObject<UObject>(nullptr, *Candidate))
                {
                    if (UClass* ScriptClass = Cast<UClass>(ScriptObject))
                    {
                        const FString ScriptPath = ScriptClass->GetPathName();
                        if (ScriptClass->HasAnyClassFlags(CLASS_Interface))
                        {
                            OutClosure.Interfaces.Add(ScriptPath);
                        }
                        else
                        {
                            OutClosure.Classes.Add(ScriptPath);
                        }
                    }
                    else if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(ScriptObject))
                    {
                        OutClosure.Structs.Add(ScriptStruct->GetPathName());
                    }
                    else if (UEnum* ScriptEnum = Cast<UEnum>(ScriptObject))
                    {
                        OutClosure.Enums.Add(ScriptEnum->GetPathName());
                    }
                    else if (UClass* ScriptObjectClass = ScriptObject->GetClass())
                    {
                        const FString ScriptObjectClassPath = ScriptObjectClass->GetPathName();
                        if (ScriptObjectClass->HasAnyClassFlags(CLASS_Interface))
                        {
                            OutClosure.Interfaces.Add(ScriptObjectClassPath);
                        }
                        else
                        {
                            OutClosure.Classes.Add(ScriptObjectClassPath);
                        }

                        AddModuleFromPath(ScriptObjectClassPath, OutClosure.Modules);
                    }
                    else
                    {
                        OutClosure.Classes.Add(CanonicalizeScriptClassPath(Candidate));
                    }
                }
                else
                {
                    OutClosure.Classes.Add(CanonicalizeScriptClassPath(Candidate));
                }

                continue;
            }

            if (!IsContentObjectPath(Candidate))
            {
                continue;
            }

            OutClosure.Assets.Add(Candidate);
            if (Candidate.Contains(TEXT("ControlRig")) || Candidate.Contains(TEXT("/ControlRig/")))
            {
                OutClosure.ControlRigs.Add(Candidate);
            }

            UObject* Loaded = TryLoadBestCandidate(Candidate);
            if (!Loaded)
            {
                continue;
            }

            if (UClass* ClassObj = Cast<UClass>(Loaded))
            {
                const FString ClassPath = ClassObj->GetPathName();
                if (ClassObj->HasAnyClassFlags(CLASS_Interface))
                {
                    OutClosure.Interfaces.Add(ClassPath);
                }
                else
                {
                    OutClosure.Classes.Add(ClassPath);
                }

                AddModuleFromPath(ClassPath, OutClosure.Modules);
            }
            else if (UScriptStruct* StructObj = Cast<UScriptStruct>(Loaded))
            {
                const FString StructPath = StructObj->GetPathName();
                OutClosure.Structs.Add(StructPath);
                AddModuleFromPath(StructPath, OutClosure.Modules);
            }
            else if (UEnum* EnumObj = Cast<UEnum>(Loaded))
            {
                const FString EnumPath = EnumObj->GetPathName();
                OutClosure.Enums.Add(EnumPath);
                AddModuleFromPath(EnumPath, OutClosure.Modules);
            }
            else if (UClass* LoadedClass = Loaded->GetClass())
            {
                const FString LoadedClassPath = LoadedClass->GetPathName();
                OutClosure.Classes.Add(LoadedClassPath);
                AddModuleFromPath(LoadedClassPath, OutClosure.Modules);
            }
        }
    }

    void AddDependencyText(const FString& Text, FBS_DependencyClosureData& OutClosure)
    {
        if (Text.IsEmpty())
        {
            return;
        }

        TSet<FString> DiscoveredPaths;
        CollectAssetPathsFromText(Text, DiscoveredPaths);
        for (const FString& Path : DiscoveredPaths)
        {
            AddDependencyPath(Path, OutClosure);
        }
    }

    FBS_DependencyClosureData BuildDependencyClosure(const FBS_BlueprintData& Data)
    {
        FBS_DependencyClosureData Closure;

        AddDependencyPath(Data.ParentClassPath, Closure);
        AddDependencyPath(Data.GeneratedClassPath, Closure);

        for (const FString& InterfacePath : Data.ImplementedInterfacePaths)
        {
            AddDependencyPath(InterfacePath, Closure);
            if (!InterfacePath.IsEmpty())
            {
                Closure.Interfaces.Add(InterfacePath);
            }
        }

        for (const FBS_VariableInfo& Var : Data.DetailedVariables)
        {
            AddDependencyPath(Var.TypeObjectPath, Closure);
            AddDependencyText(Var.VariableType, Closure);
            AddDependencyText(Var.DefaultValue, Closure);
        }

        for (const FBS_UserDefinedStructSchema& StructSchema : Data.UserDefinedStructSchemas)
        {
            AddDependencyPath(StructSchema.StructPath, Closure);
            for (const FBS_UserDefinedStructFieldSchema& Field : StructSchema.Fields)
            {
                AddDependencyPath(Field.TypeObjectPath, Closure);
                AddDependencyText(Field.Type, Closure);
            }
        }

        for (const FBS_UserDefinedEnumSchema& EnumSchema : Data.UserDefinedEnumSchemas)
        {
            AddDependencyPath(EnumSchema.EnumPath, Closure);
        }

        for (const FBS_FunctionInfo& Func : Data.DetailedFunctions)
        {
            AddDependencyPath(Func.ReturnTypeObjectPath, Closure);
            AddDependencyText(Func.FunctionPath, Closure);
            AddDependencyText(Func.ReturnType, Closure);

            for (const FString& Param : Func.InputParameters)
            {
                AddDependencyText(Param, Closure);
            }

            for (const FString& Param : Func.OutputParameters)
            {
                AddDependencyText(Param, Closure);
            }

            for (const FString& LocalVar : Func.LocalVariables)
            {
                AddDependencyText(LocalVar, Closure);
            }
        }

        for (const FBS_FunctionInfo& Delegate : Data.DelegateSignatures)
        {
            AddDependencyPath(Delegate.ReturnTypeObjectPath, Closure);
            AddDependencyText(Delegate.FunctionPath, Closure);
            AddDependencyText(Delegate.ReturnType, Closure);
            for (const FString& Param : Delegate.InputParameters)
            {
                AddDependencyText(Param, Closure);
            }
            for (const FString& Param : Delegate.OutputParameters)
            {
                AddDependencyText(Param, Closure);
            }
        }

        for (const FBS_ComponentInfo& Component : Data.DetailedComponents)
        {
            AddDependencyPath(Component.ComponentClass, Closure);
            AddDependencyText(Component.ParentOwnerClassName, Closure);
            AddDependencyPath(Component.InheritableOwnerClassPath, Closure);
            AddDependencyPath(Component.InheritableSourceTemplatePath, Closure);
            AddDependencyPath(Component.InheritableOverrideTemplatePath, Closure);
            AddDependencyText(Component.InheritableOwnerClassName, Closure);

            for (const FString& OverrideProperty : Component.InheritableOverrideProperties)
            {
                AddDependencyText(OverrideProperty, Closure);
            }

            for (const TPair<FString, FString>& Pair : Component.InheritableOverrideValues)
            {
                AddDependencyText(Pair.Key, Closure);
                AddDependencyText(Pair.Value, Closure);
            }

            for (const TPair<FString, FString>& Pair : Component.InheritableParentValues)
            {
                AddDependencyText(Pair.Key, Closure);
                AddDependencyText(Pair.Value, Closure);
            }

            for (const FString& Property : Component.ComponentProperties)
            {
                AddDependencyText(Property, Closure);
            }

            for (const FString& AssetRef : Component.AssetReferences)
            {
                AddDependencyPath(AssetRef, Closure);
            }
        }

        for (const FString& AssetRef : Data.AssetReferences)
        {
            AddDependencyPath(AssetRef, Closure);
        }

        for (const FBS_TimelineData& Timeline : Data.Timelines)
        {
            AddDependencyText(Timeline.UpdateFunctionName, Closure);
            AddDependencyText(Timeline.FinishedFunctionName, Closure);

            for (const FBS_TimelineTrackData& Track : Timeline.Tracks)
            {
                AddDependencyPath(Track.CurvePath, Closure);
                AddDependencyText(Track.PropertyName, Closure);
                AddDependencyText(Track.FunctionName, Closure);
            }
        }

        for (const FBS_AnimAssetData& AnimAsset : Data.AnimationAssets)
        {
            AddDependencyPath(AnimAsset.AssetPath, Closure);
            AddDependencyPath(AnimAsset.SkeletonPath, Closure);
            AddDependencyPath(AnimAsset.BlendSpaceData.BlendSpaceName, Closure);
            AddDependencyPath(AnimAsset.BlendSpaceData.BlendSpaceType, Closure);
        }

        for (const FBS_ControlRigData& Rig : Data.ControlRigs)
        {
            AddDependencyPath(Rig.SkeletonPath, Closure);
            AddDependencyText(Rig.RigName, Closure);
            for (const TPair<FString, FString>& Pair : Rig.RigProperties)
            {
                AddDependencyText(Pair.Key, Closure);
                AddDependencyText(Pair.Value, Closure);
            }
        }

        for (const FBS_GraphData_Ext& Graph : Data.StructuredGraphsExt)
        {
            for (const FBS_NodeData& Node : Graph.Nodes)
            {
                AddDependencyText(Node.MemberParent, Closure);
                AddDependencyText(Node.MemberName, Closure);

                if (Node.NodeType.Equals(TEXT("K2Node_MacroInstance"), ESearchCase::CaseSensitive))
                {
                    if (const FString* MacroGraphPath = Node.NodeProperties.Find(TEXT("meta.macroGraphPath")))
                    {
                        AddDependencyPath(*MacroGraphPath, Closure);
                        if (!MacroGraphPath->IsEmpty())
                        {
                            Closure.MacroGraphs.Add(*MacroGraphPath);
                            // CR-036: record call-site node GUID for this macro graph
                            if (Node.NodeGuid.IsValid())
                            {
                                Closure.MacroGraphCallSites.FindOrAdd(*MacroGraphPath).Add(Node.NodeGuid.ToString());
                            }
                        }
                    }

                    if (const FString* MacroBlueprintPath = Node.NodeProperties.Find(TEXT("meta.macroBlueprintPath")))
                    {
                        AddDependencyPath(*MacroBlueprintPath, Closure);
                        if (!MacroBlueprintPath->IsEmpty())
                        {
                            Closure.MacroBlueprints.Add(*MacroBlueprintPath);
                        }
                    }
                }

                for (const TPair<FString, FString>& Pair : Node.NodeProperties)
                {
                    AddDependencyText(Pair.Key, Closure);
                    AddDependencyText(Pair.Value, Closure);
                }

                for (const FBS_PinData& Pin : Node.Pins)
                {
                    AddDependencyPath(Pin.ObjectPath, Closure);
                    AddDependencyPath(Pin.DefaultObjectPath, Closure);
                    AddDependencyPath(Pin.ValueObjectPath, Closure);
                    AddDependencyText(Pin.ObjectType, Closure);
                    AddDependencyText(Pin.ValueObjectType, Closure);
                }
            }
        }

        return Closure;
    }

    TSharedPtr<FJsonObject> BuildDependencyClosureJson(const FBS_BlueprintData& Data)
    {
        const FBS_DependencyClosureData Closure = BuildDependencyClosure(Data);

        TSharedPtr<FJsonObject> ClosureJson = MakeShareable(new FJsonObject);
        ClosureJson->SetArrayField(TEXT("classPaths"), BuildStringArray(ToSortedArray(Closure.Classes)));
        ClosureJson->SetArrayField(TEXT("structPaths"), BuildStringArray(ToSortedArray(Closure.Structs)));
        ClosureJson->SetArrayField(TEXT("enumPaths"), BuildStringArray(ToSortedArray(Closure.Enums)));
        ClosureJson->SetArrayField(TEXT("interfacePaths"), BuildStringArray(ToSortedArray(Closure.Interfaces)));
        ClosureJson->SetArrayField(TEXT("assetPaths"), BuildStringArray(ToSortedArray(Closure.Assets)));
        ClosureJson->SetArrayField(TEXT("controlRigPaths"), BuildStringArray(ToSortedArray(Closure.ControlRigs)));
        ClosureJson->SetArrayField(TEXT("macroGraphPaths"), BuildStringArray(ToSortedArray(Closure.MacroGraphs)));
        ClosureJson->SetArrayField(TEXT("macroBlueprintPaths"), BuildStringArray(ToSortedArray(Closure.MacroBlueprints)));

        // CR-036: macro call-site map (macro path -> array of call-site node GUIDs)
        if (Closure.MacroGraphCallSites.Num() > 0)
        {
            TSharedPtr<FJsonObject> CallSiteMap = MakeShareable(new FJsonObject);
            TArray<FString> CallSiteKeys;
            Closure.MacroGraphCallSites.GetKeys(CallSiteKeys);
            CallSiteKeys.Sort();
            for (const FString& Key : CallSiteKeys)
            {
                const TArray<FString>& NodeIds = Closure.MacroGraphCallSites[Key];
                CallSiteMap->SetArrayField(Key, BuildStringArray(NodeIds));
            }
            ClosureJson->SetObjectField(TEXT("macroCallSiteMap"), CallSiteMap);
        }
        ClosureJson->SetArrayField(TEXT("moduleNames"), BuildStringArray(ToSortedArray(Closure.Modules)));

        TSet<FString> IncludeHints;
        TSet<FString> NativeIncludeHints;
        auto AddIncludeHint = [&IncludeHints, &NativeIncludeHints](const FString& ScriptPath)
        {
            if (!ScriptPath.StartsWith(TEXT("/Script/")))
            {
                return;
            }

            const FString NativeIncludeHint = ResolveMetadataIncludeHint(ScriptPath);
            if (!NativeIncludeHint.IsEmpty())
            {
                NativeIncludeHints.Add(NativeIncludeHint);
                IncludeHints.Add(NativeIncludeHint);
                return;
            }

            const FString FallbackIncludeHint = BuildFallbackIncludeHint(ScriptPath);
            if (!FallbackIncludeHint.IsEmpty())
            {
                IncludeHints.Add(FallbackIncludeHint);
            }
        };

        for (const FString& ClassPath : Closure.Classes) AddIncludeHint(ClassPath);
        for (const FString& StructPath : Closure.Structs) AddIncludeHint(StructPath);
        for (const FString& EnumPath : Closure.Enums) AddIncludeHint(EnumPath);
        for (const FString& InterfacePath : Closure.Interfaces) AddIncludeHint(InterfacePath);
        ClosureJson->SetArrayField(TEXT("nativeIncludeHints"), BuildStringArray(ToSortedArray(NativeIncludeHints)));
        ClosureJson->SetArrayField(TEXT("includeHints"), BuildStringArray(ToSortedArray(IncludeHints)));

        return ClosureJson;
    }
}

FBS_BlueprintData UBlueprintAnalyzer::AnalyzeBlueprint(UBlueprint* Blueprint)
{
	FBS_BlueprintData Data;
	
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("BlueprintAnalyzer: Invalid Blueprint provided"));
		return Data;
	}
	
	// Basic Blueprint information
	Data.SchemaVersion = TEXT("1.7");
	Data.BlueprintPath = Blueprint->GetPathName();
	Data.BlueprintName = Blueprint->GetName();
	Data.ParentClassName = Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None");
	Data.ParentClassPath = Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : TEXT("None");
	Data.GeneratedClassName = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetName() : TEXT("None");
	Data.GeneratedClassPath = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : TEXT("None");
	Data.ExtractionTimestamp = FDateTime::Now().ToString();
	
	UE_LOG(LogTemp, Warning, TEXT("Analyzing Blueprint: %s"), *Data.BlueprintName);
	
	// Extract Blueprint metadata and settings
	ExtractBlueprintMetadata(Blueprint, Data);
	ExtractClassParityData(Blueprint, Data);
	
	// Extract detailed Blueprint data
	Data.ImplementedInterfaces = ExtractImplementedInterfaces(Blueprint);
	for (const FBPInterfaceDescription& InterfaceDesc : Blueprint->ImplementedInterfaces)
	{
		if (InterfaceDesc.Interface)
		{
			Data.ImplementedInterfacePaths.Add(InterfaceDesc.Interface->GetPathName());
		}
	}
	Data.ImplementedInterfacePaths.Sort();
	
	// Legacy simple extraction (for backward compatibility)
	Data.Variables = ExtractVariables(Blueprint);
	Data.Functions = ExtractFunctions(Blueprint);
	Data.Components = ExtractComponents(Blueprint);
	
	// Enhanced detailed extraction
	Data.DetailedVariables = ExtractDetailedVariables(Blueprint);
	Data.DetailedFunctions = ExtractDetailedFunctions(Blueprint);
	Data.DelegateSignatures = ExtractDelegateSignatures(Blueprint);
	Data.DetailedComponents = ExtractDetailedComponents(Blueprint);
	Data.AssetReferences = ExtractAssetReferences(Blueprint);
	ExtractTimelineData(Blueprint, Data);
	ExtractWidgetBlueprintData(Blueprint, Data);
	
	Data.EventNodes = ExtractEventNodes(Blueprint);
	Data.GraphNodes = ExtractGraphNodes(Blueprint, Data.TotalNodeCount);
	
	// CR-034: Only ExtractStructuredGraphsExt is populated now (legacy FBS_GraphData removed).
	// FBS_GraphData_Ext is the canonical schema with rich FBS_NodeData structs, FBS_FlowEdge exec,
	// DataLinks, GraphInputPins, GraphOutputPins.
	Data.StructuredGraphsExt = ExtractStructuredGraphsExt(Blueprint, Data.TotalNodeCount);

	// Build node support report for converter gating
	{
		TSet<FString> UnsupportedTypes;
		TSet<FString> PartiallySupportedTypes;

		for (const FBS_GraphData_Ext& Graph : Data.StructuredGraphsExt)
		{
			for (const FBS_NodeData& Node : Graph.Nodes)
			{
				if (IsNodeTypeKnownSupported(Node.NodeType))
				{
					continue;
				}

				// EdGraphNode_Comment is a pure editor decoration — no converter IR, not unsupported
				if (Node.NodeType == TEXT("EdGraphNode_Comment"))
				{
					continue;
				}

				// AnimGraphNode_* types are handled via AnalyzeAnimNode() and the anim-specific
				// schema (animationAssets, animationCurves, controlRigs), not via K2 graph IR.
				// Exclude them from the converter-fallback unsupported list to avoid false positives.
				if (Node.NodeType.StartsWith(TEXT("AnimGraphNode_")))
				{
					continue;
				}

				if (Node.NodeType.StartsWith(TEXT("K2Node_")))
				{
					PartiallySupportedTypes.Add(Node.NodeType);
				}
				else
				{
					UnsupportedTypes.Add(Node.NodeType);
				}
			}
		}

		Data.UnsupportedNodeTypes = UnsupportedTypes.Array();
		Data.UnsupportedNodeTypes.Sort();
		Data.PartiallySupportedNodeTypes = PartiallySupportedTypes.Array();
		Data.PartiallySupportedNodeTypes.Sort();
	}

	// CR-035: node-to-bytecode trace linkage (second pass, after UnsupportedNodeTypes is known)
	if (!Data.UnsupportedNodeTypes.IsEmpty() || !Data.PartiallySupportedNodeTypes.IsEmpty())
	{
		TSet<FString> UnsupportedSet(Data.UnsupportedNodeTypes);
		TSet<FString> PartiallySupportedSet(Data.PartiallySupportedNodeTypes);
		for (FBS_FunctionInfo& FuncInfo : Data.DetailedFunctions)
		{
			if (FuncInfo.BytecodeSize <= 0) { continue; }
			for (UEdGraph* FGraph : Blueprint->FunctionGraphs)
			{
				if (!FGraph || FGraph->GetFName().ToString() != FuncInfo.FunctionName) { continue; }
				for (UEdGraphNode* GNode : FGraph->Nodes)
				{
					if (!GNode) { continue; }
					const FString GNodeType = GNode->GetClass()->GetName();
					if (UnsupportedSet.Contains(GNodeType) || PartiallySupportedSet.Contains(GNodeType))
					{
						FuncInfo.BytecodeNodeGuids.Add(GNode->NodeGuid.ToString());
						FuncInfo.BytecodeNodeTypes.Add(GNodeType);
					}
				}
				break;
			}
		}
	}

	// Gameplay-tag flow extraction from structured graph payloads (not just anim-variable heuristics).
	{
		auto IsLikelyGameplayTagLiteral = [](const FString& InValue) -> bool
		{
			FString Value = InValue;
			Value.TrimStartAndEndInline();
			if (Value.IsEmpty() || Value.Equals(TEXT("None"), ESearchCase::IgnoreCase))
			{
				return false;
			}

			if (!Value.Contains(TEXT(".")))
			{
				return false;
			}

			if (Value.Contains(TEXT(" ")) || Value.Contains(TEXT("\t")) || Value.Contains(TEXT("\n")))
			{
				return false;
			}

			if (Value.Contains(TEXT("/")) || Value.Contains(TEXT("(")) || Value.Contains(TEXT(")")) || Value.Contains(TEXT("=")) || Value.Contains(TEXT("\"")))
			{
				return false;
			}

			return true;
		};

		auto CollectTagNamesFromText = [&IsLikelyGameplayTagLiteral](const FString& Text, TSet<FString>& OutTagNames)
		{
			if (Text.IsEmpty())
			{
				return;
			}

			auto CollectQuotedTagNames = [&Text, &OutTagNames](const FString& Needle, const FString& Terminator)
			{
				int32 SearchFrom = 0;
				while (SearchFrom < Text.Len())
				{
					const int32 NeedleIndex = Text.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
					if (NeedleIndex == INDEX_NONE)
					{
						break;
					}

					const int32 ValueStart = NeedleIndex + Needle.Len();
					const int32 ValueEnd = Text.Find(Terminator, ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
					if (ValueEnd == INDEX_NONE)
					{
						break;
					}

					FString TagName = Text.Mid(ValueStart, ValueEnd - ValueStart);
					TagName.TrimStartAndEndInline();
					if (!TagName.IsEmpty() && !TagName.Equals(TEXT("None"), ESearchCase::IgnoreCase))
					{
						OutTagNames.Add(TagName);
					}

					SearchFrom = ValueEnd + Terminator.Len();
				}
			};

			CollectQuotedTagNames(TEXT("TagName=\""), TEXT("\""));
			CollectQuotedTagNames(TEXT("TagName='"), TEXT("'"));

			TArray<FString> DelimitedValues;
			Text.ParseIntoArray(DelimitedValues, TEXT(";"), true);
			if (DelimitedValues.Num() > 1)
			{
				for (FString Value : DelimitedValues)
				{
					Value.TrimStartAndEndInline();
					if (IsLikelyGameplayTagLiteral(Value))
					{
						OutTagNames.Add(Value);
					}
				}
				return;
			}

			if (IsLikelyGameplayTagLiteral(Text))
			{
				OutTagNames.Add(Text);
			}
		};

		auto UpsertGameplayTag = [&Data](const FString& TagName, const FString& Category, const FString& SourceGraph, const FString& SourceNodeType, const FString& SourceNodeTitle, const FString& SourceField)
		{
			if (TagName.IsEmpty() || TagName.Equals(TEXT("None"), ESearchCase::IgnoreCase))
			{
				return;
			}

			for (FBS_GameplayTagStateData& Existing : Data.GameplayTags)
			{
				if (Existing.TagName == TagName)
				{
					if (Existing.TagCategory.IsEmpty())
					{
						Existing.TagCategory = Category;
					}
					return;
				}
			}

			FBS_GameplayTagStateData TagData;
			TagData.TagName = TagName;
			TagData.TagCategory = Category;
			if (!SourceGraph.IsEmpty())
			{
				TagData.TagMetadata.Add(TEXT("sourceGraph"), SourceGraph);
			}
			if (!SourceNodeType.IsEmpty())
			{
				TagData.TagMetadata.Add(TEXT("sourceNodeType"), SourceNodeType);
			}
			if (!SourceNodeTitle.IsEmpty())
			{
				TagData.TagMetadata.Add(TEXT("sourceNodeTitle"), SourceNodeTitle);
			}
			if (!SourceField.IsEmpty())
			{
				TagData.TagMetadata.Add(TEXT("sourceField"), SourceField);
			}
			Data.GameplayTags.Add(MoveTemp(TagData));
		};

		// CR-034: Gameplay tag collection now uses StructuredGraphsExt (FBS_NodeData/FBS_PinData structs).
		for (const FBS_GraphData_Ext& GraphData : Data.StructuredGraphsExt)
		{
			for (const FBS_NodeData& Node : GraphData.Nodes)
			{
				const FString& NodeType = Node.NodeType;
				const FString& NodeTitle = Node.Title;

				// Scan NodeProperties (TMap<FString, FString>) for GameplayTag values
				for (const TPair<FString, FString>& Pair : Node.NodeProperties)
				{
					TSet<FString> TagNames;
					CollectTagNamesFromText(Pair.Value, TagNames);
					if (Pair.Key.Contains(TEXT("GameplayTag"), ESearchCase::IgnoreCase))
					{
						CollectTagNamesFromText(Pair.Key, TagNames);
					}
					for (const FString& TagName : TagNames)
					{
						UpsertGameplayTag(TagName, TEXT("GraphTagFlow"), GraphData.GraphName, NodeType, NodeTitle, Pair.Key);
					}
				}

				// Scan Pins for GameplayTag-typed pins
				for (const FBS_PinData& Pin : Node.Pins)
				{
					const bool bGameplayTagPin =
						Pin.ObjectPath.Contains(TEXT("/Script/GameplayTags.GameplayTag")) ||
						Pin.ObjectPath.Contains(TEXT("/Script/GameplayTags.GameplayTagContainer")) ||
						Pin.ValueObjectPath.Contains(TEXT("/Script/GameplayTags.GameplayTag")) ||
						Pin.ValueObjectPath.Contains(TEXT("/Script/GameplayTags.GameplayTagContainer")) ||
						Pin.ObjectType.Contains(TEXT("GameplayTag"), ESearchCase::IgnoreCase) ||
						Pin.SubCategory.Contains(TEXT("GameplayTag"), ESearchCase::IgnoreCase);

					if (bGameplayTagPin)
					{
						TSet<FString> TagNames;
						CollectTagNamesFromText(Pin.DefaultValue, TagNames);
						CollectTagNamesFromText(Pin.AutogeneratedDefaultValue, TagNames);
						const FString PinName = Pin.Name.ToString();
						for (const FString& TagName : TagNames)
						{
							UpsertGameplayTag(TagName, TEXT("GraphTagFlow"), GraphData.GraphName, NodeType, NodeTitle, PinName);
						}
					}
				}
			}
		}

		Data.GameplayTags.Sort([](const FBS_GameplayTagStateData& A, const FBS_GameplayTagStateData& B)
		{
			return A.TagName < B.TagName;
		});
	}

	// AnimBlueprint-specific extraction
	ExtractAnimBlueprintData(Blueprint, Data);
	ExtractUserTypeSchemas(Blueprint, Data);
	Data.GameplayTags.Sort([](const FBS_GameplayTagStateData& A, const FBS_GameplayTagStateData& B)
	{
		return A.TagName < B.TagName;
	});
	
	UE_LOG(LogTemp, Warning, TEXT("Blueprint %s analysis complete - Variables: %d, Functions: %d, Components: %d, Nodes: %d, Graphs: %d"),
		*Data.BlueprintName, Data.Variables.Num(), Data.Functions.Num(), Data.Components.Num(), Data.TotalNodeCount, Data.StructuredGraphsExt.Num());
	
	return Data;
}

TArray<FBS_BlueprintData> UBlueprintAnalyzer::AnalyzeAllProjectBlueprints()
{
	TArray<FBS_BlueprintData> AllBlueprintData;
	
	// Get Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	// Find all Blueprint assets (UE5.5 API)
	TArray<FAssetData> BlueprintAssets;
	FTopLevelAssetPath BlueprintPath(UBlueprint::StaticClass());
	AssetRegistry.GetAssetsByClass(BlueprintPath, BlueprintAssets);

	TArray<FAssetData> AnimBlueprintAssets;
	FTopLevelAssetPath AnimBlueprintPath(UAnimBlueprint::StaticClass());
	AssetRegistry.GetAssetsByClass(AnimBlueprintPath, AnimBlueprintAssets);

	TSet<FName> SeenPackages;
	TArray<FAssetData> AllAssets;
	AllAssets.Reserve(BlueprintAssets.Num() + AnimBlueprintAssets.Num());
	for (const FAssetData& AssetData : BlueprintAssets)
	{
		if (!SeenPackages.Contains(AssetData.PackageName))
		{
		    SeenPackages.Add(AssetData.PackageName);
			AllAssets.Add(AssetData);
		}
	}
	for (const FAssetData& AssetData : AnimBlueprintAssets)
	{
		if (!SeenPackages.Contains(AssetData.PackageName))
		{
		    SeenPackages.Add(AssetData.PackageName);
			AllAssets.Add(AssetData);
		}
	}

	AllAssets.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return A.GetObjectPathString() < B.GetObjectPathString();
	});
	
	UE_LOG(LogTemp, Warning, TEXT("Found %d Blueprint assets (%d AnimBlueprints) to analyze"), AllAssets.Num(), AnimBlueprintAssets.Num());
	
	// Analyze each Blueprint
	for (const FAssetData& AssetData : AllAssets)
	{
		// Load the Blueprint
		UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
		if (Blueprint)
		{
			FBS_BlueprintData Data = AnalyzeBlueprint(Blueprint);
			AllBlueprintData.Add(Data);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to load Blueprint: %s"), *AssetData.AssetName.ToString());
		}
	}

	AllBlueprintData.Sort([](const FBS_BlueprintData& A, const FBS_BlueprintData& B)
	{
		return A.BlueprintPath < B.BlueprintPath;
	});
	
	return AllBlueprintData;
}

TArray<FString> UBlueprintAnalyzer::ExtractVariables(UBlueprint* Blueprint)
{
	TArray<FString> Variables;
	
	if (!Blueprint || !Blueprint->GeneratedClass)
	{
		return Variables;
	}
	
	// Extract Blueprint variables from NewVariables array
	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		FString VarInfo = FString::Printf(TEXT("%s (%s)"), 
			*Variable.VarName.ToString(), 
			*Variable.VarType.PinCategory.ToString());
		Variables.Add(VarInfo);
	}
	
	// Also extract inherited properties
	for (TFieldIterator<FProperty> PropertyIt(Blueprint->GeneratedClass); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if (Property && Property->GetOwnerClass() == Blueprint->GeneratedClass)
		{
			FString PropInfo = FString::Printf(TEXT("%s (%s) [Inherited]"), 
				*Property->GetName(), 
				*Property->GetClass()->GetName());
			Variables.Add(PropInfo);
		}
	}
	
	return Variables;
}

TArray<FString> UBlueprintAnalyzer::ExtractFunctions(UBlueprint* Blueprint)
{
	TArray<FString> Functions;
	
	if (!Blueprint)
	{
		return Functions;
	}
	
	// Extract Blueprint functions
	for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
	{
		if (FunctionGraph)
		{
			FString FuncInfo = FString::Printf(TEXT("%s"), *FunctionGraph->GetName());
			Functions.Add(FuncInfo);
		}
	}
	
	// Extract functions from generated class
	if (Blueprint->GeneratedClass)
	{
		for (TFieldIterator<UFunction> FunctionIt(Blueprint->GeneratedClass); FunctionIt; ++FunctionIt)
		{
			UFunction* Function = *FunctionIt;
			if (Function && Function->GetOwnerClass() == Blueprint->GeneratedClass)
			{
				FString FuncInfo = FString::Printf(TEXT("%s [Generated]"), *Function->GetName());
				Functions.Add(FuncInfo);
			}
		}
	}
	
	return Functions;
}

TArray<FString> UBlueprintAnalyzer::ExtractComponents(UBlueprint* Blueprint)
{
	TArray<FString> Components;
	
	if (!Blueprint)
	{
		return Components;
	}
	
	for (USCS_Node* Node : GatherAllConstructionScriptNodes(Blueprint))
	{
		if (Node && Node->ComponentClass)
		{
			FString CompInfo = FString::Printf(TEXT("%s (%s)"), 
				*Node->GetVariableName().ToString(), 
				*Node->ComponentClass->GetName());
			Components.Add(CompInfo);
		}
	}
	
	return Components;
}

TArray<FString> UBlueprintAnalyzer::ExtractGraphNodes(UBlueprint* Blueprint, int32& OutTotalNodeCount)
{
	TArray<FString> GraphNodes;
	OutTotalNodeCount = 0;
	
	if (!Blueprint)
	{
		return GraphNodes;
	}
	
	// Keep the legacy flat surface complete with the canonical structured graph
	// surface. Composite BoundGraph objects are not present in the Blueprint root
	// graph arrays, and root arrays can alias the same graph, so traverse every
	// reachable graph exactly once with an explicit cycle guard.
	TSet<const UEdGraph*> VisitedGraphs;
	TFunction<void(UEdGraph*)> EmitReachableGraph;
	EmitReachableGraph = [&](UEdGraph* Graph)
	{
		if (!Graph || VisitedGraphs.Contains(Graph))
		{
			return;
		}

		VisitedGraphs.Add(Graph);
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			const FString NodeInfo = AnalyzeNodeWithConnections(Node);
			if (!NodeInfo.IsEmpty())
			{
				GraphNodes.Add(NodeInfo);
			}
			OutTotalNodeCount++;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (const UK2Node_Composite* Composite = Cast<UK2Node_Composite>(Node))
			{
				EmitReachableGraph(Composite->BoundGraph);
			}
		}
	};

	for (UEdGraph* Graph : Blueprint->UbergraphPages) EmitReachableGraph(Graph);
	for (UEdGraph* Graph : Blueprint->FunctionGraphs) EmitReachableGraph(Graph);
	for (UEdGraph* Graph : Blueprint->MacroGraphs) EmitReachableGraph(Graph);
	for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs) EmitReachableGraph(Graph);
	for (UEdGraph* Graph : Blueprint->EventGraphs) EmitReachableGraph(Graph);
	
	return GraphNodes;
}
TArray<FBS_GraphData_Ext> UBlueprintAnalyzer::ExtractStructuredGraphsExt(UBlueprint* Blueprint, int32& OutTotalNodeCount)
{
	TArray<FBS_GraphData_Ext> StructuredGraphs;
	OutTotalNodeCount = 0;
	TSet<const UEdGraph*> EmittedGraphs;
	
	if (!Blueprint)
	{
		return StructuredGraphs;
	}
	
	auto AddDataLinks = [](UEdGraphNode* Node, FBS_GraphData_Ext& GraphData, TSet<FString>& Seen)
	{
		if (!Node)
		{
			return;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin || !LinkedPin->GetOwningNode())
				{
					continue;
				}

				// Resolve source/target based on direction to keep data flow consistent
				FGuid SourceGuid;
				FName SourcePin;
				FGuid SourcePinId;
				FGuid TargetGuid;
				FName TargetPin;
				FGuid TargetPinId;

				if (Pin->Direction == EGPD_Output)
				{
					SourceGuid = Node->NodeGuid;
					SourcePin = Pin->PinName;
					SourcePinId = Pin->PinId;
					TargetGuid = LinkedPin->GetOwningNode()->NodeGuid;
					TargetPin = LinkedPin->PinName;
					TargetPinId = LinkedPin->PinId;
				}
				else if (Pin->Direction == EGPD_Input)
				{
					SourceGuid = LinkedPin->GetOwningNode()->NodeGuid;
					SourcePin = LinkedPin->PinName;
					SourcePinId = LinkedPin->PinId;
					TargetGuid = Node->NodeGuid;
					TargetPin = Pin->PinName;
					TargetPinId = Pin->PinId;
				}
				else
				{
					SourceGuid = Node->NodeGuid;
					SourcePin = Pin->PinName;
					SourcePinId = Pin->PinId;
					TargetGuid = LinkedPin->GetOwningNode()->NodeGuid;
					TargetPin = LinkedPin->PinName;
					TargetPinId = LinkedPin->PinId;
				}

				const FString Key = FString::Printf(TEXT("%s|%s|%s|%s|%s|%s"),
					*SourceGuid.ToString(),
					*SourcePin.ToString(),
					*TargetGuid.ToString(),
					*TargetPin.ToString(),
					*Pin->PinType.PinCategory.ToString(),
					*Pin->PinType.PinSubCategory.ToString());

				if (Seen.Contains(Key))
				{
					continue;
				}
				Seen.Add(Key);

				FBS_PinLinkData Link;
				Link.SourceNodeGuid = SourceGuid;
				Link.SourcePinName = SourcePin;
				Link.SourcePinId = SourcePinId;
				Link.TargetNodeGuid = TargetGuid;
				Link.TargetPinName = TargetPin;
				Link.TargetPinId = TargetPinId;
				Link.PinCategory = Pin->PinType.PinCategory.ToString();
				Link.PinSubCategory = Pin->PinType.PinSubCategory.ToString();
				if (Pin->PinType.PinSubCategoryObject.IsValid())
				{
					Link.PinSubCategoryObjectPath = Pin->PinType.PinSubCategoryObject->GetPathName();
				}
				Link.bIsExec = false;
				GraphData.DataLinks.Add(Link);
			}
		}
	};

	// Task 19: sweep a finished graph for FunctionEntry / FunctionResult boundary pins.
	// FunctionEntry output data pins → GraphInputPins (what flows INTO this graph from callers).
	// FunctionResult input data pins → GraphOutputPins (what flows OUT of this graph to callers).
	auto SweepGraphBoundaryPins = [&](UEdGraph* Graph, FBS_GraphData_Ext& GraphData)
	{
		if (!Graph) return;
		for (UEdGraphNode* GNode : Graph->Nodes)
		{
			if (!GNode) continue;
			if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(GNode))
			{
				for (UEdGraphPin* Pin : EntryNode->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Output &&
						Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
					{
						GraphData.GraphInputPins.Add(Pin->PinId.ToString());
					}
				}
			}
			if (UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(GNode))
			{
				for (UEdGraphPin* Pin : ResultNode->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Input &&
						Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
					{
						GraphData.GraphOutputPins.Add(Pin->PinId.ToString());
					}
				}
			}
		}
	};

	// Process Ubergraph pages (main event graph)
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && !EmittedGraphs.Contains(Graph))
		{
			EmittedGraphs.Add(Graph);
			FBS_GraphData_Ext GraphData;
			GraphData.GraphName = Graph->GetName();
			GraphData.GraphType = TEXT("Ubergraph");
			GraphData.GraphPath = Graph->GetPathName();
			TSet<FString> SeenDataLinks;
			
			// Process nodes in this graph
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node)
				{
					// Use structured node data
					FBS_NodeData NodeData = AnalyzeNodeToStruct(Node);
					GraphData.Nodes.Add(NodeData);
					
					// Exec edges as structured data
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->Direction == EGPD_Output)
						{
							for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
							{
								if (LinkedPin && LinkedPin->GetOwningNode())
								{
									FBS_FlowEdge Edge;
									Edge.SourceNodeGuid = Node->NodeGuid;
									Edge.SourcePinName = Pin->PinName;
									Edge.SourcePinId = Pin->PinId;
									Edge.TargetNodeGuid = LinkedPin->GetOwningNode()->NodeGuid;
									Edge.TargetPinName = LinkedPin->PinName;
									Edge.TargetPinId = LinkedPin->PinId;
									GraphData.Execution.Add(Edge);
								}
							}
						}
					}

					AddDataLinks(Node, GraphData, SeenDataLinks);
					
					OutTotalNodeCount++;
				}
			}
			
			// Task 19: record FunctionEntry/FunctionResult boundary pins for this graph
			SweepGraphBoundaryPins(Graph, GraphData);
			StructuredGraphs.Add(GraphData);
		}
	}
	
	// Process function graphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && !EmittedGraphs.Contains(Graph))
		{
			EmittedGraphs.Add(Graph);
			FBS_GraphData_Ext GraphData;
			GraphData.GraphName = Graph->GetName();
			GraphData.GraphType = IsConstructionScriptGraph(Graph) ? TEXT("ConstructionScript") : TEXT("Function");
			GraphData.GraphPath = Graph->GetPathName();
			TSet<FString> SeenDataLinks;
			
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node)
				{
					FBS_NodeData NodeData = AnalyzeNodeToStruct(Node);
					GraphData.Nodes.Add(NodeData);
					
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->Direction == EGPD_Output)
						{
							for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
							{
								if (LinkedPin && LinkedPin->GetOwningNode())
								{
									FBS_FlowEdge Edge;
									Edge.SourceNodeGuid = Node->NodeGuid;
									Edge.SourcePinName = Pin->PinName;
									Edge.SourcePinId = Pin->PinId;
									Edge.TargetNodeGuid = LinkedPin->GetOwningNode()->NodeGuid;
									Edge.TargetPinName = LinkedPin->PinName;
									Edge.TargetPinId = LinkedPin->PinId;
									GraphData.Execution.Add(Edge);
								}
							}
						}
					}

					AddDataLinks(Node, GraphData, SeenDataLinks);
					
					OutTotalNodeCount++;
				}
			}
			
			// Task 19: record FunctionEntry/FunctionResult boundary pins for this graph
			SweepGraphBoundaryPins(Graph, GraphData);
			StructuredGraphs.Add(GraphData);
		}
	}
	
	// Process macro graphs
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && !EmittedGraphs.Contains(Graph))
		{
			EmittedGraphs.Add(Graph);
			FBS_GraphData_Ext GraphData;
			GraphData.GraphName = Graph->GetName();
			GraphData.GraphType = TEXT("Macro");
			GraphData.GraphPath = Graph->GetPathName();
			TSet<FString> SeenDataLinks;
			
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node)
				{
					FBS_NodeData NodeData = AnalyzeNodeToStruct(Node);
					GraphData.Nodes.Add(NodeData);
					
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->Direction == EGPD_Output)
						{
							for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
							{
								if (LinkedPin && LinkedPin->GetOwningNode())
								{
									FBS_FlowEdge Edge;
									Edge.SourceNodeGuid = Node->NodeGuid;
									Edge.SourcePinName = Pin->PinName;
									Edge.SourcePinId = Pin->PinId;
									Edge.TargetNodeGuid = LinkedPin->GetOwningNode()->NodeGuid;
									Edge.TargetPinName = LinkedPin->PinName;
									Edge.TargetPinId = LinkedPin->PinId;
									GraphData.Execution.Add(Edge);
								}
							}
						}
					}

					AddDataLinks(Node, GraphData, SeenDataLinks);
					
					OutTotalNodeCount++;
				}
			}
			
			// Task 19: record FunctionEntry/FunctionResult boundary pins for this graph
			SweepGraphBoundaryPins(Graph, GraphData);
			StructuredGraphs.Add(GraphData);
		}
	}

	for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
	{
		if (Graph && !EmittedGraphs.Contains(Graph))
		{
			EmittedGraphs.Add(Graph);
			FBS_GraphData_Ext GraphData;
			GraphData.GraphName = Graph->GetName();
			GraphData.GraphType = TEXT("DelegateSignature");
			GraphData.GraphPath = Graph->GetPathName();
			TSet<FString> SeenDataLinks;

			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node)
				{
					FBS_NodeData NodeData = AnalyzeNodeToStruct(Node);
					GraphData.Nodes.Add(NodeData);

					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->Direction == EGPD_Output)
						{
							for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
							{
								if (LinkedPin && LinkedPin->GetOwningNode())
								{
									FBS_FlowEdge Edge;
									Edge.SourceNodeGuid = Node->NodeGuid;
									Edge.SourcePinName = Pin->PinName;
									Edge.SourcePinId = Pin->PinId;
									Edge.TargetNodeGuid = LinkedPin->GetOwningNode()->NodeGuid;
									Edge.TargetPinName = LinkedPin->PinName;
									Edge.TargetPinId = LinkedPin->PinId;
									GraphData.Execution.Add(Edge);
								}
							}
						}
					}

					AddDataLinks(Node, GraphData, SeenDataLinks);
					OutTotalNodeCount++;
				}
			}

			// Task 19: record FunctionEntry/FunctionResult boundary pins for this graph
			SweepGraphBoundaryPins(Graph, GraphData);
			StructuredGraphs.Add(GraphData);
		}
	}

	for (UEdGraph* Graph : Blueprint->EventGraphs)
	{
		if (Graph && !EmittedGraphs.Contains(Graph))
		{
			EmittedGraphs.Add(Graph);
			FBS_GraphData_Ext GraphData;
			GraphData.GraphName = Graph->GetName();
			GraphData.GraphType = TEXT("Event");
			GraphData.GraphPath = Graph->GetPathName();
			TSet<FString> SeenDataLinks;

			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node)
				{
					FBS_NodeData NodeData = AnalyzeNodeToStruct(Node);
					GraphData.Nodes.Add(NodeData);

					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->Direction == EGPD_Output)
						{
							for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
							{
								if (LinkedPin && LinkedPin->GetOwningNode())
								{
									FBS_FlowEdge Edge;
									Edge.SourceNodeGuid = Node->NodeGuid;
									Edge.SourcePinName = Pin->PinName;
									Edge.SourcePinId = Pin->PinId;
									Edge.TargetNodeGuid = LinkedPin->GetOwningNode()->NodeGuid;
									Edge.TargetPinName = LinkedPin->PinName;
									Edge.TargetPinId = LinkedPin->PinId;
									GraphData.Execution.Add(Edge);
								}
							}
						}
					}

					AddDataLinks(Node, GraphData, SeenDataLinks);
					OutTotalNodeCount++;
				}
			}

			// Task 19: record FunctionEntry/FunctionResult boundary pins for this graph
			SweepGraphBoundaryPins(Graph, GraphData);
			StructuredGraphs.Add(GraphData);
		}
	}
	
	// Recursively emit every authored collapsed-graph interior. BoundGraph objects are
	// not listed in Blueprint->UbergraphPages/FunctionGraphs/MacroGraphs, so the old
	// root-only sweep exposed the composite shell but silently omitted its body.
	TFunction<void(UEdGraph*, int32)> EmitCollapsedChildren;
	EmitCollapsedChildren = [&](UEdGraph* ParentGraph, const int32 ParentDepth)
	{
		if (!ParentGraph)
		{
			return;
		}

		for (UEdGraphNode* ParentNode : ParentGraph->Nodes)
		{
			UK2Node_Composite* Composite = Cast<UK2Node_Composite>(ParentNode);
			UEdGraph* BoundGraph = Composite ? Composite->BoundGraph : nullptr;
			if (!BoundGraph || EmittedGraphs.Contains(BoundGraph))
			{
				continue;
			}

			EmittedGraphs.Add(BoundGraph);
			FBS_GraphData_Ext GraphData;
			GraphData.GraphName = BoundGraph->GetName();
			GraphData.GraphType = TEXT("CollapsedGraph");
			GraphData.GraphPath = BoundGraph->GetPathName();
			GraphData.ParentGraphPath = ParentGraph->GetPathName();
			GraphData.OwningCompositeNodeGuid = Composite->NodeGuid.ToString();
			GraphData.GraphDepth = ParentDepth + 1;
			GraphData.bIsCollapsedGraph = true;
			TSet<FString> SeenDataLinks;

			for (UEdGraphNode* Node : BoundGraph->Nodes)
			{
				if (!Node)
				{
					continue;
				}

				GraphData.Nodes.Add(AnalyzeNodeToStruct(Node));
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (!Pin || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec || Pin->Direction != EGPD_Output)
					{
						continue;
					}

					for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
					{
						if (!LinkedPin || !LinkedPin->GetOwningNode())
						{
							continue;
						}

						FBS_FlowEdge Edge;
						Edge.SourceNodeGuid = Node->NodeGuid;
						Edge.SourcePinName = Pin->PinName;
						Edge.SourcePinId = Pin->PinId;
						Edge.TargetNodeGuid = LinkedPin->GetOwningNode()->NodeGuid;
						Edge.TargetPinName = LinkedPin->PinName;
						Edge.TargetPinId = LinkedPin->PinId;
						GraphData.Execution.Add(Edge);
					}
				}

				AddDataLinks(Node, GraphData, SeenDataLinks);
				OutTotalNodeCount++;
			}

			SweepGraphBoundaryPins(BoundGraph, GraphData);
			StructuredGraphs.Add(MoveTemp(GraphData));
			EmitCollapsedChildren(BoundGraph, ParentDepth + 1);
		}
	};

	for (UEdGraph* RootGraph : Blueprint->UbergraphPages) EmitCollapsedChildren(RootGraph, 0);
	for (UEdGraph* RootGraph : Blueprint->FunctionGraphs) EmitCollapsedChildren(RootGraph, 0);
	for (UEdGraph* RootGraph : Blueprint->MacroGraphs) EmitCollapsedChildren(RootGraph, 0);
	for (UEdGraph* RootGraph : Blueprint->DelegateSignatureGraphs) EmitCollapsedChildren(RootGraph, 0);
	for (UEdGraph* RootGraph : Blueprint->EventGraphs) EmitCollapsedChildren(RootGraph, 0);

	return StructuredGraphs;
}

void UBlueprintAnalyzer::ExtractAnimBlueprintData(UBlueprint* Blueprint, FBS_BlueprintData& OutData)
{
	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
	if (!AnimBP)
	{
		return;
	}

	OutData.bIsAnimBlueprint = true;
	if (AnimBP->TargetSkeleton)
	{
		OutData.TargetSkeletonPath = AnimBP->TargetSkeleton->GetPathName();
	}

	ExtractAnimationVariables(Blueprint, OutData);
	ExtractAnimGraphs(AnimBP, OutData.AnimGraph);
	ExtractLinkedLayers(AnimBP, OutData.AnimGraph);

	// Build a richer candidate list of referenced assets from graph node properties.
	TSet<FString> CandidateAssetPaths;
	for (const FString& AssetRef : OutData.AssetReferences)
	{
		AddCandidatePath(AssetRef, CandidateAssetPaths);
	}
	AddCandidatePath(OutData.TargetSkeletonPath, CandidateAssetPaths);

	auto IsLikelyAnimOrRigAssetClass = [](const FTopLevelAssetPath& ClassPath) -> bool
	{
		const FString ClassName = ClassPath.GetAssetName().ToString();
		return ClassName.Contains(TEXT("AnimSequence"), ESearchCase::CaseSensitive)
			|| ClassName.Contains(TEXT("AnimMontage"), ESearchCase::CaseSensitive)
			|| ClassName.Contains(TEXT("BlendSpace"), ESearchCase::CaseSensitive)
			|| ClassName.Contains(TEXT("PoseAsset"), ESearchCase::CaseSensitive)
			|| ClassName.Contains(TEXT("ControlRig"), ESearchCase::CaseSensitive);
	};

	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		FAssetRegistryDependencyOptions DependencyOptions;
		DependencyOptions.bIncludeSoftPackageReferences = true;
		DependencyOptions.bIncludeHardPackageReferences = true;
		DependencyOptions.bIncludeSearchableNames = false;
		DependencyOptions.bIncludeSoftManagementReferences = false;
		DependencyOptions.bIncludeHardManagementReferences = false;

		const FName BlueprintPackageName = *Blueprint->GetOutermost()->GetName();
		TArray<FName> DependencyPackages;
		if (AssetRegistry.K2_GetDependencies(BlueprintPackageName, DependencyOptions, DependencyPackages))
		{
			for (const FName& DependencyPackage : DependencyPackages)
			{
				TArray<FAssetData> DependencyAssets;
				if (!AssetRegistry.GetAssetsByPackageName(DependencyPackage, DependencyAssets, true))
				{
					continue;
				}

				for (const FAssetData& DependencyAsset : DependencyAssets)
				{
					if (!IsLikelyAnimOrRigAssetClass(DependencyAsset.AssetClassPath))
					{
						continue;
					}

					AddCandidatePath(DependencyAsset.GetObjectPathString(), CandidateAssetPaths);
				}
			}
		}
	}

	auto HarvestNodeCandidates = [&CandidateAssetPaths](const FBS_AnimNodeData& Node)
	{
		AddCandidatePath(Node.BlendSpaceAssetPath, CandidateAssetPaths);
		AddCandidatePath(Node.LinkedLayerAssetPath, CandidateAssetPaths);

		for (const TPair<FString, FString>& Prop : Node.AllProperties)
		{
			CollectAssetPathsFromText(Prop.Value, CandidateAssetPaths);
		}
	};

	for (const FBS_AnimNodeData& RootNode : OutData.AnimGraph.RootAnimNodes)
	{
		HarvestNodeCandidates(RootNode);
	}

	for (const FBS_StateMachineData& Machine : OutData.AnimGraph.StateMachines)
	{
		for (const TPair<FString, FString>& Prop : Machine.StateMachineProperties)
		{
			CollectAssetPathsFromText(Prop.Value, CandidateAssetPaths);
		}

		for (const FBS_AnimStateData& State : Machine.States)
		{
			for (const TPair<FString, FString>& Prop : State.StateMetadata)
			{
				CollectAssetPathsFromText(Prop.Value, CandidateAssetPaths);
			}

			for (const FBS_AnimNodeData& StateNode : State.AnimGraphNodes)
			{
				HarvestNodeCandidates(StateNode);
			}
		}

		for (const FBS_AnimTransitionData& Transition : Machine.Transitions)
		{
			for (const TPair<FString, FString>& Prop : Transition.TransitionProperties)
			{
				CollectAssetPathsFromText(Prop.Value, CandidateAssetPaths);
			}

			for (const FBS_NodeData& RuleNode : Transition.TransitionRuleNodes)
			{
				for (const TPair<FString, FString>& Prop : RuleNode.NodeProperties)
				{
					CollectAssetPathsFromText(Prop.Value, CandidateAssetPaths);
				}
			}
		}
	}

	for (const FBS_LinkedLayerData& Layer : OutData.AnimGraph.LinkedLayers)
	{
		AddCandidatePath(Layer.InterfaceAssetPath, CandidateAssetPaths);
		AddCandidatePath(Layer.ImplementationClassPath, CandidateAssetPaths);
	}

	TArray<FString> CandidatePathArray = CandidateAssetPaths.Array();
	CandidatePathArray.Sort();
	ExtractAnimAssets(CandidatePathArray, OutData);
	ExtractControlRigData(CandidatePathArray, OutData);

	// Populate gameplay tag summary (best-effort) from anim variables.
	auto AddGameplayTag = [&OutData](const FString& TagName, const FString& Category, const FString& SourceVariable, const FString& VariableType)
	{
		if (TagName.IsEmpty())
		{
			return;
		}

		for (FBS_GameplayTagStateData& Existing : OutData.GameplayTags)
		{
			if (Existing.TagName == TagName)
			{
				if (!SourceVariable.IsEmpty())
				{
					Existing.TagMetadata.Add(TEXT("sourceVariable"), SourceVariable);
				}
				if (!VariableType.IsEmpty())
				{
					Existing.TagMetadata.Add(TEXT("variableType"), VariableType);
				}
				return;
			}
		}

		FBS_GameplayTagStateData TagData;
		TagData.TagName = TagName;
		TagData.TagCategory = Category;
		if (!SourceVariable.IsEmpty())
		{
			TagData.TagMetadata.Add(TEXT("sourceVariable"), SourceVariable);
		}
		if (!VariableType.IsEmpty())
		{
			TagData.TagMetadata.Add(TEXT("variableType"), VariableType);
		}
		OutData.GameplayTags.Add(MoveTemp(TagData));
	};

	for (const FBS_AnimationVariableData& Variable : OutData.AnimationVariables)
	{
		if (Variable.VariableName.StartsWith(TEXT("GameplayTag_")))
		{
			const FString DerivedTagName = Variable.VariableName.RightChop(12);
			AddGameplayTag(DerivedTagName, TEXT("AnimVariablePrefix"), Variable.VariableName, Variable.VariableType);
		}
		else if (Variable.VariableType.Contains(TEXT("GameplayTag")))
		{
			AddGameplayTag(Variable.VariableName, TEXT("AnimVariableType"), Variable.VariableName, Variable.VariableType);
		}
	}

	// Populate fast-path analysis from BlueprintUsage on anim graph nodes.
	OutData.FastPathAnalysis = FBS_FastPathAnalysis();
	auto AnalyzeFastPathNode = [&OutData](const FBS_AnimNodeData& Node)
	{
		bool bIsFastPathNode = true;
		const FString* UsagePtr = Node.AllProperties.Find(TEXT("BlueprintUsage"));
		if (UsagePtr)
		{
			const FString Usage = *UsagePtr;
			if (Usage.Equals(TEXT("UsesBlueprint"), ESearchCase::IgnoreCase))
			{
				bIsFastPathNode = false;
				OutData.FastPathAnalysis.FastPathViolations.AddUnique(FString::Printf(TEXT("%s (%s) has BlueprintUsage=UsesBlueprint"), *Node.NodeType, *Node.NodeGuid.ToString()));
			}
		}

		OutData.FastPathAnalysis.NodeFastPathStatus.Add(Node.NodeGuid.ToString(), bIsFastPathNode);
		if (!bIsFastPathNode)
		{
			OutData.FastPathAnalysis.NonFastPathNodes.AddUnique(FString::Printf(TEXT("%s (%s)"), *Node.NodeType, *Node.NodeGuid.ToString()));
		}
	};

	for (const FBS_AnimNodeData& RootNode : OutData.AnimGraph.RootAnimNodes)
	{
		AnalyzeFastPathNode(RootNode);
	}
	for (const FBS_StateMachineData& Machine : OutData.AnimGraph.StateMachines)
	{
		for (const FBS_AnimStateData& State : Machine.States)
		{
			for (const FBS_AnimNodeData& Node : State.AnimGraphNodes)
			{
				AnalyzeFastPathNode(Node);
			}
		}
	}

	OutData.FastPathAnalysis.bIsFastPathCompliant = (OutData.FastPathAnalysis.NonFastPathNodes.Num() == 0);
	OutData.AnimGraph.bIsFastPathCompliant = OutData.FastPathAnalysis.bIsFastPathCompliant;

	UpdateExtractionCoverage(OutData.AnimGraph, OutData);
}

void UBlueprintAnalyzer::ExtractAnimGraphs(UAnimBlueprint* AnimBP, FBS_AnimGraphData& OutAnimGraphData)
{
	if (!AnimBP)
	{
		return;
	}

	OutAnimGraphData.GraphName = TEXT("AnimGraph");

	TArray<UEdGraph*> AllGraphs;
	AnimBP->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		const bool bIsAnimGraphName = Graph->GetFName().ToString().Contains(TEXT("AnimGraph"));
		if (bIsAnimGraphName)
		{
			ExtractObjectProperties(Graph, OutAnimGraphData.GraphProperties);
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node)
				{
					continue;
				}

				if (UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node))
				{
					FBS_AnimNodeData AnimNodeData = AnalyzeAnimNode(AnimNode);
					OutAnimGraphData.RootAnimNodes.Add(AnimNodeData);
				}

				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (!Pin || Pin->Direction != EGPD_Output)
					{
						continue;
					}

					for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
					{
						if (!LinkedPin || !LinkedPin->GetOwningNode())
						{
							continue;
						}

						FBS_FlowEdge Edge;
						Edge.SourceNodeGuid = Node->NodeGuid;
						Edge.SourcePinName = Pin->PinName;
						Edge.SourcePinId = Pin->PinId;
						Edge.TargetNodeGuid = LinkedPin->GetOwningNode()->NodeGuid;
						Edge.TargetPinName = LinkedPin->PinName;
						Edge.TargetPinId = LinkedPin->PinId;
						OutAnimGraphData.RootPoseConnections.Add(Edge);
					}
				}
			}
		}

		if (UAnimationStateMachineGraph* SMGraph = Cast<UAnimationStateMachineGraph>(Graph))
		{
			ExtractStateMachine(SMGraph, OutAnimGraphData);
		}
	}
}

void UBlueprintAnalyzer::ExtractStateMachine(UAnimationStateMachineGraph* SMGraph, FBS_AnimGraphData& OutAnimGraphData)
{
	if (!SMGraph)
	{
		return;
	}

	FBS_StateMachineData SMData;
	SMData.MachineName = SMGraph->GetName();
	SMData.MachineGuid = SMGraph->GraphGuid;
	ExtractObjectProperties(SMGraph, SMData.StateMachineProperties);

	TMap<FGuid, FString> StateNamesByGuid;

	for (UEdGraphNode* NodeObject : SMGraph->Nodes)
	{
		if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(NodeObject))
		{
			FBS_AnimStateData StateData;
			StateData.StateName = StateNode->GetStateName();
			StateData.StateGuid = StateNode->NodeGuid;
			StateData.StateType = TEXT("State");
			if (UEdGraphPin* OutputPin = StateNode->GetOutputPin())
			{
				StateData.bIsEndState = (OutputPin->LinkedTo.Num() == 0);
			}
			StateData.PosX = StateNode->NodePosX;
			StateData.PosY = StateNode->NodePosY;
			ExtractObjectProperties(StateNode, StateData.StateMetadata);

			if (UEdGraph* StateGraph = StateNode->GetBoundGraph())
			{
				for (UEdGraphNode* GraphNode : StateGraph->Nodes)
				{
					if (UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(GraphNode))
					{
						StateData.AnimGraphNodes.Add(AnalyzeAnimNode(AnimNode));
					}

					if (!GraphNode)
					{
						continue;
					}

					for (UEdGraphPin* Pin : GraphNode->Pins)
					{
						if (!Pin || Pin->Direction != EGPD_Output)
						{
							continue;
						}

						for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
						{
							if (!LinkedPin || !LinkedPin->GetOwningNode())
							{
								continue;
							}

							FBS_FlowEdge Edge;
							Edge.SourceNodeGuid = GraphNode->NodeGuid;
							Edge.SourcePinName = Pin->PinName;
							Edge.SourcePinId = Pin->PinId;
							Edge.TargetNodeGuid = LinkedPin->GetOwningNode()->NodeGuid;
							Edge.TargetPinName = LinkedPin->PinName;
							Edge.TargetPinId = LinkedPin->PinId;
							StateData.PoseConnections.Add(Edge);
						}
					}
				}
			}

			SMData.States.Add(StateData);
			StateNamesByGuid.Add(StateData.StateGuid, StateData.StateName);
		}
		else if (UAnimStateConduitNode* ConduitNode = Cast<UAnimStateConduitNode>(NodeObject))
		{
			FBS_AnimStateData StateData;
			StateData.StateName = ConduitNode->GetStateName();
			StateData.StateGuid = ConduitNode->NodeGuid;
			StateData.StateType = TEXT("Conduit");
			StateData.PosX = ConduitNode->NodePosX;
			StateData.PosY = ConduitNode->NodePosY;
			ExtractObjectProperties(ConduitNode, StateData.StateMetadata);

			SMData.States.Add(StateData);
			StateNamesByGuid.Add(StateData.StateGuid, StateData.StateName);
		}
#if UEARATAME_HAS_ANIM_STATE_ALIAS
		else if (UAnimStateAliasNode* AliasNode = Cast<UAnimStateAliasNode>(NodeObject))
		{
			FBS_AnimStateData StateData;
			StateData.StateName = AliasNode->GetStateName();
			StateData.StateGuid = AliasNode->NodeGuid;
			StateData.StateType = TEXT("Aliased");
			StateData.PosX = AliasNode->NodePosX;
			StateData.PosY = AliasNode->NodePosY;
			ExtractObjectProperties(AliasNode, StateData.StateMetadata);

			SMData.States.Add(StateData);
			StateNamesByGuid.Add(StateData.StateGuid, StateData.StateName);
		}
#endif
	}

	for (UEdGraphNode* NodeObject : SMGraph->Nodes)
	{
		if (UAnimStateEntryNode* EntryNode = Cast<UAnimStateEntryNode>(NodeObject))
		{
			UEdGraphPin* OutputPin = nullptr;
			for (UEdGraphPin* Pin : EntryNode->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Output)
				{
					OutputPin = Pin;
					break;
				}
			}

			if (OutputPin && OutputPin->LinkedTo.Num() > 0)
			{
				UEdGraphNode* LinkedNode = OutputPin->LinkedTo[0]->GetOwningNode();
				if (LinkedNode)
				{
					SMData.EntryStateGuid = LinkedNode->NodeGuid;
				}
			}
			break;
		}
	}

	for (UEdGraphNode* NodeObject : SMGraph->Nodes)
	{
		if (UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(NodeObject))
		{
			FBS_AnimTransitionData TransData;
			UAnimStateNodeBase* PrevState = TransitionNode->GetPreviousState();
			UAnimStateNodeBase* NextState = TransitionNode->GetNextState();
			TransData.SourceStateGuid = PrevState ? PrevState->NodeGuid : FGuid();
			TransData.TargetStateGuid = NextState ? NextState->NodeGuid : FGuid();
			TransData.TransitionGuid = TransitionNode->NodeGuid;
			TransData.Priority = TransitionNode->PriorityOrder;
			TransData.CrossfadeDuration = TransitionNode->CrossfadeDuration;
			if (const UEnum* BlendModeEnum = StaticEnum<EAlphaBlendOption>())
			{
				TransData.BlendMode = BlendModeEnum->GetNameStringByValue(static_cast<int64>(TransitionNode->BlendMode));
			}
			TransData.BlendOutTriggerTime = TransitionNode->AutomaticRuleTriggerTime;
			TransData.NotifyStateIndex = FString::FromInt(TransitionNode->TransitionInterrupt.TrackIndex);
			TransData.bAutomaticRuleBasedOnSequencePlayer = TransitionNode->bAutomaticRuleBasedOnSequencePlayerInState;

			if (const FString* SourceName = StateNamesByGuid.Find(TransData.SourceStateGuid))
			{
				TransData.SourceStateName = *SourceName;
			}
			if (const FString* TargetName = StateNamesByGuid.Find(TransData.TargetStateGuid))
			{
				TransData.TargetStateName = *TargetName;
			}

			ExtractObjectProperties(TransitionNode, TransData.TransitionProperties);
			const FName StartNotifyName = TransitionNode->TransitionStart.GetNotifyEventName();
			if (!StartNotifyName.IsNone())
			{
				TransData.TransitionProperties.Add(TEXT("transitionStartEventName"), StartNotifyName.ToString());
			}
			const FName EndNotifyName = TransitionNode->TransitionEnd.GetNotifyEventName();
			if (!EndNotifyName.IsNone())
			{
				TransData.TransitionProperties.Add(TEXT("transitionEndEventName"), EndNotifyName.ToString());
			}
			const FName InterruptNotifyName = TransitionNode->TransitionInterrupt.GetNotifyEventName();
			if (!InterruptNotifyName.IsNone())
			{
				TransData.TransitionProperties.Add(TEXT("transitionInterruptEventName"), InterruptNotifyName.ToString());
			}

			if (TransitionNode->BoundGraph)
			{
				ExtractTransitionRuleGraph(TransitionNode, TransData);
			}

			SMData.Transitions.Add(TransData);
		}
	}

	OutAnimGraphData.StateMachines.Add(SMData);
}

void UBlueprintAnalyzer::ExtractTransitionRuleGraph(UAnimStateTransitionNode* TransitionNode, FBS_AnimTransitionData& OutTransData)
{
	if (!TransitionNode || !TransitionNode->BoundGraph)
	{
		return;
	}

	UEdGraph* RuleGraph = TransitionNode->BoundGraph;
	OutTransData.TransitionRuleName = RuleGraph->GetName();

	for (UEdGraphNode* Node : RuleGraph->Nodes)
	{
		FBS_NodeData NodeData = AnalyzeNodeToStruct(Node);
		OutTransData.TransitionRuleNodes.Add(NodeData);
	}

	for (UEdGraphNode* Node : RuleGraph->Nodes)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin || !LinkedPin->GetOwningNode())
				{
					continue;
				}

				FBS_PinLinkData Link;
				Link.SourceNodeGuid = Node->NodeGuid;
				Link.SourcePinName = Pin->PinName;
				Link.SourcePinId = Pin->PinId;
				Link.TargetNodeGuid = LinkedPin->GetOwningNode()->NodeGuid;
				Link.TargetPinName = LinkedPin->PinName;
				Link.TargetPinId = LinkedPin->PinId;
				Link.PinCategory = Pin->PinType.PinCategory.ToString();
				if (UObject* SubCategoryObj = Pin->PinType.PinSubCategoryObject.Get())
				{
					Link.PinSubCategory = SubCategoryObj->GetName();
				}
				else
				{
					Link.PinSubCategory = TEXT("");
				}
				Link.bIsExec = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
				OutTransData.RulePinLinks.Add(Link);

				if (Link.bIsExec)
				{
					FBS_FlowEdge Edge;
					Edge.SourceNodeGuid = Link.SourceNodeGuid;
					Edge.SourcePinName = Link.SourcePinName;
					Edge.SourcePinId = Link.SourcePinId;
					Edge.TargetNodeGuid = Link.TargetNodeGuid;
					Edge.TargetPinName = Link.TargetPinName;
					Edge.TargetPinId = Link.TargetPinId;
					OutTransData.RuleExecutionFlow.Add(Edge);
				}
			}
		}
	}
}

FBS_AnimNodeData UBlueprintAnalyzer::AnalyzeAnimNode(UAnimGraphNode_Base* AnimNode)
{
	FBS_AnimNodeData Data;
	if (!AnimNode)
	{
		return Data;
	}

	const FBS_NodeData NodeData = AnalyzeNodeToStruct(AnimNode);
	Data.NodeGuid = NodeData.NodeGuid;
	Data.NodeType = NodeData.NodeType;
	Data.NodeTitle = NodeData.Title;
	Data.PosX = NodeData.PosX;
	Data.PosY = NodeData.PosY;
	Data.Pins = NodeData.Pins;

	if (UAnimGraphNode_BlendSpacePlayer* BlendNode = Cast<UAnimGraphNode_BlendSpacePlayer>(AnimNode))
	{
		if (UBlendSpace* BlendSpace = BlendNode->Node.GetBlendSpace())
		{
			Data.BlendSpaceAssetPath = BlendSpace->GetPathName();
			const int32 NumParams = BlendSpace->IsA<UBlendSpace1D>() ? 1 : 2;
			for (int32 Index = 0; Index < NumParams; ++Index)
			{
				const FBlendParameter& Param = BlendSpace->GetBlendParameter(Index);
				Data.BlendSpaceAxes.Add(Param.DisplayName);
			}
		}
	}
	else if (UAnimGraphNode_LayeredBoneBlend* LayeredNode = Cast<UAnimGraphNode_LayeredBoneBlend>(AnimNode))
	{
		for (const FInputBlendPose& Layer : LayeredNode->Node.LayerSetup)
		{
			for (const FBranchFilter& Filter : Layer.BranchFilters)
			{
				Data.BoneLayers.Add(FString::Printf(TEXT("%s:%d"), *Filter.BoneName.ToString(), Filter.BlendDepth));
			}
		}
	}
	else if (UAnimGraphNode_Inertialization* InertNode = Cast<UAnimGraphNode_Inertialization>(AnimNode))
	{
		// Durations are captured via property serialization on the editor node.
		(void)InertNode;
	}
	else if (UAnimGraphNode_Slot* SlotNode = Cast<UAnimGraphNode_Slot>(AnimNode))
	{
		if (FProperty* SlotNameProp = FindFProperty<FProperty>(SlotNode->GetClass(), TEXT("SlotName")))
		{
			Data.SlotName = GetPropertyValueAsString(SlotNode, SlotNameProp);
		}
		if (FProperty* GroupProp = FindFProperty<FProperty>(SlotNode->GetClass(), TEXT("GroupName")))
		{
			Data.SlotGroupName = GetPropertyValueAsString(SlotNode, GroupProp);
		}
	}
	else if (UAnimGraphNode_LinkedInputPose* LinkedNode = Cast<UAnimGraphNode_LinkedInputPose>(AnimNode))
	{
		if (FProperty* NameProp = FindFProperty<FProperty>(LinkedNode->GetClass(), TEXT("Name")))
		{
			Data.LinkedLayerName = GetPropertyValueAsString(LinkedNode, NameProp);
		}
		else if (FProperty* PoseNameProp = FindFProperty<FProperty>(LinkedNode->GetClass(), TEXT("InputPoseName")))
		{
			Data.LinkedLayerName = GetPropertyValueAsString(LinkedNode, PoseNameProp);
		}
	}
#if UEARATAME_HAS_LINKED_ANIM_LAYER
	else if (UAnimGraphNode_LinkedAnimLayer* LinkedLayerNode = Cast<UAnimGraphNode_LinkedAnimLayer>(AnimNode))
	{
		if (FProperty* LayerProp = FindFProperty<FProperty>(LinkedLayerNode->GetClass(), TEXT("Layer")))
		{
			Data.LinkedLayerName = GetPropertyValueAsString(LinkedLayerNode, LayerProp);
		}
	}
#endif

	ExtractObjectProperties(AnimNode, Data.AllProperties);
	return Data;
}

void UBlueprintAnalyzer::ExtractLinkedLayers(UAnimBlueprint* AnimBP, FBS_AnimGraphData& OutAnimGraphData)
{
	if (!AnimBP)
	{
		return;
	}

	auto GetObjectPathFromProperty = [](UObject* Object, const TCHAR* PropertyName) -> FString
	{
		if (!Object)
		{
			return FString();
		}

		if (FObjectProperty* ObjProp = FindFProperty<FObjectProperty>(Object->GetClass(), PropertyName))
		{
			if (UObject* Value = ObjProp->GetObjectPropertyValue_InContainer(Object))
			{
				return Value->GetPathName();
			}
		}
		if (FSoftObjectProperty* SoftProp = FindFProperty<FSoftObjectProperty>(Object->GetClass(), PropertyName))
		{
			const FSoftObjectPtr SoftPtr = SoftProp->GetPropertyValue_InContainer(Object);
			return SoftPtr.ToSoftObjectPath().ToString();
		}

		return FString();
	};

	TArray<UEdGraph*> AllGraphs;
	AnimBP->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			FBS_LinkedLayerData LayerData;
			bool bIsLinkedLayerNode = false;

			if (UAnimGraphNode_LinkedInputPose* LinkedNode = Cast<UAnimGraphNode_LinkedInputPose>(Node))
			{
				LayerData.InterfaceName = LinkedNode->Node.Name.ToString();
				LayerData.InterfaceAssetPath = GetObjectPathFromProperty(LinkedNode, TEXT("LinkedAnimGraphInterface"));
				bIsLinkedLayerNode = true;
			}
#if UEARATAME_HAS_LINKED_ANIM_LAYER
			else if (UAnimGraphNode_LinkedAnimLayer* LinkedLayerNode = Cast<UAnimGraphNode_LinkedAnimLayer>(Node))
			{
				LayerData.InterfaceName = LinkedLayerNode->Node.Layer.ToString();
				LayerData.InterfaceAssetPath = GetObjectPathFromProperty(LinkedLayerNode, TEXT("Interface"));
				bIsLinkedLayerNode = true;
			}
#endif

			if (bIsLinkedLayerNode)
			{
				LayerData.ImplementationClassPath = GetObjectPathFromProperty(Node, TEXT("LinkedAnimClass"));
				OutAnimGraphData.LinkedLayers.Add(LayerData);
			}
		}
	}
}

void UBlueprintAnalyzer::ExtractAnimationVariables(UBlueprint* Blueprint, FBS_BlueprintData& OutData)
{
	if (!Blueprint)
	{
		return;
	}

	for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
	{
		FBS_AnimationVariableData VarData;
		VarData.VariableName = VarDesc.VarName.ToString();
		VarData.VariableType = VarDesc.VarType.PinCategory.ToString();
		if (VarDesc.VarType.PinSubCategoryObject.IsValid())
		{
			VarData.VariableType += FString::Printf(TEXT(" (%s)"), *VarDesc.VarType.PinSubCategoryObject->GetName());
		}
		VarData.DefaultValue = ResolveBlueprintVariableDefaultValue(Blueprint, VarDesc);
		VarData.Category = VarDesc.Category.ToString();
		OutData.AnimationVariables.Add(VarData);
	}
}

void UBlueprintAnalyzer::UpdateExtractionCoverage(const FBS_AnimGraphData& AnimGraphData, FBS_BlueprintData& OutData)
{
	FBS_ExtractionCoverageData Coverage;
	int32 StateCount = 0;
	int32 TransitionCount = 0;

	for (const FBS_StateMachineData& Machine : AnimGraphData.StateMachines)
	{
		StateCount += Machine.States.Num();
		TransitionCount += Machine.Transitions.Num();

		for (const FBS_AnimStateData& State : Machine.States)
		{
			for (const FBS_AnimNodeData& Node : State.AnimGraphNodes)
			{
				Coverage.NodeTypeCounts.FindOrAdd(Node.NodeType)++;
			}
		}
	}

	for (const FBS_AnimNodeData& Node : AnimGraphData.RootAnimNodes)
	{
		Coverage.NodeTypeCounts.FindOrAdd(Node.NodeType)++;
	}

	Coverage.TotalStateNodes = StateCount;
	Coverage.TotalTransitions = TransitionCount;
	Coverage.TotalAnimAssets = OutData.AnimationAssets.Num();
	Coverage.TotalControlRigs = OutData.ControlRigs.Num();

	int32 NotifyCount = 0;
	int32 CurveCount = 0;
	for (const FBS_AnimAssetData& AssetData : OutData.AnimationAssets)
	{
		NotifyCount += AssetData.Notifies.Num();
		CurveCount += AssetData.Curves.Num();
	}
	Coverage.TotalNotifies = NotifyCount;
	Coverage.TotalCurves = CurveCount;

	OutData.Coverage = Coverage;
}

void UBlueprintAnalyzer::ExtractAnimAssets(const TArray<FString>& AssetPaths, FBS_BlueprintData& OutData)
{
	TSet<FString> SeenAssets;

	for (const FString& RawAssetPath : AssetPaths)
	{
		if (RawAssetPath.IsEmpty())
		{
			continue;
		}

		UObject* Asset = TryLoadBestCandidate(RawAssetPath);
		if (!Asset || !Asset->IsA<UAnimationAsset>())
		{
			continue;
		}

		const FString ResolvedAssetPath = NormalizeObjectPath(Asset->GetPathName(), true);
		if (ResolvedAssetPath.IsEmpty() || SeenAssets.Contains(ResolvedAssetPath))
		{
			continue;
		}
		SeenAssets.Add(ResolvedAssetPath);

		FBS_AnimAssetData AssetData;
		AssetData.AssetPath = ResolvedAssetPath;
		AssetData.AssetName = Asset->GetName();
		ExtractObjectProperties(Asset, AssetData.AssetProperties);

		if (UAnimMontage* Montage = Cast<UAnimMontage>(Asset))
		{
			AssetData.AssetType = TEXT("Montage");
			AssetData.Length = Montage->GetPlayLength();
			ExtractObjectProperties(Montage, AssetData.MontageProperties);
			ExtractAnimSequenceNotifies(Montage, AssetData);
			ExtractMontageSections(Montage, AssetData);
			if (USkeleton* Skeleton = Montage->GetSkeleton())
			{
				AssetData.SkeletonPath = Skeleton->GetPathName();
			}
		}
		else if (UBlendSpace* BlendSpace = Cast<UBlendSpace>(Asset))
		{
			AssetData.AssetType = TEXT("BlendSpace");
			ExtractBlendSpaceData(BlendSpace, AssetData);
			if (USkeleton* Skeleton = BlendSpace->GetSkeleton())
			{
				AssetData.SkeletonPath = Skeleton->GetPathName();
			}
		}
		else if (UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(Asset))
		{
			AssetData.AssetType = TEXT("Sequence");
			AssetData.Length = SequenceBase->GetPlayLength();
			if (UAnimSequence* Sequence = Cast<UAnimSequence>(SequenceBase))
			{
				AssetData.FrameRate = Sequence->GetSamplingFrameRate().AsDecimal();
				AssetData.TotalFrames = FMath::RoundToInt(AssetData.Length * AssetData.FrameRate);
				ExtractAnimationCurves(Sequence, AssetData);
			}
			ExtractAnimSequenceNotifies(SequenceBase, AssetData);
			if (USkeleton* Skeleton = SequenceBase->GetSkeleton())
			{
				AssetData.SkeletonPath = Skeleton->GetPathName();
			}
		}
		else
		{
			continue;
		}

		OutData.AnimationAssets.Add(AssetData);
	}
}

void UBlueprintAnalyzer::ExtractAnimSequenceNotifies(UAnimSequenceBase* Sequence, FBS_AnimAssetData& OutData)
{
	if (!Sequence)
	{
		return;
	}

	float FrameRate = 30.0f;
	if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(Sequence))
	{
		FrameRate = AnimSequence->GetSamplingFrameRate().AsDecimal();
	}

	auto ResolveTrackName = [Sequence](int32 TrackIndex) -> FString
	{
		if (TrackIndex >= 0 && TrackIndex < Sequence->AnimNotifyTracks.Num())
		{
			return Sequence->AnimNotifyTracks[TrackIndex].TrackName.ToString();
		}

		return FString();
	};

	for (const FAnimNotifyEvent& NotifyEvent : Sequence->Notifies)
	{
		FBS_AnimNotifyData NotifyData;
		NotifyData.NotifyName = NotifyEvent.NotifyName.ToString();
		NotifyData.TriggerTime = NotifyEvent.GetTriggerTime();
		NotifyData.TriggerFrame = FrameRate > 0.0f ? FMath::RoundToInt(NotifyData.TriggerTime * FrameRate) : 0;
		NotifyData.Duration = NotifyEvent.GetDuration();
		NotifyData.TrackIndex = NotifyEvent.TrackIndex;
		NotifyData.TrackName = ResolveTrackName(NotifyData.TrackIndex);

		const FName TriggerEventName = NotifyEvent.GetNotifyEventName();
		if (!TriggerEventName.IsNone())
		{
			NotifyData.TriggeredEventName = TriggerEventName.ToString();
		}

		if (NotifyEvent.NotifyStateClass)
		{
			NotifyData.NotifyClass = NotifyEvent.NotifyStateClass->GetClass()->GetName();
			NotifyData.bIsNotifyState = true;
			ExtractObjectProperties(NotifyEvent.NotifyStateClass, NotifyData.NotifyProperties);
		}
		else if (NotifyEvent.Notify)
		{
			NotifyData.NotifyClass = NotifyEvent.Notify->GetClass()->GetName();
			NotifyData.bIsNotifyState = false;
			ExtractObjectProperties(NotifyEvent.Notify, NotifyData.NotifyProperties);
		}

		if (!NotifyData.NotifyProperties.IsEmpty())
		{
			NotifyData.NotifyProperties.GetKeys(NotifyData.EventParameters);
			NotifyData.EventParameters.Sort();
		}

		OutData.Notifies.Add(NotifyData);
	}
}

void UBlueprintAnalyzer::ExtractAnimationCurves(UAnimSequence* Sequence, FBS_AnimAssetData& OutData)
{
	if (!Sequence)
	{
		return;
	}

	const IAnimationDataModel* DataModel = Sequence->GetDataModel();
	if (!DataModel)
	{
		return;
	}

	USkeleton* Skeleton = Sequence->GetSkeleton();
	const FString SequencePath = Sequence->GetPathName();

	auto PopulateCurveMeta = [&Skeleton](FBS_AnimCurveData& CurveData, const FName& CurveName)
	{
		if (!Skeleton)
		{
			return;
		}

		if (const FCurveMetaData* CurveMeta = Skeleton->GetCurveMetaData(CurveName))
		{
			if (CurveMeta->Type.bMaterial)
			{
				CurveData.AffectedMaterials.AddUnique(CurveName.ToString());
			}
			if (CurveMeta->Type.bMorphtarget)
			{
				CurveData.AffectedMorphTargets.AddUnique(CurveName.ToString());
			}
		}
	};

	for (const FFloatCurve& FloatCurve : DataModel->GetFloatCurves())
	{
		FBS_AnimCurveData CurveData;
		CurveData.CurveName = FloatCurve.GetName().ToString();
		CurveData.CurveType = TEXT("FloatCurve");
		CurveData.CurveAssetPath = SequencePath;
		CurveData.InterpolationMode = DescribeInterpolationMode(FloatCurve.FloatCurve);
		CurveData.AxisType = TEXT("Scalar");

		for (const FRichCurveKey& Key : FloatCurve.FloatCurve.Keys)
		{
			CurveData.KeyTimes.Add(Key.Time);
			CurveData.KeyValues.Add(Key.Value);
		}

		PopulateCurveMeta(CurveData, FloatCurve.GetName());

		OutData.Curves.Add(CurveData);
	}

	auto AddVectorCurve = [&](const FVectorCurve& SourceCurve, const FString& AxisType)
	{
		FBS_AnimCurveData CurveData;
		CurveData.CurveName = SourceCurve.GetName().ToString();
		CurveData.CurveType = TEXT("VectorCurve");
		CurveData.CurveAssetPath = SequencePath;
		CurveData.AxisType = AxisType;
		CurveData.InterpolationMode = DescribeInterpolationMode(SourceCurve.FloatCurves[0]);
		SourceCurve.GetKeys(CurveData.KeyTimes, CurveData.VectorValues);
		PopulateCurveMeta(CurveData, SourceCurve.GetName());
		OutData.Curves.Add(CurveData);
	};

	for (const FTransformCurve& TransformCurve : DataModel->GetTransformCurves())
	{
		AddVectorCurve(TransformCurve.TranslationCurve, TEXT("Translation"));
		AddVectorCurve(TransformCurve.RotationCurve, TEXT("Rotation"));
		AddVectorCurve(TransformCurve.ScaleCurve, TEXT("Scale"));

		FBS_AnimCurveData CurveData;
		CurveData.CurveName = TransformCurve.GetName().ToString();
		CurveData.CurveType = TEXT("TransformCurve");
		CurveData.CurveAssetPath = SequencePath;
		CurveData.AxisType = TEXT("Transform");
		CurveData.InterpolationMode = DescribeInterpolationMode(TransformCurve.TranslationCurve.FloatCurves[0]);

		TArray<FTransform> TransformValues;
		TransformCurve.GetKeys(CurveData.KeyTimes, TransformValues);
		CurveData.TransformValues.Reserve(TransformValues.Num());
		for (const FTransform& TransformValue : TransformValues)
		{
			CurveData.TransformValues.Add(BuildTransformString(TransformValue));
		}

		PopulateCurveMeta(CurveData, TransformCurve.GetName());
		OutData.Curves.Add(CurveData);
	}
}

void UBlueprintAnalyzer::ExtractMontageSections(UAnimMontage* Montage, FBS_AnimAssetData& OutData)
{
	if (!Montage)
	{
		return;
	}

	for (const FCompositeSection& Section : Montage->CompositeSections)
	{
		FBS_MontageSectionData SectionData;
		SectionData.SectionName = Section.SectionName.ToString();
		float SectionStartTime = 0.0f;
		float SectionEndTime = 0.0f;
		const int32 SectionIndex = Montage->GetSectionIndex(Section.SectionName);
		if (SectionIndex != INDEX_NONE)
		{
			Montage->GetSectionStartAndEndTime(SectionIndex, SectionStartTime, SectionEndTime);
		}
		SectionData.StartTime = SectionStartTime;
		SectionData.NextSectionName = Section.NextSectionName.ToString();
		if (Section.NextSectionName != NAME_None)
		{
			float NextStartTime = 0.0f;
			float NextEndTime = 0.0f;
			const int32 NextIndex = Montage->GetSectionIndex(Section.NextSectionName);
			if (NextIndex != INDEX_NONE)
			{
				Montage->GetSectionStartAndEndTime(NextIndex, NextStartTime, NextEndTime);
				SectionData.NextSectionTime = NextStartTime;
			}
		}
		OutData.Sections.Add(SectionData);
	}

	for (const FSlotAnimationTrack& SlotTrack : Montage->SlotAnimTracks)
	{
		OutData.SlotNames.Add(SlotTrack.SlotName.ToString());
	}
}

void UBlueprintAnalyzer::ExtractBlendSpaceData(UBlendSpace* BlendSpace, FBS_AnimAssetData& OutData)
{
	if (!BlendSpace)
	{
		return;
	}

	FBS_BlendSpaceData BlendData;
	BlendData.BlendSpaceName = BlendSpace->GetName();
	BlendData.BlendSpaceType = BlendSpace->IsA<UBlendSpace1D>() ? TEXT("1D") : TEXT("2D");

	const int32 NumParams = BlendSpace->IsA<UBlendSpace1D>() ? 1 : 2;
	for (int32 Index = 0; Index < NumParams; ++Index)
	{
		const FBlendParameter& Param = BlendSpace->GetBlendParameter(Index);
		BlendData.AxisNames.Add(Param.DisplayName);
		BlendData.AxisMinValues.Add(Param.Min);
		BlendData.AxisMaxValues.Add(Param.Max);
	}

	for (const FBlendSample& Sample : BlendSpace->GetBlendSamples())
	{
		FBS_BlendSpaceSamplePoint Point;
		if (Sample.Animation)
		{
			Point.AnimationName = Sample.Animation->GetName();
		}
		Point.AxisValues.Add(Sample.SampleValue.X);
		if (NumParams > 1)
		{
			Point.AxisValues.Add(Sample.SampleValue.Y);
		}
		BlendData.SamplePoints.Add(Point);
	}

	ExtractObjectProperties(BlendSpace, BlendData.BlendSpaceProperties);
	OutData.BlendSpaceData = BlendData;
}

void UBlueprintAnalyzer::ExtractControlRigData(const TArray<FString>& AssetPaths, FBS_BlueprintData& OutData)
{
#if UEARATAME_HAS_CONTROL_RIG
	TSet<FString> SeenAssets;
	for (const FString& RawAssetPath : AssetPaths)
	{
		if (RawAssetPath.IsEmpty())
		{
			continue;
		}

		UObject* Asset = TryLoadBestCandidate(RawAssetPath);
		UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(Asset);

		if (!RigBlueprint)
		{
			if (UClass* RigClass = Cast<UClass>(Asset))
			{
				RigBlueprint = Cast<UControlRigBlueprint>(RigClass->ClassGeneratedBy);
			}
		}

		if (RigBlueprint)
		{
			const FString RigPath = NormalizeObjectPath(RigBlueprint->GetPathName(), true);
			if (RigPath.IsEmpty() || SeenAssets.Contains(RigPath))
			{
				continue;
			}
			SeenAssets.Add(RigPath);

			FBS_ControlRigData RigData;
			ExtractControlRigGraph(RigBlueprint, RigData);
			OutData.ControlRigs.Add(RigData);
		}
	}
#endif
}

void UBlueprintAnalyzer::ExtractControlRigGraph(UControlRigBlueprint* RigBlueprint, FBS_ControlRigData& OutRigData)
{
#if UEARATAME_HAS_CONTROL_RIG
	if (!RigBlueprint)
	{
		return;
	}

	OutRigData.RigName = RigBlueprint->GetName();
	if (FObjectProperty* PreviewMeshProp = FindFProperty<FObjectProperty>(RigBlueprint->GetClass(), TEXT("PreviewMesh")))
	{
		if (USkeletalMesh* PreviewMesh = Cast<USkeletalMesh>(PreviewMeshProp->GetObjectPropertyValue_InContainer(RigBlueprint)))
		{
			if (USkeleton* Skeleton = PreviewMesh->GetSkeleton())
			{
				OutRigData.SkeletonPath = Skeleton->GetPathName();
			}
		}
	}

	TSet<FString> EnabledFeatures;
	TSet<FString> ControlNameSet;
	TSet<FString> BoneNameSet;
	TArray<FRigElementKey> ControlKeys;
	TMap<FString, FString> BoneLookupByNormalizedName;
	OutRigData.ControlToBoneMap.Reset();

	auto NormalizeBoneCandidate = [](FString Name) -> FString
	{
		Name = Name.ToLower();
		Name.ReplaceInline(TEXT(" "), TEXT(""));
		Name.ReplaceInline(TEXT("-"), TEXT("_"));
		Name.ReplaceInline(TEXT("."), TEXT("_"));

		while (Name.ReplaceInline(TEXT("__"), TEXT("_"), ESearchCase::CaseSensitive) > 0)
		{
		}

		while (Name.StartsWith(TEXT("_")))
		{
			Name.RightChopInline(1, EAllowShrinking::No);
		}
		while (Name.EndsWith(TEXT("_")))
		{
			Name.LeftChopInline(1, EAllowShrinking::No);
		}

		return Name;
	};

	auto BuildControlBoneCandidates = [&NormalizeBoneCandidate](const FString& ControlName) -> TArray<FString>
	{
		TArray<FString> Candidates;
		auto AddCandidate = [&Candidates, &NormalizeBoneCandidate](const FString& Raw)
		{
			const FString Normalized = NormalizeBoneCandidate(Raw);
			if (!Normalized.IsEmpty() && !Candidates.Contains(Normalized))
			{
				Candidates.Add(Normalized);
			}
		};

		FString Base = NormalizeBoneCandidate(ControlName);
		const TArray<FString> Suffixes = {
			TEXT("_control"),
			TEXT("control"),
			TEXT("_ctrl"),
			TEXT("ctrl"),
			TEXT("_pv"),
			TEXT("pv")
		};

		for (const FString& Suffix : Suffixes)
		{
			if (Base.EndsWith(Suffix))
			{
				Base.LeftChopInline(Suffix.Len(), EAllowShrinking::No);
				Base = NormalizeBoneCandidate(Base);
				break;
			}
		}

		AddCandidate(Base);

		auto AddSidedCandidates = [&AddCandidate](const FString& NameWithoutSide, const FString& SideSuffix)
		{
			if (NameWithoutSide.IsEmpty())
			{
				return;
			}

			AddCandidate(NameWithoutSide + SideSuffix);
			if (NameWithoutSide.Contains(TEXT("knee")))
			{
				AddCandidate(FString(TEXT("calf")) + SideSuffix);
			}
		};

		if (Base.StartsWith(TEXT("left")))
		{
			AddSidedCandidates(Base.Mid(4), TEXT("_l"));
		}
		else if (Base.StartsWith(TEXT("right")))
		{
			AddSidedCandidates(Base.Mid(5), TEXT("_r"));
		}

		if (Base.EndsWith(TEXT("left")))
		{
			AddSidedCandidates(Base.LeftChop(4), TEXT("_l"));
		}
		else if (Base.EndsWith(TEXT("right")))
		{
			AddSidedCandidates(Base.LeftChop(5), TEXT("_r"));
		}

		if (Base.Contains(TEXT("pelvis"))) AddCandidate(TEXT("pelvis"));
		if (Base.Contains(TEXT("root"))) AddCandidate(TEXT("root"));

		return Candidates;
	};

	URigHierarchy* RigHierarchy = RigBlueprint->GetHierarchy();
	if (RigHierarchy)
	{
		EnabledFeatures.Add(TEXT("Hierarchy"));

		const TArray<FRigElementKey> AllKeys = RigHierarchy->GetAllKeys(false, ERigElementType::All);
		for (const FRigElementKey& Key : AllKeys)
		{
			const FString KeyName = Key.Name.ToString();
			if (KeyName.IsEmpty())
			{
				continue;
			}

			if (Key.Type == ERigElementType::Control)
			{
				ControlNameSet.Add(KeyName);
				ControlKeys.Add(Key);
			}
			else if (Key.Type == ERigElementType::Bone)
			{
				BoneNameSet.Add(KeyName);
				const FString NormalizedBoneName = NormalizeBoneCandidate(KeyName);
				if (!NormalizedBoneName.IsEmpty())
				{
					BoneLookupByNormalizedName.Add(NormalizedBoneName, KeyName);
				}
			}
		}

		for (const FRigElementKey& ControlKey : ControlKeys)
		{
			const FString ControlName = ControlKey.Name.ToString();
			if (ControlName.IsEmpty())
			{
				continue;
			}

			bool bMapped = false;
			const TArray<FRigElementKey> ParentKeys = RigHierarchy->GetParents(ControlKey, true);
			for (const FRigElementKey& ParentKey : ParentKeys)
			{
				if (ParentKey.Type != ERigElementType::Bone)
				{
					continue;
				}

				const FString ParentBoneName = ParentKey.Name.ToString();
				if (!ParentBoneName.IsEmpty())
				{
					OutRigData.ControlToBoneMap.Add(ControlName, ParentBoneName);
					bMapped = true;
					break;
				}
			}

			if (bMapped)
			{
				continue;
			}

			const TArray<FString> BoneCandidates = BuildControlBoneCandidates(ControlName);
			for (const FString& Candidate : BoneCandidates)
			{
				if (const FString* BoneName = BoneLookupByNormalizedName.Find(Candidate))
				{
					OutRigData.ControlToBoneMap.Add(ControlName, *BoneName);
					break;
				}
			}
		}
	}

	if (RigBlueprint->IsModularRig())
	{
		EnabledFeatures.Add(TEXT("ModularRig"));
	}

	if (RigBlueprint->IsControlRigModule())
	{
		EnabledFeatures.Add(TEXT("ControlRigModule"));
	}

	OutRigData.ControlNames = ControlNameSet.Array();
	OutRigData.ControlNames.Sort();
	OutRigData.BoneNames = BoneNameSet.Array();
	OutRigData.BoneNames.Sort();
	OutRigData.EnabledFeatures = EnabledFeatures.Array();
	OutRigData.EnabledFeatures.Sort();

	OutRigData.FeatureSettings.Add(TEXT("controlCount"), FString::FromInt(OutRigData.ControlNames.Num()));
	OutRigData.FeatureSettings.Add(TEXT("boneCount"), FString::FromInt(OutRigData.BoneNames.Num()));
	OutRigData.FeatureSettings.Add(TEXT("mappedControlCount"), FString::FromInt(OutRigData.ControlToBoneMap.Num()));
	OutRigData.FeatureSettings.Add(TEXT("hasSkeleton"), OutRigData.SkeletonPath.IsEmpty() ? TEXT("false") : TEXT("true"));
	OutRigData.RigProperties.Add(TEXT("rigPath"), RigBlueprint->GetPathName());
	OutRigData.RigProperties.Add(TEXT("isModularRig"), RigBlueprint->IsModularRig() ? TEXT("true") : TEXT("false"));
	OutRigData.RigProperties.Add(TEXT("isControlRigModule"), RigBlueprint->IsControlRigModule() ? TEXT("true") : TEXT("false"));

	auto GetRigVMGraph = [RigBlueprint](const FName& GraphName) -> URigVMGraph*
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 4
		return RigBlueprint->GetModel(GraphName);
#else
		if (const FRigVMClient* RigVMClient = RigBlueprint->GetRigVMClient())
		{
			return RigVMClient->GetModel(GraphName.ToString());
		}
		return nullptr;
#endif
	};

	URigVMGraph* SetupGraph = GetRigVMGraph(TEXT("Setup"));
	URigVMGraph* ForwardGraph = GetRigVMGraph(TEXT("ForwardSolve"));
	URigVMGraph* BackwardGraph = GetRigVMGraph(TEXT("BackwardSolve"));

	auto ExtractRigVMGraph = [&](URigVMGraph* Graph, TArray<FBS_RigVMNodeData>& OutNodes)
	{
		if (!Graph)
		{
			return;
		}

		for (URigVMNode* VMNode : Graph->GetNodes())
		{
			if (!VMNode)
			{
				continue;
			}

			FBS_RigVMNodeData NodeData;
			NodeData.NodeGuid = MakeStableRigVMNodeGuid(VMNode);
			NodeData.NodeType = VMNode->GetClass() ? VMNode->GetClass()->GetName() : TEXT("URigVMNode");
			NodeData.NodeTitle = VMNode->GetNodeTitle();
			NodeData.PosX = VMNode->GetPosition().X;
			NodeData.PosY = VMNode->GetPosition().Y;

			for (URigVMPin* Pin : VMNode->GetPins())
			{
				FBS_PinData PinData;
				PinData.Name = Pin->GetFName();
				PinData.Category = Pin->GetCPPType();
				switch (Pin->GetDirection())
				{
				case ERigVMPinDirection::Input: PinData.Direction = TEXT("Input"); break;
				case ERigVMPinDirection::Output: PinData.Direction = TEXT("Output"); break;
				case ERigVMPinDirection::IO: PinData.Direction = TEXT("IO"); break;
				default: PinData.Direction = TEXT("Unknown"); break;
				}
				PinData.bIsOut = Pin->GetDirection() == ERigVMPinDirection::Output;
				NodeData.Pins.Add(PinData);
			}

			ExtractObjectProperties(VMNode, NodeData.NodeProperties);
			OutNodes.Add(NodeData);
		}

		for (URigVMLink* Link : Graph->GetLinks())
		{
			if (!Link || !Link->GetSourcePin() || !Link->GetTargetPin())
			{
				continue;
			}

			URigVMNode* SourceNode = Link->GetSourceNode();
			URigVMNode* TargetNode = Link->GetTargetNode();
			if (!SourceNode || !TargetNode)
			{
				continue;
			}

			FBS_PinLinkData LinkData;
			LinkData.SourceNodeGuid = MakeStableRigVMNodeGuid(SourceNode);
			LinkData.SourcePinName = Link->GetSourcePin()->GetFName();
			LinkData.TargetNodeGuid = MakeStableRigVMNodeGuid(TargetNode);
			LinkData.TargetPinName = Link->GetTargetPin()->GetFName();
			LinkData.PinCategory = Link->GetSourcePin()->GetCPPType();
			LinkData.PinSubCategory = TEXT("");
			LinkData.bIsExec = false;
			OutRigData.RigVMConnections.Add(LinkData);
		}
	};

	ExtractRigVMGraph(SetupGraph, OutRigData.SetupEventNodes);
	ExtractRigVMGraph(ForwardGraph, OutRigData.ForwardSolveNodes);
	ExtractRigVMGraph(BackwardGraph, OutRigData.BackwardSolveNodes);
#endif
}

// Preferred structured node analysis
FBS_NodeData UBlueprintAnalyzer::AnalyzeNodeToStruct(UEdGraphNode* Node)
{
    FBS_NodeData Out;
    if (!Node)
    {
        return Out;
    }

    Out.NodeGuid = Node->NodeGuid;
    Out.NodeType = Node->GetClass()->GetName();
    Out.Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
    Out.PosX = Node->NodePosX;
    Out.PosY = Node->NodePosY;
    Out.NodeWidth = Node->NodeWidth;
    Out.NodeHeight = Node->NodeHeight;
    Out.NodeComment = Node->NodeComment;
    Out.EnabledState = LexToString(Node->GetDesiredEnabledState());
    switch (Node->AdvancedPinDisplay)
    {
    case ENodeAdvancedPins::NoPins: Out.AdvancedPinDisplay = TEXT("NoPins"); break;
    case ENodeAdvancedPins::Shown:  Out.AdvancedPinDisplay = TEXT("Shown"); break;
    case ENodeAdvancedPins::Hidden: Out.AdvancedPinDisplay = TEXT("Hidden"); break;
    default: Out.AdvancedPinDisplay = TEXT("Unknown"); break;
    }
    Out.bHasCompilerMessage = Node->bHasCompilerMessage;
    Out.ErrorType = Node->ErrorType;
    Out.ErrorMsg = Node->ErrorMsg;

#if WITH_EDITORONLY_DATA
    Out.bCommentBubblePinned = Node->bCommentBubblePinned;
    Out.bCommentBubbleVisible = Node->bCommentBubbleVisible;
    Out.bCommentBubbleMakeVisible = Node->bCommentBubbleMakeVisible;
    Out.bCanResizeNode = Node->bCanResizeNode;
    Out.bCanRenameNode = Node->GetCanRenameNode();
#endif

    // Base property bag for node-specific metadata
    ExtractObjectProperties(Node, Out.NodeProperties);

    auto AddMeta = [&Out](const FString& Key, const FString& Value)
    {
        if (!Value.IsEmpty())
        {
            Out.NodeProperties.Add(Key, Value);
        }
    };
    auto AddMetaBool = [&Out](const FString& Key, bool Value)
    {
        Out.NodeProperties.Add(Key, Value ? TEXT("true") : TEXT("false"));
    };

    if (UK2Node* K2 = Cast<UK2Node>(Node))
    {
        if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(K2))
        {
            Out.MemberName = Call->FunctionReference.GetMemberName().ToString();
            if (UClass* Parent = Call->FunctionReference.GetMemberParentClass())
            {
                FString ParentName = Parent->GetName();
                ParentName.RemoveFromStart(TEXT("SKEL_"));
                ParentName.RemoveFromEnd(TEXT("_C"));
                Out.MemberParent = ParentName;
                AddMeta(TEXT("meta.memberParentPath"), Parent->GetPathName());
            }

            // 2. Self-context resolution: resolve via owning Blueprint skeleton class
            if (Out.MemberParent.IsEmpty())
            {
                if (UBlueprint* OwnerBP = Cast<UBlueprint>(Node->GetGraph()->GetTypedOuter<UBlueprint>()))
                {
                    if (UClass* SkelClass = OwnerBP->SkeletonGeneratedClass)
                    {
                        if (UClass* ScopedParent = Call->FunctionReference.GetMemberParentClass(SkelClass))
                        {
                            FString ParentName = ScopedParent->GetName();
                            ParentName.RemoveFromStart(TEXT("SKEL_"));
                            ParentName.RemoveFromEnd(TEXT("_C"));
                            Out.MemberParent = ParentName;
                            AddMeta(TEXT("meta.memberParentPath"), ScopedParent->GetPathName());
                            AddMeta(TEXT("meta.memberParentFallback"), TEXT("skelScope"));
                        }
                    }
                }
            }

            if (UFunction* TargetFunction = Call->GetTargetFunction())
            {
                AddMeta(TEXT("meta.functionName"), TargetFunction->GetName());
                if (UClass* Owner = TargetFunction->GetOwnerClass())
                {
                    AddMeta(TEXT("meta.functionOwner"), Owner->GetPathName());
                    // 3. Derive MemberParent from compiled function owner when still unresolved
                    if (Out.MemberParent.IsEmpty())
                    {
                        FString OwnerName = Owner->GetName();
                        OwnerName.RemoveFromStart(TEXT("SKEL_"));
                        OwnerName.RemoveFromEnd(TEXT("_C"));
                        Out.MemberParent = OwnerName;
                        AddMeta(TEXT("meta.memberParentFallback"), TEXT("funcOwner"));
                    }
                    // CR-014 (Task 14): interface dispatch enrichment
                    if (Owner->HasAnyClassFlags(CLASS_Interface))
                    {
                        AddMetaBool(TEXT("meta.isInterfaceCall"), true);
                        AddMeta(TEXT("meta.interfaceClassPath"), Owner->GetPathName());
                        AddMeta(TEXT("meta.interfaceFunctionPath"),
                            FString::Printf(TEXT("%s::%s"), *Owner->GetPathName(), *TargetFunction->GetName()));
                    }

                    // CR-035: Best-effort bytecode correlation.
                    // For CallFunction nodes that invoke a native C++ function, emit the canonical
                    // "OwnerClass::FunctionName" correlation string. This lets converters know the
                    // exact C++ call site without disassembling bytecode.
                    // Native = FUNC_Native set, meaning this function has a C++ implementation.
                    if (TargetFunction->HasAnyFunctionFlags(FUNC_Native))
                    {
                        FString NativeOwnerName = Owner->GetName();
                        NativeOwnerName.RemoveFromStart(TEXT("SKEL_"));
                        NativeOwnerName.RemoveFromEnd(TEXT("_C"));
                        AddMeta(TEXT("meta.bytecodeCorrelation"),
                            FString::Printf(TEXT("%s::%s"), *NativeOwnerName, *TargetFunction->GetName()));
                    }
                }
                AddMetaBool(TEXT("meta.isPure"), TargetFunction->HasAllFunctionFlags(FUNC_BlueprintPure));
                AddMetaBool(TEXT("meta.isConst"), TargetFunction->HasAllFunctionFlags(FUNC_Const));
                AddMetaBool(TEXT("meta.isStatic"), TargetFunction->HasAllFunctionFlags(FUNC_Static));
#if defined(FUNC_Latent)
                AddMetaBool(TEXT("meta.isLatent"), TargetFunction->HasAllFunctionFlags(FUNC_Latent));
#else
                AddMetaBool(TEXT("meta.isLatent"), TargetFunction->HasMetaData(TEXT("Latent")));
#endif
            }
        }

        if (UK2Node_VariableGet* VarGet = Cast<UK2Node_VariableGet>(K2))
        {
            const FMemberReference& Ref = VarGet->VariableReference;
            AddMeta(TEXT("meta.variableName"), Ref.GetMemberName().ToString());
            if (UClass* Parent = Ref.GetMemberParentClass())
            {
                AddMeta(TEXT("meta.variableOwner"), Parent->GetPathName());
            }
            AddMetaBool(TEXT("meta.isSelfContext"), Ref.IsSelfContext());
        }

        if (UK2Node_VariableSet* VarSet = Cast<UK2Node_VariableSet>(K2))
        {
            const FMemberReference& Ref = VarSet->VariableReference;
            AddMeta(TEXT("meta.variableName"), Ref.GetMemberName().ToString());
            if (UClass* Parent = Ref.GetMemberParentClass())
            {
                AddMeta(TEXT("meta.variableOwner"), Parent->GetPathName());
            }
            AddMetaBool(TEXT("meta.isSelfContext"), Ref.IsSelfContext());
        }

        if (UK2Node_VariableSetRef* VarSetRef = Cast<UK2Node_VariableSetRef>(K2))
        {
            AddMetaBool(TEXT("meta.isByRefSet"), true);
            if (UEdGraphPin* TargetPin = VarSetRef->GetTargetPin())
            {
                AddMeta(TEXT("meta.byRefPinCategory"), TargetPin->PinType.PinCategory.ToString());
                AddMeta(TEXT("meta.byRefPinSubCategory"), TargetPin->PinType.PinSubCategory.ToString());
                if (UObject* SubObj = TargetPin->PinType.PinSubCategoryObject.Get())
                {
                    AddMeta(TEXT("meta.byRefPinSubCategoryObject"), SubObj->GetPathName());
                }
            }
        }

        if (UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(K2))
        {
            if (UEdGraph* MacroGraph = Macro->GetMacroGraph())
            {
                AddMeta(TEXT("meta.macroGraphPath"), MacroGraph->GetPathName());
                const FString MacroName = MacroGraph->GetName();
                AddMeta(TEXT("meta.macroGraphName"), MacroName);
				if (UBlueprint* MacroBlueprint = FBlueprintEditorUtils::FindBlueprintForGraph(MacroGraph))
				{
					AddMeta(TEXT("meta.macroBlueprintPath"), MacroBlueprint->GetPathName());
				}
                // Task 17: detect ForEach / ForEachWithBreak loop macros and emit pin linkage
                const bool bIsForEach = MacroName.Contains(TEXT("ForEach"), ESearchCase::IgnoreCase);
                if (bIsForEach)
                {
                    AddMetaBool(TEXT("meta.isLoopMacro"), true);
                    // Named pins on the ForEach macro instance node
                    auto EmitPinId = [&](const TCHAR* PinNameStr, EEdGraphPinDirection Dir, const TCHAR* MetaKey)
                    {
                        UEdGraphPin* Pin = Node->FindPin(FName(PinNameStr), Dir);
                        if (Pin) { AddMeta(MetaKey, Pin->PinId.ToString()); }
                    };
                    EmitPinId(TEXT("Array"),        EGPD_Input,  TEXT("meta.loopArrayPinId"));
                    EmitPinId(TEXT("Array Element"), EGPD_Output, TEXT("meta.loopElementPinId"));
                    EmitPinId(TEXT("Array Index"),   EGPD_Output, TEXT("meta.loopIndexPinId"));
                    EmitPinId(TEXT("Loop Body"),     EGPD_Output, TEXT("meta.loopBodyPinId"));
                    EmitPinId(TEXT("Completed"),     EGPD_Output, TEXT("meta.loopCompletedPinId"));
                    // ForEachWithBreak additionally has a Break exec input
                    EmitPinId(TEXT("Break"),         EGPD_Input,  TEXT("meta.loopBreakPinId"));
                }
            }
        }

        if (UK2Node_DynamicCast* DynCast = Cast<UK2Node_DynamicCast>(K2))
        {
            if (UClass* TargetClass = DynCast->TargetType)
            {
                AddMeta(TEXT("meta.castTargetClass"), TargetClass->GetPathName());
            }
        }

        if (UK2Node_Timeline* Timeline = Cast<UK2Node_Timeline>(K2))
        {
            AddMeta(TEXT("meta.timelineName"), Timeline->TimelineName.ToString());
			if (UBlueprint* OwnerBlueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node))
			{
				for (UTimelineTemplate* Template : OwnerBlueprint->Timelines)
				{
					if (Template && Template->GetVariableName() == Timeline->TimelineName)
					{
						AddMeta(TEXT("meta.timelineTemplatePath"), Template->GetPathName());
						break;
					}
				}
			}
            // Task 22: emit exec/data pin IDs for full graph linkage through timeline nodes
            auto EmitTimelinePinId = [&](const TCHAR* PinName, EEdGraphPinDirection Dir, const TCHAR* MetaKey)
            {
                UEdGraphPin* Pin = Node->FindPin(FName(PinName), Dir);
                if (Pin) AddMeta(MetaKey, Pin->PinId.ToString());
            };
            EmitTimelinePinId(TEXT("Play"),           EGPD_Input,  TEXT("meta.timelinePlayPinId"));
            EmitTimelinePinId(TEXT("PlayFromStart"),  EGPD_Input,  TEXT("meta.timelinePlayFromStartPinId"));
            EmitTimelinePinId(TEXT("Stop"),           EGPD_Input,  TEXT("meta.timelineStopPinId"));
            EmitTimelinePinId(TEXT("Reverse"),        EGPD_Input,  TEXT("meta.timelineReversePinId"));
            EmitTimelinePinId(TEXT("ReverseFromEnd"), EGPD_Input,  TEXT("meta.timelineReverseFromEndPinId"));
            EmitTimelinePinId(TEXT("SetNewTime"),     EGPD_Input,  TEXT("meta.timelineSetNewTimePinId"));
            EmitTimelinePinId(TEXT("Update"),         EGPD_Output, TEXT("meta.timelineUpdatePinId"));
            EmitTimelinePinId(TEXT("Finished"),       EGPD_Output, TEXT("meta.timelineFinishedPinId"));
        }

        // --- Task 22: Branch (IfThenElse) — condition + exec output pin IDs for if/else reconstruction ---
        if (UK2Node_IfThenElse* Branch = Cast<UK2Node_IfThenElse>(K2))
        {
            AddMetaBool(TEXT("meta.isBranch"), true);
            UEdGraphPin* CondPin = Node->FindPin(TEXT("Condition"), EGPD_Input);
            if (CondPin) AddMeta(TEXT("meta.branchConditionPinId"), CondPin->PinId.ToString());
            UEdGraphPin* TruePin = Node->FindPin(TEXT("then"), EGPD_Output);
            if (TruePin) AddMeta(TEXT("meta.branchTruePinId"), TruePin->PinId.ToString());
            UEdGraphPin* FalsePin = Node->FindPin(TEXT("else"), EGPD_Output);
            if (FalsePin) AddMeta(TEXT("meta.branchFalsePinId"), FalsePin->PinId.ToString());
        }

        // --- Task 23: Sequence node — ordered output exec pin IDs for sequential control flow ---
        if (UK2Node_ExecutionSequence* Sequence = Cast<UK2Node_ExecutionSequence>(K2))
        {
            AddMetaBool(TEXT("meta.isSequence"), true);
            TArray<FString> OutputPinIds;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Output &&
                    Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                {
                    OutputPinIds.Add(Pin->PinId.ToString());
                }
            }
            AddMeta(TEXT("meta.sequenceOutputCount"), FString::FromInt(OutputPinIds.Num()));
            if (OutputPinIds.Num() > 0)
                AddMeta(TEXT("meta.sequenceOutputPinIds"), FString::Join(OutputPinIds, TEXT(";")));
        }

        // --- Task 24: Reroute (Knot) node — transparent data pass-through, emit input/output pin IDs ---
        if (UK2Node_Knot* Knot = Cast<UK2Node_Knot>(K2))
        {
            AddMetaBool(TEXT("meta.isReroute"), true);
            UEdGraphPin* InPin = nullptr;
            UEdGraphPin* OutPin = nullptr;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin) continue;
                if (Pin->Direction == EGPD_Input  && !InPin)  InPin  = Pin;
                if (Pin->Direction == EGPD_Output && !OutPin) OutPin = Pin;
            }
            if (InPin)  AddMeta(TEXT("meta.rerouteInputPinId"),  InPin->PinId.ToString());
            if (OutPin) AddMeta(TEXT("meta.rerouteOutputPinId"), OutPin->PinId.ToString());
        }

        // --- Task 25: Self node — explicit self-reference marker for `this` resolution ---
        if (UK2Node_Self* Self = Cast<UK2Node_Self>(K2))
        {
            AddMetaBool(TEXT("meta.isSelf"), true);
        }

        // --- Task 26: SpawnActorFromClass node — class + return pin IDs for actor spawn reconstruction ---
        if (UK2Node_SpawnActorFromClass* SpawnActor = Cast<UK2Node_SpawnActorFromClass>(K2))
        {
            AddMetaBool(TEXT("meta.isSpawnActor"), true);
            auto EmitSpawnPinId = [&](const TCHAR* PinName, EEdGraphPinDirection Dir, const TCHAR* MetaKey)
            {
                UEdGraphPin* Pin = Node->FindPin(FName(PinName), Dir);
                if (Pin) AddMeta(MetaKey, Pin->PinId.ToString());
            };
            EmitSpawnPinId(TEXT("Class"),           EGPD_Input,  TEXT("meta.spawnClassPinId"));
            EmitSpawnPinId(TEXT("SpawnTransform"),  EGPD_Input,  TEXT("meta.spawnTransformPinId"));
            EmitSpawnPinId(TEXT("ReturnValue"),     EGPD_Output, TEXT("meta.spawnReturnPinId"));
        }

		if (UK2Node_CallParentFunction* ParentCall = Cast<UK2Node_CallParentFunction>(K2))
		{
			AddMetaBool(TEXT("meta.callsParent"), true);
			if (ParentCall->FunctionReference.GetMemberName() != NAME_None)
			{
				AddMeta(TEXT("meta.parentFunctionName"), ParentCall->FunctionReference.GetMemberName().ToString());
			}
		}

        if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(K2))
        {
            AddMeta(TEXT("meta.eventName"), EventNode->EventReference.GetMemberName().ToString());
            if (UClass* Parent = EventNode->EventReference.GetMemberParentClass())
            {
                AddMeta(TEXT("meta.eventOwner"), Parent->GetPathName());
            }
        }

        if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(K2))
        {
            AddMeta(TEXT("meta.customEventName"), CustomEvent->CustomFunctionName.ToString());
            // Capture typed parameter signatures from UserDefinedPins
            TArray<FString> ParamDescs;
            for (const TSharedPtr<FUserPinInfo>& PinInfo : CustomEvent->UserDefinedPins)
            {
                if (PinInfo.IsValid())
                {
                    FString PinDesc = FString::Printf(TEXT("%s: %s"),
                        *PinInfo->PinName.ToString(),
                        *DescribePinTypeDetailed(PinInfo->PinType));
                    if (!PinInfo->PinDefaultValue.IsEmpty())
                    {
                        PinDesc += FString::Printf(TEXT(" = %s"), *PinInfo->PinDefaultValue);
                    }
                    ParamDescs.Add(PinDesc);
                }
            }
            if (ParamDescs.Num() > 0)
            {
                AddMeta(TEXT("meta.customEventParams"), FString::Join(ParamDescs, TEXT(";")));
                AddMeta(TEXT("meta.customEventParamCount"), FString::FromInt(ParamDescs.Num()));
            }
        }

        // --- Literal node: captures object reference path for inline object literals ---
        if (UK2Node_Literal* Literal = Cast<UK2Node_Literal>(K2))
        {
            if (UObject* ObjRef = Literal->GetObjectRef())
            {
                AddMeta(TEXT("meta.literalObjectPath"), ObjRef->GetPathName());
                AddMeta(TEXT("meta.literalObjectName"), ObjRef->GetName());
            }
        }

        // --- Delegate call: multicast delegate broadcast ---
        if (UK2Node_CallDelegate* CallDel = Cast<UK2Node_CallDelegate>(K2))
        {
            AddMeta(TEXT("meta.delegateName"), CallDel->DelegateReference.GetMemberName().ToString());
            if (UClass* Owner = CallDel->DelegateReference.GetMemberParentClass())
            {
                AddMeta(TEXT("meta.delegateOwner"), Owner->GetPathName());
            }
            AddMetaBool(TEXT("meta.isDelegateCall"), true);
            // Task 13: emit delegate signature parameters for complete dispatch reconstruction
            if (UFunction* SigFunc = CallDel->GetDelegateSignature())
            {
                TArray<FString> SigParams;
                for (TFieldIterator<FProperty> PropIt(SigFunc); PropIt; ++PropIt)
                {
                    const FProperty* Prop = *PropIt;
                    if (Prop && Prop->HasAnyPropertyFlags(CPF_Parm) && !Prop->HasAnyPropertyFlags(CPF_ReturnParm))
                    {
                        SigParams.Add(FString::Printf(TEXT("%s: %s"), *Prop->GetName(), *DescribePropertyType(Prop)));
                    }
                }
                if (SigParams.Num() > 0)
                {
                    AddMeta(TEXT("meta.delegateSignatureParams"), FString::Join(SigParams, TEXT(";")));
                    AddMeta(TEXT("meta.delegateSignatureParamCount"), FString::FromInt(SigParams.Num()));
                }
            }
        }

        // --- CreateDelegate: bind a named function to a delegate variable ---
        if (UK2Node_CreateDelegate* CreateDel = Cast<UK2Node_CreateDelegate>(K2))
        {
            AddMetaBool(TEXT("meta.isCreateDelegate"), true);
            if (!CreateDel->SelectedFunctionName.IsNone())
            {
                AddMeta(TEXT("meta.selectedFunctionName"), CreateDel->SelectedFunctionName.ToString());
            }
            if (UFunction* SigFunc = CreateDel->GetDelegateSignature())
            {
                if (UClass* SigOwner = SigFunc->GetOwnerClass())
                {
                    AddMeta(TEXT("meta.delegateSignaturePath"),
                        FString::Printf(TEXT("%s::%s"), *SigOwner->GetPathName(), *SigFunc->GetName()));
                }
                TArray<FString> SigParams;
                for (TFieldIterator<FProperty> PropIt(SigFunc); PropIt; ++PropIt)
                {
                    const FProperty* Prop = *PropIt;
                    if (Prop && Prop->HasAnyPropertyFlags(CPF_Parm) && !Prop->HasAnyPropertyFlags(CPF_ReturnParm))
                    {
                        SigParams.Add(FString::Printf(TEXT("%s: %s"), *Prop->GetName(), *DescribePropertyType(Prop)));
                    }
                }
                if (SigParams.Num() > 0)
                {
                    AddMeta(TEXT("meta.delegateSignatureParams"), FString::Join(SigParams, TEXT(";")));
                    AddMeta(TEXT("meta.delegateSignatureParamCount"), FString::FromInt(SigParams.Num()));
                }
            }
        }

        // --- Array function: distinguish array target operations from regular calls ---
        if (UK2Node_CallArrayFunction* ArrayCall = Cast<UK2Node_CallArrayFunction>(K2))
        {
            AddMetaBool(TEXT("meta.isArrayFunction"), true);
            if (UEdGraphPin* ArrayPin = ArrayCall->GetTargetArrayPin())
            {
                AddMeta(TEXT("meta.arrayTargetPinName"), ArrayPin->PinName.ToString());
            }
        }

        // --- Select node: multi-way value selector ---
        if (UK2Node_Select* Select = Cast<UK2Node_Select>(K2))
        {
            AddMetaBool(TEXT("meta.isSelect"), true);
            // Task 16: emit full pin-ID mapping for Index, Return, and all option pins
            TArray<FString> OptionPinIds, OptionPinNames;
            int32 OptionCount = 0;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin) continue;
                if (Pin->Direction == EGPD_Input &&
                    Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                {
                    if (Pin->PinName == TEXT("Index"))
                    {
                        AddMeta(TEXT("meta.selectIndexPinId"), Pin->PinId.ToString());
                    }
                    else
                    {
                        OptionPinIds.Add(Pin->PinId.ToString());
                        OptionPinNames.Add(Pin->PinName.ToString());
                        OptionCount++;
                    }
                }
                else if (Pin->Direction == EGPD_Output &&
                         Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                {
                    AddMeta(TEXT("meta.selectReturnPinId"), Pin->PinId.ToString());
                }
            }
            AddMeta(TEXT("meta.selectOptionCount"), FString::FromInt(OptionCount));
            if (OptionPinIds.Num() > 0)
            {
                AddMeta(TEXT("meta.selectOptionPinIds"),   FString::Join(OptionPinIds,   TEXT(";")));
                AddMeta(TEXT("meta.selectOptionPinNames"), FString::Join(OptionPinNames, TEXT(";")));
            }
        }

        // --- Switch nodes: control-flow branching on value ---
        if (UK2Node_SwitchEnum* SwitchEnum = Cast<UK2Node_SwitchEnum>(K2))
        {
            AddMetaBool(TEXT("meta.isSwitch"), true);
            AddMeta(TEXT("meta.switchType"), TEXT("Enum"));
            if (SwitchEnum->Enum)
            {
                AddMeta(TEXT("meta.switchEnumPath"), SwitchEnum->Enum->GetPathName());
                AddMeta(TEXT("meta.switchEnumName"), SwitchEnum->Enum->GetName());
                // Serialize enum case names
                TArray<FString> Cases;
                for (const FName& Entry : SwitchEnum->EnumEntries)
                {
                    Cases.Add(Entry.ToString());
                }
                if (Cases.Num() > 0)
                {
                    AddMeta(TEXT("meta.switchCases"), FString::Join(Cases, TEXT(",")));
                }
                AddMeta(TEXT("meta.switchCaseCount"), FString::FromInt(Cases.Num()));
                // Task 15: emit output exec pin GUIDs per case, plus default pin GUID
                TArray<FString> CasePinIds;
                for (const FName& Entry : SwitchEnum->EnumEntries)
                {
                    UEdGraphPin* CasePin = Node->FindPin(Entry, EGPD_Output);
                    CasePinIds.Add(CasePin ? CasePin->PinId.ToString() : FString());
                }
                if (CasePinIds.Num() > 0)
                {
                    AddMeta(TEXT("meta.switchCasePinIds"), FString::Join(CasePinIds, TEXT(";")));
                }
                UEdGraphPin* DefaultPin = Node->FindPin(TEXT("Default"), EGPD_Output);
                if (DefaultPin)
                {
                    AddMeta(TEXT("meta.switchDefaultPinId"), DefaultPin->PinId.ToString());
                }
            }
        }

        if (UK2Node_SwitchInteger* SwitchInt = Cast<UK2Node_SwitchInteger>(K2))
        {
            AddMetaBool(TEXT("meta.isSwitch"), true);
            AddMeta(TEXT("meta.switchType"), TEXT("Integer"));
            AddMeta(TEXT("meta.switchStartIndex"), FString::FromInt(SwitchInt->StartIndex));
            // Task 15: collect case pin IDs and corresponding integer values
            TArray<FString> CasePinIds, CaseValues;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Output &&
                    Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec &&
                    Pin->PinName != TEXT("Default"))
                {
                    CasePinIds.Add(Pin->PinId.ToString());
                    CaseValues.Add(Pin->PinName.ToString()); // name is the integer value string
                }
            }
            AddMeta(TEXT("meta.switchCaseCount"), FString::FromInt(CasePinIds.Num()));
            if (CasePinIds.Num() > 0)
            {
                AddMeta(TEXT("meta.switchCasePinIds"), FString::Join(CasePinIds, TEXT(";")));
                AddMeta(TEXT("meta.switchCaseValues"), FString::Join(CaseValues, TEXT(";")));
            }
            UEdGraphPin* DefaultPin = Node->FindPin(TEXT("Default"), EGPD_Output);
            if (DefaultPin)
            {
                AddMeta(TEXT("meta.switchDefaultPinId"), DefaultPin->PinId.ToString());
            }
        }

        if (UK2Node_SwitchName* SwitchName = Cast<UK2Node_SwitchName>(K2))
        {
            AddMetaBool(TEXT("meta.isSwitch"), true);
            AddMeta(TEXT("meta.switchType"), TEXT("Name"));
            TArray<FString> Cases;
            for (const FName& PinNameEntry : SwitchName->PinNames)
            {
                Cases.Add(PinNameEntry.ToString());
            }
            if (Cases.Num() > 0)
            {
                AddMeta(TEXT("meta.switchCases"), FString::Join(Cases, TEXT(",")));
            }
            AddMeta(TEXT("meta.switchCaseCount"), FString::FromInt(Cases.Num()));
            // Task 15: emit output exec pin GUIDs per case name, plus default pin GUID
            TArray<FString> CasePinIds;
            for (const FName& PinNameEntry : SwitchName->PinNames)
            {
                UEdGraphPin* CasePin = Node->FindPin(PinNameEntry, EGPD_Output);
                CasePinIds.Add(CasePin ? CasePin->PinId.ToString() : FString());
            }
            if (CasePinIds.Num() > 0)
            {
                AddMeta(TEXT("meta.switchCasePinIds"), FString::Join(CasePinIds, TEXT(";")));
            }
            UEdGraphPin* DefaultPin = Node->FindPin(TEXT("Default"), EGPD_Output);
            if (DefaultPin)
            {
                AddMeta(TEXT("meta.switchDefaultPinId"), DefaultPin->PinId.ToString());
            }
        }

        if (UK2Node_SwitchString* SwitchStr = Cast<UK2Node_SwitchString>(K2))
        {
            AddMetaBool(TEXT("meta.isSwitch"), true);
            AddMeta(TEXT("meta.switchType"), TEXT("String"));
            TArray<FString> Cases;
            for (const FName& PinNameEntry : SwitchStr->PinNames)
            {
                Cases.Add(PinNameEntry.ToString());
            }
            if (Cases.Num() > 0)
            {
                AddMeta(TEXT("meta.switchCases"), FString::Join(Cases, TEXT(",")));
            }
            AddMeta(TEXT("meta.switchCaseCount"), FString::FromInt(Cases.Num()));
            AddMetaBool(TEXT("meta.switchIsCaseSensitive"), SwitchStr->bIsCaseSensitive != 0);
            // Task 15: emit output exec pin GUIDs per case name, plus default pin GUID
            TArray<FString> CasePinIds;
            for (const FName& PinNameEntry : SwitchStr->PinNames)
            {
                UEdGraphPin* CasePin = Node->FindPin(PinNameEntry, EGPD_Output);
                CasePinIds.Add(CasePin ? CasePin->PinId.ToString() : FString());
            }
            if (CasePinIds.Num() > 0)
            {
                AddMeta(TEXT("meta.switchCasePinIds"), FString::Join(CasePinIds, TEXT(";")));
            }
            UEdGraphPin* DefaultPin = Node->FindPin(TEXT("Default"), EGPD_Output);
            if (DefaultPin)
            {
                AddMeta(TEXT("meta.switchDefaultPinId"), DefaultPin->PinId.ToString());
            }
        }

        // GameplayTagsEditor defines this node outside BlueprintGraph. Avoid a hard
        // module/header dependency and serialize its public reconstruction surface
        // from stable graph pins plus reflected comparison properties.
        if (K2->GetClass()->GetName() == TEXT("GameplayTagsK2Node_SwitchGameplayTag"))
        {
            AddMetaBool(TEXT("meta.isSwitch"), true);
            AddMeta(TEXT("meta.switchType"), TEXT("GameplayTag"));

            TArray<FString> Cases;
            TArray<FString> CasePinIds;
            for (UEdGraphPin* Pin : K2->Pins)
            {
                if (!Pin || Pin->Direction != EGPD_Output ||
                    Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                {
                    continue;
                }

                if (Pin->PinName == TEXT("Default"))
                {
                    AddMeta(TEXT("meta.switchDefaultPinId"), Pin->PinId.ToString());
                    continue;
                }

                Cases.Add(Pin->PinName.ToString());
                CasePinIds.Add(Pin->PinId.ToString());
            }

            AddMeta(TEXT("meta.switchCaseCount"), FString::FromInt(Cases.Num()));
            if (Cases.Num() > 0)
            {
                AddMeta(TEXT("meta.switchCases"), FString::Join(Cases, TEXT(";")));
                AddMeta(TEXT("meta.switchCasePinIds"), FString::Join(CasePinIds, TEXT(";")));
            }

            if (UEdGraphPin* SelectionPin = K2->FindPin(TEXT("Selection"), EGPD_Input))
            {
                AddMeta(TEXT("meta.switchSelectionPinId"), SelectionPin->PinId.ToString());
                AddMeta(TEXT("meta.switchSelectionType"), DescribePinTypeDetailed(SelectionPin->PinType));
            }

            if (FNameProperty* FunctionNameProperty = FindFProperty<FNameProperty>(
                    K2->GetClass(), TEXT("FunctionName")))
            {
                AddMeta(TEXT("meta.switchComparisonFunction"),
                    FunctionNameProperty->GetPropertyValue_InContainer(K2).ToString());
            }
            if (FClassProperty* FunctionClassProperty = FindFProperty<FClassProperty>(
                    K2->GetClass(), TEXT("FunctionClass")))
            {
                if (UClass* FunctionClass = Cast<UClass>(
                        FunctionClassProperty->GetObjectPropertyValue_InContainer(K2)))
                {
                    AddMeta(TEXT("meta.switchComparisonClass"), FunctionClass->GetPathName());
                }
            }
        }

        // --- Task 18: Composite (collapsed graph / tunnel) node ---
        if (UK2Node_Composite* Composite = Cast<UK2Node_Composite>(K2))
        {
            AddMetaBool(TEXT("meta.isCollapsedGraph"), true);
            if (UEdGraph* BoundGraph = Composite->BoundGraph)
            {
                AddMeta(TEXT("meta.collapsedGraphName"), BoundGraph->GetName());
                AddMeta(TEXT("meta.collapsedGraphPath"), BoundGraph->GetPathName());
            }
            // Collect non-exec data pins on the composite node itself:
            // input data pins = inputs flowing into the collapsed graph
            // output data pins = outputs flowing out of the collapsed graph
            TArray<FString> InputPins, OutputPins;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
                if (Pin->Direction == EGPD_Input)
                    InputPins.Add(Pin->PinName.ToString());
                else if (Pin->Direction == EGPD_Output)
                    OutputPins.Add(Pin->PinName.ToString());
            }
            if (InputPins.Num() > 0)
                AddMeta(TEXT("meta.collapsedInputPins"),  FString::Join(InputPins,  TEXT(";")));
            if (OutputPins.Num() > 0)
                AddMeta(TEXT("meta.collapsedOutputPins"), FString::Join(OutputPins, TEXT(";")));
        }

        // --- DynamicCast: add pure-cast flag to existing handler output ---
        // (TargetType is already captured above; add bIsPureCast here)
        if (UK2Node_DynamicCast* DynCast = Cast<UK2Node_DynamicCast>(K2))
        {
            AddMetaBool(TEXT("meta.isPureCast"), DynCast->IsNodePure());
        }

        // --- ClassDynamicCast: cast to a class type (UClass) ---
        if (UK2Node_ClassDynamicCast* ClassCast = Cast<UK2Node_ClassDynamicCast>(K2))
        {
            AddMetaBool(TEXT("meta.isClassCast"), true);
            // TargetType is also on UK2Node_DynamicCast (parent), already captured above
        }

        // --- MakeStruct / BreakStruct: struct construction/deconstruction ---
        if (UK2Node_MakeStruct* MakeStruct = Cast<UK2Node_MakeStruct>(K2))
        {
            AddMetaBool(TEXT("meta.isMakeStruct"), true);
            if (MakeStruct->StructType)
            {
                AddMeta(TEXT("meta.structTypePath"), MakeStruct->StructType->GetPathName());
                AddMeta(TEXT("meta.structTypeName"), MakeStruct->StructType->GetName());
                // Task 12: field -> input-pin mapping for complete struct construction reconstruction
                TArray<FString> FieldPinMap;
                for (TFieldIterator<FProperty> PropIt(MakeStruct->StructType); PropIt; ++PropIt)
                {
                    const FProperty* Prop = *PropIt;
                    if (!Prop) { continue; }
                    const FName FieldFName = Prop->GetFName();
                    UEdGraphPin* FoundPin = Node->FindPin(FieldFName, EGPD_Input);
                    const FString PinId = FoundPin ? FoundPin->PinId.ToString() : FString();
                    FieldPinMap.Add(FString::Printf(TEXT("%s|%s|%s"),
                        *FieldFName.ToString(), *Prop->GetCPPType(), *PinId));
                }
                if (FieldPinMap.Num() > 0)
                {
                    AddMeta(TEXT("meta.structFieldPinMap"), FString::Join(FieldPinMap, TEXT(";")));
                    AddMeta(TEXT("meta.structFieldCount"), FString::FromInt(FieldPinMap.Num()));
                }
            }
        }

        if (UK2Node_BreakStruct* BreakStruct = Cast<UK2Node_BreakStruct>(K2))
        {
            AddMetaBool(TEXT("meta.isBreakStruct"), true);
            if (BreakStruct->StructType)
            {
                AddMeta(TEXT("meta.structTypePath"), BreakStruct->StructType->GetPathName());
                AddMeta(TEXT("meta.structTypeName"), BreakStruct->StructType->GetName());
                // Task 12: field -> output-pin mapping for complete struct deconstruction reconstruction
                TArray<FString> FieldPinMap;
                for (TFieldIterator<FProperty> PropIt(BreakStruct->StructType); PropIt; ++PropIt)
                {
                    const FProperty* Prop = *PropIt;
                    if (!Prop) { continue; }
                    const FName FieldFName = Prop->GetFName();
                    UEdGraphPin* FoundPin = Node->FindPin(FieldFName, EGPD_Output);
                    const FString PinId = FoundPin ? FoundPin->PinId.ToString() : FString();
                    FieldPinMap.Add(FString::Printf(TEXT("%s|%s|%s"),
                        *FieldFName.ToString(), *Prop->GetCPPType(), *PinId));
                }
                if (FieldPinMap.Num() > 0)
                {
                    AddMeta(TEXT("meta.structFieldPinMap"), FString::Join(FieldPinMap, TEXT(";")));
                    AddMeta(TEXT("meta.structFieldCount"), FString::FromInt(FieldPinMap.Num()));
                }
            }
        }

        // --- FunctionEntry: local variable typed declarations + access specifier ---
        if (UK2Node_FunctionEntry* FuncEntry = Cast<UK2Node_FunctionEntry>(K2))
        {
            AddMetaBool(TEXT("meta.isFunctionEntry"), true);
            const int32 ExtraFlags = FuncEntry->GetExtraFlags();
            if (ExtraFlags & FUNC_Protected)
            {
                AddMeta(TEXT("meta.accessSpecifier"), TEXT("protected"));
            }
            else if (ExtraFlags & FUNC_Private)
            {
                AddMeta(TEXT("meta.accessSpecifier"), TEXT("private"));
            }
            else
            {
                AddMeta(TEXT("meta.accessSpecifier"), TEXT("public"));
            }
            // Serialize local variable type descriptions for C++ conversion
            TArray<FString> LocalVarDescs;
            for (const FBPVariableDescription& LocalVar : FuncEntry->LocalVariables)
            {
                FString VarDesc = FString::Printf(TEXT("%s: %s"),
                    *LocalVar.VarName.ToString(),
                    *DescribePinTypeDetailed(LocalVar.VarType));
                if (!LocalVar.DefaultValue.IsEmpty())
                {
                    VarDesc += FString::Printf(TEXT(" = %s"), *LocalVar.DefaultValue);
                }
                if (LocalVar.VarType.PinSubCategoryObject.IsValid())
                {
                    VarDesc += FString::Printf(TEXT(" [%s]"),
                        *LocalVar.VarType.PinSubCategoryObject->GetPathName());
                }
                LocalVarDescs.Add(VarDesc);
            }
            if (LocalVarDescs.Num() > 0)
            {
                AddMeta(TEXT("meta.localVarDecls"), FString::Join(LocalVarDescs, TEXT(";")));
                AddMeta(TEXT("meta.localVarCount"), FString::FromInt(LocalVarDescs.Num()));
            }
        }

        // --- FunctionResult: marks the return point of a function ---
        if (UK2Node_FunctionResult* FuncResult = Cast<UK2Node_FunctionResult>(K2))
        {
            AddMetaBool(TEXT("meta.isFunctionResult"), true);
        }

        // --- ComponentBoundEvent: event bound to a component's delegate ---
        if (UK2Node_ComponentBoundEvent* CompEvent = Cast<UK2Node_ComponentBoundEvent>(K2))
        {
            AddMeta(TEXT("meta.componentPropertyName"), CompEvent->ComponentPropertyName.ToString());
            AddMeta(TEXT("meta.delegatePropertyName"), CompEvent->DelegatePropertyName.ToString());
            if (UClass* Owner = CompEvent->EventReference.GetMemberParentClass())
            {
                AddMeta(TEXT("meta.eventOwner"), Owner->GetPathName());
            }
        }

        // --- Task 28: PromotableOperator — UE5-native type-promoted math/comparison node ---
        // Inherits UK2Node_CallFunction; GetTargetFunction() resolves the actual promoted op.
        if (UK2Node_PromotableOperator* PromoOp = Cast<UK2Node_PromotableOperator>(K2))
        {
            AddMetaBool(TEXT("meta.isPromotableOp"), true);
            UFunction* Func = PromoOp->GetTargetFunction();
            if (Func)
            {
                AddMeta(TEXT("meta.promotableOpFunc"), Func->GetName());
                UClass* FuncOwner = Func->GetOwnerClass();
                if (FuncOwner)
                {
                    AddMeta(TEXT("meta.promotableOpClass"), FuncOwner->GetPathName());
                }
            }
        }

        // --- Task 29: GetArrayItem — typed array element read with index ---
        // Pins[0]=array, Pins[1]=index("Dimension 1"), Pins[2]=result; pure node.
        if (UK2Node_GetArrayItem* GetItem = Cast<UK2Node_GetArrayItem>(K2))
        {
            AddMetaBool(TEXT("meta.isGetArrayItem"), true);
            UEdGraphPin* ArrPin = GetItem->GetTargetArrayPin();
            UEdGraphPin* IdxPin = GetItem->GetIndexPin();
            UEdGraphPin* RetPin = GetItem->GetResultPin();
            if (ArrPin) { AddMeta(TEXT("meta.arrayPinId"),  ArrPin->PinId.ToString()); }
            if (IdxPin) { AddMeta(TEXT("meta.indexPinId"),  IdxPin->PinId.ToString()); }
            if (RetPin) { AddMeta(TEXT("meta.resultPinId"), RetPin->PinId.ToString()); }
        }

        // --- Task 30: MakeArray / MakeMap / MakeSet — container constructors ---
        // All three inherit UK2Node_MakeContainer; GetOutputPin() gives the container output.
        // Non-exec input pins are the element (or key/value) slots.
        if (UK2Node_MakeArray* MkArr = Cast<UK2Node_MakeArray>(K2))
        {
            AddMetaBool(TEXT("meta.isMakeArray"), true);
            UEdGraphPin* OutPin = MkArr->GetOutputPin();
            if (OutPin) { AddMeta(TEXT("meta.outputPinId"), OutPin->PinId.ToString()); }
            TArray<FString> ElemPinIds;
            for (UEdGraphPin* Pin : MkArr->Pins)
            {
                if (Pin && Pin != OutPin
                    && Pin->Direction == EGPD_Input
                    && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                {
                    ElemPinIds.Add(Pin->PinId.ToString());
                }
            }
            AddMeta(TEXT("meta.elementPinCount"), FString::FromInt(ElemPinIds.Num()));
            if (ElemPinIds.Num() > 0)
            {
                AddMeta(TEXT("meta.elementPinIds"), FString::Join(ElemPinIds, TEXT(";")));
            }
        }

        if (UK2Node_MakeMap* MkMap = Cast<UK2Node_MakeMap>(K2))
        {
            AddMetaBool(TEXT("meta.isMakeMap"), true);
            UEdGraphPin* OutPin = MkMap->GetOutputPin();
            if (OutPin) { AddMeta(TEXT("meta.outputPinId"), OutPin->PinId.ToString()); }
            TArray<FString> ElemPinIds;
            for (UEdGraphPin* Pin : MkMap->Pins)
            {
                if (Pin && Pin != OutPin
                    && Pin->Direction == EGPD_Input
                    && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                {
                    ElemPinIds.Add(Pin->PinId.ToString());
                }
            }
            AddMeta(TEXT("meta.elementPinCount"), FString::FromInt(ElemPinIds.Num()));
            if (ElemPinIds.Num() > 0)
            {
                AddMeta(TEXT("meta.elementPinIds"), FString::Join(ElemPinIds, TEXT(";")));
            }
        }

        if (UK2Node_MakeSet* MkSet = Cast<UK2Node_MakeSet>(K2))
        {
            AddMetaBool(TEXT("meta.isMakeSet"), true);
            UEdGraphPin* OutPin = MkSet->GetOutputPin();
            if (OutPin) { AddMeta(TEXT("meta.outputPinId"), OutPin->PinId.ToString()); }
            TArray<FString> ElemPinIds;
            for (UEdGraphPin* Pin : MkSet->Pins)
            {
                if (Pin && Pin != OutPin
                    && Pin->Direction == EGPD_Input
                    && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                {
                    ElemPinIds.Add(Pin->PinId.ToString());
                }
            }
            AddMeta(TEXT("meta.elementPinCount"), FString::FromInt(ElemPinIds.Num()));
            if (ElemPinIds.Num() > 0)
            {
                AddMeta(TEXT("meta.elementPinIds"), FString::Join(ElemPinIds, TEXT(";")));
            }
        }

        // --- Task 31: Tunnel — collapsed sub-graph or macro entry/exit boundary ---
        // K2Node_FunctionEntry and K2Node_FunctionResult also inherit UK2Node_Tunnel;
        // exclude them since they have dedicated handlers above.
        if (UK2Node_Tunnel* Tunnel = Cast<UK2Node_Tunnel>(K2))
        {
            if (!Cast<UK2Node_FunctionEntry>(K2) && !Cast<UK2Node_FunctionResult>(K2))
            {
                AddMetaBool(TEXT("meta.isTunnel"), true);
                // GetInputSink/GetOutputSource link the entry and exit tunnel pair.
                UK2Node_Tunnel* Sink   = Tunnel->GetInputSink();
                UK2Node_Tunnel* Source = Tunnel->GetOutputSource();
                if (Sink)   { AddMeta(TEXT("meta.tunnelSinkNodeGuid"),   Sink->NodeGuid.ToString()); }
                if (Source) { AddMeta(TEXT("meta.tunnelSourceNodeGuid"), Source->NodeGuid.ToString()); }
            }
        }

        // --- Task 32a: EnumEquality / EnumInequality — enum comparison nodes ---
        // EnumInequality inherits EnumEquality, so one Cast handles both; distinguish by subtype.
        if (UK2Node_EnumEquality* EnumEq = Cast<UK2Node_EnumEquality>(K2))
        {
            const bool bIsInequality = (Cast<UK2Node_EnumInequality>(K2) != nullptr);
            AddMetaBool(bIsInequality ? TEXT("meta.isEnumInequality") : TEXT("meta.isEnumEquality"), true);
            UEdGraphPin* Pin1   = EnumEq->GetInput1Pin();
            UEdGraphPin* Pin2   = EnumEq->GetInput2Pin();
            UEdGraphPin* RetPin = EnumEq->GetReturnValuePin();
            if (Pin1)   { AddMeta(TEXT("meta.inputAPin"),   Pin1->PinId.ToString()); }
            if (Pin2)   { AddMeta(TEXT("meta.inputBPin"),   Pin2->PinId.ToString()); }
            if (RetPin) { AddMeta(TEXT("meta.returnPinId"), RetPin->PinId.ToString()); }
            // Capture the enum class from the first input pin's type for C++ codegen
            if (Pin1 && Pin1->PinType.PinSubCategoryObject.IsValid())
            {
                AddMeta(TEXT("meta.enumClass"), Pin1->PinType.PinSubCategoryObject->GetPathName());
            }
        }

        // --- Task 32b: SetVariableOnPersistentFrame — assigns a local var on the ubergraph frame ---
        // Data input pin names are the target variable names (resolved by FindPropertyInScope at compile).
        if (UK2Node_SetVariableOnPersistentFrame* SetPF = Cast<UK2Node_SetVariableOnPersistentFrame>(K2))
        {
            AddMetaBool(TEXT("meta.isSetPersistentFrame"), true);
            TArray<FString> VarNames;
            TArray<FString> VarPinIds;
            for (UEdGraphPin* Pin : SetPF->Pins)
            {
                if (Pin
                    && Pin->Direction == EGPD_Input
                    && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                {
                    VarNames.Add(Pin->PinName.ToString());
                    VarPinIds.Add(Pin->PinId.ToString());
                }
            }
            if (VarNames.Num() > 0)
            {
                AddMeta(TEXT("meta.persistentVarNames"),  FString::Join(VarNames,  TEXT(";")));
                AddMeta(TEXT("meta.persistentVarPinIds"), FString::Join(VarPinIds, TEXT(";")));
                AddMeta(TEXT("meta.persistentVarCount"),  FString::FromInt(VarNames.Num()));
            }
        }

        // =====================================================================
        // Tasks 33-50: remaining partially-supported K2Node type handlers
        // =====================================================================

        // --- Task 33: CallMaterialParameterCollectionFunction ---
        // Inherits UK2Node_CallFunction — function ref already extracted by CallFunction handler.
        // Add a marker so the converter distinguishes MPC parameter calls from generic function calls.
        if (Cast<UK2Node_CallMaterialParameterCollectionFunction>(K2))
        {
            AddMetaBool(TEXT("meta.isCallMaterialParamCollection"), true);
        }

        // --- Task 34: GetSubsystem / GetEngineSubsystem / GetSubsystemFromPC ---
        // All three inherit UK2Node_GetSubsystem; CustomClass (protected) holds the subsystem type.
        // Use reflection to access it; GetResultPin() is public.
        if (UK2Node_GetSubsystem* SubsysNode = Cast<UK2Node_GetSubsystem>(K2))
        {
            AddMetaBool(TEXT("meta.isGetSubsystem"), true);
            // CustomClass is protected — access via reflection
            if (FClassProperty* CCP = FindFProperty<FClassProperty>(
                    SubsysNode->GetClass(), TEXT("CustomClass")))
            {
                UClass* SubsysClass = Cast<UClass>(
                    CCP->GetObjectPropertyValue_InContainer(SubsysNode));
                if (SubsysClass)
                {
                    AddMeta(TEXT("meta.subsystemClass"), SubsysClass->GetPathName());
                    AddMeta(TEXT("meta.subsystemName"),  SubsysClass->GetName());
                }
            }
            // Distinguish which subsystem accessor variant this is
            if (Cast<UK2Node_GetEngineSubsystem>(K2))
                AddMeta(TEXT("meta.subsystemVariant"), TEXT("Engine"));
            else if (Cast<UK2Node_GetSubsystemFromPC>(K2))
                AddMeta(TEXT("meta.subsystemVariant"), TEXT("LocalPlayer"));
            else
                AddMeta(TEXT("meta.subsystemVariant"), TEXT("World"));
            UEdGraphPin* ResultPin = SubsysNode->GetResultPin();
            if (ResultPin)
                AddMeta(TEXT("meta.resultPinId"), ResultPin->PinId.ToString());
        }

        // --- Task 35: LatentAbilityCall / LatentGameplayTaskCall / AsyncAction ---
        // All ultimately inherit UK2Node_BaseAsyncTask which carries the proxy class metadata.
        // The proxy UPROPERTYs (ProxyFactoryFunctionName, ProxyFactoryClass, ProxyClass,
        // ProxyActivateFunctionName) are protected — access them via the reflection system.
        // LatentGameplayTaskCall also stores SpawnParamPins for dynamically exposed properties.
        if (UK2Node_BaseAsyncTask* AsyncBase = Cast<UK2Node_BaseAsyncTask>(K2))
        {
            AddMetaBool(TEXT("meta.isBaseAsyncTask"), true);
            UClass* AsyncClass = AsyncBase->GetClass();
            // ProxyFactoryFunctionName (protected UPROPERTY)
            if (FNameProperty* P = FindFProperty<FNameProperty>(
                    AsyncClass, TEXT("ProxyFactoryFunctionName")))
            {
                FName Val = P->GetPropertyValue_InContainer(AsyncBase);
                if (!Val.IsNone())
                    AddMeta(TEXT("meta.proxyFactoryFunctionName"), Val.ToString());
            }
            // ProxyFactoryClass (protected UPROPERTY)
            if (FClassProperty* P = FindFProperty<FClassProperty>(
                    AsyncClass, TEXT("ProxyFactoryClass")))
            {
                UClass* Val = Cast<UClass>(P->GetObjectPropertyValue_InContainer(AsyncBase));
                if (Val)
                {
                    AddMeta(TEXT("meta.proxyFactoryClass"),     Val->GetPathName());
                    AddMeta(TEXT("meta.proxyFactoryClassName"), Val->GetName());
                }
            }
            // ProxyClass (protected UPROPERTY)
            if (FClassProperty* P = FindFProperty<FClassProperty>(
                    AsyncClass, TEXT("ProxyClass")))
            {
                UClass* Val = Cast<UClass>(P->GetObjectPropertyValue_InContainer(AsyncBase));
                if (Val)
                {
                    AddMeta(TEXT("meta.proxyClass"),     Val->GetPathName());
                    AddMeta(TEXT("meta.proxyClassName"), Val->GetName());
                }
            }
            // ProxyActivateFunctionName (protected UPROPERTY)
            if (FNameProperty* P = FindFProperty<FNameProperty>(
                    AsyncClass, TEXT("ProxyActivateFunctionName")))
            {
                FName Val = P->GetPropertyValue_InContainer(AsyncBase);
                if (!Val.IsNone())
                    AddMeta(TEXT("meta.proxyActivateFunctionName"), Val.ToString());
            }

            // Variant identification
            if (Cast<UK2Node_LatentAbilityCall>(K2))
                AddMeta(TEXT("meta.asyncTaskVariant"), TEXT("AbilityTask"));
            else if (UK2Node_LatentGameplayTaskCall* TaskCall =
                     Cast<UK2Node_LatentGameplayTaskCall>(K2))
            {
                AddMeta(TEXT("meta.asyncTaskVariant"), TEXT("GameplayTask"));
                if (TaskCall->SpawnParamPins.Num() > 0)
                {
                    TArray<FString> PinStrs;
                    for (const FName& PN : TaskCall->SpawnParamPins)
                        PinStrs.Add(PN.ToString());
                    AddMeta(TEXT("meta.spawnParamPins"),
                        FString::Join(PinStrs, TEXT(";")));
                    AddMeta(TEXT("meta.spawnParamPinCount"),
                        FString::FromInt(TaskCall->SpawnParamPins.Num()));
                }
            }
            else if (Cast<UK2Node_AsyncAction>(K2))
                AddMeta(TEXT("meta.asyncTaskVariant"), TEXT("AsyncAction"));
            else
                AddMeta(TEXT("meta.asyncTaskVariant"), TEXT("BaseAsyncTask"));

            // Collect delegate output exec pins (one per assignable delegate on the proxy class)
            TArray<FString> DelegatePinNames;
            for (UEdGraphPin* Pin : AsyncBase->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Output
                    && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                {
                    const FString PinName = Pin->PinName.ToString();
                    if (!PinName.Equals(TEXT("then"), ESearchCase::IgnoreCase))
                        DelegatePinNames.Add(PinName + TEXT(":") + Pin->PinId.ToString());
                }
            }
            if (DelegatePinNames.Num() > 0)
            {
                AddMeta(TEXT("meta.asyncDelegatePins"),
                    FString::Join(DelegatePinNames, TEXT(";")));
                AddMeta(TEXT("meta.asyncDelegatePinCount"),
                    FString::FromInt(DelegatePinNames.Num()));
            }

            TArray<FString> InputPins;
            TArray<FString> OutputDataPins;
            for (UEdGraphPin* Pin : AsyncBase->Pins)
            {
                if (!Pin || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                {
                    continue;
                }

                const FString Description = FString::Printf(TEXT("%s:%s:%s"),
                    *Pin->PinName.ToString(),
                    *Pin->PinId.ToString(),
                    *DescribePinTypeDetailed(Pin->PinType));
                if (Pin->Direction == EGPD_Input)
                {
                    InputPins.Add(Description);
                }
                else if (Pin->Direction == EGPD_Output)
                {
                    OutputDataPins.Add(Description);
                }
            }
            if (InputPins.Num() > 0)
                AddMeta(TEXT("meta.asyncInputPins"), FString::Join(InputPins, TEXT(";")));
            if (OutputDataPins.Num() > 0)
                AddMeta(TEXT("meta.asyncOutputDataPins"), FString::Join(OutputDataPins, TEXT(";")));
        }

        // --- Task 36: AddComponent — adds a component instance defined by a CDO template ---
        if (UK2Node_AddComponent* AddComp = Cast<UK2Node_AddComponent>(K2))
        {
            AddMetaBool(TEXT("meta.isAddComponent"), true);
            if (!AddComp->TemplateBlueprint.IsEmpty())
                AddMeta(TEXT("meta.componentTemplateBlueprint"), AddComp->TemplateBlueprint);
            if (AddComp->TemplateType)
            {
                AddMeta(TEXT("meta.componentClass"),     AddComp->TemplateType->GetPathName());
                AddMeta(TEXT("meta.componentClassName"), AddComp->TemplateType->GetName());
            }
            AddMetaBool(TEXT("meta.componentHasExposedVariable"),
                AddComp->bHasExposedVariable != 0);
            UActorComponent* CompTemplate = AddComp->GetTemplateFromNode();
            if (CompTemplate)
                AddMeta(TEXT("meta.componentTemplateName"), CompTemplate->GetName());
        }

        // --- Task 37: SetFieldsInStruct — impure MakeStruct variant (sets specific fields only) ---
        if (UK2Node_SetFieldsInStruct* SetFields = Cast<UK2Node_SetFieldsInStruct>(K2))
        {
            AddMetaBool(TEXT("meta.isSetFieldsInStruct"), true);
            AddMetaBool(TEXT("meta.allStructPinsShown"), SetFields->AllPinsAreShown());
        }

        // --- Task 38: AsyncAction — UBlueprintAsyncActionBase factory ---
        // Proxy metadata already extracted by the UK2Node_BaseAsyncTask branch above.
        if (Cast<UK2Node_AsyncAction>(K2))
        {
            AddMetaBool(TEXT("meta.isAsyncAction"), true);
        }

        // --- Task 39: Delegate management nodes (Add / Assign / Remove) ---
        // All share UK2Node_BaseMCDelegate which stores the FMemberReference to the delegate property.
        if (UK2Node_BaseMCDelegate* MCDel = Cast<UK2Node_BaseMCDelegate>(K2))
        {
            const FName DelegatePropName = MCDel->GetPropertyName();
            if (!DelegatePropName.IsNone())
                AddMeta(TEXT("meta.delegatePropertyName"), DelegatePropName.ToString());

            // Owner class / package for delegate resolution
            const FMemberReference& DelRef = MCDel->DelegateReference;
            if (DelRef.GetMemberParentClass() != nullptr)
                AddMeta(TEXT("meta.delegateOwnerClass"),
                    DelRef.GetMemberParentClass()->GetPathName());
            else if (UPackage* OwnerPkg = DelRef.GetMemberParentPackage())
                AddMeta(TEXT("meta.delegateOwnerPackage"), OwnerPkg->GetName());

            // Signature function carries the parameter type list
            UFunction* Sig = MCDel->GetDelegateSignature();
            if (Sig)
            {
                AddMeta(TEXT("meta.delegateSignatureName"), Sig->GetName());
                if (Sig->GetOwnerClass())
                    AddMeta(TEXT("meta.delegateSignatureClass"),
                        Sig->GetOwnerClass()->GetPathName());
            }
            UEdGraphPin* DelPin = MCDel->GetDelegatePin();
            if (DelPin)
                AddMeta(TEXT("meta.delegatePinId"), DelPin->PinId.ToString());

            // Identify the specific delegate operation
            if (Cast<UK2Node_AssignDelegate>(K2))
                AddMeta(TEXT("meta.delegateOp"), TEXT("Assign"));
            else if (Cast<UK2Node_AddDelegate>(K2))
                AddMeta(TEXT("meta.delegateOp"), TEXT("Add"));
            else if (Cast<UK2Node_RemoveDelegate>(K2))
                AddMeta(TEXT("meta.delegateOp"), TEXT("Remove"));
        }

        // --- Task 40: Message — interface function call dispatched dynamically at runtime ---
        // Inherits UK2Node_CallFunction; function reference already extracted by that handler.
        if (Cast<UK2Node_Message>(K2))
        {
            AddMetaBool(TEXT("meta.isInterfaceMessage"), true);
        }

        // --- Task 41: GenericCreateObject / CreateWidget — object / widget factory nodes ---
        // Both inherit UK2Node_ConstructObjectFromClass which exposes the class pin.
        // K2Node_CreateWidget has a private header; detect it by class name.
        if (UK2Node_ConstructObjectFromClass* CreateObj =
            Cast<UK2Node_ConstructObjectFromClass>(K2))
        {
            const FString NodeClassName = K2->GetClass()->GetName();
            if (NodeClassName.Equals(TEXT("K2Node_CreateWidget")))
                AddMetaBool(TEXT("meta.isCreateWidget"), true);
            else
                AddMetaBool(TEXT("meta.isGenericCreateObject"), true);

            UEdGraphPin* ClassPin  = CreateObj->GetClassPin();
            UEdGraphPin* ResultPin = CreateObj->GetResultPin();
            if (ClassPin)
            {
                AddMeta(TEXT("meta.createClassPinId"), ClassPin->PinId.ToString());
                if (!ClassPin->DefaultValue.IsEmpty())
                    AddMeta(TEXT("meta.createClassDefault"), ClassPin->DefaultValue);
                if (ClassPin->PinType.PinSubCategoryObject.IsValid())
                    AddMeta(TEXT("meta.createClassBaseType"),
                        ClassPin->PinType.PinSubCategoryObject->GetPathName());
            }
            if (ResultPin)
                AddMeta(TEXT("meta.createResultPinId"), ResultPin->PinId.ToString());
        }

        // AddComponentByClass inherits ConstructObjectFromClass, but its compiler expansion has
        // component-specific attachment and deferred-finish semantics that the generic factory
        // metadata above cannot express. Keep this handler class-name based so the serializer
        // remains source-compatible with UE versions where the concrete node's API visibility
        // differs, while still exporting every reconstructable input from the stable pin contract.
        if (K2->GetClass()->GetName() == TEXT("K2Node_AddComponentByClass"))
        {
            AddMetaBool(TEXT("meta.isAddComponentByClass"), true);
            AddMeta(TEXT("meta.addComponentExpansionFunction"),
                TEXT("/Script/Engine.Actor:AddComponentByClass"));
            AddMeta(TEXT("meta.addComponentFinishFunction"),
                TEXT("/Script/Engine.Actor:FinishAddComponent"));

            auto EmitAddComponentPin = [&](const TCHAR* PinName,
                                           EEdGraphPinDirection Direction,
                                           const TCHAR* PinIdKey) -> UEdGraphPin*
            {
                UEdGraphPin* Pin = K2->FindPin(FName(PinName), Direction);
                if (Pin)
                {
                    AddMeta(PinIdKey, Pin->PinId.ToString());
                    AddMeta(FString(PinIdKey) + TEXT("Type"),
                        DescribePinTypeDetailed(Pin->PinType));
                }
                return Pin;
            };

            EmitAddComponentPin(TEXT("execute"), EGPD_Input,
                TEXT("meta.addComponentExecutePinId"));
            EmitAddComponentPin(TEXT("self"), EGPD_Input,
                TEXT("meta.addComponentOwnerPinId"));
            EmitAddComponentPin(TEXT("then"), EGPD_Output,
                TEXT("meta.addComponentThenPinId"));
            UEdGraphPin* ClassPin = EmitAddComponentPin(TEXT("Class"), EGPD_Input,
                TEXT("meta.addComponentClassPinId"));
            UEdGraphPin* ResultPin = EmitAddComponentPin(TEXT("ReturnValue"), EGPD_Output,
                TEXT("meta.addComponentResultPinId"));
            UEdGraphPin* ManualAttachmentPin = EmitAddComponentPin(
                TEXT("bManualAttachment"), EGPD_Input,
                TEXT("meta.addComponentManualAttachmentPinId"));
            UEdGraphPin* RelativeTransformPin = EmitAddComponentPin(
                TEXT("RelativeTransform"), EGPD_Input,
                TEXT("meta.addComponentRelativeTransformPinId"));

            if (ClassPin)
            {
                AddMetaBool(TEXT("meta.addComponentClassIsDynamic"),
                    ClassPin->LinkedTo.Num() > 0);
                if (!ClassPin->DefaultValue.IsEmpty())
                    AddMeta(TEXT("meta.addComponentClassDefault"), ClassPin->DefaultValue);
                if (ClassPin->DefaultObject)
                {
                    AddMeta(TEXT("meta.addComponentClassPath"),
                        ClassPin->DefaultObject->GetPathName());
                    AddMeta(TEXT("meta.addComponentClassName"),
                        ClassPin->DefaultObject->GetName());
                    if (const UClass* ComponentClass = Cast<UClass>(ClassPin->DefaultObject))
                    {
                        AddMetaBool(TEXT("meta.addComponentClassIsSceneComponent"),
                            ComponentClass->IsChildOf(USceneComponent::StaticClass()));
                    }
                }
            }

            if (ResultPin)
            {
                if (ResultPin->PinType.PinSubCategoryObject.IsValid())
                {
                    AddMeta(TEXT("meta.addComponentResultClass"),
                        ResultPin->PinType.PinSubCategoryObject->GetPathName());
                }
            }

            if (ManualAttachmentPin)
            {
                AddMeta(TEXT("meta.addComponentManualAttachmentDefaultRaw"),
                    ManualAttachmentPin->DefaultValue);
                AddMetaBool(TEXT("meta.addComponentManualAttachmentDefault"),
                    ManualAttachmentPin->DefaultValue.Equals(
                        TEXT("true"), ESearchCase::IgnoreCase));
                AddMetaBool(TEXT("meta.addComponentManualAttachmentPinHidden"),
                    ManualAttachmentPin->bHidden);
            }

            if (RelativeTransformPin)
            {
                AddMeta(TEXT("meta.addComponentRelativeTransformDefaultRaw"),
                    RelativeTransformPin->DefaultValue);
                AddMetaBool(TEXT("meta.addComponentRelativeTransformUsesIdentityDefault"),
                    RelativeTransformPin->LinkedTo.Num() == 0
                    && RelativeTransformPin->DefaultValue.IsEmpty()
                    && RelativeTransformPin->DefaultObject == nullptr
                    && RelativeTransformPin->DefaultTextValue.IsEmpty());
                AddMetaBool(TEXT("meta.addComponentRelativeTransformPinHidden"),
                    RelativeTransformPin->bHidden);
            }

            TArray<FString> ExposeOnSpawnPins;
            for (UEdGraphPin* Pin : K2->Pins)
            {
                if (!Pin || Pin->Direction != EGPD_Input
                    || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
                    || Pin->PinName == UEdGraphSchema_K2::PN_Self
                    || Pin->PinName == TEXT("Class")
                    || Pin->PinName == TEXT("bManualAttachment")
                    || Pin->PinName == TEXT("RelativeTransform"))
                {
                    continue;
                }

                ExposeOnSpawnPins.Add(FString::Printf(TEXT("%s:%s:%s"),
                    *Pin->PinName.ToString(),
                    *Pin->PinId.ToString(),
                    *DescribePinTypeDetailed(Pin->PinType)));
            }
            ExposeOnSpawnPins.Sort();
            AddMetaBool(TEXT("meta.addComponentHasExposeOnSpawnPins"),
                ExposeOnSpawnPins.Num() > 0);
            AddMeta(TEXT("meta.addComponentExposeOnSpawnPinCount"),
                FString::FromInt(ExposeOnSpawnPins.Num()));
            if (ExposeOnSpawnPins.Num() > 0)
            {
                AddMeta(TEXT("meta.addComponentExposeOnSpawnPins"),
                    FString::Join(ExposeOnSpawnPins, TEXT(";")));
            }
            AddMetaBool(TEXT("meta.addComponentRequiresDeferredFinish"),
                ExposeOnSpawnPins.Num() > 0);
        }

        // --- Task 42: FormatText — FText formatting with named dynamic argument slots ---
        if (UK2Node_FormatText* FmtText = Cast<UK2Node_FormatText>(K2))
        {
            AddMetaBool(TEXT("meta.isFormatText"), true);
            AddMeta(TEXT("meta.formatArgumentCount"),
                FString::FromInt(FmtText->GetArgumentCount()));
            // PinNames is private; use the public GetArgumentCount()/GetArgumentName(i) accessors
            const int32 ArgCount = FmtText->GetArgumentCount();
            if (ArgCount > 0)
            {
                TArray<FString> ArgNames;
                for (int32 ArgIdx = 0; ArgIdx < ArgCount; ++ArgIdx)
                    ArgNames.Add(FmtText->GetArgumentName(ArgIdx).ToString());
                AddMeta(TEXT("meta.formatArgumentNames"),
                    FString::Join(ArgNames, TEXT(";")));
            }
            UEdGraphPin* FormatPin = FmtText->GetFormatPin();
            if (FormatPin)
            {
                AddMeta(TEXT("meta.formatPinId"), FormatPin->PinId.ToString());
                if (!FormatPin->DefaultValue.IsEmpty())
                    AddMeta(TEXT("meta.formatDefaultText"), FormatPin->DefaultValue);
            }
        }

        // --- Task 43: InputKey — raw input key / button event node ---
        if (UK2Node_InputKey* InpKey = Cast<UK2Node_InputKey>(K2))
        {
            AddMetaBool(TEXT("meta.isInputKey"), true);
            AddMeta(TEXT("meta.inputKeyName"), InpKey->InputKey.GetFName().ToString());
            AddMeta(TEXT("meta.inputKeyDisplayName"),
                InpKey->InputKey.GetDisplayName().ToString());
            AddMetaBool(TEXT("meta.inputKeyConsumed"),       InpKey->bConsumeInput  != 0);
            AddMetaBool(TEXT("meta.inputKeyWhenPaused"),     InpKey->bExecuteWhenPaused != 0);
            AddMetaBool(TEXT("meta.inputKeyOverrideParent"), InpKey->bOverrideParentBinding != 0);
            AddMetaBool(TEXT("meta.inputKeyCtrl"),    InpKey->bControl  != 0);
            AddMetaBool(TEXT("meta.inputKeyAlt"),     InpKey->bAlt      != 0);
            AddMetaBool(TEXT("meta.inputKeyShift"),   InpKey->bShift    != 0);
            AddMetaBool(TEXT("meta.inputKeyCmd"),     InpKey->bCommand  != 0);
            UEdGraphPin* PressedPin  = InpKey->GetPressedPin();
            UEdGraphPin* ReleasedPin = InpKey->GetReleasedPin();
            if (PressedPin)  AddMeta(TEXT("meta.pressedPinId"),  PressedPin->PinId.ToString());
            if (ReleasedPin) AddMeta(TEXT("meta.releasedPinId"), ReleasedPin->PinId.ToString());
        }

        // --- Task 44: MathExpression — expression-string-driven inline math sub-graph ---
        if (UK2Node_MathExpression* MathExpr = Cast<UK2Node_MathExpression>(K2))
        {
            AddMetaBool(TEXT("meta.isMathExpression"), true);
            if (!MathExpr->Expression.IsEmpty())
                AddMeta(TEXT("meta.expression"), MathExpr->Expression);
        }

        // --- Task 45: EnhancedInputAction — UInputAction-bound event node ---
        if (UK2Node_EnhancedInputAction* EIA = Cast<UK2Node_EnhancedInputAction>(K2))
        {
            AddMetaBool(TEXT("meta.isEnhancedInputAction"), true);
            if (EIA->InputAction)
            {
                AddMeta(TEXT("meta.inputActionPath"), EIA->InputAction->GetPathName());
                AddMeta(TEXT("meta.inputActionName"), EIA->InputAction->GetName());
            }
            // Collect trigger event exec output pins (Triggered, Started, Ongoing, etc.)
            TArray<FString> TriggerPins;
            for (UEdGraphPin* Pin : EIA->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Output
                    && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                {
                    TriggerPins.Add(Pin->PinName.ToString()
                        + TEXT(":") + Pin->PinId.ToString());
                }
            }
            if (TriggerPins.Num() > 0)
                AddMeta(TEXT("meta.triggerEventPins"),
                    FString::Join(TriggerPins, TEXT(";")));
        }

        // --- Task 46: AssignmentStatement — wildcard lvalue/rvalue assignment ---
        if (UK2Node_AssignmentStatement* AssignStmt =
            Cast<UK2Node_AssignmentStatement>(K2))
        {
            AddMetaBool(TEXT("meta.isAssignmentStatement"), true);
            UEdGraphPin* VarPin = AssignStmt->GetVariablePin();
            UEdGraphPin* ValPin = AssignStmt->GetValuePin();
            if (VarPin)
            {
                AddMeta(TEXT("meta.assignVariablePinId"),
                    VarPin->PinId.ToString());
                AddMeta(TEXT("meta.assignVariableCategory"),
                    VarPin->PinType.PinCategory.ToString());
                if (VarPin->PinType.PinSubCategoryObject.IsValid())
                    AddMeta(TEXT("meta.assignVariableType"),
                        VarPin->PinType.PinSubCategoryObject->GetPathName());
            }
            if (ValPin)
            {
                AddMeta(TEXT("meta.assignValuePinId"),
                    ValPin->PinId.ToString());
                AddMeta(TEXT("meta.assignValueCategory"),
                    ValPin->PinType.PinCategory.ToString());
            }
        }

        // --- Task 47: TemporaryVariable — compiler-generated typed local storage slot ---
        if (UK2Node_TemporaryVariable* TmpVar = Cast<UK2Node_TemporaryVariable>(K2))
        {
            AddMetaBool(TEXT("meta.isTemporaryVariable"), true);
            AddMetaBool(TEXT("meta.tempVarIsPersistent"), TmpVar->bIsPersistent);
            const FEdGraphPinType& PT = TmpVar->VariableType;
            AddMeta(TEXT("meta.tempVarCategory"),    PT.PinCategory.ToString());
            AddMeta(TEXT("meta.tempVarSubCategory"), PT.PinSubCategory.ToString());
            if (PT.PinSubCategoryObject.IsValid())
                AddMeta(TEXT("meta.tempVarSubCategoryObject"),
                    PT.PinSubCategoryObject->GetPathName());
            AddMetaBool(TEXT("meta.tempVarIsArray"), PT.IsArray());
            AddMetaBool(TEXT("meta.tempVarIsSet"),   PT.IsSet());
            AddMetaBool(TEXT("meta.tempVarIsMap"),   PT.IsMap());
            AddMetaBool(TEXT("meta.tempVarIsRef"),   PT.bIsReference);
            UEdGraphPin* VarPin = TmpVar->GetVariablePin();
            if (VarPin)
                AddMeta(TEXT("meta.tempVarPinId"), VarPin->PinId.ToString());
        }

        // --- Task 48: PropertyAccess — property-path accessor (private header; use reflection) ---
        // K2Node_PropertyAccess.h lives in a Private module directory and cannot be included
        // externally.  We detect the node by class FName and extract UPROPERTYs via reflection.
        {
            static const FName PropAccessClassName(TEXT("K2Node_PropertyAccess"));
            if (K2->GetClass()->GetFName() == PropAccessClassName)
            {
                AddMetaBool(TEXT("meta.isPropertyAccess"), true);
                // Extract Path (TArray<FString>) — the property access chain segments
                if (FArrayProperty* PathProp = FindFProperty<FArrayProperty>(
                        K2->GetClass(), TEXT("Path")))
                {
                    FScriptArrayHelper PathHelper(PathProp,
                        PathProp->ContainerPtrToValuePtr<void>(K2));
                    FStrProperty* ElemProp =
                        CastField<FStrProperty>(PathProp->Inner);
                    TArray<FString> Segments;
                    if (ElemProp)
                    {
                        for (int32 Idx = 0; Idx < PathHelper.Num(); ++Idx)
                            Segments.Add(ElemProp->GetPropertyValue(
                                PathHelper.GetRawPtr(Idx)));
                    }
                    if (Segments.Num() > 0)
                    {
                        AddMeta(TEXT("meta.propertyAccessPath"),
                            FString::Join(Segments, TEXT(".")));
                        AddMeta(TEXT("meta.propertyAccessDepth"),
                            FString::FromInt(Segments.Num()));
                    }
                }
                // Extract ContextId (FName) — compilation context hint
                if (FNameProperty* CtxProp = FindFProperty<FNameProperty>(
                        K2->GetClass(), TEXT("ContextId")))
                {
                    FName CtxId = CtxProp->GetPropertyValue_InContainer(K2);
                    if (!CtxId.IsNone())
                        AddMeta(TEXT("meta.propertyAccessContextId"),
                            CtxId.ToString());
                }
            }
        }

        // --- Task 49: GetDataTableRow — row struct lookup from a UDataTable asset ---
        if (UK2Node_GetDataTableRow* DTRow = Cast<UK2Node_GetDataTableRow>(K2))
        {
            AddMetaBool(TEXT("meta.isGetDataTableRow"), true);
            UEdGraphPin* TablePin    = DTRow->GetDataTablePin();
            UEdGraphPin* RowPin      = DTRow->GetRowNamePin();
            UEdGraphPin* NotFoundPin = DTRow->GetRowNotFoundPin();
            UEdGraphPin* ResultPin   = DTRow->GetResultPin();
            if (TablePin)
            {
                AddMeta(TEXT("meta.dataTablePinId"), TablePin->PinId.ToString());
                if (!TablePin->DefaultValue.IsEmpty())
                    AddMeta(TEXT("meta.dataTableDefault"), TablePin->DefaultValue);
            }
            if (RowPin)
            {
                AddMeta(TEXT("meta.rowNamePinId"), RowPin->PinId.ToString());
                if (!RowPin->DefaultValue.IsEmpty())
                    AddMeta(TEXT("meta.rowNameDefault"), RowPin->DefaultValue);
            }
            if (NotFoundPin)
                AddMeta(TEXT("meta.rowNotFoundPinId"), NotFoundPin->PinId.ToString());
            if (ResultPin)
            {
                AddMeta(TEXT("meta.dtRowResultPinId"), ResultPin->PinId.ToString());
                if (ResultPin->PinType.PinSubCategoryObject.IsValid())
                    AddMeta(TEXT("meta.dtRowStructType"),
                        ResultPin->PinType.PinSubCategoryObject->GetPathName());
            }
        }

        // --- Task 50: GetEnumeratorName — enum value → FName display name ---
        // Also handles K2Node_GetEnumeratorNameAsString (subclass returning FString instead of FName).
        if (Cast<UK2Node_GetEnumeratorName>(K2))
        {
            AddMetaBool(TEXT("meta.isGetEnumeratorName"), true);
            // Distinguish the string-returning variant for converter output-type accuracy
            if (K2->GetClass()->GetName() == TEXT("K2Node_GetEnumeratorNameAsString"))
                AddMetaBool(TEXT("meta.isGetEnumeratorNameAsString"), true);
            // Locate the enum input pin (byte category, linked to enum type)
            for (UEdGraphPin* Pin : K2->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Input
                    && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
                {
                    AddMeta(TEXT("meta.enumeratorInputPinId"), Pin->PinId.ToString());
                    if (Pin->PinType.PinSubCategoryObject.IsValid())
                        AddMeta(TEXT("meta.enumeratorEnumClass"),
                            Pin->PinType.PinSubCategoryObject->GetPathName());
                    break;
                }
            }
        }

        // Project/plugin map-loop nodes can be reconstructed entirely from their
        // stable pin contract even when their concrete class header is unavailable.
        if (K2->GetClass()->GetName() == TEXT("K2Node_MapForEach"))
        {
            AddMetaBool(TEXT("meta.isMapForEach"), true);
            auto EmitMapPin = [&](const TCHAR* PinName, EEdGraphPinDirection Direction, const TCHAR* MetaKey)
            {
                if (UEdGraphPin* Pin = K2->FindPin(FName(PinName), Direction))
                {
                    AddMeta(MetaKey, Pin->PinId.ToString());
                    if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                    {
                        AddMeta(FString(MetaKey) + TEXT("Type"), DescribePinTypeDetailed(Pin->PinType));
                    }
                }
            };
            EmitMapPin(TEXT("execute"), EGPD_Input, TEXT("meta.mapForEachExecutePinId"));
            EmitMapPin(TEXT("MapPin"), EGPD_Input, TEXT("meta.mapForEachMapPinId"));
            EmitMapPin(TEXT("BreakPin"), EGPD_Input, TEXT("meta.mapForEachBreakPinId"));
            EmitMapPin(TEXT("then"), EGPD_Output, TEXT("meta.mapForEachLoopBodyPinId"));
            EmitMapPin(TEXT("KeyPin"), EGPD_Output, TEXT("meta.mapForEachKeyPinId"));
            EmitMapPin(TEXT("ValuePin"), EGPD_Output, TEXT("meta.mapForEachValuePinId"));
            EmitMapPin(TEXT("CompletedPin"), EGPD_Output, TEXT("meta.mapForEachCompletedPinId"));
        }

        if (UK2Node_GetInputActionValue* InputValue = Cast<UK2Node_GetInputActionValue>(K2))
        {
            AddMetaBool(TEXT("meta.isGetInputActionValue"), true);
            if (InputValue->InputAction)
            {
                AddMeta(TEXT("meta.inputActionPath"), InputValue->InputAction->GetPathName());
                AddMeta(TEXT("meta.inputActionName"), InputValue->InputAction->GetName());
            }
            if (UEdGraphPin* ValuePin = K2->FindPin(TEXT("ReturnValue"), EGPD_Output))
            {
                AddMeta(TEXT("meta.inputActionValuePinId"), ValuePin->PinId.ToString());
                AddMeta(TEXT("meta.inputActionValueType"), DescribePinTypeDetailed(ValuePin->PinType));
            }
        }

        if (K2->GetClass()->GetName() == TEXT("K2Node_LoadAssetClass"))
        {
            AddMetaBool(TEXT("meta.isAsyncLoadClass"), true);
            auto EmitLoadPin = [&](const TCHAR* PinName, EEdGraphPinDirection Direction, const TCHAR* MetaKey)
            {
                if (UEdGraphPin* Pin = K2->FindPin(FName(PinName), Direction))
                {
                    AddMeta(MetaKey, Pin->PinId.ToString());
                    AddMeta(FString(MetaKey) + TEXT("Type"), DescribePinTypeDetailed(Pin->PinType));
                    if (!Pin->DefaultValue.IsEmpty())
                        AddMeta(FString(MetaKey) + TEXT("Default"), Pin->DefaultValue);
                    if (Pin->DefaultObject)
                        AddMeta(FString(MetaKey) + TEXT("DefaultObject"), Pin->DefaultObject->GetPathName());
                }
            };
            EmitLoadPin(TEXT("execute"), EGPD_Input, TEXT("meta.asyncLoadExecutePinId"));
            EmitLoadPin(TEXT("AssetClass"), EGPD_Input, TEXT("meta.asyncLoadAssetClassPinId"));
            EmitLoadPin(TEXT("then"), EGPD_Output, TEXT("meta.asyncLoadThenPinId"));
            EmitLoadPin(TEXT("Completed"), EGPD_Output, TEXT("meta.asyncLoadCompletedPinId"));
            EmitLoadPin(TEXT("Class"), EGPD_Output, TEXT("meta.asyncLoadResultClassPinId"));
        }

        // --- Task 51: MultiGate — distributes execution across multiple output exec pins ---
        // bIsRandom, bLoop, StartIndex control sequencing; output pin count = gate count.
        // Properties may be public or accessible via the UEdGraphNode UPROPERTY system.
        // K2Node_MultiGate: distributes execution across multiple output exec pins.
        // GetIsRandomPin/GetLoopPin/GetStartIndexPin/GetNumOutPins are NOT BLUEPRINTGRAPH_API
        // so we cannot call them across DLL boundaries. Replicate their logic directly:
        //   GetIsRandomPin()  -> FindPin("IsRandom")
        //   GetLoopPin()      -> FindPin("Loop")
        //   GetStartIndexPin() -> FindPin("StartIndex")
        //   GetNumOutPins()   -> count EGPD_Output pins
        // FindPin() and Pins are on UEdGraphNode (always exported).
#if BPSLZR_HAS_K2NODE_MULTIGATE
        if (Cast<UK2Node_MultiGate>(K2))
#else
        if (K2->GetClass()->GetName() == TEXT("K2Node_MultiGate"))
#endif
        {
            AddMetaBool(TEXT("meta.isMultiGate"), true);
            if (UEdGraphPin* RandPin = K2->FindPin(TEXT("IsRandom")))
            {
                if (!RandPin->DefaultValue.IsEmpty())
                    AddMeta(TEXT("meta.multiGateIsRandom"), RandPin->DefaultValue);
                AddMeta(TEXT("meta.multiGateIsRandomPinId"), RandPin->PinId.ToString());
            }
            if (UEdGraphPin* LoopPin = K2->FindPin(TEXT("Loop")))
            {
                if (!LoopPin->DefaultValue.IsEmpty())
                    AddMeta(TEXT("meta.multiGateLoop"), LoopPin->DefaultValue);
                AddMeta(TEXT("meta.multiGateLoopPinId"), LoopPin->PinId.ToString());
            }
            if (UEdGraphPin* StartPin = K2->FindPin(TEXT("StartIndex")))
            {
                if (!StartPin->DefaultValue.IsEmpty())
                    AddMeta(TEXT("meta.multiGateStartIndex"), StartPin->DefaultValue);
                AddMeta(TEXT("meta.multiGateStartIndexPinId"), StartPin->PinId.ToString());
            }
            int32 GateCount = 0;
            for (UEdGraphPin* Pin : K2->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Output)
                    GateCount++;
            }
            if (GateCount > 0)
                AddMeta(TEXT("meta.multiGateCount"), FString::FromInt(GateCount));
        }

        // --- Task 51: Annotate remaining newly-recognized K2Node types with basic identity flags ---
        // K2Node_AsyncAction_ListenForGameplayMessages: handled by UK2Node_BaseAsyncTask branch above.
        // K2Node_GetEnumeratorNameAsString: handled by UK2Node_GetEnumeratorName branch above.
        // K2Node_ClearDelegate: handled by UK2Node_BaseMCDelegate branch above.
        // K2Node_PlayMontage: uses UK2Node_BaseAsyncTask proxy pattern — BaseAsyncTask branch handles it.
        // K2Node_LoadAssetClass: uses UK2Node_BaseAsyncTask proxy pattern — BaseAsyncTask branch handles it.
        // Remaining types: set a recognition flag so meta is non-empty even if no deeper handler runs.
        {
            const FString NodeClassName = K2->GetClass()->GetName();
            if (NodeClassName == TEXT("K2Node_ConvertAsset"))
                AddMetaBool(TEXT("meta.isConvertAsset"), true);
            else if (NodeClassName == TEXT("K2Node_AnimNodeReference"))
                AddMetaBool(TEXT("meta.isAnimNodeReference"), true);
            else if (NodeClassName == TEXT("K2Node_InstancedStruct"))
                AddMetaBool(TEXT("meta.isInstancedStruct"), true);
            else if (NodeClassName == TEXT("K2Node_CastByteToEnum"))
            {
                AddMetaBool(TEXT("meta.isCastByteToEnum"), true);
                // Capture target enum class from any pin subtype reference
                for (UEdGraphPin* Pin : K2->Pins)
                {
                    if (Pin && Pin->PinType.PinSubCategoryObject.IsValid())
                    {
                        AddMeta(TEXT("meta.castByteToEnumClass"),
                            Pin->PinType.PinSubCategoryObject->GetPathName());
                        break;
                    }
                }
            }
            else if (NodeClassName == TEXT("K2Node_GetInputAxisKeyValue"))
                AddMetaBool(TEXT("meta.isGetInputAxisKeyValue"), true);
        }
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (!Pin) continue;
        FBS_PinData P;
        P.PinId = Pin->PinId;
        P.Name = Pin->PinName;
        P.Direction = (Pin->Direction == EGPD_Input) ? TEXT("Input") : TEXT("Output");
        P.Category = Pin->PinType.PinCategory.ToString();
        P.SubCategory = Pin->PinType.PinSubCategory.ToString();
        if (Pin->PinType.PinSubCategoryObject.IsValid())
        {
            P.ObjectType = Pin->PinType.PinSubCategoryObject->GetName();
            P.ObjectPath = Pin->PinType.PinSubCategoryObject->GetPathName();
        }
        const FSimpleMemberReference& SubMemberRef = Pin->PinType.PinSubCategoryMemberReference;
        if (SubMemberRef.MemberParent)
        {
            P.SubCategoryMemberParent = SubMemberRef.MemberParent->GetPathName();
        }
        if (SubMemberRef.MemberName != NAME_None)
        {
            P.SubCategoryMemberName = SubMemberRef.MemberName.ToString();
        }
        if (SubMemberRef.MemberGuid.IsValid())
        {
            P.SubCategoryMemberGuid = SubMemberRef.MemberGuid.ToString();
        }
        P.bIsReference = Pin->PinType.bIsReference;
#if ENGINE_MAJOR_VERSION >= 5
        P.bIsConst = Pin->PinType.bIsConst;
#endif
        P.bIsWeakPointer = Pin->PinType.bIsWeakPointer;
        P.bIsUObjectWrapper = Pin->PinType.bIsUObjectWrapper;
        switch (Pin->PinType.ContainerType)
        {
        case EPinContainerType::Array: P.bIsArray = true; break;
        case EPinContainerType::Map:   P.bIsMap = true;   break;
        case EPinContainerType::Set:   P.bIsSet = true;   break;
        default: break;
        }
        P.bIsExec = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
        // is_out flag: only for non-exec output pins (output parameters)
        P.bIsOut = (Pin->Direction == EGPD_Output) && 
                   (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec);
        P.bConnected = (Pin->LinkedTo.Num() > 0);
        P.SourceIndex = Pin->SourceIndex;
        if (Pin->ParentPin && Pin->ParentPin->PinId.IsValid())
        {
            P.ParentPinId = Pin->ParentPin->PinId;
        }
#if WITH_EDITORONLY_DATA
        for (UEdGraphPin* SubPin : Pin->SubPins)
        {
            if (SubPin && SubPin->PinId.IsValid())
            {
                P.SubPinIds.Add(SubPin->PinId);
            }
        }
#endif

#if WITH_EDITORONLY_DATA
        P.bHidden = Pin->bHidden;
        P.bNotConnectable = Pin->bNotConnectable;
        P.bDefaultValueIsReadOnly = Pin->bDefaultValueIsReadOnly;
        P.bDefaultValueIsIgnored = Pin->bDefaultValueIsIgnored;
        P.bAdvancedView = Pin->bAdvancedView;
        P.bDisplayAsMutableRef = Pin->bDisplayAsMutableRef;
        P.bAllowFriendlyName = Pin->bAllowFriendlyName;
        P.bOrphanedPin = Pin->bOrphanedPin;
        if (Pin->PersistentGuid.IsValid())
        {
            P.PersistentGuid = Pin->PersistentGuid.ToString();
        }
        if (!Pin->PinFriendlyName.IsEmpty())
        {
            P.FriendlyName = Pin->PinFriendlyName.ToString();
        }
#endif
        P.ToolTip = Pin->PinToolTip;
        
        // Default value
        if (!Pin->DefaultValue.IsEmpty())
        {
            P.DefaultValue = Pin->DefaultValue;
            P.bIsOptional = true; // Has default value
        }
        else if (!Pin->DefaultTextValue.IsEmpty())
        {
            P.DefaultValue = Pin->DefaultTextValue.ToString();
            P.bIsOptional = true; // Has default value
        }
        else if (Pin->DefaultObject)
        {
            P.DefaultValue = Pin->DefaultObject->GetName();
            P.DefaultObjectPath = Pin->DefaultObject->GetPathName();
            P.bIsOptional = true; // Has default value
        }

        if (!Pin->AutogeneratedDefaultValue.IsEmpty())
        {
            P.AutogeneratedDefaultValue = Pin->AutogeneratedDefaultValue;
        }
        
        // Check AdvancedDisplay
#if WITH_EDITORONLY_DATA
        if (Pin->bAdvancedView)
        {
            P.bIsOptional = true;
        }
#endif

        // Map value type (for containers)
        const FEdGraphTerminalType& ValueType = Pin->PinType.PinValueType;
        if (ValueType.TerminalCategory != NAME_None)
        {
            P.ValueCategory = ValueType.TerminalCategory.ToString();
        }
        if (ValueType.TerminalSubCategory != NAME_None)
        {
            P.ValueSubCategory = ValueType.TerminalSubCategory.ToString();
        }
        if (ValueType.TerminalSubCategoryObject.IsValid())
        {
            P.ValueObjectType = ValueType.TerminalSubCategoryObject->GetName();
            P.ValueObjectPath = ValueType.TerminalSubCategoryObject->GetPathName();
        }
        P.bValueIsConst = ValueType.bTerminalIsConst;
        P.bValueIsWeakPointer = ValueType.bTerminalIsWeakPointer;
        P.bValueIsUObjectWrapper = ValueType.bTerminalIsUObjectWrapper;
        
        Out.Pins.Add(MoveTemp(P));
    }

    return Out;
}

// JSON object helper (built from structured node)
TSharedPtr<FJsonObject> UBlueprintAnalyzer::AnalyzeNodeToJsonObject(UEdGraphNode* Node)
{
    FBS_NodeData S = AnalyzeNodeToStruct(Node);
    TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
    Obj->SetStringField(TEXT("nodeGuid"), S.NodeGuid.ToString());
    Obj->SetStringField(TEXT("nodeType"), S.NodeType);
    Obj->SetStringField(TEXT("nodeTitle"), S.Title);
    Obj->SetNumberField(TEXT("posX"), S.PosX);
    Obj->SetNumberField(TEXT("posY"), S.PosY);
    Obj->SetNumberField(TEXT("width"), S.NodeWidth);
    Obj->SetNumberField(TEXT("height"), S.NodeHeight);
    if (!S.NodeComment.IsEmpty()) Obj->SetStringField(TEXT("comment"), S.NodeComment);
    if (!S.EnabledState.IsEmpty()) Obj->SetStringField(TEXT("enabledState"), S.EnabledState);
    if (!S.AdvancedPinDisplay.IsEmpty()) Obj->SetStringField(TEXT("advancedPinDisplay"), S.AdvancedPinDisplay);
    if (S.bCanRenameNode) Obj->SetBoolField(TEXT("canRename"), true);
    if (S.bCanResizeNode) Obj->SetBoolField(TEXT("canResize"), true);
    if (S.bCommentBubblePinned) Obj->SetBoolField(TEXT("commentBubblePinned"), true);
    if (S.bCommentBubbleVisible) Obj->SetBoolField(TEXT("commentBubbleVisible"), true);
    if (S.bCommentBubbleMakeVisible) Obj->SetBoolField(TEXT("commentBubbleMakeVisible"), true);
    if (S.bHasCompilerMessage) Obj->SetBoolField(TEXT("hasCompilerMessage"), true);
    if (S.ErrorType != 0) Obj->SetNumberField(TEXT("errorType"), S.ErrorType);
    if (!S.ErrorMsg.IsEmpty()) Obj->SetStringField(TEXT("errorMsg"), S.ErrorMsg);
    if (!S.MemberParent.IsEmpty()) Obj->SetStringField(TEXT("memberParent"), S.MemberParent);
    if (!S.MemberName.IsEmpty()) Obj->SetStringField(TEXT("memberName"), S.MemberName);
    if (S.NodeProperties.Num() > 0)
    {
        AddSortedStringMap(Obj, TEXT("nodeProperties"), S.NodeProperties);
    }

    TArray<TSharedPtr<FJsonValue>> PinsJson;
    for (const FBS_PinData& P : S.Pins)
    {
        TSharedPtr<FJsonObject> PJ = MakeShareable(new FJsonObject);
        PJ->SetStringField(TEXT("pinName"), P.Name.ToString());
        if (P.PinId.IsValid()) PJ->SetStringField(TEXT("pinId"), P.PinId.ToString());
        if (!P.Direction.IsEmpty()) PJ->SetStringField(TEXT("direction"), P.Direction);
        PJ->SetStringField(TEXT("pinType"), P.Category);
        PJ->SetStringField(TEXT("pinSubType"), P.SubCategory);
        if (!P.ObjectType.IsEmpty()) PJ->SetStringField(TEXT("objectType"), P.ObjectType);
        if (!P.ObjectPath.IsEmpty()) PJ->SetStringField(TEXT("objectPath"), P.ObjectPath);
        if (!P.SubCategoryMemberParent.IsEmpty()) PJ->SetStringField(TEXT("subCategoryMemberParent"), P.SubCategoryMemberParent);
        if (!P.SubCategoryMemberName.IsEmpty()) PJ->SetStringField(TEXT("subCategoryMemberName"), P.SubCategoryMemberName);
        if (!P.SubCategoryMemberGuid.IsEmpty()) PJ->SetStringField(TEXT("subCategoryMemberGuid"), P.SubCategoryMemberGuid);
        PJ->SetBoolField(TEXT("is_reference"), P.bIsReference);
        PJ->SetBoolField(TEXT("is_const"), P.bIsConst);
        if (P.bIsWeakPointer) PJ->SetBoolField(TEXT("is_weak_pointer"), true);
        if (P.bIsUObjectWrapper) PJ->SetBoolField(TEXT("is_uobject_wrapper"), true);
        PJ->SetBoolField(TEXT("is_array"), P.bIsArray);
        PJ->SetBoolField(TEXT("is_map"), P.bIsMap);
        PJ->SetBoolField(TEXT("is_set"), P.bIsSet);
        if (P.bIsExec) PJ->SetBoolField(TEXT("is_exec"), true);
        PJ->SetBoolField(TEXT("is_out"), P.bIsOut);
        PJ->SetBoolField(TEXT("is_optional"), P.bIsOptional);
        PJ->SetBoolField(TEXT("bConnected"), P.bConnected);
        if (P.SourceIndex != INDEX_NONE) PJ->SetNumberField(TEXT("sourceIndex"), P.SourceIndex);
        if (P.ParentPinId.IsValid()) PJ->SetStringField(TEXT("parentPinId"), P.ParentPinId.ToString());
        if (P.SubPinIds.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> SubPinArray;
            for (const FGuid& SubId : P.SubPinIds)
            {
                SubPinArray.Add(MakeShareable(new FJsonValueString(SubId.ToString())));
            }
            PJ->SetArrayField(TEXT("subPinIds"), SubPinArray);
        }
        if (P.bHidden) PJ->SetBoolField(TEXT("hidden"), true);
        if (P.bNotConnectable) PJ->SetBoolField(TEXT("not_connectable"), true);
        if (P.bDefaultValueIsReadOnly) PJ->SetBoolField(TEXT("default_value_readonly"), true);
        if (P.bDefaultValueIsIgnored) PJ->SetBoolField(TEXT("default_value_ignored"), true);
        if (P.bAdvancedView) PJ->SetBoolField(TEXT("advanced"), true);
        if (P.bDisplayAsMutableRef) PJ->SetBoolField(TEXT("display_as_mutable_ref"), true);
        if (P.bAllowFriendlyName) PJ->SetBoolField(TEXT("allow_friendly_name"), true);
        if (P.bOrphanedPin) PJ->SetBoolField(TEXT("orphaned"), true);
        if (!P.FriendlyName.IsEmpty()) PJ->SetStringField(TEXT("friendlyName"), P.FriendlyName);
        if (!P.ToolTip.IsEmpty()) PJ->SetStringField(TEXT("toolTip"), P.ToolTip);
        if (!P.DefaultValue.IsEmpty()) PJ->SetStringField(TEXT("defaultValue"), P.DefaultValue);
        if (!P.AutogeneratedDefaultValue.IsEmpty()) PJ->SetStringField(TEXT("autogeneratedDefaultValue"), P.AutogeneratedDefaultValue);
        if (!P.DefaultObjectPath.IsEmpty()) PJ->SetStringField(TEXT("defaultObjectPath"), P.DefaultObjectPath);
        if (!P.PersistentGuid.IsEmpty()) PJ->SetStringField(TEXT("persistentGuid"), P.PersistentGuid);
        if (!P.ValueCategory.IsEmpty()) PJ->SetStringField(TEXT("valueCategory"), P.ValueCategory);
        if (!P.ValueSubCategory.IsEmpty()) PJ->SetStringField(TEXT("valueSubCategory"), P.ValueSubCategory);
        if (!P.ValueObjectType.IsEmpty()) PJ->SetStringField(TEXT("valueObjectType"), P.ValueObjectType);
        if (!P.ValueObjectPath.IsEmpty()) PJ->SetStringField(TEXT("valueObjectPath"), P.ValueObjectPath);
        if (P.bValueIsConst) PJ->SetBoolField(TEXT("valueIsConst"), true);
        if (P.bValueIsWeakPointer) PJ->SetBoolField(TEXT("valueIsWeakPointer"), true);
        if (P.bValueIsUObjectWrapper) PJ->SetBoolField(TEXT("valueIsUObjectWrapper"), true);
        PinsJson.Add(MakeShareable(new FJsonValueObject(PJ)));
    }
    Obj->SetArrayField(TEXT("pins"), PinsJson);

    return Obj;
}

TArray<FString> UBlueprintAnalyzer::ExtractEventNodes(UBlueprint* Blueprint)
{
	TArray<FString> EventNodes;
	
	if (!Blueprint)
	{
		return EventNodes;
	}
	
	// Find event nodes specifically (UE5.5 compatible approach)
	TArray<UEdGraph*> AllGraphs;
	
	// Add ubergraph pages (main event graph)
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph)
		{
			AllGraphs.Add(Graph);
		}
	}
	
	// Add function graphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph)
		{
			AllGraphs.Add(Graph);
		}
	}
	
    for (UEdGraph* Graph : AllGraphs)
    {
        if (!Graph) continue;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node) continue;
            if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
            {
                const FName EvName = EventNode->EventReference.GetMemberName();
                const FString Info = FString::Printf(TEXT("Event: %s"), *EvName.ToString());
                EventNodes.Add(Info);
            }
            else if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node))
            {
                const FString Info = FString::Printf(TEXT("CustomEvent: %s"), *CustomEventNode->CustomFunctionName.ToString());
                EventNodes.Add(Info);
            }
        }
    }
	
	return EventNodes;
}

TArray<FString> UBlueprintAnalyzer::ExtractImplementedInterfaces(UBlueprint* Blueprint)
{
	TArray<FString> Interfaces;
	
	if (!Blueprint)
	{
		return Interfaces;
	}
	
	// Extract implemented interfaces
	for (const FBPInterfaceDescription& InterfaceDesc : Blueprint->ImplementedInterfaces)
	{
		if (InterfaceDesc.Interface)
		{
			Interfaces.Add(InterfaceDesc.Interface->GetName());
		}
	}

	Interfaces.Sort();
	
	return Interfaces;
}

FString UBlueprintAnalyzer::ExportBlueprintDataToJSON(const FBS_BlueprintData& Data)
{
	TSharedPtr<FJsonObject> JsonObject = BlueprintDataToJsonObject(Data);
	
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	
	return OutputString;
}

FString UBlueprintAnalyzer::ExportMultipleBlueprintsToJSON(const TArray<FBS_BlueprintData>& DataArray)
{
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> BlueprintArray;

	TArray<FBS_BlueprintData> SortedData = DataArray;
	SortedData.Sort([](const FBS_BlueprintData& A, const FBS_BlueprintData& B)
	{
		return A.BlueprintPath < B.BlueprintPath;
	});

	for (const FBS_BlueprintData& Data : SortedData)
	{
		TSharedPtr<FJsonObject> BlueprintObj = BlueprintDataToJsonObject(Data);
		BlueprintArray.Add(MakeShareable(new FJsonValueObject(BlueprintObj)));
	}
	
	RootObject->SetArrayField(TEXT("blueprints"), BlueprintArray);
	RootObject->SetNumberField(TEXT("totalCount"), SortedData.Num());
	RootObject->SetStringField(TEXT("exportTimestamp"), FDateTime::Now().ToString());
	
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	
	return OutputString;
}

TSharedPtr<FJsonObject> UBlueprintAnalyzer::BlueprintDataToJsonObject(const FBS_BlueprintData& Data)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	
	// Version info at the top
	const FString SchemaVersion = Data.SchemaVersion.IsEmpty() ? TEXT("1.7") : Data.SchemaVersion;
	JsonObject->SetStringField(TEXT("schemaVersion"), SchemaVersion);
	JsonObject->SetStringField(TEXT("version"), SchemaVersion);
	JsonObject->SetStringField(TEXT("engine_version"), FString::Printf(TEXT("%d.%d.%d"), 
		ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION));
	JsonObject->SetStringField(TEXT("plugin_version"), TEXT("1.0.0"));
	
	// Metadata object (inspired by NodeToCode)
	TSharedPtr<FJsonObject> Metadata = MakeShareable(new FJsonObject);
	Metadata->SetStringField(TEXT("name"), Data.BlueprintName);
	Metadata->SetStringField(TEXT("blueprint_type"), Data.bIsInterface ? TEXT("Interface") : 
		(Data.bIsMacroLibrary ? TEXT("MacroLibrary") : 
		(Data.bIsFunctionLibrary ? TEXT("FunctionLibrary") : TEXT("Blueprint"))));
	Metadata->SetStringField(TEXT("blueprint_class"), Data.ParentClassPath.IsEmpty() ? Data.ParentClassName : Data.ParentClassPath);
	Metadata->SetStringField(TEXT("blueprint_namespace"), Data.BlueprintNamespace);
	JsonObject->SetObjectField(TEXT("metadata"), Metadata);
	
	// Blueprint basic info
	JsonObject->SetStringField(TEXT("blueprintPath"), Data.BlueprintPath);
	JsonObject->SetStringField(TEXT("blueprintName"), Data.BlueprintName);
	JsonObject->SetStringField(TEXT("parentClassName"), Data.ParentClassName);
	JsonObject->SetStringField(TEXT("parentClassPath"), Data.ParentClassPath);
	JsonObject->SetStringField(TEXT("generatedClassName"), Data.GeneratedClassName);
	JsonObject->SetStringField(TEXT("generatedClassPath"), Data.GeneratedClassPath);
	JsonObject->SetStringField(TEXT("blueprintNamespace"), Data.BlueprintNamespace);
	JsonObject->SetNumberField(TEXT("totalNodeCount"), Data.TotalNodeCount);
	JsonObject->SetStringField(TEXT("extractionTimestamp"), Data.ExtractionTimestamp);
	JsonObject->SetStringField(TEXT("classConfigName"), Data.ClassConfigName);
	JsonObject->SetArrayField(TEXT("classSpecifiers"), BuildStringArray(Data.ClassSpecifiers));
	AddSortedBoolMap(JsonObject, TEXT("classConfigFlags"), Data.ClassConfigFlags);
	AddSortedStringMap(JsonObject, TEXT("classDefaultValues"), Data.ClassDefaultValues);
	AddSortedStringMap(JsonObject, TEXT("classDefaultValueDelta"), Data.ClassDefaultValueDelta);
	JsonObject->SetObjectField(TEXT("dependencyClosure"), BuildDependencyClosureJson(Data));

	// Canonical blueprint info for schema-based consumers
	{
		TSharedPtr<FJsonObject> BlueprintInfo = MakeShareable(new FJsonObject);
		BlueprintInfo->SetStringField(TEXT("blueprintName"), Data.BlueprintName);
		BlueprintInfo->SetStringField(TEXT("blueprintPath"), Data.BlueprintPath);
		BlueprintInfo->SetStringField(TEXT("parentClassPath"), Data.ParentClassPath);
		BlueprintInfo->SetStringField(TEXT("generatedClassPath"), Data.GeneratedClassPath);
		BlueprintInfo->SetStringField(TEXT("blueprintNamespace"), Data.BlueprintNamespace);
		BlueprintInfo->SetBoolField(TEXT("isAnimBlueprint"), Data.bIsAnimBlueprint);
		BlueprintInfo->SetStringField(TEXT("targetSkeleton"), Data.TargetSkeletonPath);
		JsonObject->SetObjectField(TEXT("blueprintInfo"), BlueprintInfo);
	}
	
	// Convert arrays to JSON arrays
	TArray<TSharedPtr<FJsonValue>> InterfaceArray;
	for (const FString& Interface : Data.ImplementedInterfaces)
	{
		InterfaceArray.Add(MakeShareable(new FJsonValueString(Interface)));
	}
	JsonObject->SetArrayField(TEXT("implementedInterfaces"), InterfaceArray);

	TArray<TSharedPtr<FJsonValue>> InterfacePathArray;
	for (const FString& InterfacePath : Data.ImplementedInterfacePaths)
	{
		InterfacePathArray.Add(MakeShareable(new FJsonValueString(InterfacePath)));
	}
	JsonObject->SetArrayField(TEXT("implementedInterfacePaths"), InterfacePathArray);

	TArray<TSharedPtr<FJsonValue>> ImportedNamespaceArray;
	for (const FString& ImportedNamespace : Data.ImportedNamespaces)
	{
		ImportedNamespaceArray.Add(MakeShareable(new FJsonValueString(ImportedNamespace)));
	}
	JsonObject->SetArrayField(TEXT("importedNamespaces"), ImportedNamespaceArray);

	TArray<TSharedPtr<FJsonValue>> UserStructSchemaArray;
	for (const FBS_UserDefinedStructSchema& StructSchema : Data.UserDefinedStructSchemas)
	{
		TSharedPtr<FJsonObject> StructObj = MakeShareable(new FJsonObject);
		StructObj->SetStringField(TEXT("name"), StructSchema.StructName);
		StructObj->SetStringField(TEXT("path"), StructSchema.StructPath);

		TArray<TSharedPtr<FJsonValue>> FieldArray;
		for (const FBS_UserDefinedStructFieldSchema& FieldSchema : StructSchema.Fields)
		{
			TSharedPtr<FJsonObject> FieldObj = MakeShareable(new FJsonObject);
			FieldObj->SetStringField(TEXT("name"), FieldSchema.Name);
			FieldObj->SetStringField(TEXT("type"), FieldSchema.Type);
			FieldObj->SetStringField(TEXT("typeObjectPath"), FieldSchema.TypeObjectPath);
			FieldObj->SetBoolField(TEXT("isArray"), FieldSchema.bIsArray);
			FieldObj->SetBoolField(TEXT("isMap"), FieldSchema.bIsMap);
			FieldObj->SetBoolField(TEXT("isSet"), FieldSchema.bIsSet);
			// CR-028: default value and metadata per struct field
			if (!FieldSchema.DefaultValue.IsEmpty())
			{
				FieldObj->SetStringField(TEXT("defaultValue"), FieldSchema.DefaultValue);
			}
			if (!FieldSchema.DisplayName.IsEmpty())
			{
				FieldObj->SetStringField(TEXT("displayName"), FieldSchema.DisplayName);
			}
			if (!FieldSchema.Tooltip.IsEmpty())
			{
				FieldObj->SetStringField(TEXT("tooltip"), FieldSchema.Tooltip);
			}
			FieldArray.Add(MakeShareable(new FJsonValueObject(FieldObj)));
		}

		StructObj->SetArrayField(TEXT("fields"), FieldArray);
		UserStructSchemaArray.Add(MakeShareable(new FJsonValueObject(StructObj)));
	}
	JsonObject->SetArrayField(TEXT("userDefinedStructSchemas"), UserStructSchemaArray);

	TArray<TSharedPtr<FJsonValue>> UserEnumSchemaArray;
	for (const FBS_UserDefinedEnumSchema& EnumSchema : Data.UserDefinedEnumSchemas)
	{
		TSharedPtr<FJsonObject> EnumObj = MakeShareable(new FJsonObject);
		EnumObj->SetStringField(TEXT("name"), EnumSchema.EnumName);
		EnumObj->SetStringField(TEXT("path"), EnumSchema.EnumPath);
		EnumObj->SetArrayField(TEXT("enumerators"), BuildStringArray(EnumSchema.Enumerators));
		// CR-028: per-enumerator metadata (display names, tooltips)
		if (EnumSchema.EnumeratorMetadata.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> MetaArray;
			for (const FBS_UserDefinedEnumValueMeta& ValueMeta : EnumSchema.EnumeratorMetadata)
			{
				TSharedPtr<FJsonObject> MetaObj = MakeShareable(new FJsonObject);
				MetaObj->SetStringField(TEXT("name"), ValueMeta.Name);
				if (!ValueMeta.DisplayName.IsEmpty())
				{
					MetaObj->SetStringField(TEXT("displayName"), ValueMeta.DisplayName);
				}
				if (!ValueMeta.Tooltip.IsEmpty())
				{
					MetaObj->SetStringField(TEXT("tooltip"), ValueMeta.Tooltip);
				}
				MetaArray.Add(MakeShareable(new FJsonValueObject(MetaObj)));
			}
			EnumObj->SetArrayField(TEXT("enumeratorMetadata"), MetaArray);
		}
		UserEnumSchemaArray.Add(MakeShareable(new FJsonValueObject(EnumObj)));
	}
	JsonObject->SetArrayField(TEXT("userDefinedEnumSchemas"), UserEnumSchemaArray);
	
	TArray<TSharedPtr<FJsonValue>> VariableArray;
	for (const FString& Variable : Data.Variables)
	{
		VariableArray.Add(MakeShareable(new FJsonValueString(Variable)));
	}
	JsonObject->SetArrayField(TEXT("variables"), VariableArray);
	
	TArray<TSharedPtr<FJsonValue>> FunctionArray;
	for (const FString& Function : Data.Functions)
	{
		FunctionArray.Add(MakeShareable(new FJsonValueString(Function)));
	}
	JsonObject->SetArrayField(TEXT("functions"), FunctionArray);
	
	TArray<TSharedPtr<FJsonValue>> ComponentArray;
	for (const FString& Component : Data.Components)
	{
		ComponentArray.Add(MakeShareable(new FJsonValueString(Component)));
	}
	JsonObject->SetArrayField(TEXT("components"), ComponentArray);
	
	TArray<TSharedPtr<FJsonValue>> EventArray;
	for (const FString& Event : Data.EventNodes)
	{
		EventArray.Add(MakeShareable(new FJsonValueString(Event)));
	}
	JsonObject->SetArrayField(TEXT("eventNodes"), EventArray);
	
	TArray<TSharedPtr<FJsonValue>> NodeArray;
	for (const FString& Node : Data.GraphNodes)
	{
		NodeArray.Add(MakeShareable(new FJsonValueString(Node)));
	}
	JsonObject->SetArrayField(TEXT("graphNodes"), NodeArray);
	
    // Enhanced detailed information
	JsonObject->SetStringField(TEXT("blueprintDescription"), Data.BlueprintDescription);
	JsonObject->SetStringField(TEXT("blueprintCategory"), Data.BlueprintCategory);
	JsonObject->SetBoolField(TEXT("isInterface"), Data.bIsInterface);
	JsonObject->SetBoolField(TEXT("isMacroLibrary"), Data.bIsMacroLibrary);
	JsonObject->SetBoolField(TEXT("isFunctionLibrary"), Data.bIsFunctionLibrary);
	
	// Blueprint tags
	TArray<TSharedPtr<FJsonValue>> TagArray;
	for (const FString& Tag : Data.BlueprintTags)
	{
		TagArray.Add(MakeShareable(new FJsonValueString(Tag)));
	}
	JsonObject->SetArrayField(TEXT("blueprintTags"), TagArray);
	
	// Asset references
	TArray<TSharedPtr<FJsonValue>> AssetRefArray;
	for (const FString& AssetRef : Data.AssetReferences)
	{
		AssetRefArray.Add(MakeShareable(new FJsonValueString(AssetRef)));
	}
	JsonObject->SetArrayField(TEXT("assetReferences"), AssetRefArray);
	JsonObject->SetArrayField(TEXT("unsupportedNodeTypes"), BuildStringArray(Data.UnsupportedNodeTypes));
	JsonObject->SetArrayField(TEXT("partiallySupportedNodeTypes"), BuildStringArray(Data.PartiallySupportedNodeTypes));

	TArray<TSharedPtr<FJsonValue>> BytecodeBackedFunctionArray;
	int32 BytecodeBackedFunctionCount = 0;
	for (const FBS_FunctionInfo& FuncInfo : Data.DetailedFunctions)
	{
		if (FuncInfo.BytecodeSize <= 0 && FuncInfo.RawInMemoryBytecodeMd5.IsEmpty())
		{
			continue;
		}

		TSharedPtr<FJsonObject> BytecodeObj = MakeShareable(new FJsonObject);
		BytecodeObj->SetStringField(TEXT("name"), FuncInfo.FunctionName);
		BytecodeObj->SetStringField(TEXT("functionPath"), FuncInfo.FunctionPath);
		BytecodeObj->SetNumberField(TEXT("bytecodeSize"), FuncInfo.BytecodeSize);
		if (!FuncInfo.RawInMemoryBytecodeMd5.IsEmpty())
		{
			BytecodeObj->SetObjectField(
				TEXT("rawInMemoryBytecodeDiagnostic"),
				BuildRawInMemoryBytecodeDiagnostic(FuncInfo.RawInMemoryBytecodeMd5));
		}
		BytecodeObj->SetBoolField(TEXT("isOverride"), FuncInfo.bIsOverride);
		BytecodeObj->SetBoolField(TEXT("callsParent"), FuncInfo.bCallsParent);
		// CR-035: emit node traces for unsupported/partially-supported nodes in this bytecode function
		if (FuncInfo.BytecodeNodeGuids.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> NodeTraceArray;
			for (int32 TraceIdx = 0; TraceIdx < FuncInfo.BytecodeNodeGuids.Num(); TraceIdx++)
			{
				TSharedPtr<FJsonObject> TraceObj = MakeShareable(new FJsonObject);
				TraceObj->SetStringField(TEXT("nodeGuid"), FuncInfo.BytecodeNodeGuids[TraceIdx]);
				TraceObj->SetStringField(TEXT("nodeType"), FuncInfo.BytecodeNodeTypes[TraceIdx]);
				NodeTraceArray.Add(MakeShareable(new FJsonValueObject(TraceObj)));
			}
			BytecodeObj->SetArrayField(TEXT("nodeTraces"), NodeTraceArray);
		}
		BytecodeBackedFunctionArray.Add(MakeShareable(new FJsonValueObject(BytecodeObj)));
		BytecodeBackedFunctionCount++;
	}

	TSharedPtr<FJsonObject> CompilerIRFallbackObj = MakeShareable(new FJsonObject);
	CompilerIRFallbackObj->SetStringField(TEXT("strategy"), TEXT("function-bytecode"));
	CompilerIRFallbackObj->SetBoolField(TEXT("hasUnsupportedNodes"), Data.UnsupportedNodeTypes.Num() > 0);
	CompilerIRFallbackObj->SetNumberField(TEXT("unsupportedNodeTypeCount"), Data.UnsupportedNodeTypes.Num());
	CompilerIRFallbackObj->SetArrayField(TEXT("unsupportedNodeTypes"), BuildStringArray(Data.UnsupportedNodeTypes));
	CompilerIRFallbackObj->SetNumberField(TEXT("partiallySupportedNodeTypeCount"), Data.PartiallySupportedNodeTypes.Num());
	CompilerIRFallbackObj->SetArrayField(TEXT("partiallySupportedNodeTypes"), BuildStringArray(Data.PartiallySupportedNodeTypes));
	CompilerIRFallbackObj->SetBoolField(TEXT("hasBytecodeFallback"), BytecodeBackedFunctionCount > 0);
	CompilerIRFallbackObj->SetNumberField(TEXT("bytecodeBackedFunctionCount"), BytecodeBackedFunctionCount);
	CompilerIRFallbackObj->SetArrayField(TEXT("bytecodeBackedFunctions"), BytecodeBackedFunctionArray);
	JsonObject->SetObjectField(TEXT("compilerIRFallback"), CompilerIRFallbackObj);
	
	// Detailed variables
	TArray<TSharedPtr<FJsonValue>> DetailedVarArray;
	for (const FBS_VariableInfo& VarInfo : Data.DetailedVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShareable(new FJsonObject);
		VarObj->SetStringField(TEXT("name"), VarInfo.VariableName);
		VarObj->SetStringField(TEXT("type"), VarInfo.VariableType);
		VarObj->SetStringField(TEXT("defaultValue"), VarInfo.DefaultValue);
		VarObj->SetStringField(TEXT("category"), VarInfo.Category);
		VarObj->SetStringField(TEXT("tooltip"), VarInfo.Tooltip);
		VarObj->SetBoolField(TEXT("isPublic"), VarInfo.bIsPublic);
		VarObj->SetBoolField(TEXT("isReplicated"), VarInfo.bIsReplicated);
		VarObj->SetBoolField(TEXT("isExposedOnSpawn"), VarInfo.bIsExposedOnSpawn);
		VarObj->SetBoolField(TEXT("isEditable"), VarInfo.bIsEditable);
		VarObj->SetStringField(TEXT("replicationCondition"), VarInfo.RepCondition);
		VarObj->SetStringField(TEXT("repNotifyFunction"), VarInfo.RepNotifyFunction);
		VarObj->SetStringField(TEXT("descriptorPropertyFlags"), VarInfo.DescriptorPropertyFlags);
		VarObj->SetBoolField(TEXT("descriptorHasNetFlag"), VarInfo.bDescriptorHasNetFlag);
		VarObj->SetBoolField(TEXT("descriptorHasRepNotifyFlag"), VarInfo.bDescriptorHasRepNotifyFlag);
		VarObj->SetNumberField(TEXT("descriptorReplicationConditionValue"), VarInfo.DescriptorReplicationConditionValue);
		VarObj->SetStringField(TEXT("replicationConditionSource"), VarInfo.ReplicationConditionSource);
		VarObj->SetBoolField(TEXT("compiledPropertyFound"), VarInfo.bCompiledPropertyFound);
		if (VarInfo.bCompiledPropertyFound)
		{
			VarObj->SetStringField(TEXT("compiledPropertyPath"), VarInfo.CompiledPropertyPath);
			VarObj->SetStringField(TEXT("compiledPropertyFlags"), VarInfo.CompiledPropertyFlags);
			VarObj->SetBoolField(TEXT("compiledHasNetFlag"), VarInfo.bCompiledHasNetFlag);
			VarObj->SetBoolField(TEXT("compiledHasRepNotifyFlag"), VarInfo.bCompiledHasRepNotifyFlag);
			VarObj->SetStringField(TEXT("compiledRepNotifyFunction"), VarInfo.CompiledRepNotifyFunction);
			VarObj->SetNumberField(TEXT("compiledRepIndex"), VarInfo.CompiledRepIndex);
		}
		VarObj->SetStringField(TEXT("replicationDecisionSource"), VarInfo.ReplicationDecisionSource);
		VarObj->SetBoolField(TEXT("replicationMetadataConsistent"), VarInfo.bReplicationMetadataConsistent);
		VarObj->SetStringField(TEXT("typeCategory"), VarInfo.TypeCategory);
		VarObj->SetStringField(TEXT("typeSubCategory"), VarInfo.TypeSubCategory);
		VarObj->SetStringField(TEXT("typeObjectPath"), VarInfo.TypeObjectPath);
		VarObj->SetBoolField(TEXT("isArray"), VarInfo.bIsArray);
		VarObj->SetBoolField(TEXT("isMap"), VarInfo.bIsMap);
		VarObj->SetBoolField(TEXT("isSet"), VarInfo.bIsSet);
		VarObj->SetArrayField(TEXT("declarationSpecifiers"), BuildStringArray(VarInfo.DeclarationSpecifiers));
		DetailedVarArray.Add(MakeShareable(new FJsonValueObject(VarObj)));
	}
	JsonObject->SetArrayField(TEXT("detailedVariables"), DetailedVarArray);
	
	// Detailed functions
	TArray<TSharedPtr<FJsonValue>> DetailedFuncArray;
	for (const FBS_FunctionInfo& FuncInfo : Data.DetailedFunctions)
	{
		TSharedPtr<FJsonObject> FuncObj = MakeShareable(new FJsonObject);
		FuncObj->SetStringField(TEXT("name"), FuncInfo.FunctionName);
		FuncObj->SetStringField(TEXT("functionPath"), FuncInfo.FunctionPath);
		FuncObj->SetStringField(TEXT("category"), FuncInfo.Category);
		FuncObj->SetStringField(TEXT("tooltip"), FuncInfo.Tooltip);
		FuncObj->SetBoolField(TEXT("isPure"), FuncInfo.bIsPure);
		FuncObj->SetBoolField(TEXT("isPublic"), FuncInfo.bIsPublic);
		FuncObj->SetBoolField(TEXT("isProtected"), FuncInfo.bIsProtected);
		FuncObj->SetBoolField(TEXT("isPrivate"), FuncInfo.bIsPrivate);
		FuncObj->SetBoolField(TEXT("isStatic"), FuncInfo.bIsStatic);
		FuncObj->SetBoolField(TEXT("isLatent"), FuncInfo.bIsLatent);
		FuncObj->SetBoolField(TEXT("isOverride"), FuncInfo.bIsOverride);
		FuncObj->SetBoolField(TEXT("callsParent"), FuncInfo.bCallsParent);
		FuncObj->SetBoolField(TEXT("isNet"), FuncInfo.bIsNet);
		FuncObj->SetBoolField(TEXT("isNetServer"), FuncInfo.bIsNetServer);
		FuncObj->SetBoolField(TEXT("isNetClient"), FuncInfo.bIsNetClient);
		FuncObj->SetBoolField(TEXT("isNetMulticast"), FuncInfo.bIsNetMulticast);
		FuncObj->SetBoolField(TEXT("isReliable"), FuncInfo.bIsReliable);
		FuncObj->SetBoolField(TEXT("blueprintAuthorityOnly"), FuncInfo.bBlueprintAuthorityOnly);
		FuncObj->SetBoolField(TEXT("blueprintCosmetic"), FuncInfo.bBlueprintCosmetic);
		FuncObj->SetBoolField(TEXT("callInEditor"), FuncInfo.bCallInEditor);
		FuncObj->SetStringField(TEXT("accessSpecifier"), FuncInfo.AccessSpecifier);
		FuncObj->SetStringField(TEXT("returnType"), FuncInfo.ReturnType);
		FuncObj->SetStringField(TEXT("returnTypeObjectPath"), FuncInfo.ReturnTypeObjectPath);
		FuncObj->SetNumberField(TEXT("bytecodeSize"), FuncInfo.BytecodeSize);
		if (!FuncInfo.RawInMemoryBytecodeMd5.IsEmpty())
		{
			FuncObj->SetObjectField(
				TEXT("rawInMemoryBytecodeDiagnostic"),
				BuildRawInMemoryBytecodeDiagnostic(FuncInfo.RawInMemoryBytecodeMd5));
		}
		FuncObj->SetArrayField(TEXT("declarationSpecifiers"), BuildStringArray(FuncInfo.DeclarationSpecifiers));
		
		TArray<TSharedPtr<FJsonValue>> InputParamArray;
		for (const FString& Param : FuncInfo.InputParameters)
		{
			InputParamArray.Add(MakeShareable(new FJsonValueString(Param)));
		}
		FuncObj->SetArrayField(TEXT("inputParameters"), InputParamArray);
		
		TArray<TSharedPtr<FJsonValue>> OutputParamArray;
		for (const FString& Param : FuncInfo.OutputParameters)
		{
			OutputParamArray.Add(MakeShareable(new FJsonValueString(Param)));
		}
		FuncObj->SetArrayField(TEXT("outputParameters"), OutputParamArray);

		TArray<TSharedPtr<FJsonValue>> LocalVarArray;
		for (const FString& LocalVar : FuncInfo.LocalVariables)
		{
			LocalVarArray.Add(MakeShareable(new FJsonValueString(LocalVar)));
		}
		FuncObj->SetArrayField(TEXT("localVariables"), LocalVarArray);

		// Structured typed local variables for C++ code generation
		TArray<TSharedPtr<FJsonValue>> DetailedLocalVarArray;
		for (const FBS_LocalVarInfo& LVI : FuncInfo.DetailedLocalVariables)
		{
			TSharedPtr<FJsonObject> LVObj = MakeShareable(new FJsonObject);
			LVObj->SetStringField(TEXT("varName"),          LVI.VarName);
			LVObj->SetStringField(TEXT("typeCategory"),     LVI.TypeCategory);
			if (!LVI.TypeSubCategory.IsEmpty())
				LVObj->SetStringField(TEXT("typeSubCategory"), LVI.TypeSubCategory);
			if (!LVI.TypeObjectPath.IsEmpty())
				LVObj->SetStringField(TEXT("typeObjectPath"),  LVI.TypeObjectPath);
			if (!LVI.TypeObjectName.IsEmpty())
				LVObj->SetStringField(TEXT("typeObjectName"),  LVI.TypeObjectName);
			LVObj->SetStringField(TEXT("containerType"),    LVI.ContainerType);
			if (!LVI.MapValueCategory.IsEmpty())
				LVObj->SetStringField(TEXT("mapValueCategory"),   LVI.MapValueCategory);
			if (!LVI.MapValueObjectPath.IsEmpty())
				LVObj->SetStringField(TEXT("mapValueObjectPath"), LVI.MapValueObjectPath);
			LVObj->SetBoolField(TEXT("isReference"), LVI.bIsReference);
			LVObj->SetBoolField(TEXT("isConst"),     LVI.bIsConst);
			if (!LVI.DefaultValue.IsEmpty())
				LVObj->SetStringField(TEXT("defaultValue"), LVI.DefaultValue);
			if (!LVI.Description.IsEmpty())
				LVObj->SetStringField(TEXT("description"), LVI.Description);
			DetailedLocalVarArray.Add(MakeShareable(new FJsonValueObject(LVObj)));
		}
		if (DetailedLocalVarArray.Num() > 0)
			FuncObj->SetArrayField(TEXT("detailedLocalVariables"), DetailedLocalVarArray);

		// Structured typed input parameters for C++ code generation
		TArray<TSharedPtr<FJsonValue>> DetailedInputParamArray;
		for (const FBS_ParamInfo& PInfo : FuncInfo.DetailedInputParams)
		{
				TSharedPtr<FJsonObject> PIObj = MakeShareable(new FJsonObject);
				PIObj->SetStringField(TEXT("paramName"),      PInfo.ParamName);
				PIObj->SetStringField(TEXT("typeCategory"),   PInfo.TypeCategory);
				if (!PInfo.TypeSubCategory.IsEmpty())
					PIObj->SetStringField(TEXT("typeSubCategory"), PInfo.TypeSubCategory);
				if (!PInfo.TypeObjectPath.IsEmpty())
					PIObj->SetStringField(TEXT("typeObjectPath"),  PInfo.TypeObjectPath);
				if (!PInfo.TypeObjectName.IsEmpty())
					PIObj->SetStringField(TEXT("typeObjectName"),  PInfo.TypeObjectName);
				PIObj->SetStringField(TEXT("containerType"),  PInfo.ContainerType);
				if (!PInfo.MapValueCategory.IsEmpty())
					PIObj->SetStringField(TEXT("mapValueCategory"),   PInfo.MapValueCategory);
				if (!PInfo.MapValueObjectPath.IsEmpty())
					PIObj->SetStringField(TEXT("mapValueObjectPath"), PInfo.MapValueObjectPath);
				PIObj->SetBoolField(TEXT("isReference"), PInfo.bIsReference);
				PIObj->SetBoolField(TEXT("isConst"),     PInfo.bIsConst);
				PIObj->SetBoolField(TEXT("isOut"),       PInfo.bIsOut);
				if (!PInfo.DefaultValue.IsEmpty())
					PIObj->SetStringField(TEXT("defaultValue"), PInfo.DefaultValue);
				if (!PInfo.Description.IsEmpty())
					PIObj->SetStringField(TEXT("description"), PInfo.Description);
				DetailedInputParamArray.Add(MakeShareable(new FJsonValueObject(PIObj)));
		}
		if (DetailedInputParamArray.Num() > 0)
			FuncObj->SetArrayField(TEXT("detailedInputParams"), DetailedInputParamArray);

		// Structured typed output parameters for C++ code generation
		TArray<TSharedPtr<FJsonValue>> DetailedOutputParamArray;
		for (const FBS_ParamInfo& PInfo : FuncInfo.DetailedOutputParams)
		{
				TSharedPtr<FJsonObject> PIObj = MakeShareable(new FJsonObject);
				PIObj->SetStringField(TEXT("paramName"),      PInfo.ParamName);
				PIObj->SetStringField(TEXT("typeCategory"),   PInfo.TypeCategory);
				if (!PInfo.TypeSubCategory.IsEmpty())
					PIObj->SetStringField(TEXT("typeSubCategory"), PInfo.TypeSubCategory);
				if (!PInfo.TypeObjectPath.IsEmpty())
					PIObj->SetStringField(TEXT("typeObjectPath"),  PInfo.TypeObjectPath);
				if (!PInfo.TypeObjectName.IsEmpty())
					PIObj->SetStringField(TEXT("typeObjectName"),  PInfo.TypeObjectName);
				PIObj->SetStringField(TEXT("containerType"),  PInfo.ContainerType);
				if (!PInfo.MapValueCategory.IsEmpty())
					PIObj->SetStringField(TEXT("mapValueCategory"),   PInfo.MapValueCategory);
				if (!PInfo.MapValueObjectPath.IsEmpty())
					PIObj->SetStringField(TEXT("mapValueObjectPath"), PInfo.MapValueObjectPath);
				PIObj->SetBoolField(TEXT("isReference"), PInfo.bIsReference);
				PIObj->SetBoolField(TEXT("isConst"),     PInfo.bIsConst);
				PIObj->SetBoolField(TEXT("isOut"),       PInfo.bIsOut);
				if (!PInfo.DefaultValue.IsEmpty())
					PIObj->SetStringField(TEXT("defaultValue"), PInfo.DefaultValue);
				if (!PInfo.Description.IsEmpty())
					PIObj->SetStringField(TEXT("description"), PInfo.Description);
				DetailedOutputParamArray.Add(MakeShareable(new FJsonValueObject(PIObj)));
		}
		if (DetailedOutputParamArray.Num() > 0)
			FuncObj->SetArrayField(TEXT("detailedOutputParams"), DetailedOutputParamArray);

		DetailedFuncArray.Add(MakeShareable(new FJsonValueObject(FuncObj)));
	}
	JsonObject->SetArrayField(TEXT("detailedFunctions"), DetailedFuncArray);

	TArray<TSharedPtr<FJsonValue>> DelegateSignatureArray;
	for (const FBS_FunctionInfo& DelegateInfo : Data.DelegateSignatures)
	{
		TSharedPtr<FJsonObject> DelegateObj = MakeShareable(new FJsonObject);
		DelegateObj->SetStringField(TEXT("name"), DelegateInfo.FunctionName);
		DelegateObj->SetStringField(TEXT("signaturePath"), DelegateInfo.FunctionPath);
		DelegateObj->SetStringField(TEXT("returnType"), DelegateInfo.ReturnType);
		DelegateObj->SetStringField(TEXT("returnTypeObjectPath"), DelegateInfo.ReturnTypeObjectPath);
		DelegateObj->SetArrayField(TEXT("declarationSpecifiers"), BuildStringArray(DelegateInfo.DeclarationSpecifiers));
		DelegateObj->SetArrayField(TEXT("inputParameters"), BuildStringArray(DelegateInfo.InputParameters));
		DelegateObj->SetArrayField(TEXT("outputParameters"), BuildStringArray(DelegateInfo.OutputParameters));
		DelegateSignatureArray.Add(MakeShareable(new FJsonValueObject(DelegateObj)));
	}
	JsonObject->SetArrayField(TEXT("delegateSignatures"), DelegateSignatureArray);
	
    // Detailed components
	TArray<TSharedPtr<FJsonValue>> DetailedCompArray;
	for (const FBS_ComponentInfo& CompInfo : Data.DetailedComponents)
	{
		TSharedPtr<FJsonObject> CompObj = MakeShareable(new FJsonObject);
		CompObj->SetStringField(TEXT("name"), CompInfo.ComponentName);
		CompObj->SetStringField(TEXT("class"), CompInfo.ComponentClass);
		CompObj->SetStringField(TEXT("parentComponent"), CompInfo.ParentComponent);
		CompObj->SetStringField(TEXT("parentOwnerClassName"), CompInfo.ParentOwnerClassName);
		CompObj->SetStringField(TEXT("transform"), CompInfo.Transform);
		CompObj->SetBoolField(TEXT("isRootComponent"), CompInfo.bIsRootComponent);
		CompObj->SetBoolField(TEXT("isInherited"), CompInfo.bIsInherited);
		CompObj->SetBoolField(TEXT("isParentComponentNative"), CompInfo.bIsParentComponentNative);
		CompObj->SetBoolField(TEXT("hasInheritableOverride"), CompInfo.bHasInheritableOverride);
		CompObj->SetStringField(TEXT("inheritableOwnerClassPath"), CompInfo.InheritableOwnerClassPath);
		CompObj->SetStringField(TEXT("inheritableOwnerClassName"), CompInfo.InheritableOwnerClassName);
		CompObj->SetStringField(TEXT("inheritableComponentGuid"), CompInfo.InheritableComponentGuid);
		CompObj->SetStringField(TEXT("inheritableSourceTemplatePath"), CompInfo.InheritableSourceTemplatePath);
		CompObj->SetStringField(TEXT("inheritableOverrideTemplatePath"), CompInfo.InheritableOverrideTemplatePath);
		CompObj->SetArrayField(TEXT("inheritableOverrideProperties"), BuildStringArray(CompInfo.InheritableOverrideProperties));
		AddSortedStringMap(CompObj, TEXT("inheritableOverrideValues"), CompInfo.InheritableOverrideValues);
		AddSortedStringMap(CompObj, TEXT("inheritableParentValues"), CompInfo.InheritableParentValues);
		
		TArray<TSharedPtr<FJsonValue>> PropArray;
		for (const FString& Prop : CompInfo.ComponentProperties)
		{
			PropArray.Add(MakeShareable(new FJsonValueString(Prop)));
		}
		CompObj->SetArrayField(TEXT("properties"), PropArray);
		
		TArray<TSharedPtr<FJsonValue>> CompAssetRefArray;
		for (const FString& AssetRef : CompInfo.AssetReferences)
		{
			CompAssetRefArray.Add(MakeShareable(new FJsonValueString(AssetRef)));
		}
		CompObj->SetArrayField(TEXT("assetReferences"), CompAssetRefArray);
		
		DetailedCompArray.Add(MakeShareable(new FJsonValueObject(CompObj)));
	}
    JsonObject->SetArrayField(TEXT("detailedComponents"), DetailedCompArray);

	TArray<TSharedPtr<FJsonValue>> TimelineArray;
	for (const FBS_TimelineData& Timeline : Data.Timelines)
	{
		TSharedPtr<FJsonObject> TimelineObj = MakeShareable(new FJsonObject);
		TimelineObj->SetStringField(TEXT("name"), Timeline.TimelineName);
		TimelineObj->SetNumberField(TEXT("length"), Timeline.TimelineLength);
		TimelineObj->SetStringField(TEXT("lengthMode"), Timeline.LengthMode);
		TimelineObj->SetBoolField(TEXT("autoPlay"), Timeline.bAutoPlay);
		TimelineObj->SetBoolField(TEXT("loop"), Timeline.bLoop);
		TimelineObj->SetBoolField(TEXT("replicated"), Timeline.bReplicated);
		TimelineObj->SetBoolField(TEXT("ignoreTimeDilation"), Timeline.bIgnoreTimeDilation);
		TimelineObj->SetStringField(TEXT("updateFunction"), Timeline.UpdateFunctionName);
		TimelineObj->SetStringField(TEXT("finishedFunction"), Timeline.FinishedFunctionName);

		TArray<TSharedPtr<FJsonValue>> TrackArray;
		for (const FBS_TimelineTrackData& Track : Timeline.Tracks)
		{
			TSharedPtr<FJsonObject> TrackObj = MakeShareable(new FJsonObject);
			TrackObj->SetStringField(TEXT("name"), Track.TrackName);
			TrackObj->SetStringField(TEXT("type"), Track.TrackType);
			TrackObj->SetStringField(TEXT("curvePath"), Track.CurvePath);
			TrackObj->SetStringField(TEXT("propertyName"), Track.PropertyName);
			TrackObj->SetStringField(TEXT("functionName"), Track.FunctionName);
			TrackObj->SetNumberField(TEXT("keyCount"), Track.KeyCount);
			TrackArray.Add(MakeShareable(new FJsonValueObject(TrackObj)));
		}
		TimelineObj->SetArrayField(TEXT("tracks"), TrackArray);
		TimelineArray.Add(MakeShareable(new FJsonValueObject(TimelineObj)));
	}
	JsonObject->SetArrayField(TEXT("timelines"), TimelineArray);

	TSharedPtr<FJsonObject> WidgetBlueprintObj = MakeShareable(new FJsonObject);
	WidgetBlueprintObj->SetBoolField(TEXT("isWidgetBlueprint"), Data.WidgetBlueprint.bIsWidgetBlueprint);
	WidgetBlueprintObj->SetBoolField(TEXT("hasSourceWidgetTree"), Data.WidgetBlueprint.bHasSourceWidgetTree);
	if (!Data.WidgetBlueprint.WidgetTreePath.IsEmpty())
		WidgetBlueprintObj->SetStringField(TEXT("widgetTreePath"), Data.WidgetBlueprint.WidgetTreePath);
	if (!Data.WidgetBlueprint.RootWidgetName.IsEmpty())
		WidgetBlueprintObj->SetStringField(TEXT("rootWidgetName"), Data.WidgetBlueprint.RootWidgetName);
	WidgetBlueprintObj->SetArrayField(TEXT("namedSlotBindings"), BuildStringArray(Data.WidgetBlueprint.NamedSlotBindings));
	WidgetBlueprintObj->SetArrayField(TEXT("generatedNamedSlots"), BuildStringArray(Data.WidgetBlueprint.GeneratedNamedSlots));

	TArray<TSharedPtr<FJsonValue>> WidgetTemplateArray;
	for (const FBS_WidgetTemplateData& Widget : Data.WidgetBlueprint.Widgets)
	{
		TSharedPtr<FJsonObject> WidgetObj = MakeShareable(new FJsonObject);
		WidgetObj->SetStringField(TEXT("name"), Widget.WidgetName);
		WidgetObj->SetStringField(TEXT("path"), Widget.WidgetPath);
		WidgetObj->SetStringField(TEXT("classPath"), Widget.WidgetClassPath);
		if (!Widget.ParentWidgetName.IsEmpty())
			WidgetObj->SetStringField(TEXT("parentName"), Widget.ParentWidgetName);
		if (!Widget.ParentWidgetPath.IsEmpty())
			WidgetObj->SetStringField(TEXT("parentPath"), Widget.ParentWidgetPath);
		WidgetObj->SetNumberField(TEXT("childIndex"), Widget.ChildIndex);
		WidgetObj->SetNumberField(TEXT("depth"), Widget.Depth);
		if (!Widget.SlotPath.IsEmpty())
			WidgetObj->SetStringField(TEXT("slotPath"), Widget.SlotPath);
		if (!Widget.SlotClassPath.IsEmpty())
			WidgetObj->SetStringField(TEXT("slotClassPath"), Widget.SlotClassPath);
		if (!Widget.NamedSlotName.IsEmpty())
			WidgetObj->SetStringField(TEXT("namedSlotName"), Widget.NamedSlotName);
		AddSortedStringMap(WidgetObj, TEXT("properties"), Widget.WidgetProperties);
		AddSortedStringMap(WidgetObj, TEXT("slotProperties"), Widget.SlotProperties);
		WidgetObj->SetArrayField(TEXT("referencedObjectPaths"), BuildStringArray(Widget.ReferencedObjectPaths));
		WidgetTemplateArray.Add(MakeShareable(new FJsonValueObject(WidgetObj)));
	}
	WidgetBlueprintObj->SetArrayField(TEXT("widgets"), WidgetTemplateArray);

	TArray<TSharedPtr<FJsonValue>> WidgetBindingArray;
	for (const FBS_WidgetBindingData& Binding : Data.WidgetBlueprint.Bindings)
	{
		TSharedPtr<FJsonObject> BindingObj = MakeShareable(new FJsonObject);
		BindingObj->SetStringField(TEXT("objectName"), Binding.ObjectName);
		BindingObj->SetStringField(TEXT("propertyName"), Binding.PropertyName);
		BindingObj->SetStringField(TEXT("functionName"), Binding.FunctionName);
		BindingObj->SetStringField(TEXT("sourceProperty"), Binding.SourceProperty);
		BindingObj->SetStringField(TEXT("sourcePath"), Binding.SourcePath);
		BindingObj->SetStringField(TEXT("memberGuid"), Binding.MemberGuid);
		BindingObj->SetStringField(TEXT("kind"), Binding.BindingKind);
		WidgetBindingArray.Add(MakeShareable(new FJsonValueObject(BindingObj)));
	}
	WidgetBlueprintObj->SetArrayField(TEXT("bindings"), WidgetBindingArray);

	TArray<TSharedPtr<FJsonValue>> WidgetAnimationArray;
	for (const FBS_WidgetAnimationData& Animation : Data.WidgetBlueprint.Animations)
	{
		TSharedPtr<FJsonObject> AnimationObj = MakeShareable(new FJsonObject);
		AnimationObj->SetStringField(TEXT("name"), Animation.AnimationName);
		AnimationObj->SetStringField(TEXT("path"), Animation.AnimationPath);
		AnimationObj->SetStringField(TEXT("movieScenePath"), Animation.MovieScenePath);
		AnimationObj->SetStringField(TEXT("playbackRange"), Animation.PlaybackRange);
		AnimationObj->SetStringField(TEXT("tickResolution"), Animation.TickResolution);
		AnimationObj->SetStringField(TEXT("displayRate"), Animation.DisplayRate);
		AnimationObj->SetArrayField(TEXT("masterTracks"), BuildStringArray(Animation.MasterTracks));
		AnimationObj->SetArrayField(TEXT("movieSceneBindings"), BuildStringArray(Animation.MovieSceneBindings));
		AnimationObj->SetArrayField(TEXT("referencedObjectPaths"), BuildStringArray(Animation.ReferencedObjectPaths));
		AddSortedStringMap(AnimationObj, TEXT("properties"), Animation.AnimationProperties);
		AddSortedStringMap(AnimationObj, TEXT("movieSceneProperties"), Animation.MovieSceneProperties);

		TArray<TSharedPtr<FJsonValue>> AnimationBindingArray;
		for (const FBS_WidgetAnimationBindingData& Binding : Animation.WidgetBindings)
		{
			TSharedPtr<FJsonObject> BindingObj = MakeShareable(new FJsonObject);
			BindingObj->SetStringField(TEXT("widgetName"), Binding.WidgetName);
			BindingObj->SetStringField(TEXT("slotWidgetName"), Binding.SlotWidgetName);
			BindingObj->SetStringField(TEXT("animationGuid"), Binding.AnimationGuid);
			BindingObj->SetBoolField(TEXT("isRootWidget"), Binding.bIsRootWidget);
			AnimationBindingArray.Add(MakeShareable(new FJsonValueObject(BindingObj)));
		}
		AnimationObj->SetArrayField(TEXT("widgetBindings"), AnimationBindingArray);
		WidgetAnimationArray.Add(MakeShareable(new FJsonValueObject(AnimationObj)));
	}
	WidgetBlueprintObj->SetArrayField(TEXT("animations"), WidgetAnimationArray);
	JsonObject->SetObjectField(TEXT("widgetBlueprint"), WidgetBlueprintObj);

    if (Data.StructuredGraphsExt.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> GraphsArray;
        TMap<FString, int32> NodeTypeCounts;
        
        for (const FBS_GraphData_Ext& GraphData : Data.StructuredGraphsExt)
        {
            TSharedPtr<FJsonObject> GraphObj = MakeShareable(new FJsonObject);
            GraphObj->SetStringField(TEXT("name"), GraphData.GraphName);
            GraphObj->SetStringField(TEXT("graphType"), GraphData.GraphType);
            if (!GraphData.GraphPath.IsEmpty())
                GraphObj->SetStringField(TEXT("graphPath"), GraphData.GraphPath);
            if (!GraphData.ParentGraphPath.IsEmpty())
                GraphObj->SetStringField(TEXT("parentGraphPath"), GraphData.ParentGraphPath);
            if (!GraphData.OwningCompositeNodeGuid.IsEmpty())
                GraphObj->SetStringField(TEXT("owningCompositeNodeGuid"), GraphData.OwningCompositeNodeGuid);
            GraphObj->SetNumberField(TEXT("graphDepth"), GraphData.GraphDepth);
            GraphObj->SetBoolField(TEXT("isCollapsedGraph"), GraphData.bIsCollapsedGraph);
            
            // Serialize nodes as proper JSON objects
            TArray<TSharedPtr<FJsonValue>> NodesArray;
            for (const FBS_NodeData& NodeData : GraphData.Nodes)
            {
                TSharedPtr<FJsonObject> NodeObj = MakeShareable(new FJsonObject);
                NodeObj->SetStringField(TEXT("nodeGuid"), NodeData.NodeGuid.ToString());
                NodeObj->SetStringField(TEXT("nodeType"), NodeData.NodeType);
                NodeObj->SetStringField(TEXT("title"), NodeData.Title);
                NodeObj->SetNumberField(TEXT("posX"), NodeData.PosX);
                NodeObj->SetNumberField(TEXT("posY"), NodeData.PosY);
                NodeObj->SetNumberField(TEXT("width"), NodeData.NodeWidth);
                NodeObj->SetNumberField(TEXT("height"), NodeData.NodeHeight);
                if (!NodeData.NodeComment.IsEmpty())
                    NodeObj->SetStringField(TEXT("comment"), NodeData.NodeComment);
                if (!NodeData.EnabledState.IsEmpty())
                    NodeObj->SetStringField(TEXT("enabledState"), NodeData.EnabledState);
                if (!NodeData.AdvancedPinDisplay.IsEmpty())
                    NodeObj->SetStringField(TEXT("advancedPinDisplay"), NodeData.AdvancedPinDisplay);
                if (NodeData.bCanRenameNode)
                    NodeObj->SetBoolField(TEXT("canRename"), true);
                if (NodeData.bCanResizeNode)
                    NodeObj->SetBoolField(TEXT("canResize"), true);
                if (NodeData.bCommentBubblePinned)
                    NodeObj->SetBoolField(TEXT("commentBubblePinned"), true);
                if (NodeData.bCommentBubbleVisible)
                    NodeObj->SetBoolField(TEXT("commentBubbleVisible"), true);
                if (NodeData.bCommentBubbleMakeVisible)
                    NodeObj->SetBoolField(TEXT("commentBubbleMakeVisible"), true);
                if (NodeData.bHasCompilerMessage)
                    NodeObj->SetBoolField(TEXT("hasCompilerMessage"), true);
                if (NodeData.ErrorType != 0)
                    NodeObj->SetNumberField(TEXT("errorType"), NodeData.ErrorType);
                if (!NodeData.ErrorMsg.IsEmpty())
                    NodeObj->SetStringField(TEXT("errorMsg"), NodeData.ErrorMsg);
                
                // Add optional fields only if present
                if (!NodeData.MemberParent.IsEmpty())
                    NodeObj->SetStringField(TEXT("memberParent"), NodeData.MemberParent);
                if (!NodeData.MemberName.IsEmpty())
                    NodeObj->SetStringField(TEXT("memberName"), NodeData.MemberName);
                
                // Serialize pins
                TArray<TSharedPtr<FJsonValue>> PinsArray;
                for (const FBS_PinData& Pin : NodeData.Pins)
                {
                    TSharedPtr<FJsonObject> PinObj = MakeShareable(new FJsonObject);
                    PinObj->SetStringField(TEXT("name"), Pin.Name.ToString());
                    if (Pin.PinId.IsValid())
                        PinObj->SetStringField(TEXT("pinId"), Pin.PinId.ToString());
                    if (!Pin.Direction.IsEmpty())
                        PinObj->SetStringField(TEXT("direction"), Pin.Direction);
                    
                    // Only include type for non-exec pins (like NodeToCode)
                    if (Pin.Category != TEXT("exec"))
                    {
                        PinObj->SetStringField(TEXT("category"), Pin.Category);
                        if (!Pin.SubCategory.IsEmpty())
                            PinObj->SetStringField(TEXT("subCategory"), Pin.SubCategory);
                        if (!Pin.ObjectType.IsEmpty())
                            PinObj->SetStringField(TEXT("objectType"), Pin.ObjectType);
                        if (!Pin.ObjectPath.IsEmpty())
                            PinObj->SetStringField(TEXT("objectPath"), Pin.ObjectPath);
                    }
                    else
                    {
                        PinObj->SetStringField(TEXT("category"), TEXT("exec"));
                    }

                    if (!Pin.SubCategoryMemberParent.IsEmpty())
                        PinObj->SetStringField(TEXT("subCategoryMemberParent"), Pin.SubCategoryMemberParent);
                    if (!Pin.SubCategoryMemberName.IsEmpty())
                        PinObj->SetStringField(TEXT("subCategoryMemberName"), Pin.SubCategoryMemberName);
                    if (!Pin.SubCategoryMemberGuid.IsEmpty())
                        PinObj->SetStringField(TEXT("subCategoryMemberGuid"), Pin.SubCategoryMemberGuid);
                    
                    // Only include flags if true
                    if (Pin.bIsReference)
                        PinObj->SetBoolField(TEXT("is_reference"), true);
                    if (Pin.bIsConst)
                        PinObj->SetBoolField(TEXT("is_const"), true);
                    if (Pin.bIsWeakPointer)
                        PinObj->SetBoolField(TEXT("is_weak_pointer"), true);
                    if (Pin.bIsUObjectWrapper)
                        PinObj->SetBoolField(TEXT("is_uobject_wrapper"), true);
                    if (Pin.bIsArray)
                        PinObj->SetBoolField(TEXT("is_array"), true);
                    if (Pin.bIsMap)
                        PinObj->SetBoolField(TEXT("is_map"), true);
                    if (Pin.bIsSet)
                        PinObj->SetBoolField(TEXT("is_set"), true);
                    if (Pin.bIsExec)
                        PinObj->SetBoolField(TEXT("is_exec"), true);
                    if (Pin.bIsOut)
                        PinObj->SetBoolField(TEXT("is_out"), true);
                    if (Pin.bIsOptional)
                        PinObj->SetBoolField(TEXT("is_optional"), true);
                    if (Pin.bConnected)
                        PinObj->SetBoolField(TEXT("connected"), true);
                    if (Pin.SourceIndex != INDEX_NONE)
                        PinObj->SetNumberField(TEXT("sourceIndex"), Pin.SourceIndex);
                    if (Pin.ParentPinId.IsValid())
                        PinObj->SetStringField(TEXT("parentPinId"), Pin.ParentPinId.ToString());
                    if (Pin.SubPinIds.Num() > 0)
                    {
                        TArray<TSharedPtr<FJsonValue>> SubPinArray;
                        for (const FGuid& SubId : Pin.SubPinIds)
                        {
                            SubPinArray.Add(MakeShareable(new FJsonValueString(SubId.ToString())));
                        }
                        PinObj->SetArrayField(TEXT("subPinIds"), SubPinArray);
                    }
                    if (Pin.bHidden)
                        PinObj->SetBoolField(TEXT("hidden"), true);
                    if (Pin.bNotConnectable)
                        PinObj->SetBoolField(TEXT("not_connectable"), true);
                    if (Pin.bDefaultValueIsReadOnly)
                        PinObj->SetBoolField(TEXT("default_value_readonly"), true);
                    if (Pin.bDefaultValueIsIgnored)
                        PinObj->SetBoolField(TEXT("default_value_ignored"), true);
                    if (Pin.bAdvancedView)
                        PinObj->SetBoolField(TEXT("advanced"), true);
                    if (Pin.bDisplayAsMutableRef)
                        PinObj->SetBoolField(TEXT("display_as_mutable_ref"), true);
                    if (Pin.bAllowFriendlyName)
                        PinObj->SetBoolField(TEXT("allow_friendly_name"), true);
                    if (Pin.bOrphanedPin)
                        PinObj->SetBoolField(TEXT("orphaned"), true);
                    if (!Pin.FriendlyName.IsEmpty())
                        PinObj->SetStringField(TEXT("friendlyName"), Pin.FriendlyName);
                    if (!Pin.ToolTip.IsEmpty())
                        PinObj->SetStringField(TEXT("toolTip"), Pin.ToolTip);
                    
                    // Default value only if present
                    if (!Pin.DefaultValue.IsEmpty())
                        PinObj->SetStringField(TEXT("defaultValue"), Pin.DefaultValue);
                    if (!Pin.AutogeneratedDefaultValue.IsEmpty())
                        PinObj->SetStringField(TEXT("autogeneratedDefaultValue"), Pin.AutogeneratedDefaultValue);
                    if (!Pin.DefaultObjectPath.IsEmpty())
                        PinObj->SetStringField(TEXT("defaultObjectPath"), Pin.DefaultObjectPath);
                    if (!Pin.PersistentGuid.IsEmpty())
                        PinObj->SetStringField(TEXT("persistentGuid"), Pin.PersistentGuid);
                    if (!Pin.ValueCategory.IsEmpty())
                        PinObj->SetStringField(TEXT("valueCategory"), Pin.ValueCategory);
                    if (!Pin.ValueSubCategory.IsEmpty())
                        PinObj->SetStringField(TEXT("valueSubCategory"), Pin.ValueSubCategory);
                    if (!Pin.ValueObjectType.IsEmpty())
                        PinObj->SetStringField(TEXT("valueObjectType"), Pin.ValueObjectType);
                    if (!Pin.ValueObjectPath.IsEmpty())
                        PinObj->SetStringField(TEXT("valueObjectPath"), Pin.ValueObjectPath);
                    if (Pin.bValueIsConst)
                        PinObj->SetBoolField(TEXT("valueIsConst"), true);
                    if (Pin.bValueIsWeakPointer)
                        PinObj->SetBoolField(TEXT("valueIsWeakPointer"), true);
                    if (Pin.bValueIsUObjectWrapper)
                        PinObj->SetBoolField(TEXT("valueIsUObjectWrapper"), true);
                    
                    PinsArray.Add(MakeShareable(new FJsonValueObject(PinObj)));
                }
                NodeObj->SetArrayField(TEXT("pins"), PinsArray);

                if (NodeData.NodeProperties.Num() > 0)
                {
                    AddSortedStringMap(NodeObj, TEXT("nodeProperties"), NodeData.NodeProperties);
                }
                
                // Track node type for coverage
                NodeTypeCounts.FindOrAdd(NodeData.NodeType)++;
                
                NodesArray.Add(MakeShareable(new FJsonValueObject(NodeObj)));
            }
            GraphObj->SetArrayField(TEXT("nodes"), NodesArray);
            
            // Serialize execution flows
            TSharedPtr<FJsonObject> FlowsObj = MakeShareable(new FJsonObject);
            TArray<TSharedPtr<FJsonValue>> ExecutionArray;
            for (const FBS_FlowEdge& Edge : GraphData.Execution)
            {
                TSharedPtr<FJsonObject> EdgeObj = MakeShareable(new FJsonObject);
                EdgeObj->SetStringField(TEXT("sourceNodeGuid"), Edge.SourceNodeGuid.ToString());
                EdgeObj->SetStringField(TEXT("sourcePinName"), Edge.SourcePinName.ToString());
                if (Edge.SourcePinId.IsValid())
                    EdgeObj->SetStringField(TEXT("sourcePinId"), Edge.SourcePinId.ToString());
                EdgeObj->SetStringField(TEXT("targetNodeGuid"), Edge.TargetNodeGuid.ToString());
                EdgeObj->SetStringField(TEXT("targetPinName"), Edge.TargetPinName.ToString());
                if (Edge.TargetPinId.IsValid())
                    EdgeObj->SetStringField(TEXT("targetPinId"), Edge.TargetPinId.ToString());
                ExecutionArray.Add(MakeShareable(new FJsonValueObject(EdgeObj)));
            }
            FlowsObj->SetArrayField(TEXT("execution"), ExecutionArray);

            // Serialize data flows
            TArray<TSharedPtr<FJsonValue>> DataArray;
            for (const FBS_PinLinkData& Link : GraphData.DataLinks)
            {
                TSharedPtr<FJsonObject> LinkObj = MakeShareable(new FJsonObject);
                LinkObj->SetStringField(TEXT("sourceNodeGuid"), Link.SourceNodeGuid.ToString());
                LinkObj->SetStringField(TEXT("sourcePinName"), Link.SourcePinName.ToString());
                if (Link.SourcePinId.IsValid())
                    LinkObj->SetStringField(TEXT("sourcePinId"), Link.SourcePinId.ToString());
                LinkObj->SetStringField(TEXT("targetNodeGuid"), Link.TargetNodeGuid.ToString());
                LinkObj->SetStringField(TEXT("targetPinName"), Link.TargetPinName.ToString());
                if (Link.TargetPinId.IsValid())
                    LinkObj->SetStringField(TEXT("targetPinId"), Link.TargetPinId.ToString());
                LinkObj->SetStringField(TEXT("pinCategory"), Link.PinCategory);
                LinkObj->SetStringField(TEXT("pinSubCategory"), Link.PinSubCategory);
                if (!Link.PinSubCategoryObjectPath.IsEmpty())
                    LinkObj->SetStringField(TEXT("pinSubCategoryObjectPath"), Link.PinSubCategoryObjectPath);
                LinkObj->SetBoolField(TEXT("isExec"), Link.bIsExec);
                DataArray.Add(MakeShareable(new FJsonValueObject(LinkObj)));
            }
            FlowsObj->SetArrayField(TEXT("data"), DataArray);
            
            GraphObj->SetObjectField(TEXT("flows"), FlowsObj);

            // Task 19: emit graph-level boundary pin GUIDs
            if (GraphData.GraphInputPins.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> InputPinsArr;
                for (const FString& PinId : GraphData.GraphInputPins)
                    InputPinsArr.Add(MakeShareable(new FJsonValueString(PinId)));
                GraphObj->SetArrayField(TEXT("graphInputPins"), InputPinsArr);
            }
            if (GraphData.GraphOutputPins.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> OutputPinsArr;
                for (const FString& PinId : GraphData.GraphOutputPins)
                    OutputPinsArr.Add(MakeShareable(new FJsonValueString(PinId)));
                GraphObj->SetArrayField(TEXT("graphOutputPins"), OutputPinsArr);
            }

            GraphsArray.Add(MakeShareable(new FJsonValueObject(GraphObj)));
        }

        // Override structuredGraphs with the properly formatted version
        JsonObject->SetArrayField(TEXT("structuredGraphs"), GraphsArray);
        
        // Update coverage with new counts
        TSharedPtr<FJsonObject> Coverage = MakeShareable(new FJsonObject);
        Coverage->SetNumberField(TEXT("totalNodeCount"), Data.TotalNodeCount);

        TSharedPtr<FJsonObject> NodeTypes = MakeShareable(new FJsonObject);
        for (const TPair<FString, int32>& P : NodeTypeCounts)
        {
            NodeTypes->SetNumberField(P.Key, P.Value);
        }
        Coverage->SetObjectField(TEXT("nodeTypeCounts"), NodeTypes);
        Coverage->SetArrayField(TEXT("unsupportedNodeTypes"), BuildStringArray(Data.UnsupportedNodeTypes));
        Coverage->SetArrayField(TEXT("partiallySupportedNodeTypes"), BuildStringArray(Data.PartiallySupportedNodeTypes));

        JsonObject->SetObjectField(TEXT("coverage"), Coverage);

        // Graphs summary (counts per graph)
        {
            TArray<TSharedPtr<FJsonValue>> Summaries;
            for (const FBS_GraphData_Ext& GraphData : Data.StructuredGraphsExt)
            {
                TSharedPtr<FJsonObject> Summ = MakeShareable(new FJsonObject);
                Summ->SetStringField(TEXT("name"), GraphData.GraphName);
                Summ->SetStringField(TEXT("graphType"), GraphData.GraphType);
                if (GraphData.GraphType == TEXT("ConstructionScript"))
                {
                    Summ->SetStringField(TEXT("role"), TEXT("Construction"));
                }
                else if (GraphData.GraphType == TEXT("DelegateSignature"))
                {
                    Summ->SetStringField(TEXT("role"), TEXT("DelegateSignature"));
                }
                else
                {
                    Summ->SetStringField(TEXT("role"), GraphData.GraphType);
                }
                Summ->SetNumberField(TEXT("nodes"), GraphData.Nodes.Num());
                Summ->SetNumberField(TEXT("execEdges"), GraphData.Execution.Num());

                // Optional top node types (up to 5)
                TMap<FString, int32> TypeCounts;
                for (const FBS_NodeData& N : GraphData.Nodes)
                {
                    TypeCounts.FindOrAdd(N.NodeType)++;
                }
                TArray<TPair<FString,int32>> Sorted;
                for (const TPair<FString,int32>& P : TypeCounts)
                {
                    Sorted.Add({P.Key, P.Value});
                }
                Sorted.Sort([](const TPair<FString,int32>& A, const TPair<FString,int32>& B){ return A.Value > B.Value; });
                TArray<TSharedPtr<FJsonValue>> TopTypes;
                const int32 MaxTop = FMath::Min(5, Sorted.Num());
                for (int32 i=0; i<MaxTop; ++i)
                {
                    const TPair<FString,int32>& P = Sorted[i];
                    TSharedPtr<FJsonObject> Entry = MakeShareable(new FJsonObject);
                    Entry->SetStringField(TEXT("type"), P.Key);
                    Entry->SetNumberField(TEXT("count"), P.Value);
                    TopTypes.Add(MakeShareable(new FJsonValueObject(Entry)));
                }
                if (TopTypes.Num() > 0)
                {
                    Summ->SetArrayField(TEXT("topNodeTypes"), TopTypes);
                }

                Summaries.Add(MakeShareable(new FJsonValueObject(Summ)));
            }
            if (Summaries.Num() > 0)
            {
                JsonObject->SetArrayField(TEXT("graphsSummary"), Summaries);
            }
        }
    }

	// Always emit these fields for stable schemas even when empty.
	if (!JsonObject->HasField(TEXT("structuredGraphs")))
	{
		JsonObject->SetArrayField(TEXT("structuredGraphs"), TArray<TSharedPtr<FJsonValue>>{});
	}
	if (!JsonObject->HasField(TEXT("graphsSummary")))
	{
		JsonObject->SetArrayField(TEXT("graphsSummary"), TArray<TSharedPtr<FJsonValue>>{});
	}
	if (!JsonObject->HasField(TEXT("coverage")))
	{
		TSharedPtr<FJsonObject> CoverageObj = MakeShareable(new FJsonObject);
		CoverageObj->SetNumberField(TEXT("totalNodeCount"), Data.TotalNodeCount);
		CoverageObj->SetObjectField(TEXT("nodeTypeCounts"), MakeShareable(new FJsonObject));
		CoverageObj->SetArrayField(TEXT("unsupportedNodeTypes"), BuildStringArray(Data.UnsupportedNodeTypes));
		CoverageObj->SetArrayField(TEXT("partiallySupportedNodeTypes"), BuildStringArray(Data.PartiallySupportedNodeTypes));
		JsonObject->SetObjectField(TEXT("coverage"), CoverageObj);
	}

	// AnimBlueprint-specific output (schema-aligned)
	if (Data.bIsAnimBlueprint)
	{
		auto BuildPinArray = [](const TArray<FBS_PinData>& Pins)
		{
			TArray<TSharedPtr<FJsonValue>> PinArray;
			for (const FBS_PinData& Pin : Pins)
			{
				TSharedPtr<FJsonObject> PinObj = MakeShareable(new FJsonObject);
				PinObj->SetStringField(TEXT("name"), Pin.Name.ToString());
				if (Pin.PinId.IsValid())
					PinObj->SetStringField(TEXT("pinId"), Pin.PinId.ToString());
				if (!Pin.Direction.IsEmpty())
					PinObj->SetStringField(TEXT("direction"), Pin.Direction);
				PinObj->SetStringField(TEXT("category"), Pin.Category);
				PinObj->SetStringField(TEXT("subCategory"), Pin.SubCategory);
				PinObj->SetStringField(TEXT("objectType"), Pin.ObjectType);
				if (!Pin.ObjectPath.IsEmpty())
					PinObj->SetStringField(TEXT("objectPath"), Pin.ObjectPath);
				if (!Pin.SubCategoryMemberParent.IsEmpty())
					PinObj->SetStringField(TEXT("subCategoryMemberParent"), Pin.SubCategoryMemberParent);
				if (!Pin.SubCategoryMemberName.IsEmpty())
					PinObj->SetStringField(TEXT("subCategoryMemberName"), Pin.SubCategoryMemberName);
				if (!Pin.SubCategoryMemberGuid.IsEmpty())
					PinObj->SetStringField(TEXT("subCategoryMemberGuid"), Pin.SubCategoryMemberGuid);
				PinObj->SetBoolField(TEXT("isReference"), Pin.bIsReference);
				PinObj->SetBoolField(TEXT("isConst"), Pin.bIsConst);
				if (Pin.bIsWeakPointer)
					PinObj->SetBoolField(TEXT("isWeakPointer"), true);
				if (Pin.bIsUObjectWrapper)
					PinObj->SetBoolField(TEXT("isUObjectWrapper"), true);
				PinObj->SetBoolField(TEXT("isArray"), Pin.bIsArray);
				PinObj->SetBoolField(TEXT("isMap"), Pin.bIsMap);
				PinObj->SetBoolField(TEXT("isSet"), Pin.bIsSet);
				if (Pin.bIsExec)
					PinObj->SetBoolField(TEXT("isExec"), true);
				PinObj->SetBoolField(TEXT("connected"), Pin.bConnected);
				if (!Pin.DefaultValue.IsEmpty())
					PinObj->SetStringField(TEXT("defaultValue"), Pin.DefaultValue);
				if (!Pin.AutogeneratedDefaultValue.IsEmpty())
					PinObj->SetStringField(TEXT("autogeneratedDefaultValue"), Pin.AutogeneratedDefaultValue);
				if (!Pin.DefaultObjectPath.IsEmpty())
					PinObj->SetStringField(TEXT("defaultObjectPath"), Pin.DefaultObjectPath);
				if (!Pin.PersistentGuid.IsEmpty())
					PinObj->SetStringField(TEXT("persistentGuid"), Pin.PersistentGuid);
				PinObj->SetBoolField(TEXT("isOut"), Pin.bIsOut);
				PinObj->SetBoolField(TEXT("isOptional"), Pin.bIsOptional);
				if (Pin.SourceIndex != INDEX_NONE)
					PinObj->SetNumberField(TEXT("sourceIndex"), Pin.SourceIndex);
				if (Pin.ParentPinId.IsValid())
					PinObj->SetStringField(TEXT("parentPinId"), Pin.ParentPinId.ToString());
				if (Pin.SubPinIds.Num() > 0)
				{
					TArray<TSharedPtr<FJsonValue>> SubPinArray;
					for (const FGuid& SubId : Pin.SubPinIds)
					{
						SubPinArray.Add(MakeShareable(new FJsonValueString(SubId.ToString())));
					}
					PinObj->SetArrayField(TEXT("subPinIds"), SubPinArray);
				}
				if (Pin.bHidden)
					PinObj->SetBoolField(TEXT("hidden"), true);
				if (Pin.bNotConnectable)
					PinObj->SetBoolField(TEXT("notConnectable"), true);
				if (Pin.bDefaultValueIsReadOnly)
					PinObj->SetBoolField(TEXT("defaultValueReadOnly"), true);
				if (Pin.bDefaultValueIsIgnored)
					PinObj->SetBoolField(TEXT("defaultValueIgnored"), true);
				if (Pin.bAdvancedView)
					PinObj->SetBoolField(TEXT("advanced"), true);
				if (Pin.bDisplayAsMutableRef)
					PinObj->SetBoolField(TEXT("displayAsMutableRef"), true);
				if (Pin.bAllowFriendlyName)
					PinObj->SetBoolField(TEXT("allowFriendlyName"), true);
				if (Pin.bOrphanedPin)
					PinObj->SetBoolField(TEXT("orphaned"), true);
				if (!Pin.FriendlyName.IsEmpty())
					PinObj->SetStringField(TEXT("friendlyName"), Pin.FriendlyName);
				if (!Pin.ToolTip.IsEmpty())
					PinObj->SetStringField(TEXT("toolTip"), Pin.ToolTip);
				if (!Pin.ValueCategory.IsEmpty())
					PinObj->SetStringField(TEXT("valueCategory"), Pin.ValueCategory);
				if (!Pin.ValueSubCategory.IsEmpty())
					PinObj->SetStringField(TEXT("valueSubCategory"), Pin.ValueSubCategory);
				if (!Pin.ValueObjectType.IsEmpty())
					PinObj->SetStringField(TEXT("valueObjectType"), Pin.ValueObjectType);
				if (!Pin.ValueObjectPath.IsEmpty())
					PinObj->SetStringField(TEXT("valueObjectPath"), Pin.ValueObjectPath);
				if (Pin.bValueIsConst)
					PinObj->SetBoolField(TEXT("valueIsConst"), true);
				if (Pin.bValueIsWeakPointer)
					PinObj->SetBoolField(TEXT("valueIsWeakPointer"), true);
				if (Pin.bValueIsUObjectWrapper)
					PinObj->SetBoolField(TEXT("valueIsUObjectWrapper"), true);
				PinArray.Add(MakeShareable(new FJsonValueObject(PinObj)));
			}
			return PinArray;
		};

		auto BuildFlowArray = [](const TArray<FBS_FlowEdge>& Edges)
		{
			TArray<TSharedPtr<FJsonValue>> FlowArray;
			for (const FBS_FlowEdge& Edge : Edges)
			{
				TSharedPtr<FJsonObject> EdgeObj = MakeShareable(new FJsonObject);
				EdgeObj->SetStringField(TEXT("sourceNodeGuid"), Edge.SourceNodeGuid.ToString());
				EdgeObj->SetStringField(TEXT("sourcePinName"), Edge.SourcePinName.ToString());
				if (Edge.SourcePinId.IsValid())
					EdgeObj->SetStringField(TEXT("sourcePinId"), Edge.SourcePinId.ToString());
				EdgeObj->SetStringField(TEXT("targetNodeGuid"), Edge.TargetNodeGuid.ToString());
				EdgeObj->SetStringField(TEXT("targetPinName"), Edge.TargetPinName.ToString());
				if (Edge.TargetPinId.IsValid())
					EdgeObj->SetStringField(TEXT("targetPinId"), Edge.TargetPinId.ToString());
				FlowArray.Add(MakeShareable(new FJsonValueObject(EdgeObj)));
			}
			return FlowArray;
		};

		auto BuildPinLinksArray = [](const TArray<FBS_PinLinkData>& Links)
		{
			TArray<TSharedPtr<FJsonValue>> LinkArray;
			for (const FBS_PinLinkData& Link : Links)
			{
				TSharedPtr<FJsonObject> LinkObj = MakeShareable(new FJsonObject);
				LinkObj->SetStringField(TEXT("sourceNodeGuid"), Link.SourceNodeGuid.ToString());
				LinkObj->SetStringField(TEXT("sourcePinName"), Link.SourcePinName.ToString());
				if (Link.SourcePinId.IsValid())
					LinkObj->SetStringField(TEXT("sourcePinId"), Link.SourcePinId.ToString());
				LinkObj->SetStringField(TEXT("targetNodeGuid"), Link.TargetNodeGuid.ToString());
				LinkObj->SetStringField(TEXT("targetPinName"), Link.TargetPinName.ToString());
				if (Link.TargetPinId.IsValid())
					LinkObj->SetStringField(TEXT("targetPinId"), Link.TargetPinId.ToString());
				LinkObj->SetStringField(TEXT("pinCategory"), Link.PinCategory);
				LinkObj->SetStringField(TEXT("pinSubCategory"), Link.PinSubCategory);
				if (!Link.PinSubCategoryObjectPath.IsEmpty())
					LinkObj->SetStringField(TEXT("pinSubCategoryObjectPath"), Link.PinSubCategoryObjectPath);
				LinkObj->SetBoolField(TEXT("isExec"), Link.bIsExec);
				LinkArray.Add(MakeShareable(new FJsonValueObject(LinkObj)));
			}
			return LinkArray;
		};

		// Animation variables
		TArray<TSharedPtr<FJsonValue>> AnimVars;
		for (const FBS_AnimationVariableData& Var : Data.AnimationVariables)
		{
			TSharedPtr<FJsonObject> VarObj = MakeShareable(new FJsonObject);
			VarObj->SetStringField(TEXT("variableName"), Var.VariableName);
			VarObj->SetStringField(TEXT("variableType"), Var.VariableType);
			VarObj->SetStringField(TEXT("defaultValue"), Var.DefaultValue);
			VarObj->SetStringField(TEXT("category"), Var.Category);
			AnimVars.Add(MakeShareable(new FJsonValueObject(VarObj)));
		}
		JsonObject->SetArrayField(TEXT("animationVariables"), AnimVars);

		// AnimGraph
		TSharedPtr<FJsonObject> AnimGraphObj = MakeShareable(new FJsonObject);
		AnimGraphObj->SetStringField(TEXT("graphName"), Data.AnimGraph.GraphName);
		AnimGraphObj->SetBoolField(TEXT("isFastPathCompliant"), Data.AnimGraph.bIsFastPathCompliant);

		TArray<TSharedPtr<FJsonValue>> RootNodes;
		for (const FBS_AnimNodeData& Node : Data.AnimGraph.RootAnimNodes)
		{
			TSharedPtr<FJsonObject> NodeObj = MakeShareable(new FJsonObject);
			NodeObj->SetStringField(TEXT("nodeGuid"), Node.NodeGuid.ToString());
			NodeObj->SetStringField(TEXT("nodeType"), Node.NodeType);
			NodeObj->SetStringField(TEXT("nodeTitle"), Node.NodeTitle);
			NodeObj->SetNumberField(TEXT("posX"), Node.PosX);
			NodeObj->SetNumberField(TEXT("posY"), Node.PosY);
			NodeObj->SetStringField(TEXT("blendSpaceAssetPath"), Node.BlendSpaceAssetPath);
			NodeObj->SetArrayField(TEXT("blendSpaceAxes"), BuildStringArray(Node.BlendSpaceAxes));
			NodeObj->SetArrayField(TEXT("boneLayers"), BuildStringArray(Node.BoneLayers));
			NodeObj->SetStringField(TEXT("blendMaskAssetPath"), Node.BlendMaskAssetPath);
			NodeObj->SetStringField(TEXT("slotName"), Node.SlotName);
			NodeObj->SetStringField(TEXT("slotGroupName"), Node.SlotGroupName);
			NodeObj->SetStringField(TEXT("linkedLayerName"), Node.LinkedLayerName);
			NodeObj->SetStringField(TEXT("linkedLayerAssetPath"), Node.LinkedLayerAssetPath);
			NodeObj->SetNumberField(TEXT("inertializationDuration"), Node.InertializationDuration);
			NodeObj->SetNumberField(TEXT("blendOutDuration"), Node.BlendOutDuration);
			NodeObj->SetBoolField(TEXT("isStateMachine"), Node.bIsStateMachine);
			NodeObj->SetStringField(TEXT("stateMachineGroupName"), Node.StateMachineGroupName);
			NodeObj->SetArrayField(TEXT("pins"), BuildPinArray(Node.Pins));
			AddSortedStringMap(NodeObj, TEXT("allProperties"), Node.AllProperties);
			RootNodes.Add(MakeShareable(new FJsonValueObject(NodeObj)));
		}
		AnimGraphObj->SetArrayField(TEXT("rootNodes"), RootNodes);

		if (Data.AnimGraph.RootPoseConnections.Num() > 0)
		{
			AnimGraphObj->SetArrayField(TEXT("rootPoseConnections"), BuildFlowArray(Data.AnimGraph.RootPoseConnections));
		}

		TArray<TSharedPtr<FJsonValue>> StateMachines;
		for (const FBS_StateMachineData& Machine : Data.AnimGraph.StateMachines)
		{
			TSharedPtr<FJsonObject> MachineObj = MakeShareable(new FJsonObject);
			MachineObj->SetStringField(TEXT("machineName"), Machine.MachineName);
			MachineObj->SetStringField(TEXT("machineGuid"), Machine.MachineGuid.ToString());
			MachineObj->SetStringField(TEXT("entryStateGuid"), Machine.EntryStateGuid.ToString());
			MachineObj->SetBoolField(TEXT("isSubStateMachine"), Machine.bIsSubStateMachine);
			MachineObj->SetStringField(TEXT("parentStateName"), Machine.ParentStateName);
			AddSortedStringMap(MachineObj, TEXT("stateMachineProperties"), Machine.StateMachineProperties);

			TArray<TSharedPtr<FJsonValue>> StatesArray;
			for (const FBS_AnimStateData& State : Machine.States)
			{
				TSharedPtr<FJsonObject> StateObj = MakeShareable(new FJsonObject);
				StateObj->SetStringField(TEXT("stateName"), State.StateName);
				StateObj->SetStringField(TEXT("stateGuid"), State.StateGuid.ToString());
				StateObj->SetStringField(TEXT("stateType"), State.StateType);
				StateObj->SetBoolField(TEXT("isEndState"), State.bIsEndState);
				StateObj->SetNumberField(TEXT("posX"), State.PosX);
				StateObj->SetNumberField(TEXT("posY"), State.PosY);
				AddSortedStringMap(StateObj, TEXT("stateMetadata"), State.StateMetadata);

				TArray<TSharedPtr<FJsonValue>> StateNodesArray;
				for (const FBS_AnimNodeData& Node : State.AnimGraphNodes)
				{
					TSharedPtr<FJsonObject> NodeObj = MakeShareable(new FJsonObject);
					NodeObj->SetStringField(TEXT("nodeGuid"), Node.NodeGuid.ToString());
					NodeObj->SetStringField(TEXT("nodeType"), Node.NodeType);
					NodeObj->SetStringField(TEXT("nodeTitle"), Node.NodeTitle);
					NodeObj->SetNumberField(TEXT("posX"), Node.PosX);
					NodeObj->SetNumberField(TEXT("posY"), Node.PosY);
					NodeObj->SetStringField(TEXT("blendSpaceAssetPath"), Node.BlendSpaceAssetPath);
					NodeObj->SetArrayField(TEXT("blendSpaceAxes"), BuildStringArray(Node.BlendSpaceAxes));
					NodeObj->SetArrayField(TEXT("boneLayers"), BuildStringArray(Node.BoneLayers));
					NodeObj->SetStringField(TEXT("blendMaskAssetPath"), Node.BlendMaskAssetPath);
					NodeObj->SetStringField(TEXT("slotName"), Node.SlotName);
					NodeObj->SetStringField(TEXT("slotGroupName"), Node.SlotGroupName);
					NodeObj->SetStringField(TEXT("linkedLayerName"), Node.LinkedLayerName);
					NodeObj->SetStringField(TEXT("linkedLayerAssetPath"), Node.LinkedLayerAssetPath);
					NodeObj->SetNumberField(TEXT("inertializationDuration"), Node.InertializationDuration);
					NodeObj->SetNumberField(TEXT("blendOutDuration"), Node.BlendOutDuration);
					NodeObj->SetBoolField(TEXT("isStateMachine"), Node.bIsStateMachine);
					NodeObj->SetStringField(TEXT("stateMachineGroupName"), Node.StateMachineGroupName);
					NodeObj->SetArrayField(TEXT("pins"), BuildPinArray(Node.Pins));
					AddSortedStringMap(NodeObj, TEXT("allProperties"), Node.AllProperties);
					StateNodesArray.Add(MakeShareable(new FJsonValueObject(NodeObj)));
				}
				StateObj->SetArrayField(TEXT("animNodes"), StateNodesArray);

				if (State.PoseConnections.Num() > 0)
				{
					StateObj->SetArrayField(TEXT("poseConnections"), BuildFlowArray(State.PoseConnections));
				}

				StatesArray.Add(MakeShareable(new FJsonValueObject(StateObj)));
			}
			MachineObj->SetArrayField(TEXT("states"), StatesArray);

			TArray<TSharedPtr<FJsonValue>> TransitionsArray;
			for (const FBS_AnimTransitionData& Transition : Machine.Transitions)
			{
				TSharedPtr<FJsonObject> TransitionObj = MakeShareable(new FJsonObject);
				TransitionObj->SetStringField(TEXT("transitionGuid"), Transition.TransitionGuid.ToString());
				TransitionObj->SetStringField(TEXT("sourceStateGuid"), Transition.SourceStateGuid.ToString());
				TransitionObj->SetStringField(TEXT("targetStateGuid"), Transition.TargetStateGuid.ToString());
				TransitionObj->SetStringField(TEXT("sourceStateName"), Transition.SourceStateName);
				TransitionObj->SetStringField(TEXT("targetStateName"), Transition.TargetStateName);
				TransitionObj->SetStringField(TEXT("transitionRuleName"), Transition.TransitionRuleName);
				TransitionObj->SetNumberField(TEXT("crossfadeDuration"), Transition.CrossfadeDuration);
				TransitionObj->SetStringField(TEXT("blendMode"), Transition.BlendMode);
				TransitionObj->SetBoolField(TEXT("interruptible"), Transition.bInterruptible);
				TransitionObj->SetNumberField(TEXT("blendOutTriggerTime"), Transition.BlendOutTriggerTime);
				TransitionObj->SetBoolField(TEXT("automaticRuleBasedOnSequencePlayer"), Transition.bAutomaticRuleBasedOnSequencePlayer);
				TransitionObj->SetStringField(TEXT("notifyStateIndex"), Transition.NotifyStateIndex);
				TransitionObj->SetNumberField(TEXT("priority"), Transition.Priority);
				AddSortedStringMap(TransitionObj, TEXT("transitionProperties"), Transition.TransitionProperties);

				TArray<TSharedPtr<FJsonValue>> RuleNodesArray;
				for (const FBS_NodeData& Node : Transition.TransitionRuleNodes)
				{
					TSharedPtr<FJsonObject> NodeObj = MakeShareable(new FJsonObject);
					NodeObj->SetStringField(TEXT("nodeGuid"), Node.NodeGuid.ToString());
					NodeObj->SetStringField(TEXT("nodeType"), Node.NodeType);
					NodeObj->SetStringField(TEXT("title"), Node.Title);
					NodeObj->SetNumberField(TEXT("posX"), Node.PosX);
					NodeObj->SetNumberField(TEXT("posY"), Node.PosY);
					NodeObj->SetArrayField(TEXT("pins"), BuildPinArray(Node.Pins));
					if (!Node.MemberName.IsEmpty())
					{
						TSharedPtr<FJsonObject> MemberObj = MakeShareable(new FJsonObject);
						MemberObj->SetStringField(TEXT("functionName"), Node.MemberName);
						MemberObj->SetStringField(TEXT("parentClass"), Node.MemberParent);
						NodeObj->SetObjectField(TEXT("memberInfo"), MemberObj);
					}
					RuleNodesArray.Add(MakeShareable(new FJsonValueObject(NodeObj)));
				}
				TransitionObj->SetArrayField(TEXT("transitionRuleNodes"), RuleNodesArray);
				TransitionObj->SetArrayField(TEXT("ruleExecutionFlow"), BuildFlowArray(Transition.RuleExecutionFlow));
				TransitionObj->SetArrayField(TEXT("rulePinLinks"), BuildPinLinksArray(Transition.RulePinLinks));

				TransitionsArray.Add(MakeShareable(new FJsonValueObject(TransitionObj)));
			}
			MachineObj->SetArrayField(TEXT("transitions"), TransitionsArray);

			StateMachines.Add(MakeShareable(new FJsonValueObject(MachineObj)));
		}
		AnimGraphObj->SetArrayField(TEXT("stateMachines"), StateMachines);

		TArray<TSharedPtr<FJsonValue>> LinkedLayersArray;
		for (const FBS_LinkedLayerData& Layer : Data.AnimGraph.LinkedLayers)
		{
			TSharedPtr<FJsonObject> LayerObj = MakeShareable(new FJsonObject);
			LayerObj->SetStringField(TEXT("interfaceName"), Layer.InterfaceName);
			LayerObj->SetStringField(TEXT("interfaceAssetPath"), Layer.InterfaceAssetPath);
			LayerObj->SetStringField(TEXT("implementationClassPath"), Layer.ImplementationClassPath);
			LinkedLayersArray.Add(MakeShareable(new FJsonValueObject(LayerObj)));
		}
		AnimGraphObj->SetArrayField(TEXT("linkedLayers"), LinkedLayersArray);
		AddSortedStringMap(AnimGraphObj, TEXT("graphProperties"), Data.AnimGraph.GraphProperties);

		JsonObject->SetObjectField(TEXT("animGraph"), AnimGraphObj);

		// Animation assets
		TArray<TSharedPtr<FJsonValue>> AnimAssets;
		for (const FBS_AnimAssetData& AssetData : Data.AnimationAssets)
		{
			TSharedPtr<FJsonObject> AssetObj = MakeShareable(new FJsonObject);
			AssetObj->SetStringField(TEXT("assetName"), AssetData.AssetName);
			AssetObj->SetStringField(TEXT("assetPath"), AssetData.AssetPath);
			AssetObj->SetStringField(TEXT("assetType"), AssetData.AssetType);
			AssetObj->SetStringField(TEXT("skeletonPath"), AssetData.SkeletonPath);
			AssetObj->SetNumberField(TEXT("length"), AssetData.Length);
			AssetObj->SetNumberField(TEXT("frameRate"), AssetData.FrameRate);
			AssetObj->SetNumberField(TEXT("totalFrames"), AssetData.TotalFrames);

			TArray<TSharedPtr<FJsonValue>> NotifiesArray;
			for (const FBS_AnimNotifyData& Notify : AssetData.Notifies)
			{
				TSharedPtr<FJsonObject> NotifyObj = MakeShareable(new FJsonObject);
				NotifyObj->SetStringField(TEXT("notifyName"), Notify.NotifyName);
				NotifyObj->SetStringField(TEXT("notifyClass"), Notify.NotifyClass);
				NotifyObj->SetBoolField(TEXT("isNotifyState"), Notify.bIsNotifyState);
				NotifyObj->SetNumberField(TEXT("triggerTime"), Notify.TriggerTime);
				NotifyObj->SetNumberField(TEXT("triggerFrame"), Notify.TriggerFrame);
				NotifyObj->SetNumberField(TEXT("duration"), Notify.Duration);
				NotifyObj->SetStringField(TEXT("trackName"), Notify.TrackName);
				NotifyObj->SetNumberField(TEXT("trackIndex"), Notify.TrackIndex);
				AddSortedStringMap(NotifyObj, TEXT("notifyProperties"), Notify.NotifyProperties);
				NotifyObj->SetStringField(TEXT("triggeredEventName"), Notify.TriggeredEventName);
				NotifyObj->SetArrayField(TEXT("eventParameters"), BuildStringArray(Notify.EventParameters));
				NotifiesArray.Add(MakeShareable(new FJsonValueObject(NotifyObj)));
			}
			AssetObj->SetArrayField(TEXT("notifies"), NotifiesArray);

			TArray<TSharedPtr<FJsonValue>> CurvesArray;
			for (const FBS_AnimCurveData& Curve : AssetData.Curves)
			{
				TSharedPtr<FJsonObject> CurveObj = MakeShareable(new FJsonObject);
				CurveObj->SetStringField(TEXT("curveName"), Curve.CurveName);
				CurveObj->SetStringField(TEXT("curveType"), Curve.CurveType);
				CurveObj->SetStringField(TEXT("curveAssetPath"), Curve.CurveAssetPath);
				CurveObj->SetArrayField(TEXT("keyTimes"), BuildFloatArray(Curve.KeyTimes));
				CurveObj->SetArrayField(TEXT("keyValues"), BuildFloatArray(Curve.KeyValues));
				CurveObj->SetArrayField(TEXT("vectorValues"), BuildVectorArray(Curve.VectorValues));
				CurveObj->SetArrayField(TEXT("transformValues"), BuildStringArray(Curve.TransformValues));
				CurveObj->SetStringField(TEXT("interpolationMode"), Curve.InterpolationMode);
				CurveObj->SetStringField(TEXT("axisType"), Curve.AxisType);
				CurveObj->SetArrayField(TEXT("affectedMaterials"), BuildStringArray(Curve.AffectedMaterials));
				CurveObj->SetArrayField(TEXT("affectedMorphTargets"), BuildStringArray(Curve.AffectedMorphTargets));
				CurvesArray.Add(MakeShareable(new FJsonValueObject(CurveObj)));
			}
			AssetObj->SetArrayField(TEXT("curves"), CurvesArray);

			TArray<TSharedPtr<FJsonValue>> SectionsArray;
			for (const FBS_MontageSectionData& Section : AssetData.Sections)
			{
				TSharedPtr<FJsonObject> SectionObj = MakeShareable(new FJsonObject);
				SectionObj->SetStringField(TEXT("sectionName"), Section.SectionName);
				SectionObj->SetNumberField(TEXT("startTime"), Section.StartTime);
				SectionObj->SetNumberField(TEXT("startFrame"), Section.StartFrame);
				SectionObj->SetStringField(TEXT("nextSectionName"), Section.NextSectionName);
				SectionObj->SetNumberField(TEXT("nextSectionTime"), Section.NextSectionTime);
				SectionsArray.Add(MakeShareable(new FJsonValueObject(SectionObj)));
			}
			AssetObj->SetArrayField(TEXT("sections"), SectionsArray);
			AssetObj->SetArrayField(TEXT("slotNames"), BuildStringArray(AssetData.SlotNames));

			TSharedPtr<FJsonObject> BlendObj = MakeShareable(new FJsonObject);
			BlendObj->SetStringField(TEXT("blendSpaceName"), AssetData.BlendSpaceData.BlendSpaceName);
			BlendObj->SetStringField(TEXT("blendSpaceType"), AssetData.BlendSpaceData.BlendSpaceType);
			BlendObj->SetArrayField(TEXT("axisNames"), BuildStringArray(AssetData.BlendSpaceData.AxisNames));
			BlendObj->SetArrayField(TEXT("axisMinValues"), BuildFloatArray(AssetData.BlendSpaceData.AxisMinValues));
			BlendObj->SetArrayField(TEXT("axisMaxValues"), BuildFloatArray(AssetData.BlendSpaceData.AxisMaxValues));

			TArray<TSharedPtr<FJsonValue>> SamplePointsArray;
			for (const FBS_BlendSpaceSamplePoint& Point : AssetData.BlendSpaceData.SamplePoints)
			{
				TSharedPtr<FJsonObject> PointObj = MakeShareable(new FJsonObject);
				PointObj->SetStringField(TEXT("animationName"), Point.AnimationName);
				PointObj->SetArrayField(TEXT("axisValues"), BuildFloatArray(Point.AxisValues));
				SamplePointsArray.Add(MakeShareable(new FJsonValueObject(PointObj)));
			}
			BlendObj->SetArrayField(TEXT("samplePoints"), SamplePointsArray);
			AddSortedStringMap(BlendObj, TEXT("blendSpaceProperties"), AssetData.BlendSpaceData.BlendSpaceProperties);
			AssetObj->SetObjectField(TEXT("blendSpaceData"), BlendObj);

			AddSortedStringMap(AssetObj, TEXT("assetProperties"), AssetData.AssetProperties);
			AddSortedStringMap(AssetObj, TEXT("montageProperties"), AssetData.MontageProperties);

			AnimAssets.Add(MakeShareable(new FJsonValueObject(AssetObj)));
		}
		JsonObject->SetArrayField(TEXT("animationAssets"), AnimAssets);

		// Control rigs
		TArray<TSharedPtr<FJsonValue>> RigArray;
		for (const FBS_ControlRigData& Rig : Data.ControlRigs)
		{
			TSharedPtr<FJsonObject> RigObj = MakeShareable(new FJsonObject);
			RigObj->SetStringField(TEXT("rigName"), Rig.RigName);
			RigObj->SetStringField(TEXT("skeletonPath"), Rig.SkeletonPath);
			RigObj->SetArrayField(TEXT("controls"), BuildStringArray(Rig.ControlNames));
			RigObj->SetArrayField(TEXT("bones"), BuildStringArray(Rig.BoneNames));
			AddSortedStringMap(RigObj, TEXT("controlToBoneMap"), Rig.ControlToBoneMap);

			auto BuildRigNodes = [&](const TArray<FBS_RigVMNodeData>& Nodes)
			{
				TArray<TSharedPtr<FJsonValue>> NodesArray;
				for (const FBS_RigVMNodeData& Node : Nodes)
				{
					TSharedPtr<FJsonObject> NodeObj = MakeShareable(new FJsonObject);
					NodeObj->SetStringField(TEXT("nodeGuid"), Node.NodeGuid.ToString());
					NodeObj->SetStringField(TEXT("nodeType"), Node.NodeType);
					NodeObj->SetStringField(TEXT("nodeTitle"), Node.NodeTitle);
					NodeObj->SetNumberField(TEXT("posX"), Node.PosX);
					NodeObj->SetNumberField(TEXT("posY"), Node.PosY);
					NodeObj->SetArrayField(TEXT("pins"), BuildPinArray(Node.Pins));
					AddSortedStringMap(NodeObj, TEXT("nodeProperties"), Node.NodeProperties);
					NodesArray.Add(MakeShareable(new FJsonValueObject(NodeObj)));
				}
				return NodesArray;
			};

			RigObj->SetArrayField(TEXT("setupEventNodes"), BuildRigNodes(Rig.SetupEventNodes));
			RigObj->SetArrayField(TEXT("forwardSolveNodes"), BuildRigNodes(Rig.ForwardSolveNodes));
			RigObj->SetArrayField(TEXT("backwardSolveNodes"), BuildRigNodes(Rig.BackwardSolveNodes));
			RigObj->SetArrayField(TEXT("rigVmConnections"), BuildPinLinksArray(Rig.RigVMConnections));
			RigObj->SetArrayField(TEXT("enabledFeatures"), BuildStringArray(Rig.EnabledFeatures));
			AddSortedStringMap(RigObj, TEXT("featureSettings"), Rig.FeatureSettings);
			AddSortedStringMap(RigObj, TEXT("rigProperties"), Rig.RigProperties);
			RigArray.Add(MakeShareable(new FJsonValueObject(RigObj)));
		}
		JsonObject->SetArrayField(TEXT("controlRigs"), RigArray);

		// Gameplay tags
		TArray<TSharedPtr<FJsonValue>> GameplayTagArray;
		for (const FBS_GameplayTagStateData& Tag : Data.GameplayTags)
		{
			TSharedPtr<FJsonObject> TagObj = MakeShareable(new FJsonObject);
			TagObj->SetStringField(TEXT("tagName"), Tag.TagName);
			TagObj->SetStringField(TEXT("tagCategory"), Tag.TagCategory);
			TagObj->SetArrayField(TEXT("parentTags"), BuildStringArray(Tag.ParentTags));
			TagObj->SetArrayField(TEXT("associatedStates"), BuildStringArray(Tag.AssociatedAnimStates));
			AddSortedStringMap(TagObj, TEXT("tagMetadata"), Tag.TagMetadata);
			GameplayTagArray.Add(MakeShareable(new FJsonValueObject(TagObj)));
		}
		JsonObject->SetArrayField(TEXT("gameplayTags"), GameplayTagArray);

		// Fast path analysis
		{
			TSharedPtr<FJsonObject> FastPathObj = MakeShareable(new FJsonObject);
			FastPathObj->SetBoolField(TEXT("isFastPathCompliant"), Data.FastPathAnalysis.bIsFastPathCompliant);
			FastPathObj->SetArrayField(TEXT("fastPathViolations"), BuildStringArray(Data.FastPathAnalysis.FastPathViolations));
			FastPathObj->SetArrayField(TEXT("nonFastPathNodes"), BuildStringArray(Data.FastPathAnalysis.NonFastPathNodes));
			AddSortedBoolMap(FastPathObj, TEXT("nodeFastPathStatus"), Data.FastPathAnalysis.NodeFastPathStatus);
			JsonObject->SetObjectField(TEXT("fastPathAnalysis"), FastPathObj);
		}

		// Coverage
		{
			TSharedPtr<FJsonObject> CoverageObj = MakeShareable(new FJsonObject);
			CoverageObj->SetNumberField(TEXT("totalNodeCount"), Data.TotalNodeCount);
			CoverageObj->SetNumberField(TEXT("totalStateNodes"), Data.Coverage.TotalStateNodes);
			CoverageObj->SetNumberField(TEXT("totalTransitions"), Data.Coverage.TotalTransitions);
			CoverageObj->SetNumberField(TEXT("totalNotifies"), Data.Coverage.TotalNotifies);
			CoverageObj->SetNumberField(TEXT("totalCurves"), Data.Coverage.TotalCurves);
			CoverageObj->SetNumberField(TEXT("totalAnimAssets"), Data.Coverage.TotalAnimAssets);
			CoverageObj->SetNumberField(TEXT("totalControlRigs"), Data.Coverage.TotalControlRigs);
			AddSortedIntMap(CoverageObj, TEXT("nodeTypeCounts"), Data.Coverage.NodeTypeCounts);
			CoverageObj->SetArrayField(TEXT("unsupportedNodeTypes"), BuildStringArray(Data.UnsupportedNodeTypes));
			CoverageObj->SetArrayField(TEXT("partiallySupportedNodeTypes"), BuildStringArray(Data.PartiallySupportedNodeTypes));
			JsonObject->SetObjectField(TEXT("coverage"), CoverageObj);
		}
	}
 	
	return JsonObject;
}

bool UBlueprintAnalyzer::SaveBlueprintDataToFile(const FString& JsonData, const FString& FilePath)
{
	return FFileHelper::SaveStringToFile(JsonData, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool UBlueprintAnalyzer::ExportSingleBlueprintToJSON(const FString& BlueprintPath, const FString& OutputDirectory)
{
	UE_LOG(LogTemp, Warning, TEXT("🚀 Starting single Blueprint export for: %s"), *BlueprintPath);
	
	// Load the Blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ Could not load Blueprint: %s"), *BlueprintPath);
		return false;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("✅ Blueprint loaded successfully"));
	
	// Analyze the Blueprint
	FBS_BlueprintData BlueprintData = AnalyzeBlueprint(Blueprint);
	if (BlueprintData.BlueprintName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("❌ Blueprint analysis failed"));
		return false;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("📊 Blueprint analyzed: %d nodes, %d variables, %d functions"), 
		BlueprintData.TotalNodeCount, BlueprintData.Variables.Num(), BlueprintData.Functions.Num());
	
	// Convert to JSON
	FString JsonData = ExportBlueprintDataToJSON(BlueprintData);
	if (JsonData.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("❌ JSON conversion failed"));
		return false;
	}
	
	// Determine output directory
	FString ExportDirectory = OutputDirectory;
	if (ExportDirectory.IsEmpty())
	{
		ExportDirectory = FPaths::ProjectSavedDir() / TEXT("BlueprintExports");
	}

	const bool bLooksLikeEngineRelativeProjectPath = ExportDirectory.StartsWith(TEXT("../")) || ExportDirectory.StartsWith(TEXT("..\\"));
	if (FPaths::IsRelative(ExportDirectory) && !bLooksLikeEngineRelativeProjectPath)
	{
		ExportDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ExportDirectory);
	}
	else
	{
		ExportDirectory = FPaths::ConvertRelativePathToFull(ExportDirectory);
	}

	FPaths::NormalizeDirectoryName(ExportDirectory);
	FPaths::CollapseRelativeDirectories(ExportDirectory);
	
	// Create directory if it doesn't exist
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*ExportDirectory))
	{
		PlatformFile.CreateDirectoryTree(*ExportDirectory);
	}
	
	// Generate filename with timestamp
	const FString ArtifactStem = BuildBlueprintArtifactStem(BlueprintData);
	FString FileName = FString::Printf(TEXT("BP_SLZR_Blueprint_%s_%s.json"),
		*ArtifactStem,
		*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	
	FString FullFilePath = ExportDirectory / FileName;
	
	// Save to file
	if (SaveBlueprintDataToFile(JsonData, FullFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("🎯 Single Blueprint export successful! File: %s"), *FullFilePath);
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("❌ Failed to save file: %s"), *FullFilePath);
		return false;
	}
}

FString UBlueprintAnalyzer::AnalyzeNodeWithConnections(UEdGraphNode* Node)
{
	if (!Node)
	{
		return FString();
	}

	TSharedPtr<FJsonObject> NodeJson = MakeShareable(new FJsonObject);
	
	// Basic node information
	NodeJson->SetStringField(TEXT("nodeType"), Node->GetClass()->GetName());
	NodeJson->SetStringField(TEXT("nodeTitle"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
	NodeJson->SetStringField(TEXT("nodeGuid"), Node->NodeGuid.ToString());
	
	// Node position
	NodeJson->SetNumberField(TEXT("posX"), Node->NodePosX);
	NodeJson->SetNumberField(TEXT("posY"), Node->NodePosY);
	NodeJson->SetNumberField(TEXT("width"), Node->NodeWidth);
	NodeJson->SetNumberField(TEXT("height"), Node->NodeHeight);
	if (!Node->NodeComment.IsEmpty())
	{
		NodeJson->SetStringField(TEXT("comment"), Node->NodeComment);
	}
	NodeJson->SetStringField(TEXT("enabledState"), LexToString(Node->GetDesiredEnabledState()));
	switch (Node->AdvancedPinDisplay)
	{
	case ENodeAdvancedPins::NoPins: NodeJson->SetStringField(TEXT("advancedPinDisplay"), TEXT("NoPins")); break;
	case ENodeAdvancedPins::Shown:  NodeJson->SetStringField(TEXT("advancedPinDisplay"), TEXT("Shown")); break;
	case ENodeAdvancedPins::Hidden: NodeJson->SetStringField(TEXT("advancedPinDisplay"), TEXT("Hidden")); break;
	default: NodeJson->SetStringField(TEXT("advancedPinDisplay"), TEXT("Unknown")); break;
	}
	if (Node->bHasCompilerMessage)
	{
		NodeJson->SetBoolField(TEXT("hasCompilerMessage"), true);
	}
	if (Node->ErrorType != 0)
	{
		NodeJson->SetNumberField(TEXT("errorType"), Node->ErrorType);
	}
	if (!Node->ErrorMsg.IsEmpty())
	{
		NodeJson->SetStringField(TEXT("errorMsg"), Node->ErrorMsg);
	}
#if WITH_EDITORONLY_DATA
	if (Node->bCommentBubblePinned) NodeJson->SetBoolField(TEXT("commentBubblePinned"), true);
	if (Node->bCommentBubbleVisible) NodeJson->SetBoolField(TEXT("commentBubbleVisible"), true);
	if (Node->bCommentBubbleMakeVisible) NodeJson->SetBoolField(TEXT("commentBubbleMakeVisible"), true);
	if (Node->bCanResizeNode) NodeJson->SetBoolField(TEXT("canResize"), true);
	if (Node->GetCanRenameNode()) NodeJson->SetBoolField(TEXT("canRename"), true);
#endif
	
	// Specific node type analysis
	if (UK2Node* K2Node = Cast<UK2Node>(Node))
	{
		// Add K2-specific information
		NodeJson->SetStringField(TEXT("k2NodeClass"), K2Node->GetClass()->GetName());
		
		// Analyze specific K2Node types
		if (UK2Node_CallFunction* CallFuncNode = Cast<UK2Node_CallFunction>(K2Node))
		{
			NodeJson->SetStringField(TEXT("functionName"), CallFuncNode->FunctionReference.GetMemberName().ToString());
			if (CallFuncNode->FunctionReference.GetMemberParentClass())
			{
				NodeJson->SetStringField(TEXT("functionClass"), CallFuncNode->FunctionReference.GetMemberParentClass()->GetName());
			}
		}
		else if (UK2Node_VariableGet* VarGetNode = Cast<UK2Node_VariableGet>(K2Node))
		{
			NodeJson->SetStringField(TEXT("variableName"), VarGetNode->VariableReference.GetMemberName().ToString());
			NodeJson->SetStringField(TEXT("variableType"), TEXT("VariableGet"));
		}
		else if (UK2Node_VariableSet* VarSetNode = Cast<UK2Node_VariableSet>(K2Node))
		{
			NodeJson->SetStringField(TEXT("variableName"), VarSetNode->VariableReference.GetMemberName().ToString());
			NodeJson->SetStringField(TEXT("variableType"), TEXT("VariableSet"));
		}
		else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(K2Node))
		{
			NodeJson->SetStringField(TEXT("eventName"), EventNode->EventReference.GetMemberName().ToString());
			NodeJson->SetStringField(TEXT("eventType"), TEXT("Event"));
		}
		else if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(K2Node))
		{
			NodeJson->SetStringField(TEXT("customEventName"), CustomEventNode->CustomFunctionName.ToString());
			NodeJson->SetStringField(TEXT("eventType"), TEXT("CustomEvent"));
		}
	}

	// Include nodeProperties (best-effort UPROPERTY export)
	{
		TMap<FString, FString> NodeProps;
		ExtractObjectProperties(Node, NodeProps);
		if (NodeProps.Num() > 0)
		{
			AddSortedStringMap(NodeJson, TEXT("nodeProperties"), NodeProps);
		}
	}
	
	// Extract all pins and their connections
	TArray<TSharedPtr<FJsonValue>> PinsArray;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			TSharedPtr<FJsonObject> PinJson = MakeShareable(new FJsonObject);
			
			// Pin basic info
			PinJson->SetStringField(TEXT("pinName"), Pin->PinName.ToString());
			if (Pin->PinId.IsValid())
			{
				PinJson->SetStringField(TEXT("pinId"), Pin->PinId.ToString());
			}
			PinJson->SetStringField(TEXT("pinType"), Pin->PinType.PinCategory.ToString());
			PinJson->SetStringField(TEXT("pinSubType"), Pin->PinType.PinSubCategory.ToString());
			PinJson->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
			PinJson->SetBoolField(TEXT("isExec"), Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
			
			// Pin object type if available
			if (Pin->PinType.PinSubCategoryObject.IsValid())
			{
				PinJson->SetStringField(TEXT("objectType"), Pin->PinType.PinSubCategoryObject->GetName());
				PinJson->SetStringField(TEXT("objectPath"), Pin->PinType.PinSubCategoryObject->GetPathName());
			}

			// Pin flags
			if (Pin->PinType.bIsReference) PinJson->SetBoolField(TEXT("isReference"), true);
			if (Pin->PinType.bIsConst) PinJson->SetBoolField(TEXT("isConst"), true);
			if (Pin->PinType.bIsWeakPointer) PinJson->SetBoolField(TEXT("isWeakPointer"), true);
			if (Pin->PinType.bIsUObjectWrapper) PinJson->SetBoolField(TEXT("isUObjectWrapper"), true);
			switch (Pin->PinType.ContainerType)
			{
			case EPinContainerType::Array: PinJson->SetBoolField(TEXT("isArray"), true); break;
			case EPinContainerType::Map:   PinJson->SetBoolField(TEXT("isMap"), true); break;
			case EPinContainerType::Set:   PinJson->SetBoolField(TEXT("isSet"), true); break;
			default: break;
			}
			if (Pin->SourceIndex != INDEX_NONE) PinJson->SetNumberField(TEXT("sourceIndex"), Pin->SourceIndex);
#if WITH_EDITORONLY_DATA
			if (Pin->ParentPin && Pin->ParentPin->PinId.IsValid())
			{
				PinJson->SetStringField(TEXT("parentPinId"), Pin->ParentPin->PinId.ToString());
			}
			if (Pin->SubPins.Num() > 0)
			{
				TArray<TSharedPtr<FJsonValue>> SubPinArray;
				for (UEdGraphPin* SubPin : Pin->SubPins)
				{
					if (SubPin && SubPin->PinId.IsValid())
					{
						SubPinArray.Add(MakeShareable(new FJsonValueString(SubPin->PinId.ToString())));
					}
				}
				if (SubPinArray.Num() > 0)
				{
					PinJson->SetArrayField(TEXT("subPinIds"), SubPinArray);
				}
			}
#endif
#if WITH_EDITORONLY_DATA
			if (Pin->bHidden) PinJson->SetBoolField(TEXT("hidden"), true);
			if (Pin->bNotConnectable) PinJson->SetBoolField(TEXT("notConnectable"), true);
			if (Pin->bDefaultValueIsReadOnly) PinJson->SetBoolField(TEXT("defaultValueReadOnly"), true);
			if (Pin->bDefaultValueIsIgnored) PinJson->SetBoolField(TEXT("defaultValueIgnored"), true);
			if (Pin->bAdvancedView) PinJson->SetBoolField(TEXT("advanced"), true);
			if (Pin->bDisplayAsMutableRef) PinJson->SetBoolField(TEXT("displayAsMutableRef"), true);
			if (Pin->bAllowFriendlyName) PinJson->SetBoolField(TEXT("allowFriendlyName"), true);
			if (Pin->bOrphanedPin) PinJson->SetBoolField(TEXT("orphaned"), true);
			if (!Pin->PinFriendlyName.IsEmpty()) PinJson->SetStringField(TEXT("friendlyName"), Pin->PinFriendlyName.ToString());
			if (Pin->PersistentGuid.IsValid())
			{
				PinJson->SetStringField(TEXT("persistentGuid"), Pin->PersistentGuid.ToString());
			}
#endif
			if (!Pin->PinToolTip.IsEmpty()) PinJson->SetStringField(TEXT("toolTip"), Pin->PinToolTip);
			
			// Default value
			if (!Pin->DefaultValue.IsEmpty())
			{
				PinJson->SetStringField(TEXT("defaultValue"), Pin->DefaultValue);
			}
			if (!Pin->AutogeneratedDefaultValue.IsEmpty())
			{
				PinJson->SetStringField(TEXT("autogeneratedDefaultValue"), Pin->AutogeneratedDefaultValue);
			}
			if (Pin->DefaultObject)
			{
				PinJson->SetStringField(TEXT("defaultObjectPath"), Pin->DefaultObject->GetPathName());
			}

			// Map/Set value terminal type
			const FEdGraphTerminalType& ValueType = Pin->PinType.PinValueType;
			if (ValueType.TerminalCategory != NAME_None)
			{
				PinJson->SetStringField(TEXT("valueCategory"), ValueType.TerminalCategory.ToString());
			}
			if (ValueType.TerminalSubCategory != NAME_None)
			{
				PinJson->SetStringField(TEXT("valueSubCategory"), ValueType.TerminalSubCategory.ToString());
			}
			if (ValueType.TerminalSubCategoryObject.IsValid())
			{
				PinJson->SetStringField(TEXT("valueObjectType"), ValueType.TerminalSubCategoryObject->GetName());
				PinJson->SetStringField(TEXT("valueObjectPath"), ValueType.TerminalSubCategoryObject->GetPathName());
			}
			if (ValueType.bTerminalIsConst) PinJson->SetBoolField(TEXT("valueIsConst"), true);
			if (ValueType.bTerminalIsWeakPointer) PinJson->SetBoolField(TEXT("valueIsWeakPointer"), true);
			if (ValueType.bTerminalIsUObjectWrapper) PinJson->SetBoolField(TEXT("valueIsUObjectWrapper"), true);
			
			// Pin connections - this is the crucial part!
			TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (LinkedPin && LinkedPin->GetOwningNode())
				{
					TSharedPtr<FJsonObject> ConnectionJson = MakeShareable(new FJsonObject);
					ConnectionJson->SetStringField(TEXT("connectedNodeGuid"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
					ConnectionJson->SetStringField(TEXT("connectedNodeType"), LinkedPin->GetOwningNode()->GetClass()->GetName());
					ConnectionJson->SetStringField(TEXT("connectedPinName"), LinkedPin->PinName.ToString());
					if (LinkedPin->PinId.IsValid())
					{
						ConnectionJson->SetStringField(TEXT("connectedPinId"), LinkedPin->PinId.ToString());
					}
					ConnectionJson->SetStringField(TEXT("connectedPinType"), LinkedPin->PinType.PinCategory.ToString());
					ConnectionJson->SetStringField(TEXT("connectedDirection"), LinkedPin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
					
					ConnectionsArray.Add(MakeShareable(new FJsonValueObject(ConnectionJson)));
				}
			}
			PinJson->SetArrayField(TEXT("connections"), ConnectionsArray);
			
			PinsArray.Add(MakeShareable(new FJsonValueObject(PinJson)));
		}
	}
	NodeJson->SetArrayField(TEXT("pins"), PinsArray);
	
	// Convert to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(NodeJson.ToSharedRef(), Writer);
	
	return OutputString;
}

void UBlueprintAnalyzer::ExtractBlueprintMetadata(UBlueprint* Blueprint, FBS_BlueprintData& OutData)
{
	if (!Blueprint)
	{
		return;
	}

	// Blueprint type classification
	OutData.bIsInterface = Blueprint->BlueprintType == BPTYPE_Interface;
	OutData.bIsMacroLibrary = Blueprint->BlueprintType == BPTYPE_MacroLibrary;
	OutData.bIsFunctionLibrary = Blueprint->BlueprintType == BPTYPE_FunctionLibrary;
	
	// Blueprint description and category
	OutData.BlueprintDescription = Blueprint->BlueprintDescription;
	OutData.BlueprintCategory = Blueprint->BlueprintCategory;
	OutData.BlueprintNamespace = Blueprint->BlueprintNamespace;

	for (const FString& ImportedNamespace : Blueprint->ImportedNamespaces)
	{
		OutData.ImportedNamespaces.Add(ImportedNamespace);
	}
	OutData.ImportedNamespaces.Sort();
	
	// Blueprint tags
	for (const FString& Tag : Blueprint->HideCategories)
	{
		OutData.BlueprintTags.Add(FString::Printf(TEXT("HideCategory: %s"), *Tag));
	}
	OutData.BlueprintTags.Sort();
}

void UBlueprintAnalyzer::ExtractClassParityData(UBlueprint* Blueprint, FBS_BlueprintData& OutData)
{
	OutData.ClassSpecifiers.Reset();
	OutData.ClassConfigName.Reset();
	OutData.ClassConfigFlags.Reset();
	OutData.ClassDefaultValues.Reset();
	OutData.ClassDefaultValueDelta.Reset();

	if (!Blueprint)
	{
		return;
	}

	UClass* BlueprintClass = Blueprint->GeneratedClass ? Blueprint->GeneratedClass : Blueprint->SkeletonGeneratedClass;
	if (!BlueprintClass)
	{
		return;
	}

	auto AddClassSpecifier = [&OutData](const TCHAR* Specifier)
	{
		OutData.ClassSpecifiers.AddUnique(Specifier);
	};

	auto AddClassConfigFlag = [&OutData](const TCHAR* FlagName, bool bValue)
	{
		OutData.ClassConfigFlags.Add(FlagName, bValue);
	};

	const EClassFlags ClassFlags = BlueprintClass->GetClassFlags();
	const bool bIsConfigClass = EnumHasAnyFlags(ClassFlags, CLASS_Config);
	const bool bIsDefaultConfig = EnumHasAnyFlags(ClassFlags, CLASS_DefaultConfig);
	const bool bIsPerObjectConfig = EnumHasAnyFlags(ClassFlags, CLASS_PerObjectConfig);
	const bool bIsGlobalUserConfig = EnumHasAnyFlags(ClassFlags, CLASS_GlobalUserConfig);
	const bool bIsProjectUserConfig = EnumHasAnyFlags(ClassFlags, CLASS_ProjectUserConfig);
	const bool bIsPerPlatformConfig = EnumHasAnyFlags(ClassFlags, CLASS_PerPlatformConfig);
	const bool bConfigDoesNotCheckDefaults = EnumHasAnyFlags(ClassFlags, CLASS_ConfigDoNotCheckDefaults);

	AddClassConfigFlag(TEXT("isConfigClass"), bIsConfigClass);
	AddClassConfigFlag(TEXT("isDefaultConfig"), bIsDefaultConfig);
	AddClassConfigFlag(TEXT("isPerObjectConfig"), bIsPerObjectConfig);
	AddClassConfigFlag(TEXT("isGlobalUserConfig"), bIsGlobalUserConfig);
	AddClassConfigFlag(TEXT("isProjectUserConfig"), bIsProjectUserConfig);
	AddClassConfigFlag(TEXT("isPerPlatformConfig"), bIsPerPlatformConfig);
	AddClassConfigFlag(TEXT("configDoesNotCheckDefaults"), bConfigDoesNotCheckDefaults);

	if (EnumHasAnyFlags(ClassFlags, CLASS_Abstract)) AddClassSpecifier(TEXT("Abstract"));
	if (Blueprint->BlueprintType != BPTYPE_MacroLibrary)
	{
		AddClassSpecifier(TEXT("BlueprintType"));
		AddClassSpecifier(TEXT("Blueprintable"));
	}
	if (BlueprintClass->HasMetaData(TEXT("NotBlueprintable")))
	{
		AddClassSpecifier(TEXT("NotBlueprintable"));
	}
	if (EnumHasAnyFlags(ClassFlags, CLASS_NotPlaceable)) AddClassSpecifier(TEXT("NotPlaceable"));
	if (!EnumHasAnyFlags(ClassFlags, CLASS_NotPlaceable)) AddClassSpecifier(TEXT("Placeable"));
	if (bIsConfigClass) AddClassSpecifier(TEXT("Config"));
	if (bIsDefaultConfig) AddClassSpecifier(TEXT("DefaultConfig"));
	if (bIsPerObjectConfig) AddClassSpecifier(TEXT("PerObjectConfig"));
	if (bIsGlobalUserConfig) AddClassSpecifier(TEXT("GlobalUserConfig"));
	if (bIsProjectUserConfig) AddClassSpecifier(TEXT("ProjectUserConfig"));
	if (bIsPerPlatformConfig) AddClassSpecifier(TEXT("PerPlatformConfig"));
	if (bConfigDoesNotCheckDefaults) AddClassSpecifier(TEXT("ConfigDoNotCheckDefaults"));
	if (EnumHasAnyFlags(ClassFlags, CLASS_Transient)) AddClassSpecifier(TEXT("Transient"));
	if (EnumHasAnyFlags(ClassFlags, CLASS_Deprecated)) AddClassSpecifier(TEXT("Deprecated"));
	if (EnumHasAnyFlags(ClassFlags, CLASS_EditInlineNew)) AddClassSpecifier(TEXT("EditInlineNew"));
	if (EnumHasAnyFlags(ClassFlags, CLASS_Interface)) AddClassSpecifier(TEXT("Interface"));
	if (EnumHasAnyFlags(ClassFlags, CLASS_Const)) AddClassSpecifier(TEXT("Const"));
	if (EnumHasAnyFlags(ClassFlags, CLASS_MinimalAPI)) AddClassSpecifier(TEXT("MinimalAPI"));

	if (Blueprint->bGenerateAbstractClass)
	{
		AddClassSpecifier(TEXT("GeneratedAbstract"));
	}

	OutData.ClassConfigName = BlueprintClass->ClassConfigName.ToString();
	const bool bHasConfigName = !OutData.ClassConfigName.IsEmpty() && !OutData.ClassConfigName.Equals(TEXT("None"), ESearchCase::IgnoreCase);
	AddClassConfigFlag(TEXT("hasConfigName"), bHasConfigName);
	if (bHasConfigName)
	{
		AddClassSpecifier(TEXT("ConfigNamed"));
	}

	OutData.ClassSpecifiers.Sort();

	UObject* ClassDefaultObject = BlueprintClass->GetDefaultObject(false);
	UObject* ParentClassDefaultObject = nullptr;
	if (UClass* SuperClass = BlueprintClass->GetSuperClass())
	{
		ParentClassDefaultObject = SuperClass->GetDefaultObject(false);
	}

	if (!ClassDefaultObject)
	{
		return;
	}

	// CR-024: CDO filtering scope.
	// TFieldIterator<FProperty>(BlueprintClass) with no EFieldIteratorFlags walks only properties
	// declared directly on BlueprintClass (i.e., properties the Blueprint author set), NOT inherited
	// properties from parent C++ classes. This is the correct behaviour for converter fidelity:
	// we only want what differs at this Blueprint level.
	//
	// ClassDefaultValues: all Blueprint-declared, non-deprecated, non-transient, converter-relevant
	//   properties with their current CDO value (for reference).
	// ClassDefaultValueDelta: subset that differs from the immediate parent CDO — this is the
	//   authoritative list for C++ code generation (what UPROPERTY initializers to emit).
	//
	// Properties that are engine-default (e.g. unmodified AActor or UObject fields) are excluded
	// because they are not walked by TFieldIterator on BlueprintClass — they live on parent classes.
	// The CPF_Edit|CPF_BlueprintVisible|CPF_Config|CPF_SaveGame|CPF_Net flag filter further
	// restricts to properties that matter for a C++ converter.
	for (TFieldIterator<FProperty> PropIt(BlueprintClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property || Property->HasAnyPropertyFlags(CPF_Deprecated))
		{
			continue;
		}

		if (Property->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

		const bool bRelevantForClassDefaults = Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible | CPF_Config | CPF_SaveGame | CPF_Net);
		if (!bRelevantForClassDefaults)
		{
			continue;
		}

		const FString PropertyName = Property->GetName();
		const FString CurrentValue = GetPropertyValueAsString(ClassDefaultObject, Property);
		OutData.ClassDefaultValues.Add(PropertyName, CurrentValue);

		if (!ParentClassDefaultObject)
		{
			OutData.ClassDefaultValueDelta.Add(PropertyName, CurrentValue);
			continue;
		}

		FProperty* ParentProperty = FindFProperty<FProperty>(ParentClassDefaultObject->GetClass(), Property->GetFName());
		if (!ParentProperty)
		{
			// CR-024: No parent property means this property is new at this Blueprint level — always include in delta.
			OutData.ClassDefaultValueDelta.Add(PropertyName, CurrentValue);
			continue;
		}

		const FString ParentValue = GetPropertyValueAsString(ParentClassDefaultObject, ParentProperty);
		if (CurrentValue != ParentValue)
		{
			// CR-024: Only emit delta entries that actually differ from the parent CDO value.
			// This ensures the converter only generates initializer code for properties the Blueprint author changed.
			OutData.ClassDefaultValueDelta.Add(PropertyName, CurrentValue);
		}
	}
}

void UBlueprintAnalyzer::ExtractUserTypeSchemas(UBlueprint* Blueprint, FBS_BlueprintData& OutData)
{
	OutData.UserDefinedStructSchemas.Reset();
	OutData.UserDefinedEnumSchemas.Reset();

	if (!Blueprint)
	{
		return;
	}

	TSet<FString> CandidateStructPaths;
	TSet<FString> CandidateEnumPaths;

	auto ResolveTypeObject = [](const FString& RawPath) -> UObject*
	{
		if (RawPath.IsEmpty())
		{
			return nullptr;
		}

		const FString NormalizedPath = NormalizeObjectPath(RawPath, false);
		if (NormalizedPath.IsEmpty())
		{
			return nullptr;
		}

		if (UObject* Loaded = TryLoadBestCandidate(NormalizedPath))
		{
			return Loaded;
		}

		if (NormalizedPath.StartsWith(TEXT("/Script/")))
		{
			if (UObject* ScriptObject = FindObject<UObject>(nullptr, *NormalizedPath))
			{
				return ScriptObject;
			}
		}

		return nullptr;
	};

	auto AddTypeCandidate = [&](const FString& RawPath)
	{
		if (UObject* TypeObject = ResolveTypeObject(RawPath))
		{
			if (UScriptStruct* StructObject = Cast<UScriptStruct>(TypeObject))
			{
				if (StructObject->IsA<UUserDefinedStruct>() || StructObject->GetPathName().StartsWith(TEXT("/Game/")))
				{
					CandidateStructPaths.Add(StructObject->GetPathName());
				}
			}
			else if (UEnum* EnumObject = Cast<UEnum>(TypeObject))
			{
				if (EnumObject->IsA<UUserDefinedEnum>() || EnumObject->GetPathName().StartsWith(TEXT("/Game/")))
				{
					CandidateEnumPaths.Add(EnumObject->GetPathName());
				}
			}
		}
	};

	auto AddTypeCandidatesFromText = [&](const FString& Text)
	{
		TSet<FString> DiscoveredPaths;
		CollectAssetPathsFromText(Text, DiscoveredPaths);
		for (const FString& DiscoveredPath : DiscoveredPaths)
		{
			AddTypeCandidate(DiscoveredPath);
		}
	};

	for (const FBS_VariableInfo& VarInfo : OutData.DetailedVariables)
	{
		AddTypeCandidate(VarInfo.TypeObjectPath);
		AddTypeCandidatesFromText(VarInfo.VariableType);
	}

	for (const FBS_FunctionInfo& FuncInfo : OutData.DetailedFunctions)
	{
		AddTypeCandidate(FuncInfo.ReturnTypeObjectPath);
		for (const FString& Param : FuncInfo.InputParameters)
		{
			AddTypeCandidatesFromText(Param);
		}
		for (const FString& Param : FuncInfo.OutputParameters)
		{
			AddTypeCandidatesFromText(Param);
		}
		for (const FString& LocalVar : FuncInfo.LocalVariables)
		{
			AddTypeCandidatesFromText(LocalVar);
		}
	}

	for (const FBS_FunctionInfo& DelegateInfo : OutData.DelegateSignatures)
	{
		AddTypeCandidate(DelegateInfo.ReturnTypeObjectPath);
		for (const FString& Param : DelegateInfo.InputParameters)
		{
			AddTypeCandidatesFromText(Param);
		}
		for (const FString& Param : DelegateInfo.OutputParameters)
		{
			AddTypeCandidatesFromText(Param);
		}
	}

	for (const FBS_GraphData_Ext& Graph : OutData.StructuredGraphsExt)
	{
		for (const FBS_NodeData& Node : Graph.Nodes)
		{
			for (const FBS_PinData& Pin : Node.Pins)
			{
				AddTypeCandidate(Pin.ObjectPath);
				AddTypeCandidate(Pin.ValueObjectPath);
				AddTypeCandidatesFromText(Pin.ObjectType);
				AddTypeCandidatesFromText(Pin.ValueObjectType);
			}
		}
	}

	TArray<FString> StructPaths = CandidateStructPaths.Array();
	StructPaths.Sort();
	for (const FString& StructPath : StructPaths)
	{
		UScriptStruct* StructObject = Cast<UScriptStruct>(ResolveTypeObject(StructPath));
		if (!StructObject)
		{
			continue;
		}

		if (!StructObject->IsA<UUserDefinedStruct>() && !StructObject->GetPathName().StartsWith(TEXT("/Game/")))
		{
			continue;
		}

		FBS_UserDefinedStructSchema StructSchema;
		StructSchema.StructName = StructObject->GetName();
		StructSchema.StructPath = StructObject->GetPathName();

		// CR-028: read default values from the struct's CDO.
		// UUserDefinedStruct::GetDefaultInstance() returns a pointer to the struct's default memory
		// (the Blueprint-authored default values for each field).
		// We copy it into a locally-owned buffer so we can safely call ExportText_InContainer on it.
		void* StructDefaultMem = nullptr;
		if (UUserDefinedStruct* UDS = Cast<UUserDefinedStruct>(StructObject))
		{
			const int32 StructSize = UDS->GetStructureSize();
			if (StructSize > 0)
			{
				const uint8* DefaultMem = UDS->GetDefaultInstance();
				if (DefaultMem)
				{
					StructDefaultMem = FMemory::Malloc(StructSize, UDS->GetMinAlignment());
					// Initialize to zero first so ExportText has a valid 'Default' for delta-export
					UDS->InitializeStructIgnoreDefaults(StructDefaultMem);
					// Then copy the actual defaults on top
					UDS->CopyScriptStruct(StructDefaultMem, DefaultMem);
				}
			}
		}

		for (TFieldIterator<FProperty> PropIt(StructObject); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property || Property->HasAnyPropertyFlags(CPF_Deprecated))
			{
				continue;
			}

			FBS_UserDefinedStructFieldSchema FieldSchema;
			FieldSchema.Name = Property->GetName();
			FieldSchema.Type = DescribePropertyType(Property);
			FieldSchema.TypeObjectPath = GetPropertyObjectTypePath(Property);
			FieldSchema.bIsArray = CastField<FArrayProperty>(Property) != nullptr;
			FieldSchema.bIsMap = CastField<FMapProperty>(Property) != nullptr;
			FieldSchema.bIsSet = CastField<FSetProperty>(Property) != nullptr;

			// CR-028: per-field default value from the struct's default memory.
			// Pass nullptr as the Delta parameter so ExportText always writes the value,
			// not just the delta vs. some reference object.
			if (StructDefaultMem)
			{
				FString DefaultExport;
				Property->ExportText_InContainer(0, DefaultExport, StructDefaultMem, nullptr, nullptr, PPF_None);
				if (!DefaultExport.IsEmpty() && DefaultExport != TEXT("()") && DefaultExport != TEXT("(0)"))
				{
					FieldSchema.DefaultValue = DefaultExport;
				}
			}

			// CR-028: display name and tooltip from UHT metadata (FriendlyName, ToolTip)
			if (Property->HasMetaData(TEXT("FriendlyName")))
			{
				FieldSchema.DisplayName = Property->GetMetaData(TEXT("FriendlyName"));
			}
			if (FieldSchema.DisplayName.IsEmpty() && Property->HasMetaData(TEXT("DisplayName")))
			{
				FieldSchema.DisplayName = Property->GetMetaData(TEXT("DisplayName"));
			}
			if (Property->HasMetaData(TEXT("ToolTip")))
			{
				FieldSchema.Tooltip = Property->GetMetaData(TEXT("ToolTip"));
			}

			StructSchema.Fields.Add(MoveTemp(FieldSchema));
		}

		if (StructDefaultMem)
		{
			if (UUserDefinedStruct* UDS = Cast<UUserDefinedStruct>(StructObject))
			{
				UDS->DestroyStruct(StructDefaultMem);
			}
			FMemory::Free(StructDefaultMem);
			StructDefaultMem = nullptr;
		}

		StructSchema.Fields.Sort([](const FBS_UserDefinedStructFieldSchema& A, const FBS_UserDefinedStructFieldSchema& B)
		{
			return A.Name < B.Name;
		});

		OutData.UserDefinedStructSchemas.Add(MoveTemp(StructSchema));
	}

	TArray<FString> EnumPaths = CandidateEnumPaths.Array();
	EnumPaths.Sort();
	for (const FString& EnumPath : EnumPaths)
	{
		UEnum* EnumObject = Cast<UEnum>(ResolveTypeObject(EnumPath));
		if (!EnumObject)
		{
			continue;
		}

		if (!EnumObject->IsA<UUserDefinedEnum>() && !EnumObject->GetPathName().StartsWith(TEXT("/Game/")))
		{
			continue;
		}

		FBS_UserDefinedEnumSchema EnumSchema;
		EnumSchema.EnumName = EnumObject->GetName();
		EnumSchema.EnumPath = EnumObject->GetPathName();

		const int32 NumEnums = EnumObject->NumEnums();
		for (int32 EnumIndex = 0; EnumIndex < NumEnums; ++EnumIndex)
		{
			const FString EnumeratorName = EnumObject->GetNameStringByIndex(EnumIndex);
			if (EnumeratorName.IsEmpty())
			{
				continue;
			}

			if (EnumIndex == NumEnums - 1 && EnumeratorName.EndsWith(TEXT("_MAX")))
			{
				continue;
			}

			EnumSchema.Enumerators.Add(EnumeratorName);

			// CR-028: per-enumerator metadata — authored display name and tooltip.
			// GetAuthoredNameStringByIndex returns the Blueprint-visible display name for UUserDefinedEnum values.
			// The ToolTip metadata key stores the per-value tooltip set in the UE enum editor.
			FBS_UserDefinedEnumValueMeta ValueMeta;
			ValueMeta.Name = EnumeratorName;
			if (UUserDefinedEnum* UDE = Cast<UUserDefinedEnum>(EnumObject))
			{
				const FString AuthoredName = UDE->GetAuthoredNameStringByIndex(EnumIndex);
				if (!AuthoredName.IsEmpty() && AuthoredName != EnumeratorName)
				{
					ValueMeta.DisplayName = AuthoredName;
				}
			}
			if (ValueMeta.DisplayName.IsEmpty())
			{
				// Fallback: try DisplayName metadata on the enum field
				const FString DisplayNameMeta = EnumObject->GetMetaData(TEXT("DisplayName"), EnumIndex);
				if (!DisplayNameMeta.IsEmpty())
				{
					ValueMeta.DisplayName = DisplayNameMeta;
				}
			}
			// Per-index ToolTip metadata
			const FString ToolTipMeta = EnumObject->GetMetaData(TEXT("ToolTip"), EnumIndex);
			if (!ToolTipMeta.IsEmpty())
			{
				ValueMeta.Tooltip = ToolTipMeta;
			}
			EnumSchema.EnumeratorMetadata.Add(MoveTemp(ValueMeta));
		}

		EnumSchema.Enumerators.Sort();
		// Note: EnumeratorMetadata is in original index order, not sorted, so it stays in sync
		// with the enum value order. Consumers should use Enumerators[] for sorted lookup and
		// EnumeratorMetadata[] for ordered metadata.
		OutData.UserDefinedEnumSchemas.Add(MoveTemp(EnumSchema));
	}

	OutData.UserDefinedStructSchemas.Sort([](const FBS_UserDefinedStructSchema& A, const FBS_UserDefinedStructSchema& B)
	{
		return A.StructPath < B.StructPath;
	});

	OutData.UserDefinedEnumSchemas.Sort([](const FBS_UserDefinedEnumSchema& A, const FBS_UserDefinedEnumSchema& B)
	{
		return A.EnumPath < B.EnumPath;
	});
}

void UBlueprintAnalyzer::ExtractTimelineData(UBlueprint* Blueprint, FBS_BlueprintData& OutData)
{
	if (!Blueprint)
	{
		return;
	}

	OutData.Timelines.Reset();

	for (UTimelineTemplate* Timeline : Blueprint->Timelines)
	{
		if (!Timeline)
		{
			continue;
		}

		FBS_TimelineData TimelineData;
		TimelineData.TimelineName = Timeline->GetVariableName().ToString();
		TimelineData.TimelineLength = Timeline->TimelineLength;
		switch (Timeline->LengthMode)
		{
		case TL_TimelineLength:
			TimelineData.LengthMode = TEXT("TimelineLength");
			break;
		case TL_LastKeyFrame:
			TimelineData.LengthMode = TEXT("LastKeyFrame");
			break;
		default:
			TimelineData.LengthMode = TEXT("Unknown");
			break;
		}
		TimelineData.bAutoPlay = Timeline->bAutoPlay;
		TimelineData.bLoop = Timeline->bLoop;
		TimelineData.bReplicated = Timeline->bReplicated;
		TimelineData.bIgnoreTimeDilation = Timeline->bIgnoreTimeDilation;
		TimelineData.UpdateFunctionName = Timeline->GetUpdateFunctionName().ToString();
		TimelineData.FinishedFunctionName = Timeline->GetFinishedFunctionName().ToString();

		for (const FTTEventTrack& Track : Timeline->EventTracks)
		{
			FBS_TimelineTrackData TrackData;
			TrackData.TrackName = Track.GetTrackName().ToString();
			TrackData.TrackType = TEXT("Event");
			TrackData.FunctionName = Track.GetFunctionName().ToString();
			if (Track.CurveKeys)
			{
				TrackData.CurvePath = Track.CurveKeys->GetPathName();
				TrackData.KeyCount = Track.CurveKeys->FloatCurve.GetNumKeys();
			}
			TimelineData.Tracks.Add(MoveTemp(TrackData));
		}

		for (const FTTFloatTrack& Track : Timeline->FloatTracks)
		{
			FBS_TimelineTrackData TrackData;
			TrackData.TrackName = Track.GetTrackName().ToString();
			TrackData.TrackType = TEXT("Float");
			TrackData.PropertyName = Track.GetPropertyName().ToString();
			if (Track.CurveFloat)
			{
				TrackData.CurvePath = Track.CurveFloat->GetPathName();
				TrackData.KeyCount = Track.CurveFloat->FloatCurve.GetNumKeys();
			}
			TimelineData.Tracks.Add(MoveTemp(TrackData));
		}

		for (const FTTVectorTrack& Track : Timeline->VectorTracks)
		{
			FBS_TimelineTrackData TrackData;
			TrackData.TrackName = Track.GetTrackName().ToString();
			TrackData.TrackType = TEXT("Vector");
			TrackData.PropertyName = Track.GetPropertyName().ToString();
			if (Track.CurveVector)
			{
				TrackData.CurvePath = Track.CurveVector->GetPathName();
				TrackData.KeyCount =
					Track.CurveVector->FloatCurves[0].GetNumKeys() +
					Track.CurveVector->FloatCurves[1].GetNumKeys() +
					Track.CurveVector->FloatCurves[2].GetNumKeys();
			}
			TimelineData.Tracks.Add(MoveTemp(TrackData));
		}

		for (const FTTLinearColorTrack& Track : Timeline->LinearColorTracks)
		{
			FBS_TimelineTrackData TrackData;
			TrackData.TrackName = Track.GetTrackName().ToString();
			TrackData.TrackType = TEXT("LinearColor");
			TrackData.PropertyName = Track.GetPropertyName().ToString();
			if (Track.CurveLinearColor)
			{
				TrackData.CurvePath = Track.CurveLinearColor->GetPathName();
				TrackData.KeyCount =
					Track.CurveLinearColor->FloatCurves[0].GetNumKeys() +
					Track.CurveLinearColor->FloatCurves[1].GetNumKeys() +
					Track.CurveLinearColor->FloatCurves[2].GetNumKeys() +
					Track.CurveLinearColor->FloatCurves[3].GetNumKeys();
			}
			TimelineData.Tracks.Add(MoveTemp(TrackData));
		}

		TimelineData.Tracks.Sort([](const FBS_TimelineTrackData& A, const FBS_TimelineTrackData& B)
		{
			if (A.TrackType == B.TrackType)
			{
				return A.TrackName < B.TrackName;
			}
			return A.TrackType < B.TrackType;
		});

		OutData.Timelines.Add(MoveTemp(TimelineData));
	}

	OutData.Timelines.Sort([](const FBS_TimelineData& A, const FBS_TimelineData& B)
	{
		return A.TimelineName < B.TimelineName;
	});
}

void UBlueprintAnalyzer::ExtractWidgetBlueprintData(UBlueprint* Blueprint, FBS_BlueprintData& OutData)
{
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint);
	if (!WidgetBlueprint)
	{
		return;
	}

	FBS_WidgetBlueprintData& WidgetData = OutData.WidgetBlueprint;
	WidgetData.bIsWidgetBlueprint = true;

	UWidgetBlueprintGeneratedClass* GeneratedWidgetClass =
		Cast<UWidgetBlueprintGeneratedClass>(Blueprint->GeneratedClass);
	UWidgetTree* WidgetTree = nullptr;
#if WITH_EDITORONLY_DATA
	WidgetTree = WidgetBlueprint->WidgetTree;
	WidgetData.bHasSourceWidgetTree = WidgetTree != nullptr;
#endif
	if (!WidgetTree && GeneratedWidgetClass)
	{
		WidgetTree = GeneratedWidgetClass->GetWidgetTreeArchetype();
	}

	if (GeneratedWidgetClass)
	{
		for (const FName& NamedSlot : GeneratedWidgetClass->NamedSlots)
		{
			WidgetData.GeneratedNamedSlots.Add(NamedSlot.ToString());
		}
		WidgetData.GeneratedNamedSlots.Sort();
	}

	if (WidgetTree)
	{
		WidgetData.WidgetTreePath = WidgetTree->GetPathName();
		WidgetData.RootWidgetName = WidgetTree->RootWidget
			? WidgetTree->RootWidget->GetName()
			: FString();

		TMap<const UWidget*, FString> NamedSlotByContent;
		TArray<FName> NamedSlotNames;
		WidgetTree->GetSlotNames(NamedSlotNames);
		NamedSlotNames.Sort([](const FName& A, const FName& B)
		{
			return A.LexicalLess(B);
		});
		for (const FName& SlotName : NamedSlotNames)
		{
			UWidget* Content = WidgetTree->GetContentForSlot(SlotName);
			const FString ContentPath = Content ? Content->GetPathName() : TEXT("None");
			WidgetData.NamedSlotBindings.Add(
				FString::Printf(TEXT("%s=%s"), *SlotName.ToString(), *ContentPath));
			if (Content)
			{
				NamedSlotByContent.Add(Content, SlotName.ToString());
			}
		}

		WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (!Widget)
			{
				return;
			}

			FBS_WidgetTemplateData TemplateData;
			TemplateData.WidgetName = Widget->GetName();
			TemplateData.WidgetPath = Widget->GetPathName();
			TemplateData.WidgetClassPath = Widget->GetClass()->GetPathName();
			if (UPanelWidget* Parent = Widget->GetParent())
			{
				TemplateData.ParentWidgetName = Parent->GetName();
				TemplateData.ParentWidgetPath = Parent->GetPathName();
				TemplateData.ChildIndex = Parent->GetChildIndex(Widget);
			}

			for (UWidget* Ancestor = Widget->GetParent(); Ancestor; Ancestor = Ancestor->GetParent())
			{
				TemplateData.Depth++;
			}

			if (Widget->Slot)
			{
				TemplateData.SlotPath = Widget->Slot->GetPathName();
				TemplateData.SlotClassPath = Widget->Slot->GetClass()->GetPathName();
				ExtractObjectProperties(Widget->Slot, TemplateData.SlotProperties);
			}
			if (const FString* NamedSlot = NamedSlotByContent.Find(Widget))
			{
				TemplateData.NamedSlotName = *NamedSlot;
			}

			ExtractObjectProperties(Widget, TemplateData.WidgetProperties);
			TSet<FString> ReferencedPaths;
			CollectPropertyAssetPaths(TemplateData.WidgetProperties, ReferencedPaths);
			CollectPropertyAssetPaths(TemplateData.SlotProperties, ReferencedPaths);
			TemplateData.ReferencedObjectPaths = ReferencedPaths.Array();
			TemplateData.ReferencedObjectPaths.Sort();
			for (const FString& ReferencedPath : TemplateData.ReferencedObjectPaths)
			{
				OutData.AssetReferences.AddUnique(ReferencedPath);
			}
			WidgetData.Widgets.Add(MoveTemp(TemplateData));
		});

		WidgetData.Widgets.Sort([](const FBS_WidgetTemplateData& A, const FBS_WidgetTemplateData& B)
		{
			if (A.Depth != B.Depth)
			{
				return A.Depth < B.Depth;
			}
			return A.WidgetPath < B.WidgetPath;
		});
	}

#if WITH_EDITORONLY_DATA
	for (const FDelegateEditorBinding& Binding : WidgetBlueprint->Bindings)
	{
		FBS_WidgetBindingData BindingData;
		BindingData.ObjectName = Binding.ObjectName;
		BindingData.PropertyName = Binding.PropertyName.ToString();
		BindingData.FunctionName = Binding.FunctionName.ToString();
		BindingData.SourceProperty = Binding.SourceProperty.ToString();
		BindingData.SourcePath = Binding.SourcePath.GetDisplayText().ToString();
		BindingData.MemberGuid = Binding.MemberGuid.ToString();
		BindingData.BindingKind = Binding.Kind == EBindingKind::Function
			? TEXT("Function")
			: TEXT("Property");
		WidgetData.Bindings.Add(MoveTemp(BindingData));
	}

	for (UWidgetAnimation* Animation : WidgetBlueprint->Animations)
	{
		if (!Animation)
		{
			continue;
		}

		FBS_WidgetAnimationData AnimationData;
		AnimationData.AnimationName = Animation->GetName();
		AnimationData.AnimationPath = Animation->GetPathName();
		ExtractObjectProperties(Animation, AnimationData.AnimationProperties);

		for (const FWidgetAnimationBinding& Binding : Animation->GetBindings())
		{
			FBS_WidgetAnimationBindingData BindingData;
			BindingData.WidgetName = Binding.WidgetName.ToString();
			BindingData.SlotWidgetName = Binding.SlotWidgetName.ToString();
			BindingData.AnimationGuid = Binding.AnimationGuid.ToString();
			BindingData.bIsRootWidget = Binding.bIsRootWidget;
			AnimationData.WidgetBindings.Add(MoveTemp(BindingData));
		}

		if (UMovieScene* MovieScene = Animation->GetMovieScene())
		{
			AnimationData.MovieScenePath = MovieScene->GetPathName();
			AnimationData.PlaybackRange = FormatFrameRange(MovieScene->GetPlaybackRange());
			AnimationData.TickResolution = FormatFrameRate(MovieScene->GetTickResolution());
			AnimationData.DisplayRate = FormatFrameRate(MovieScene->GetDisplayRate());
			ExtractObjectProperties(MovieScene, AnimationData.MovieSceneProperties);

			for (UMovieSceneTrack* Track : MovieScene->GetTracks())
			{
				if (Track)
				{
					AnimationData.MasterTracks.Add(FString::Printf(TEXT("%s|%s"),
						*Track->GetClass()->GetPathName(), *Track->GetPathName()));
				}
			}
			const UMovieScene* ConstMovieScene = MovieScene;
			for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
			{
				TArray<FString> Tracks;
				for (UMovieSceneTrack* Track : Binding.GetTracks())
				{
					if (Track)
					{
						Tracks.Add(FString::Printf(TEXT("%s@%s"),
							*Track->GetClass()->GetPathName(), *Track->GetPathName()));
					}
				}
				AnimationData.MovieSceneBindings.Add(FString::Printf(TEXT("%s|%s"),
					*Binding.GetObjectGuid().ToString(),
					*FString::Join(Tracks, TEXT(";"))));
			}
		}

		AnimationData.MasterTracks.Sort();
		AnimationData.MovieSceneBindings.Sort();
		TSet<FString> ReferencedPaths;
		CollectPropertyAssetPaths(AnimationData.AnimationProperties, ReferencedPaths);
		CollectPropertyAssetPaths(AnimationData.MovieSceneProperties, ReferencedPaths);
		AnimationData.ReferencedObjectPaths = ReferencedPaths.Array();
		AnimationData.ReferencedObjectPaths.Sort();
		for (const FString& ReferencedPath : AnimationData.ReferencedObjectPaths)
		{
			OutData.AssetReferences.AddUnique(ReferencedPath);
		}
		WidgetData.Animations.Add(MoveTemp(AnimationData));
	}
#endif

	WidgetData.Bindings.Sort([](const FBS_WidgetBindingData& A, const FBS_WidgetBindingData& B)
	{
		if (A.ObjectName != B.ObjectName)
		{
			return A.ObjectName < B.ObjectName;
		}
		return A.PropertyName < B.PropertyName;
	});
	WidgetData.Animations.Sort([](const FBS_WidgetAnimationData& A, const FBS_WidgetAnimationData& B)
	{
		return A.AnimationPath < B.AnimationPath;
	});
	OutData.AssetReferences.Sort();
}

TArray<FBS_VariableInfo> UBlueprintAnalyzer::ExtractDetailedVariables(UBlueprint* Blueprint)
{
	TArray<FBS_VariableInfo> DetailedVars;
	
	if (!Blueprint)
	{
		return DetailedVars;
	}
	
	// Extract Blueprint variables with full metadata
	for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
	{
		FBS_VariableInfo VarInfo;
		
		VarInfo.VariableName = VarDesc.VarName.ToString();
		VarInfo.VariableType = DescribePinTypeDetailed(VarDesc.VarType);
		VarInfo.TypeCategory = VarDesc.VarType.PinCategory.ToString();
		VarInfo.TypeSubCategory = VarDesc.VarType.PinSubCategory.ToString();
		VarInfo.bIsArray = VarDesc.VarType.ContainerType == EPinContainerType::Array;
		VarInfo.bIsMap = VarDesc.VarType.ContainerType == EPinContainerType::Map;
		VarInfo.bIsSet = VarDesc.VarType.ContainerType == EPinContainerType::Set;
		
		if (VarDesc.VarType.PinSubCategoryObject.IsValid())
		{
			VarInfo.TypeObjectPath = VarDesc.VarType.PinSubCategoryObject->GetPathName();
		}
		
		// Default value
		VarInfo.DefaultValue = ResolveBlueprintVariableDefaultValue(Blueprint, VarDesc);
		
		// Category and tooltip
		VarInfo.Category = VarDesc.Category.ToString();
		VarInfo.Tooltip = VarDesc.FriendlyName;
		
		// Variable properties
		VarInfo.bIsPublic = (VarDesc.PropertyFlags & CPF_DisableEditOnInstance) == 0;
		VarInfo.bIsReplicated = (VarDesc.PropertyFlags & CPF_Net) != 0;
		VarInfo.DescriptorPropertyFlags = FormatPropertyFlags(
			static_cast<EPropertyFlags>(VarDesc.PropertyFlags));
		VarInfo.bDescriptorHasNetFlag = (VarDesc.PropertyFlags & CPF_Net) != 0;
		VarInfo.bDescriptorHasRepNotifyFlag = (VarDesc.PropertyFlags & CPF_RepNotify) != 0;
		VarInfo.DescriptorReplicationConditionValue = static_cast<int32>(VarDesc.ReplicationCondition);
		VarInfo.ReplicationConditionSource = TEXT("FBPVariableDescription.ReplicationCondition");

		UClass* PropertyOwnerClass = Blueprint->GeneratedClass
			? Blueprint->GeneratedClass
			: Blueprint->SkeletonGeneratedClass;
		FProperty* CompiledProperty = PropertyOwnerClass
			? FindFProperty<FProperty>(PropertyOwnerClass, VarDesc.VarName)
			: nullptr;
		if (CompiledProperty)
		{
			VarInfo.bCompiledPropertyFound = true;
			VarInfo.CompiledPropertyPath = CompiledProperty->GetPathName();
			VarInfo.CompiledPropertyFlags = FormatPropertyFlags(CompiledProperty->GetPropertyFlags());
			VarInfo.bCompiledHasNetFlag = CompiledProperty->HasAnyPropertyFlags(CPF_Net);
			VarInfo.bCompiledHasRepNotifyFlag = CompiledProperty->HasAnyPropertyFlags(CPF_RepNotify);
			VarInfo.CompiledRepNotifyFunction = CompiledProperty->RepNotifyFunc.ToString();
			VarInfo.CompiledRepIndex = CompiledProperty->RepIndex;
			VarInfo.bIsReplicated = VarInfo.bCompiledHasNetFlag;
			VarInfo.ReplicationDecisionSource = TEXT("FProperty.CPF_Net");

			const bool bNotifyNamesAgree =
				VarDesc.RepNotifyFunc.IsNone() ||
				CompiledProperty->RepNotifyFunc.IsNone() ||
				VarDesc.RepNotifyFunc == CompiledProperty->RepNotifyFunc;
			VarInfo.bReplicationMetadataConsistent =
				VarInfo.bDescriptorHasNetFlag == VarInfo.bCompiledHasNetFlag &&
				VarInfo.bDescriptorHasRepNotifyFlag == VarInfo.bCompiledHasRepNotifyFlag &&
				bNotifyNamesAgree;
		}
		else
		{
			VarInfo.ReplicationDecisionSource = TEXT("FBPVariableDescription.PropertyFlags.CPF_Net");
		}
		VarInfo.bIsExposedOnSpawn = (VarDesc.PropertyFlags & CPF_ExposeOnSpawn) != 0;
		VarInfo.bIsEditable = (VarDesc.PropertyFlags & CPF_Edit) != 0;
		
		// Replication condition
		VarInfo.RepNotifyFunction = VarDesc.RepNotifyFunc.ToString();
		VarInfo.RepCondition = DescribeLifetimeCondition(VarDesc.ReplicationCondition);

		TSet<FString> Specifiers;
		// CR-029: edit/visible specifiers — prefer the most specific one.
		// Priority: EditDefaultsOnly/EditInstanceOnly > EditAnywhere > VisibleDefaultsOnly/VisibleInstanceOnly > VisibleAnywhere.
		const bool bEdit = (VarDesc.PropertyFlags & CPF_Edit) != 0;
		const bool bDisableOnInstance = (VarDesc.PropertyFlags & CPF_DisableEditOnInstance) != 0;
		const bool bDisableOnTemplate = (VarDesc.PropertyFlags & CPF_DisableEditOnTemplate) != 0;
		const bool bBlueprintVisible = (VarDesc.PropertyFlags & CPF_BlueprintVisible) != 0;
		// CPF_EditConst means the property is read-only in details panels (maps to Visible* specifiers in UPROPERTY)
		const bool bEditConst = (VarDesc.PropertyFlags & CPF_EditConst) != 0;

		if (bEdit && !bEditConst)
		{
			if (bDisableOnInstance)
			{
				Specifiers.Add(TEXT("EditDefaultsOnly"));
			}
			else if (bDisableOnTemplate)
			{
				Specifiers.Add(TEXT("EditInstanceOnly"));
			}
			else
			{
				Specifiers.Add(TEXT("EditAnywhere"));
			}
		}
		else if (bEdit && bEditConst)
		{
			// Read-only in details panel
			if (bDisableOnInstance)
			{
				Specifiers.Add(TEXT("VisibleDefaultsOnly"));
			}
			else if (bDisableOnTemplate)
			{
				Specifiers.Add(TEXT("VisibleInstanceOnly"));
			}
			else
			{
				Specifiers.Add(TEXT("VisibleAnywhere"));
			}
		}
		if (bBlueprintVisible)
		{
			Specifiers.Add(TEXT("BlueprintReadWrite"));
		}
		if (VarDesc.PropertyFlags & CPF_BlueprintReadOnly)
		{
			Specifiers.Add(TEXT("BlueprintReadOnly"));
		}
		if (VarDesc.PropertyFlags & CPF_ExposeOnSpawn)
		{
			Specifiers.Add(TEXT("ExposeOnSpawn"));
		}
		if (VarDesc.PropertyFlags & CPF_Config)
		{
			Specifiers.Add(TEXT("Config"));
		}
		if (VarDesc.PropertyFlags & CPF_GlobalConfig)
		{
			Specifiers.Add(TEXT("GlobalConfig"));
		}
		if (VarDesc.PropertyFlags & CPF_SaveGame)
		{
			Specifiers.Add(TEXT("SaveGame"));
		}
		if (VarDesc.PropertyFlags & CPF_Transient)
		{
			Specifiers.Add(TEXT("Transient"));
		}
		if (VarDesc.PropertyFlags & CPF_InstancedReference)
		{
			Specifiers.Add(TEXT("Instanced"));
		}
		if (VarDesc.PropertyFlags & CPF_AdvancedDisplay)
		{
			Specifiers.Add(TEXT("AdvancedDisplay"));
		}
		if (VarDesc.PropertyFlags & CPF_Interp)
		{
			Specifiers.Add(TEXT("Interp"));
		}
		if (VarDesc.PropertyFlags & CPF_Net)
		{
			Specifiers.Add(TEXT("Replicated"));
		}
		if (VarDesc.PropertyFlags & CPF_RepNotify)
		{
			Specifiers.Add(TEXT("ReplicatedUsing"));
		}

		// CR-029: Category specifier (matches what a C++ UPROPERTY macro would emit).
		// The Category value is also in VarInfo.Category but including it in declarationSpecifiers
		// lets consumers produce a complete UPROPERTY() specifier list without a separate lookup.
		if (!VarDesc.Category.IsEmpty())
		{
			Specifiers.Add(FString::Printf(TEXT("Category=\"%s\""), *VarDesc.Category.ToString()));
		}

		VarInfo.DeclarationSpecifiers = Specifiers.Array();
		VarInfo.DeclarationSpecifiers.Sort();
		
		DetailedVars.Add(VarInfo);
	}
	
	return DetailedVars;
}

TArray<FBS_FunctionInfo> UBlueprintAnalyzer::ExtractDetailedFunctions(UBlueprint* Blueprint)
{
	TArray<FBS_FunctionInfo> DetailedFuncs;
	
	if (!Blueprint)
	{
		return DetailedFuncs;
	}


	// Lambda: build FBS_ParamInfo from compiled FProperty
	auto PopulatePIFromProp = [&](const FProperty* Prop, UK2Node_FunctionEntry* ENode) -> FBS_ParamInfo
	{
		FBS_ParamInfo PInfo;
		if (!Prop) return PInfo;
		PInfo.ParamName = Prop->GetName();
		const FProperty* Inner = Prop;
		if (const FArrayProperty* A = CastField<FArrayProperty>(Prop))
		{
			PInfo.ContainerType = TEXT("Array"); Inner = A->Inner;
		}
		else if (const FSetProperty* S = CastField<FSetProperty>(Prop))
		{
			PInfo.ContainerType = TEXT("Set"); Inner = S->ElementProp;
		}
		else if (const FMapProperty* M = CastField<FMapProperty>(Prop))
		{
			PInfo.ContainerType = TEXT("Map"); Inner = M->KeyProp;
			if (M->ValueProp)
			{
				PInfo.MapValueCategory   = M->ValueProp->GetCPPType();
				PInfo.MapValueObjectPath = GetPropertyObjectTypePath(M->ValueProp);
			}
		}
		else { PInfo.ContainerType = TEXT("None"); }
		if (Inner)
		{
			if      (CastField<FBoolProperty>(Inner))        PInfo.TypeCategory = TEXT("bool");
			else if (CastField<FByteProperty>(Inner))        PInfo.TypeCategory = TEXT("byte");
			else if (CastField<FIntProperty>(Inner))         PInfo.TypeCategory = TEXT("int");
			else if (CastField<FInt64Property>(Inner))       PInfo.TypeCategory = TEXT("int64");
			else if (CastField<FFloatProperty>(Inner))       PInfo.TypeCategory = TEXT("float");
			else if (CastField<FDoubleProperty>(Inner))      PInfo.TypeCategory = TEXT("real");
			else if (CastField<FStrProperty>(Inner))         PInfo.TypeCategory = TEXT("string");
			else if (CastField<FNameProperty>(Inner))        PInfo.TypeCategory = TEXT("name");
			else if (CastField<FTextProperty>(Inner))        PInfo.TypeCategory = TEXT("text");
			else if (CastField<FInterfaceProperty>(Inner))   PInfo.TypeCategory = TEXT("interface");
			else if (CastField<FEnumProperty>(Inner))        PInfo.TypeCategory = TEXT("byte");
			else if (CastField<FSoftClassProperty>(Inner))   PInfo.TypeCategory = TEXT("softclass");
			else if (CastField<FSoftObjectProperty>(Inner))  PInfo.TypeCategory = TEXT("softobject");
			else if (CastField<FClassProperty>(Inner))       PInfo.TypeCategory = TEXT("class");
			else if (CastField<FObjectPropertyBase>(Inner))  PInfo.TypeCategory = TEXT("object");
			else if (CastField<FStructProperty>(Inner))      PInfo.TypeCategory = TEXT("struct");
			else                                             PInfo.TypeCategory = Inner->GetCPPType();
			PInfo.TypeObjectPath = GetPropertyObjectTypePath(Inner);
			if (!PInfo.TypeObjectPath.IsEmpty())
			{
				int32 LastSlash = INDEX_NONE;
				PInfo.TypeObjectPath.FindLastChar(TEXT('/'), LastSlash);
				PInfo.TypeObjectName = (LastSlash != INDEX_NONE)
					? PInfo.TypeObjectPath.RightChop(LastSlash + 1) : PInfo.TypeObjectPath;
				PInfo.TypeObjectName.RemoveFromEnd(TEXT("_C"));
			}
		}
		PInfo.bIsReference = Prop->HasAnyPropertyFlags(CPF_ReferenceParm);
		PInfo.bIsConst     = Prop->HasAnyPropertyFlags(CPF_ConstParm);
		PInfo.bIsOut       = Prop->HasAnyPropertyFlags(CPF_OutParm);
		if (ENode)
		{
			if (UEdGraphPin* ParamPin = ENode->FindPin(Prop->GetFName(), EGPD_Input))
			{
				if (!ParamPin->DefaultValue.IsEmpty())
					PInfo.DefaultValue = ParamPin->DefaultValue;
				else if (!ParamPin->DefaultTextValue.IsEmpty())
					PInfo.DefaultValue = ParamPin->DefaultTextValue.ToString();
				else if (ParamPin->DefaultObject)
					PInfo.DefaultValue = ParamPin->DefaultObject->GetPathName();
			}
		}
		PInfo.Description = FString::Printf(TEXT("%s: %s"), *PInfo.ParamName, *DescribePropertyType(Prop));
		return PInfo;
	};

	// Lambda: build FBS_ParamInfo from graph pin type (Blueprint-only functions)
	auto PopulatePIFromPin = [&](const UEdGraphPin* Pin, bool bIsOutput) -> FBS_ParamInfo
	{
		FBS_ParamInfo PInfo;
		if (!Pin) return PInfo;
		PInfo.ParamName       = Pin->PinName.ToString();
		PInfo.TypeCategory    = Pin->PinType.PinCategory.ToString();
		PInfo.TypeSubCategory = Pin->PinType.PinSubCategory.ToString();
		if (Pin->PinType.PinSubCategoryObject.IsValid())
		{
			PInfo.TypeObjectPath = Pin->PinType.PinSubCategoryObject->GetPathName();
			PInfo.TypeObjectName = Pin->PinType.PinSubCategoryObject->GetName();
		}
		switch (Pin->PinType.ContainerType)
		{
		case EPinContainerType::Array: PInfo.ContainerType = TEXT("Array"); break;
		case EPinContainerType::Set:   PInfo.ContainerType = TEXT("Set");   break;
		case EPinContainerType::Map:
			PInfo.ContainerType = TEXT("Map");
			PInfo.MapValueCategory = Pin->PinType.PinValueType.TerminalCategory.ToString();
			if (Pin->PinType.PinValueType.TerminalSubCategoryObject.IsValid())
				PInfo.MapValueObjectPath = Pin->PinType.PinValueType.TerminalSubCategoryObject->GetPathName();
			break;
		default: PInfo.ContainerType = TEXT("None"); break;
		}
		PInfo.bIsReference = Pin->PinType.bIsReference;
		PInfo.bIsConst     = Pin->PinType.bIsConst;
		PInfo.bIsOut       = bIsOutput;
		if (!bIsOutput && !Pin->DefaultValue.IsEmpty())
			PInfo.DefaultValue = Pin->DefaultValue;
		PInfo.Description = FString::Printf(TEXT("%s: %s"), *Pin->PinName.ToString(), *DescribePinTypeDetailed(Pin->PinType));
		return PInfo;
	};

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		FBS_FunctionInfo FuncInfo;
		FuncInfo.FunctionName = Graph->GetFName().ToString();
		FuncInfo.FunctionPath = Graph->GetPathName();

		UK2Node_FunctionEntry* EntryNode = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* CandidateEntry = Cast<UK2Node_FunctionEntry>(Node))
			{
				EntryNode = CandidateEntry;
				break;
			}
		}

		UFunction* Func = nullptr;
		if (Blueprint->GeneratedClass)
		{
			Func = Blueprint->GeneratedClass->FindFunctionByName(Graph->GetFName());
		}
		if (!Func && Blueprint->SkeletonGeneratedClass)
		{
			Func = Blueprint->SkeletonGeneratedClass->FindFunctionByName(Graph->GetFName());
		}

		if (Func)
		{
			bool bHasLatentInfoParam = false;
			for (TFieldIterator<FProperty> PropIt(Func); PropIt; ++PropIt)
			{
				const FProperty* Prop = *PropIt;
				if (Prop && Prop->HasAnyPropertyFlags(CPF_Parm) && Prop->GetFName() == TEXT("LatentInfo"))
				{
					bHasLatentInfoParam = true;
					break;
				}
			}

			FuncInfo.bIsPure = Func->HasAnyFunctionFlags(FUNC_BlueprintPure);
			FuncInfo.bCallInEditor = Func->HasMetaData(TEXT("CallInEditor"));
			FuncInfo.bIsStatic = Func->HasAnyFunctionFlags(FUNC_Static);
			FuncInfo.bIsLatent = Func->HasMetaData(TEXT("Latent")) || bHasLatentInfoParam;
			FuncInfo.bIsNet = Func->HasAnyFunctionFlags(FUNC_Net);
			FuncInfo.bIsNetServer = Func->HasAnyFunctionFlags(FUNC_NetServer);
			FuncInfo.bIsNetClient = Func->HasAnyFunctionFlags(FUNC_NetClient);
			FuncInfo.bIsNetMulticast = Func->HasAnyFunctionFlags(FUNC_NetMulticast);
			FuncInfo.bIsReliable = Func->HasAnyFunctionFlags(FUNC_NetReliable);
			FuncInfo.bBlueprintAuthorityOnly = Func->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly);
			FuncInfo.bBlueprintCosmetic = Func->HasAnyFunctionFlags(FUNC_BlueprintCosmetic);

			if (Func->HasAnyFunctionFlags(FUNC_Protected))
			{
				FuncInfo.bIsProtected = true;
				FuncInfo.AccessSpecifier = TEXT("protected");
			}
			else if (Func->HasAnyFunctionFlags(FUNC_Private))
			{
				FuncInfo.bIsPrivate = true;
				FuncInfo.AccessSpecifier = TEXT("private");
			}
			else
			{
				FuncInfo.bIsPublic = true;
				FuncInfo.AccessSpecifier = TEXT("public");
			}

			if (Func->HasMetaData(TEXT("Category")))
			{
				FuncInfo.Category = Func->GetMetaData(TEXT("Category"));
			}
			if (Func->HasMetaData(TEXT("ToolTip")))
			{
				FuncInfo.Tooltip = Func->GetMetaData(TEXT("ToolTip"));
			}

			if (UClass* Owner = Func->GetOwnerClass())
			{
				FuncInfo.FunctionPath = FString::Printf(TEXT("%s::%s"), *Owner->GetPathName(), *Func->GetName());
			}

			if (Func->Script.Num() > 0)
			{
				FuncInfo.BytecodeSize = Func->Script.Num();
				// UFunction::Script stores process-resolved FName IDs and UObject/FField
				// addresses. Preserve the raw MD5 only as an explicitly noncanonical,
				// single-loaded-instance diagnostic; it is not change/equivalence proof.
				FuncInfo.RawInMemoryBytecodeMd5 = FMD5::HashBytes(
					Func->Script.GetData(),
					Func->Script.Num());
			}

			for (TFieldIterator<FProperty> PropIt(Func); PropIt; ++PropIt)
			{
				FProperty* Prop = *PropIt;
				if (!Prop || !Prop->HasAnyPropertyFlags(CPF_Parm))
				{
					continue;
				}

				const FString TypeString = DescribePropertyType(Prop);
				if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					FuncInfo.ReturnType = TypeString;
					FuncInfo.ReturnTypeObjectPath = GetPropertyObjectTypePath(Prop);
					continue;
				}

				FString ParamInfo = FString::Printf(TEXT("%s: %s"), *Prop->GetName(), *TypeString);

				if (EntryNode)
				{
					if (UEdGraphPin* ParamPin = EntryNode->FindPin(Prop->GetFName(), EGPD_Input))
					{
						if (!ParamPin->DefaultValue.IsEmpty())
						{
							ParamInfo += FString::Printf(TEXT(" = %s"), *ParamPin->DefaultValue);
						}
						else if (!ParamPin->DefaultTextValue.IsEmpty())
						{
							ParamInfo += FString::Printf(TEXT(" = %s"), *ParamPin->DefaultTextValue.ToString());
						}
						else if (ParamPin->DefaultObject)
						{
							ParamInfo += FString::Printf(TEXT(" = %s"), *ParamPin->DefaultObject->GetPathName());
						}
					}
				}

				if (Prop->HasAnyPropertyFlags(CPF_OutParm) || Prop->HasAnyPropertyFlags(CPF_ReferenceParm))
				{
					FuncInfo.OutputParameters.Add(ParamInfo);
					FuncInfo.DetailedOutputParams.Add(PopulatePIFromProp(Prop, EntryNode));
				}
				else
				{
					FuncInfo.InputParameters.Add(ParamInfo);
					FuncInfo.DetailedInputParams.Add(PopulatePIFromProp(Prop, EntryNode));
				}
			}
		}
		else if (EntryNode)
		{
			FuncInfo.bIsPure = EntryNode->IsNodePure();
			FuncInfo.Tooltip = EntryNode->GetTooltipText().ToString();
			FuncInfo.bIsPublic = true;
			FuncInfo.AccessSpecifier = TEXT("public");

			for (UEdGraphPin* Pin : EntryNode->Pins)
			{
				if (!Pin)
				{
					continue;
				}

				if (Pin->Direction == EGPD_Output && Pin->PinName != UEdGraphSchema_K2::PN_Then)
				{
					if (Pin->PinName == UEdGraphSchema_K2::PN_ReturnValue || Pin->PinName == TEXT("ReturnValue"))
					{
						FuncInfo.ReturnType = DescribePinTypeDetailed(Pin->PinType);
						if (Pin->PinType.PinSubCategoryObject.IsValid())
						{
							FuncInfo.ReturnTypeObjectPath = Pin->PinType.PinSubCategoryObject->GetPathName();
						}
					}
					else
					{
						FuncInfo.OutputParameters.Add(FString::Printf(TEXT("%s: %s"), *Pin->PinName.ToString(), *DescribePinTypeDetailed(Pin->PinType)));
					FuncInfo.DetailedOutputParams.Add(PopulatePIFromPin(Pin, true));
					}
				}
				else if (Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PN_Execute)
				{
					FString ParamInfo = FString::Printf(TEXT("%s: %s"), *Pin->PinName.ToString(), *DescribePinTypeDetailed(Pin->PinType));
					if (!Pin->DefaultValue.IsEmpty())
					{
						ParamInfo += FString::Printf(TEXT(" = %s"), *Pin->DefaultValue);
					}
					FuncInfo.InputParameters.Add(ParamInfo);
					FuncInfo.DetailedInputParams.Add(PopulatePIFromPin(Pin, false));
				}
			}
		}

		if (EntryNode)
		{
			for (const FBPVariableDescription& LocalVar : EntryNode->LocalVariables)
			{
				FString LocalDesc = FString::Printf(TEXT("%s: %s"), *LocalVar.VarName.ToString(), *DescribePinTypeDetailed(LocalVar.VarType));
				if (!LocalVar.DefaultValue.IsEmpty())
				{
					LocalDesc += FString::Printf(TEXT(" = %s"), *LocalVar.DefaultValue);
				}
				FuncInfo.LocalVariables.Add(LocalDesc);

				// Structured typed form for C++ conversion
				FBS_LocalVarInfo LVI;
				LVI.VarName = LocalVar.VarName.ToString();
				LVI.TypeCategory = LocalVar.VarType.PinCategory.ToString();
				LVI.TypeSubCategory = LocalVar.VarType.PinSubCategory.ToString();
				if (LocalVar.VarType.PinSubCategoryObject.IsValid())
				{
					LVI.TypeObjectPath = LocalVar.VarType.PinSubCategoryObject->GetPathName();
					LVI.TypeObjectName = LocalVar.VarType.PinSubCategoryObject->GetName();
				}
				switch (LocalVar.VarType.ContainerType)
				{
				case EPinContainerType::Array: LVI.ContainerType = TEXT("Array"); break;
				case EPinContainerType::Set:   LVI.ContainerType = TEXT("Set"); break;
				case EPinContainerType::Map:
					LVI.ContainerType = TEXT("Map");
					LVI.MapValueCategory = LocalVar.VarType.PinValueType.TerminalCategory.ToString();
					if (LocalVar.VarType.PinValueType.TerminalSubCategoryObject.IsValid())
					{
						LVI.MapValueObjectPath = LocalVar.VarType.PinValueType.TerminalSubCategoryObject->GetPathName();
					}
					break;
				default: LVI.ContainerType = TEXT("None"); break;
				}
				LVI.bIsReference = LocalVar.VarType.bIsReference;
				LVI.bIsConst = LocalVar.VarType.bIsConst;
				LVI.DefaultValue = LocalVar.DefaultValue;
				LVI.Description = LocalDesc;
				FuncInfo.DetailedLocalVariables.Add(LVI);
			}
			FuncInfo.LocalVariables.Sort();
		}

		if (Blueprint->ParentClass && Blueprint->ParentClass->FindFunctionByName(Graph->GetFName()))
		{
			FuncInfo.bIsOverride = true;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Cast<UK2Node_CallParentFunction>(Node))
			{
				FuncInfo.bCallsParent = true;
				break;
			}
		}

		TSet<FString> FunctionSpecifiers;
		if (FuncInfo.bIsPublic) FunctionSpecifiers.Add(TEXT("Public"));
		if (FuncInfo.bIsProtected) FunctionSpecifiers.Add(TEXT("Protected"));
		if (FuncInfo.bIsPrivate) FunctionSpecifiers.Add(TEXT("Private"));
		if (FuncInfo.bIsPure) FunctionSpecifiers.Add(TEXT("BlueprintPure"));
		if (FuncInfo.bIsStatic) FunctionSpecifiers.Add(TEXT("Static"));
		if (FuncInfo.bCallInEditor) FunctionSpecifiers.Add(TEXT("CallInEditor"));
		if (FuncInfo.bIsLatent) FunctionSpecifiers.Add(TEXT("Latent"));
		if (FuncInfo.bIsOverride) FunctionSpecifiers.Add(TEXT("Override"));
		if (FuncInfo.bCallsParent) FunctionSpecifiers.Add(TEXT("CallsParent"));
		if (FuncInfo.bIsNet) FunctionSpecifiers.Add(TEXT("Net"));
		if (FuncInfo.bIsNetServer) FunctionSpecifiers.Add(TEXT("Server"));
		if (FuncInfo.bIsNetClient) FunctionSpecifiers.Add(TEXT("Client"));
		if (FuncInfo.bIsNetMulticast) FunctionSpecifiers.Add(TEXT("NetMulticast"));
		if (FuncInfo.bIsReliable) FunctionSpecifiers.Add(TEXT("Reliable"));
		if (FuncInfo.bBlueprintAuthorityOnly) FunctionSpecifiers.Add(TEXT("BlueprintAuthorityOnly"));
		if (FuncInfo.bBlueprintCosmetic) FunctionSpecifiers.Add(TEXT("BlueprintCosmetic"));

		if (Func)
		{
			if (Func->HasAnyFunctionFlags(FUNC_BlueprintCallable)) FunctionSpecifiers.Add(TEXT("BlueprintCallable"));
			if (Func->HasAnyFunctionFlags(FUNC_BlueprintEvent)) FunctionSpecifiers.Add(TEXT("BlueprintEvent"));
			// CR-029: BlueprintImplementableEvent vs BlueprintNativeEvent distinction.
			// BlueprintImplementableEvent = FUNC_BlueprintEvent + !FUNC_Native (C++ provides no default impl).
			// BlueprintNativeEvent       = FUNC_BlueprintEvent + FUNC_Native  (C++ provides _Implementation).
			if (Func->HasAnyFunctionFlags(FUNC_BlueprintEvent))
			{
				if (Func->HasAnyFunctionFlags(FUNC_Native))
				{
					FunctionSpecifiers.Add(TEXT("BlueprintNativeEvent"));
				}
				else
				{
					FunctionSpecifiers.Add(TEXT("BlueprintImplementableEvent"));
				}
			}
			if (Func->HasAnyFunctionFlags(FUNC_Event)) FunctionSpecifiers.Add(TEXT("Event"));
			if (Func->HasAnyFunctionFlags(FUNC_Exec)) FunctionSpecifiers.Add(TEXT("Exec"));
			if (Func->HasAnyFunctionFlags(FUNC_Const)) FunctionSpecifiers.Add(TEXT("Const"));
			if (Func->HasAnyFunctionFlags(FUNC_Final)) FunctionSpecifiers.Add(TEXT("Final"));
			if (Func->HasMetaData(TEXT("DeprecatedFunction"))) FunctionSpecifiers.Add(TEXT("DeprecatedFunction"));
			// CR-029: Category specifier — emit it in declarationSpecifiers as well (already in FuncInfo.Category field).
			// The Category value is also surfaced here so consumers can see it without a separate field lookup.
			if (Func->HasMetaData(TEXT("Category")))
			{
				const FString Cat = Func->GetMetaData(TEXT("Category"));
				if (!Cat.IsEmpty())
				{
					FunctionSpecifiers.Add(FString::Printf(TEXT("Category=\"%s\""), *Cat));
				}
			}
		}

		if (FuncInfo.ReturnType.IsEmpty())
		{
			FuncInfo.ReturnType = TEXT("void");
		}

		FuncInfo.DeclarationSpecifiers = FunctionSpecifiers.Array();
		FuncInfo.DeclarationSpecifiers.Sort();

		DetailedFuncs.Add(MoveTemp(FuncInfo));
	}

	DetailedFuncs.Sort([](const FBS_FunctionInfo& A, const FBS_FunctionInfo& B)
	{
		return A.FunctionName < B.FunctionName;
	});
	
	return DetailedFuncs;
}

TArray<FBS_FunctionInfo> UBlueprintAnalyzer::ExtractDelegateSignatures(UBlueprint* Blueprint)
{
	TArray<FBS_FunctionInfo> DelegateInfos;

	if (!Blueprint)
	{
		return DelegateInfos;
	}

	for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		FBS_FunctionInfo DelegateInfo;
		DelegateInfo.FunctionName = Graph->GetName();
		DelegateInfo.FunctionPath = Graph->GetPathName();
		DelegateInfo.AccessSpecifier = TEXT("public");
		DelegateInfo.bIsPublic = true;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
			{
				for (UEdGraphPin* Pin : EntryNode->Pins)
				{
					if (!Pin)
					{
						continue;
					}

					if (Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PN_Execute)
					{
						FString ParamInfo = FString::Printf(TEXT("%s: %s"), *Pin->PinName.ToString(), *DescribePinTypeDetailed(Pin->PinType));
						DelegateInfo.InputParameters.Add(ParamInfo);
					}
					else if (Pin->Direction == EGPD_Output && Pin->PinName != UEdGraphSchema_K2::PN_Then)
					{
						if (Pin->PinName == UEdGraphSchema_K2::PN_ReturnValue || Pin->PinName == TEXT("ReturnValue"))
						{
							DelegateInfo.ReturnType = DescribePinTypeDetailed(Pin->PinType);
							if (Pin->PinType.PinSubCategoryObject.IsValid())
							{
								DelegateInfo.ReturnTypeObjectPath = Pin->PinType.PinSubCategoryObject->GetPathName();
							}
						}
						else
						{
							FString ParamInfo = FString::Printf(TEXT("%s: %s"), *Pin->PinName.ToString(), *DescribePinTypeDetailed(Pin->PinType));
							DelegateInfo.OutputParameters.Add(ParamInfo);
						}
					}
				}
				break;
			}
		}

		DelegateInfo.InputParameters.Sort();
		DelegateInfo.OutputParameters.Sort();
		if (DelegateInfo.ReturnType.IsEmpty())
		{
			DelegateInfo.ReturnType = TEXT("void");
		}
		DelegateInfo.DeclarationSpecifiers = { TEXT("Delegate"), TEXT("Public") };
		DelegateInfos.Add(MoveTemp(DelegateInfo));
	}

	DelegateInfos.Sort([](const FBS_FunctionInfo& A, const FBS_FunctionInfo& B)
	{
		return A.FunctionName < B.FunctionName;
	});

	return DelegateInfos;
}

TArray<FBS_ComponentInfo> UBlueprintAnalyzer::ExtractDetailedComponents(UBlueprint* Blueprint)
{
	TArray<FBS_ComponentInfo> DetailedComps;
	
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return DetailedComps;
	}

	UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
	UInheritableComponentHandler* InheritableHandler = GeneratedClass ? GeneratedClass->GetInheritableComponentHandler(false) : nullptr;

	auto ExtractOverrideDelta = [](UActorComponent* OverrideTemplate, UActorComponent* ParentTemplate, TMap<FString, FString>& OutOverrideValues, TMap<FString, FString>& OutParentValues, TArray<FString>& OutChangedProperties)
	{
		if (!OverrideTemplate || !ParentTemplate)
		{
			return;
		}

		UClass* OverrideClass = OverrideTemplate->GetClass();
		UClass* ParentClass = ParentTemplate->GetClass();
		if (!OverrideClass || !ParentClass)
		{
			return;
		}

		for (FProperty* Property : TFieldRange<FProperty>(OverrideClass))
		{
			if (!Property || !Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Deprecated))
			{
				continue;
			}

			FProperty* ParentProperty = FindFProperty<FProperty>(ParentClass, Property->GetFName());
			if (!ParentProperty || ParentProperty->HasAnyPropertyFlags(CPF_Deprecated))
			{
				continue;
			}

			const FString OverrideValue = GetPropertyValueAsString(OverrideTemplate, Property);
			const FString ParentValue = GetPropertyValueAsString(ParentTemplate, ParentProperty);
			if (OverrideValue == ParentValue)
			{
				continue;
			}

			const FString PropertyName = Property->GetName();
			OutOverrideValues.Add(PropertyName, OverrideValue);
			OutParentValues.Add(PropertyName, ParentValue);
			OutChangedProperties.Add(PropertyName);
		}

		OutChangedProperties.Sort();
	};

	for (USCS_Node* Node : GatherAllConstructionScriptNodes(Blueprint))
	{
		if (!Node)
		{
			continue;
		}

		UActorComponent* ComponentTemplate = Node->ComponentTemplate;
		if (GeneratedClass)
		{
			if (UActorComponent* ActualTemplate = Node->GetActualComponentTemplate(GeneratedClass))
			{
				ComponentTemplate = ActualTemplate;
			}
		}

		if (!ComponentTemplate)
		{
			continue;
		}

		FBS_ComponentInfo CompInfo;
		CompInfo.ComponentName = Node->GetVariableName().ToString();
		CompInfo.ComponentClass = ComponentTemplate->GetClass()->GetPathName();
		CompInfo.bIsRootComponent = Node->IsRootNode();
		CompInfo.bIsParentComponentNative = Node->bIsParentComponentNative;
		CompInfo.ParentOwnerClassName = Node->ParentComponentOwnerClassName.ToString();

		const FComponentKey ComponentKey(Node);
		if (ComponentKey.IsValid())
		{
			if (UClass* OwnerClass = ComponentKey.GetComponentOwner())
			{
				CompInfo.InheritableOwnerClassPath = OwnerClass->GetPathName();
				CompInfo.InheritableOwnerClassName = OwnerClass->GetName();

				if (Blueprint->GeneratedClass && OwnerClass != Blueprint->GeneratedClass)
				{
					CompInfo.bIsInherited = true;
				}
			}

			CompInfo.InheritableComponentGuid = ComponentKey.GetAssociatedGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
		}

		if (Node->ParentComponentOrVariableName != NAME_None)
		{
			CompInfo.ParentComponent = Node->ParentComponentOrVariableName.ToString();
		}
		else
		{
			CompInfo.ParentComponent = CompInfo.bIsRootComponent ? TEXT("None") : TEXT("Unknown");
		}

		if (!CompInfo.bIsInherited && Blueprint->GeneratedClass && ComponentTemplate->GetOuter() != Blueprint->GeneratedClass)
		{
			CompInfo.bIsInherited = true;
		}

		UActorComponent* OverrideTemplate = nullptr;
		UActorComponent* SourceTemplate = nullptr;
		if (InheritableHandler && ComponentKey.IsValid())
		{
			OverrideTemplate = InheritableHandler->GetOverridenComponentTemplate(ComponentKey);
			if (OverrideTemplate)
			{
				CompInfo.bHasInheritableOverride = true;
				CompInfo.InheritableOverrideTemplatePath = OverrideTemplate->GetPathName();

				SourceTemplate = ComponentKey.GetOriginalTemplate(Node->GetVariableName());
				if (SourceTemplate)
				{
					CompInfo.InheritableSourceTemplatePath = SourceTemplate->GetPathName();
				}
			}
		}

		UActorComponent* PropertySourceTemplate = OverrideTemplate ? OverrideTemplate : ComponentTemplate;

		if (USceneComponent* SceneComp = Cast<USceneComponent>(PropertySourceTemplate))
		{
			CompInfo.Transform = SceneComp->GetRelativeTransform().ToString();
		}

		UClass* CompClass = PropertySourceTemplate->GetClass();
		for (FProperty* Property : TFieldRange<FProperty>(CompClass))
		{
			if (!Property || !Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Deprecated))
			{
				continue;
			}

			const FString PropertyType = DescribePropertyType(Property);
			const FString PropertyValue = GetPropertyValueAsString(PropertySourceTemplate, Property);
			if (!PropertyType.IsEmpty())
			{
				if (!PropertyValue.IsEmpty())
				{
					CompInfo.ComponentProperties.Add(FString::Printf(TEXT("%s: %s = %s"), *Property->GetName(), *PropertyType, *PropertyValue));
				}
				else
				{
					CompInfo.ComponentProperties.Add(FString::Printf(TEXT("%s: %s"), *Property->GetName(), *PropertyType));
				}
			}
		}

		TSet<FString> ComponentAssetReferences;
		TArray<UObject*> ReferencedObjects;
		FReferenceFinder ComponentRefCollector(ReferencedObjects, nullptr, false, true, false, true);
		ComponentRefCollector.FindReferences(PropertySourceTemplate);

		for (UObject* ReferencedObject : ReferencedObjects)
		{
			if (ReferencedObject && ReferencedObject != PropertySourceTemplate)
			{
				AddCandidatePath(ReferencedObject->GetPathName(), ComponentAssetReferences);
			}
		}

		if (CompInfo.bHasInheritableOverride)
		{
			ExtractOverrideDelta(OverrideTemplate ? OverrideTemplate : PropertySourceTemplate, SourceTemplate, CompInfo.InheritableOverrideValues, CompInfo.InheritableParentValues, CompInfo.InheritableOverrideProperties);
		}

		CompInfo.ComponentProperties.Sort();
		CompInfo.AssetReferences = ComponentAssetReferences.Array();
		CompInfo.AssetReferences.Sort();
		CompInfo.InheritableOverrideProperties.Sort();

		DetailedComps.Add(MoveTemp(CompInfo));
	}

	DetailedComps.Sort([](const FBS_ComponentInfo& A, const FBS_ComponentInfo& B)
	{
		return A.ComponentName < B.ComponentName;
	});
	
	return DetailedComps;
}

TArray<FString> UBlueprintAnalyzer::ExtractAssetReferences(UBlueprint* Blueprint)
{
	TArray<FString> AssetRefs;
	TSet<FString> UniqueAssetRefs;
	
	if (!Blueprint)
	{
		return AssetRefs;
	}
	
	// Extract asset references from Blueprint
	TArray<UObject*> ReferencedObjects;
	FReferenceFinder AssetReferenceCollector(ReferencedObjects, nullptr, false, true, false, true);
	AssetReferenceCollector.FindReferences(Blueprint);
	
	for (UObject* ReferencedObject : ReferencedObjects)
	{
		if (ReferencedObject && ReferencedObject != Blueprint && !ReferencedObject->IsIn(Blueprint))
		{
			const FString AssetPath = ReferencedObject->GetPathName();
			AddCandidatePath(AssetPath, UniqueAssetRefs);
		}
	}

	AssetRefs = UniqueAssetRefs.Array();
	AssetRefs.Sort();
	
	return AssetRefs;
}
