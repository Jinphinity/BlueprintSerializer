#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
// Forward declarations - BlueprintGraph includes moved to implementation files
class UK2Node;
class UAnimBlueprint;
class UAnimSequence;
class UAnimSequenceBase;
class UAnimMontage;
class UBlendSpace;
class UControlRigBlueprint;
class UAnimGraphNode_Base;
class UAnimationStateMachineGraph;
class UAnimStateTransitionNode;

#include "Dom/JsonObject.h"
#include "BlueprintAnalyzer.generated.h"

/**
 * Structure for detailed variable information
 * Used by BlueprintSerializer to capture Blueprint variable metadata
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_VariableInfo
{
	GENERATED_BODY()

	FBS_VariableInfo()
		: bIsPublic(false)
		, bIsReplicated(false)
		, bIsExposedOnSpawn(false)
		, bIsEditable(false)
	{
	}

	UPROPERTY()
	FString VariableName;
	
	UPROPERTY()
	FString VariableType;
	
	UPROPERTY()
	FString DefaultValue;
	
	UPROPERTY()
	FString Category;
	
	UPROPERTY()
	FString Tooltip;
	
	UPROPERTY()
	bool bIsPublic;
	
	UPROPERTY()
	bool bIsReplicated;
	
	UPROPERTY()
	bool bIsExposedOnSpawn;
	
	UPROPERTY()
	bool bIsEditable;
	
	UPROPERTY()
	FString RepCondition;

	UPROPERTY()
	FString RepNotifyFunction;

	// Exact replication provenance. `ReplicationCondition == COND_None` does not
	// distinguish an ordinary replicated property from a non-replicated property,
	// so consumers must be able to inspect the source flags used for the decision.
	UPROPERTY()
	FString DescriptorPropertyFlags;

	UPROPERTY()
	bool bDescriptorHasNetFlag = false;

	UPROPERTY()
	bool bDescriptorHasRepNotifyFlag = false;

	UPROPERTY()
	int32 DescriptorReplicationConditionValue = 0;

	UPROPERTY()
	FString ReplicationConditionSource;

	UPROPERTY()
	bool bCompiledPropertyFound = false;

	UPROPERTY()
	FString CompiledPropertyPath;

	UPROPERTY()
	FString CompiledPropertyFlags;

	UPROPERTY()
	bool bCompiledHasNetFlag = false;

	UPROPERTY()
	bool bCompiledHasRepNotifyFlag = false;

	UPROPERTY()
	FString CompiledRepNotifyFunction;

	UPROPERTY()
	int32 CompiledRepIndex = INDEX_NONE;

	UPROPERTY()
	FString ReplicationDecisionSource;

	UPROPERTY()
	bool bReplicationMetadataConsistent = true;

	UPROPERTY()
	FString TypeCategory;

	UPROPERTY()
	FString TypeSubCategory;

	UPROPERTY()
	FString TypeObjectPath;

	UPROPERTY()
	bool bIsArray = false;

	UPROPERTY()
	bool bIsMap = false;

	UPROPERTY()
	bool bIsSet = false;

	UPROPERTY()
	TArray<FString> DeclarationSpecifiers;
};

/**
 * Structure for detailed component information
 * Used by BlueprintSerializer to capture component hierarchy and properties
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_ComponentInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FString ComponentName;
	
	UPROPERTY()
	FString ComponentClass;
	
	UPROPERTY()
	FString ParentComponent;
	
	UPROPERTY()
	FString Transform;
	
	UPROPERTY()
	TArray<FString> ComponentProperties;
	
	UPROPERTY()
	TArray<FString> AssetReferences;

	UPROPERTY()
	bool bIsRootComponent = false;

	UPROPERTY()
	bool bIsInherited = false;

	UPROPERTY()
	bool bIsParentComponentNative = false;

	UPROPERTY()
	FString ParentOwnerClassName;

	UPROPERTY()
	bool bHasInheritableOverride = false;

	UPROPERTY()
	FString InheritableOwnerClassPath;

	UPROPERTY()
	FString InheritableOwnerClassName;

	UPROPERTY()
	FString InheritableComponentGuid;

	UPROPERTY()
	FString InheritableSourceTemplatePath;

	UPROPERTY()
	FString InheritableOverrideTemplatePath;

	UPROPERTY()
	TArray<FString> InheritableOverrideProperties;

	UPROPERTY()
	TMap<FString, FString> InheritableOverrideValues;

	UPROPERTY()
	TMap<FString, FString> InheritableParentValues;
};

/**
 * Structured function parameter for fully-typed function signatures.
 * Populated from both FProperty iterators (compiled functions) and
 * graph entry node pins (Blueprint-only functions).
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_ParamInfo
{
    GENERATED_BODY()

    UPROPERTY() FString ParamName;
    UPROPERTY() FString TypeCategory;      // "bool", "int", "float", "real", "object", "struct", etc.
    UPROPERTY() FString TypeSubCategory;   // PinSubCategory (pin-source only)
    UPROPERTY() FString TypeObjectPath;    // Full asset path (class/struct/enum)
    UPROPERTY() FString TypeObjectName;    // Short name of TypeObject
    UPROPERTY() FString ContainerType;     // "None", "Array", "Set", "Map"
    UPROPERTY() FString MapValueCategory;
    UPROPERTY() FString MapValueObjectPath;
    UPROPERTY() bool bIsReference = false; // CPF_ReferenceParm / PinType.bIsReference
    UPROPERTY() bool bIsConst = false;     // CPF_ConstParm / PinType.bIsConst
    UPROPERTY() bool bIsOut = false;       // CPF_OutParm / output pin direction
    UPROPERTY() FString DefaultValue;
    UPROPERTY() FString Description;       // Backward-compat string form
};
/**
 * Structured local variable declaration for function-scoped variables.
 * Provides full type resolution needed for C++ code generation.
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_LocalVarInfo
{
    GENERATED_BODY()

    UPROPERTY() FString VarName;
    UPROPERTY() FString TypeCategory;      // PinCategory (bool, int, float, object, struct, …)
    UPROPERTY() FString TypeSubCategory;   // PinSubCategory
    UPROPERTY() FString TypeObjectPath;    // PinSubCategoryObject path (class/struct/enum)
    UPROPERTY() FString TypeObjectName;    // PinSubCategoryObject short name
    UPROPERTY() FString ContainerType;     // "None", "Array", "Set", "Map"
    UPROPERTY() FString MapValueCategory;
    UPROPERTY() FString MapValueObjectPath;
    UPROPERTY() bool bIsReference = false;
    UPROPERTY() bool bIsConst = false;
    UPROPERTY() FString DefaultValue;
    UPROPERTY() FString Description;       // Backward-compat string form (same as LocalVariables entry)
};

/**
 * Structure for detailed function information
 * Captures function signatures, parameters, and metadata
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_FunctionInfo
{
	GENERATED_BODY()

	FBS_FunctionInfo()
		: bIsPure(false)
		, bIsPublic(false)
		, bIsProtected(false)
		, bIsPrivate(false)
		, bIsStatic(false)
		, bCallInEditor(false)
		, bIsLatent(false)
	{
	}

	UPROPERTY()
	FString FunctionName;
	
	UPROPERTY()
	FString Category;
	
	UPROPERTY()
	FString Tooltip;
	
	UPROPERTY()
	bool bIsPure;
	
	UPROPERTY()
	bool bIsPublic;
	
	UPROPERTY()
	bool bIsProtected;
	
	UPROPERTY()
	bool bIsPrivate;
	
	UPROPERTY()
	bool bIsStatic;
	
	UPROPERTY()
	bool bCallInEditor;
	
	UPROPERTY()
	bool bIsLatent;

	UPROPERTY()
	bool bIsOverride = false;

	UPROPERTY()
	bool bCallsParent = false;

	UPROPERTY()
	bool bIsNet = false;

	UPROPERTY()
	bool bIsNetServer = false;

	UPROPERTY()
	bool bIsNetClient = false;

	UPROPERTY()
	bool bIsNetMulticast = false;

	UPROPERTY()
	bool bIsReliable = false;

	UPROPERTY()
	bool bBlueprintAuthorityOnly = false;

	UPROPERTY()
	bool bBlueprintCosmetic = false;
	
	UPROPERTY()
	TArray<FString> InputParameters;
	
	UPROPERTY()
	TArray<FString> OutputParameters;
	
	UPROPERTY()
	FString ReturnType;

	UPROPERTY()
	FString ReturnTypeObjectPath;

	UPROPERTY()
	TArray<FString> LocalVariables;

	UPROPERTY()
	FString FunctionPath;

	UPROPERTY()
	int32 BytecodeSize = 0;

	UPROPERTY()
	FString RawInMemoryBytecodeMd5;
	
	UPROPERTY()
	FString AccessSpecifier; // "public", "protected", "private"

	UPROPERTY()
	TArray<FString> DeclarationSpecifiers;

    // Structured local variable declarations (typed, for C++ code generation)
    UPROPERTY()
    TArray<FBS_LocalVarInfo> DetailedLocalVariables;

    // Structured parameter declarations (typed, for C++ code generation)
    UPROPERTY()
    TArray<FBS_ParamInfo> DetailedInputParams;

    UPROPERTY()
    TArray<FBS_ParamInfo> DetailedOutputParams;

    // CR-035: GUIDs and types of unsupported/partially-supported nodes in bytecode-backed functions
    UPROPERTY()
    TArray<FString> BytecodeNodeGuids;

    UPROPERTY()
    TArray<FString> BytecodeNodeTypes;
};

USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_TimelineTrackData
{
	GENERATED_BODY()

	UPROPERTY()
	FString TrackName;

	UPROPERTY()
	FString TrackType;

	UPROPERTY()
	FString CurvePath;

	UPROPERTY()
	FString PropertyName;

	UPROPERTY()
	FString FunctionName;

	UPROPERTY()
	int32 KeyCount = 0;
};

USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_TimelineData
{
	GENERATED_BODY()

	UPROPERTY()
	FString TimelineName;

	UPROPERTY()
	float TimelineLength = 0.0f;

	UPROPERTY()
	FString LengthMode;

	UPROPERTY()
	bool bAutoPlay = false;

	UPROPERTY()
	bool bLoop = false;

	UPROPERTY()
	bool bReplicated = false;

	UPROPERTY()
	bool bIgnoreTimeDilation = false;

	UPROPERTY()
	FString UpdateFunctionName;

	UPROPERTY()
	FString FinishedFunctionName;

	UPROPERTY()
	TArray<FBS_TimelineTrackData> Tracks;
};

USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_UserDefinedStructFieldSchema
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString Type;

	UPROPERTY()
	FString TypeObjectPath;

	UPROPERTY()
	bool bIsArray = false;

	UPROPERTY()
	bool bIsMap = false;

	UPROPERTY()
	bool bIsSet = false;

	// CR-028: default value read from the struct's CDO
	UPROPERTY()
	FString DefaultValue;

	// CR-028: friendly/display name (UDS uses FriendlyName metadata)
	UPROPERTY()
	FString DisplayName;

	// CR-028: tooltip metadata string
	UPROPERTY()
	FString Tooltip;
};

USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_UserDefinedStructSchema
{
	GENERATED_BODY()

	UPROPERTY()
	FString StructName;

	UPROPERTY()
	FString StructPath;

	UPROPERTY()
	TArray<FBS_UserDefinedStructFieldSchema> Fields;
};

// CR-028: per-enumerator metadata (display name and tooltip) for user-defined enums.
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_UserDefinedEnumValueMeta
{
	GENERATED_BODY()

	// Internal name (same value as the parallel Enumerators array entry)
	UPROPERTY()
	FString Name;

	// CR-028: authored display name from GetAuthoredNameStringByIndex or FriendlyName metadata
	UPROPERTY()
	FString DisplayName;

	// CR-028: tooltip from ToolTip metadata
	UPROPERTY()
	FString Tooltip;
};

USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_UserDefinedEnumSchema
{
	GENERATED_BODY()

	UPROPERTY()
	FString EnumName;

	UPROPERTY()
	FString EnumPath;

	// Legacy: enumerator name strings (kept for backward compatibility)
	UPROPERTY()
	TArray<FString> Enumerators;

	// CR-028: per-enumerator metadata (display names, tooltips). Parallel to Enumerators.
	UPROPERTY()
	TArray<FBS_UserDefinedEnumValueMeta> EnumeratorMetadata;
};

/**
 * Structured pin data for nodes
 * Represents input/output pins on Blueprint nodes
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_PinData
{
    GENERATED_BODY()

    UPROPERTY() FGuid PinId;
    UPROPERTY() FName Name;
    UPROPERTY() FString Direction;     // "Input" or "Output"
    UPROPERTY() FString Category;      // PinCategory
    UPROPERTY() FString SubCategory;   // PinSubCategory
    UPROPERTY() FString ObjectType;    // PinSubCategoryObject name
    UPROPERTY() FString ObjectPath;    // PinSubCategoryObject path
    UPROPERTY() FString SubCategoryMemberParent;
    UPROPERTY() FString SubCategoryMemberName;
    UPROPERTY() FString SubCategoryMemberGuid;
    UPROPERTY() bool bIsReference = false;
    UPROPERTY() bool bIsConst = false;
    UPROPERTY() bool bIsWeakPointer = false;
    UPROPERTY() bool bIsUObjectWrapper = false;
    UPROPERTY() bool bIsArray = false;
    UPROPERTY() bool bIsMap = false;
    UPROPERTY() bool bIsSet = false;
    UPROPERTY() bool bIsExec = false;
    UPROPERTY() bool bConnected = false;
    UPROPERTY() FString DefaultValue;
    UPROPERTY() FString AutogeneratedDefaultValue;
    UPROPERTY() FString DefaultObjectPath;
    UPROPERTY() bool bIsOut = false;   // Non-exec output pins (output parameters)
    UPROPERTY() bool bIsOptional = false;  // AdvancedDisplay or has default value
    UPROPERTY() int32 SourceIndex = INDEX_NONE;
    UPROPERTY() FGuid ParentPinId;
    UPROPERTY() TArray<FGuid> SubPinIds;
    UPROPERTY() bool bHidden = false;
    UPROPERTY() bool bNotConnectable = false;
    UPROPERTY() bool bDefaultValueIsReadOnly = false;
    UPROPERTY() bool bDefaultValueIsIgnored = false;
    UPROPERTY() bool bAdvancedView = false;
    UPROPERTY() bool bDisplayAsMutableRef = false;
    UPROPERTY() bool bAllowFriendlyName = false;
    UPROPERTY() bool bOrphanedPin = false;
    UPROPERTY() FString FriendlyName;
    UPROPERTY() FString ToolTip;
    UPROPERTY() FString PersistentGuid;

    // Map/Set value terminal type (if applicable)
    UPROPERTY() FString ValueCategory;
    UPROPERTY() FString ValueSubCategory;
    UPROPERTY() FString ValueObjectType;
    UPROPERTY() FString ValueObjectPath;
    UPROPERTY() bool bValueIsConst = false;
    UPROPERTY() bool bValueIsWeakPointer = false;
    UPROPERTY() bool bValueIsUObjectWrapper = false;
};

/**
 * Structured node data
 * Complete representation of a Blueprint graph node
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_NodeData
{
    GENERATED_BODY()

    UPROPERTY() FGuid NodeGuid;
    UPROPERTY() FString NodeType;    // Class name
    UPROPERTY() FString Title;       // Node title
    UPROPERTY() int32 PosX = 0;
    UPROPERTY() int32 PosY = 0;
    UPROPERTY() int32 NodeWidth = 0;
    UPROPERTY() int32 NodeHeight = 0;
    UPROPERTY() FString NodeComment;
    UPROPERTY() FString EnabledState;
    UPROPERTY() FString AdvancedPinDisplay;
    UPROPERTY() bool bCanRenameNode = false;
    UPROPERTY() bool bCanResizeNode = false;
    UPROPERTY() bool bCommentBubblePinned = false;
    UPROPERTY() bool bCommentBubbleVisible = false;
    UPROPERTY() bool bCommentBubbleMakeVisible = false;
    UPROPERTY() bool bHasCompilerMessage = false;
    UPROPERTY() int32 ErrorType = 0;
    UPROPERTY() FString ErrorMsg;
    UPROPERTY() TArray<FBS_PinData> Pins;
    // For CallFunction: member parent/name (cleaned where possible)
    UPROPERTY() FString MemberParent;
    UPROPERTY() FString MemberName;
    UPROPERTY() TMap<FString, FString> NodeProperties;
};

/**
 * Structured execution edge between nodes (exec flow)
 * Represents execution connections between Blueprint nodes
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_FlowEdge
{
    GENERATED_BODY()

    UPROPERTY() FGuid SourceNodeGuid;
    UPROPERTY() FName SourcePinName;
    UPROPERTY() FGuid SourcePinId;
    UPROPERTY() FGuid TargetNodeGuid;
    UPROPERTY() FName TargetPinName;
    UPROPERTY() FGuid TargetPinId;
};

/**
 * Structured pin-to-pin link data (exec + data)
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_PinLinkData
{
    GENERATED_BODY()

    UPROPERTY() FGuid SourceNodeGuid;
    UPROPERTY() FName SourcePinName;
    UPROPERTY() FGuid SourcePinId;
    UPROPERTY() FGuid TargetNodeGuid;
    UPROPERTY() FName TargetPinName;
    UPROPERTY() FGuid TargetPinId;
    UPROPERTY() FString PinCategory;
    UPROPERTY() FString PinSubCategory;
    // Full asset path of PinSubCategoryObject (e.g. class/struct/enum path) — needed for typed wire resolution
    UPROPERTY() FString PinSubCategoryObjectPath;
    UPROPERTY() bool bIsExec = false;
};

// Extended graph data with structured containers (preferred new format)
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_GraphData_Ext
{
    GENERATED_BODY()

    UPROPERTY() FString GraphName;
    UPROPERTY() FString GraphType;
    UPROPERTY() FString GraphPath;
    UPROPERTY() FString ParentGraphPath;
    UPROPERTY() FString OwningCompositeNodeGuid;
    UPROPERTY() int32 GraphDepth = 0;
    UPROPERTY() bool bIsCollapsedGraph = false;
    UPROPERTY() TArray<FBS_NodeData> Nodes;
    UPROPERTY() TArray<FBS_FlowEdge> Execution;
    UPROPERTY() TArray<FBS_PinLinkData> DataLinks;
    // Task 19: cross-linkage – pin GUIDs on FunctionEntry outputs (graph data inputs)
    //          and FunctionResult inputs (graph data outputs)
    UPROPERTY() TArray<FString> GraphInputPins;
    UPROPERTY() TArray<FString> GraphOutputPins;
};

/** One authored widget template plus its hierarchy and layout slot evidence. */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_WidgetTemplateData
{
    GENERATED_BODY()

    UPROPERTY() FString WidgetName;
    UPROPERTY() FString WidgetPath;
    UPROPERTY() FString WidgetClassPath;
    UPROPERTY() FString ParentWidgetName;
    UPROPERTY() FString ParentWidgetPath;
    UPROPERTY() int32 ChildIndex = INDEX_NONE;
    UPROPERTY() int32 Depth = 0;
    UPROPERTY() FString SlotPath;
    UPROPERTY() FString SlotClassPath;
    UPROPERTY() FString NamedSlotName;
    UPROPERTY() TMap<FString, FString> WidgetProperties;
    UPROPERTY() TMap<FString, FString> SlotProperties;
    UPROPERTY() TArray<FString> ReferencedObjectPaths;
};

/** An editor-authored UMG property/function binding. */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_WidgetBindingData
{
    GENERATED_BODY()

    UPROPERTY() FString ObjectName;
    UPROPERTY() FString PropertyName;
    UPROPERTY() FString FunctionName;
    UPROPERTY() FString SourceProperty;
    UPROPERTY() FString SourcePath;
    UPROPERTY() FString MemberGuid;
    UPROPERTY() FString BindingKind;
};

/** A widget binding inside one UWidgetAnimation. */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_WidgetAnimationBindingData
{
    GENERATED_BODY()

    UPROPERTY() FString WidgetName;
    UPROPERTY() FString SlotWidgetName;
    UPROPERTY() FString AnimationGuid;
    UPROPERTY() bool bIsRootWidget = false;
};

/** Authored UMG animation and MovieScene topology. */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_WidgetAnimationData
{
    GENERATED_BODY()

    UPROPERTY() FString AnimationName;
    UPROPERTY() FString AnimationPath;
    UPROPERTY() FString MovieScenePath;
    UPROPERTY() FString PlaybackRange;
    UPROPERTY() FString TickResolution;
    UPROPERTY() FString DisplayRate;
    UPROPERTY() TArray<FString> MasterTracks;
    UPROPERTY() TArray<FString> MovieSceneBindings;
    UPROPERTY() TArray<FBS_WidgetAnimationBindingData> WidgetBindings;
    UPROPERTY() TMap<FString, FString> AnimationProperties;
    UPROPERTY() TMap<FString, FString> MovieSceneProperties;
    UPROPERTY() TArray<FString> ReferencedObjectPaths;
};

/** WidgetBlueprint-only reconstruction evidence. */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_WidgetBlueprintData
{
    GENERATED_BODY()

    UPROPERTY() bool bIsWidgetBlueprint = false;
    UPROPERTY() bool bHasSourceWidgetTree = false;
    UPROPERTY() FString WidgetTreePath;
    UPROPERTY() FString RootWidgetName;
    UPROPERTY() TArray<FBS_WidgetTemplateData> Widgets;
    UPROPERTY() TArray<FString> NamedSlotBindings;
    UPROPERTY() TArray<FString> GeneratedNamedSlots;
    UPROPERTY() TArray<FBS_WidgetBindingData> Bindings;
    UPROPERTY() TArray<FBS_WidgetAnimationData> Animations;
};

/**
 * Animation variable summary (AnimBP variables)
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_AnimationVariableData
{
    GENERATED_BODY()

    UPROPERTY() FString VariableName;
    UPROPERTY() FString VariableType;
    UPROPERTY() FString DefaultValue;
    UPROPERTY() FString Category;
};

/**
 * Extraction coverage summary (deterministic for diffs)
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_ExtractionCoverageData
{
    GENERATED_BODY()

    UPROPERTY() int32 TotalStateNodes = 0;
    UPROPERTY() int32 TotalTransitions = 0;
    UPROPERTY() int32 TotalNotifies = 0;
    UPROPERTY() int32 TotalCurves = 0;
    UPROPERTY() int32 TotalAnimAssets = 0;
    UPROPERTY() int32 TotalControlRigs = 0;
    UPROPERTY() TMap<FString, int32> NodeTypeCounts;
};

/**
 * AnimGraph node data
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_AnimNodeData
{
    GENERATED_BODY()

    UPROPERTY() FGuid NodeGuid;
    UPROPERTY() FString NodeType;
    UPROPERTY() FString NodeTitle;
    UPROPERTY() int32 PosX = 0;
    UPROPERTY() int32 PosY = 0;

    UPROPERTY() FString BlendSpaceAssetPath;
    UPROPERTY() TArray<FString> BlendSpaceAxes;
    UPROPERTY() TArray<FString> BoneLayers;
    UPROPERTY() FString BlendMaskAssetPath;
    UPROPERTY() FString SlotName;
    UPROPERTY() FString SlotGroupName;
    UPROPERTY() FString LinkedLayerName;
    UPROPERTY() FString LinkedLayerAssetPath;

    UPROPERTY() float InertializationDuration = 0.2f;
    UPROPERTY() float BlendOutDuration = 0.2f;

    UPROPERTY() bool bIsStateMachine = false;
    UPROPERTY() FString StateMachineGroupName;

    UPROPERTY() TArray<FBS_PinData> Pins;
    UPROPERTY() TMap<FString, FString> AllProperties;
};

/**
 * Anim state data
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_AnimStateData
{
    GENERATED_BODY()

    UPROPERTY() FString StateName;
    UPROPERTY() FGuid StateGuid;
    UPROPERTY() FString StateType; // Entry, State, Conduit, Aliased
    UPROPERTY() bool bIsEndState = false;
    UPROPERTY() int32 PosX = 0;
    UPROPERTY() int32 PosY = 0;

    UPROPERTY() TArray<FBS_AnimNodeData> AnimGraphNodes;
    UPROPERTY() TArray<FBS_FlowEdge> PoseConnections;

    UPROPERTY() TMap<FString, FString> StateMetadata;
};

/**
 * Anim transition data
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_AnimTransitionData
{
    GENERATED_BODY()

    UPROPERTY() FGuid TransitionGuid;
    UPROPERTY() FGuid SourceStateGuid;
    UPROPERTY() FGuid TargetStateGuid;
    UPROPERTY() FString SourceStateName;
    UPROPERTY() FString TargetStateName;

    UPROPERTY() FString TransitionRuleName;
    UPROPERTY() TArray<FBS_NodeData> TransitionRuleNodes;
    UPROPERTY() TArray<FBS_FlowEdge> RuleExecutionFlow;
    UPROPERTY() TArray<FBS_PinLinkData> RulePinLinks;

    UPROPERTY() float CrossfadeDuration = 0.2f;
    UPROPERTY() FString BlendMode;
    UPROPERTY() bool bInterruptible = true;
    UPROPERTY() float BlendOutTriggerTime = -1.0f;
    UPROPERTY() bool bAutomaticRuleBasedOnSequencePlayer = false;
    UPROPERTY() FString NotifyStateIndex;
    UPROPERTY() int32 Priority = 0;

    UPROPERTY() TMap<FString, FString> TransitionProperties;
};

/**
 * Anim state machine data
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_StateMachineData
{
    GENERATED_BODY()

    UPROPERTY() FString MachineName;
    UPROPERTY() FGuid MachineGuid;
    UPROPERTY() TArray<FBS_AnimStateData> States;
    UPROPERTY() TArray<FBS_AnimTransitionData> Transitions;
    UPROPERTY() FGuid EntryStateGuid;

    UPROPERTY() bool bIsSubStateMachine = false;
    UPROPERTY() FString ParentStateName;
    UPROPERTY() TMap<FString, FString> StateMachineProperties;
};

/**
 * Linked layer data
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_LinkedLayerData
{
    GENERATED_BODY()

    UPROPERTY() FString InterfaceName;
    UPROPERTY() FString InterfaceAssetPath;
    UPROPERTY() FString ImplementationClassPath;
};

/**
 * AnimGraph data
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_AnimGraphData
{
    GENERATED_BODY()

    UPROPERTY() FString GraphName;
    UPROPERTY() bool bIsFastPathCompliant = false;
    UPROPERTY() TArray<FBS_AnimNodeData> RootAnimNodes;
    UPROPERTY() TArray<FBS_FlowEdge> RootPoseConnections;
    UPROPERTY() TArray<FBS_StateMachineData> StateMachines;
    UPROPERTY() TArray<FBS_LinkedLayerData> LinkedLayers;
    UPROPERTY() TMap<FString, FString> GraphProperties;
};

/**
 * Animation asset data
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_AnimNotifyData
{
    GENERATED_BODY()

    UPROPERTY() FString NotifyName;
    UPROPERTY() FString NotifyClass;
    UPROPERTY() bool bIsNotifyState = false;
    UPROPERTY() float TriggerTime = 0.0f;
    UPROPERTY() int32 TriggerFrame = 0;
    UPROPERTY() float Duration = 0.0f;
    UPROPERTY() FString TrackName;
    UPROPERTY() int32 TrackIndex = 0;
    UPROPERTY() TMap<FString, FString> NotifyProperties;
    UPROPERTY() FString TriggeredEventName;
    UPROPERTY() TArray<FString> EventParameters;
};

USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_AnimCurveData
{
    GENERATED_BODY()

    UPROPERTY() FString CurveName;
    UPROPERTY() FString CurveType;
    UPROPERTY() FString CurveAssetPath;
    UPROPERTY() TArray<float> KeyTimes;
    UPROPERTY() TArray<float> KeyValues;
    UPROPERTY() TArray<FVector> VectorValues;
    UPROPERTY() TArray<FString> TransformValues;
    UPROPERTY() FString InterpolationMode;
    UPROPERTY() FString AxisType;
    UPROPERTY() TArray<FString> AffectedMaterials;
    UPROPERTY() TArray<FString> AffectedMorphTargets;
};

USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_SyncMarkerData
{
    GENERATED_BODY()

    UPROPERTY() FString MarkerName;
    UPROPERTY() float Time = 0.0f;
    UPROPERTY() int32 Frame = 0;
};

USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_MontageSectionData
{
    GENERATED_BODY()

    UPROPERTY() FString SectionName;
    UPROPERTY() float StartTime = 0.0f;
    UPROPERTY() int32 StartFrame = 0;
    UPROPERTY() FString NextSectionName;
    UPROPERTY() float NextSectionTime = 0.0f;
    UPROPERTY() TArray<FString> CompositeLinks;
};

// BlendSpace sample point (must be at global scope for USTRUCT)
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_BlendSpaceSamplePoint
{
    GENERATED_BODY()

    UPROPERTY() FString AnimationName;
    UPROPERTY() TArray<float> AxisValues;
};

USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_BlendSpaceData
{
    GENERATED_BODY()

    UPROPERTY() FString BlendSpaceName;
    UPROPERTY() FString BlendSpaceType;
    UPROPERTY() TArray<FString> AxisNames;
    UPROPERTY() TArray<float> AxisMinValues;
    UPROPERTY() TArray<float> AxisMaxValues;
    UPROPERTY() TArray<FBS_BlendSpaceSamplePoint> SamplePoints;
    UPROPERTY() TMap<FString, FString> BlendSpaceProperties;
};

USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_AnimAssetData
{
    GENERATED_BODY()

    UPROPERTY() FString AssetPath;
    UPROPERTY() FString AssetName;
    UPROPERTY() FString AssetType;
    UPROPERTY() FString SkeletonPath;
    UPROPERTY() float Length = 0.0f;
    UPROPERTY() float FrameRate = 30.0f;
    UPROPERTY() int32 TotalFrames = 0;
    UPROPERTY() TArray<FBS_AnimNotifyData> Notifies;
    UPROPERTY() TArray<FBS_AnimCurveData> Curves;
    UPROPERTY() TArray<FBS_SyncMarkerData> SyncMarkers;
    UPROPERTY() TArray<FBS_MontageSectionData> Sections;
    UPROPERTY() TArray<FString> SlotNames;
    UPROPERTY() TMap<FString, FString> MontageProperties;
    UPROPERTY() FBS_BlendSpaceData BlendSpaceData;
    UPROPERTY() TMap<FString, FString> AssetProperties;
};

/**
 * Control Rig data
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_RigVMNodeData
{
    GENERATED_BODY()

    UPROPERTY() FGuid NodeGuid;
    UPROPERTY() FString NodeType;
    UPROPERTY() FString NodeTitle;
    UPROPERTY() int32 PosX = 0;
    UPROPERTY() int32 PosY = 0;
    UPROPERTY() TArray<FBS_PinData> Pins;
    UPROPERTY() TMap<FString, FString> NodeProperties;
};

USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_ControlRigData
{
    GENERATED_BODY()

    UPROPERTY() FString RigName;
    UPROPERTY() FString SkeletonPath;
    UPROPERTY() TArray<FString> ControlNames;
    UPROPERTY() TArray<FString> BoneNames;
    UPROPERTY() TMap<FString, FString> ControlToBoneMap;
    UPROPERTY() TArray<FBS_RigVMNodeData> SetupEventNodes;
    UPROPERTY() TArray<FBS_RigVMNodeData> ForwardSolveNodes;
    UPROPERTY() TArray<FBS_RigVMNodeData> BackwardSolveNodes;
    UPROPERTY() TArray<FBS_PinLinkData> RigVMConnections;
    UPROPERTY() TArray<FString> EnabledFeatures;
    UPROPERTY() TMap<FString, FString> FeatureSettings;
    UPROPERTY() TMap<FString, FString> RigProperties;
};

/**
 * GASP-specific data
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_GameplayTagStateData
{
    GENERATED_BODY()

    UPROPERTY() FString TagName;
    UPROPERTY() FString TagCategory;
    UPROPERTY() TArray<FString> ParentTags;
    UPROPERTY() TArray<FString> AssociatedAnimStates;
    UPROPERTY() TMap<FString, FString> TagMetadata;
};

USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_FastPathAnalysis
{
    GENERATED_BODY()

    UPROPERTY() bool bIsFastPathCompliant = false;
    UPROPERTY() TArray<FString> FastPathViolations;
    UPROPERTY() TArray<FString> NonFastPathNodes;
    UPROPERTY() TMap<FString, bool> NodeFastPathStatus;
};

/**
 * Complete Blueprint data structure
 * Main data structure containing all serialized Blueprint information
 */
USTRUCT(BlueprintType)
struct BLUEPRINTSERIALIZER_API FBS_BlueprintData
{
	GENERATED_BODY()

	FBS_BlueprintData()
		: bIsInterface(false)
		, bIsMacroLibrary(false)
		, bIsFunctionLibrary(false)
		, bIsAnimBlueprint(false)
		, TotalNodeCount(0)
	{
	}

	UPROPERTY()
	FString SchemaVersion;

	UPROPERTY()
	FString BlueprintPath;
	
	UPROPERTY()
	FString BlueprintName;
	
	UPROPERTY()
	FString ParentClassName;

	UPROPERTY()
	FString ParentClassPath;
	
	UPROPERTY()
	FString GeneratedClassName;

	UPROPERTY()
	FString GeneratedClassPath;

	UPROPERTY()
	FString BlueprintNamespace;
	
	UPROPERTY()
	FString BlueprintDescription;
	
	UPROPERTY()
	FString BlueprintCategory;
	
	UPROPERTY()
	TArray<FString> BlueprintTags;
	
	UPROPERTY()
	TArray<FString> ImplementedInterfaces;

	UPROPERTY()
	TArray<FString> ImplementedInterfacePaths;

	UPROPERTY()
	TArray<FString> ImportedNamespaces;

	UPROPERTY()
	TArray<FString> ClassSpecifiers;

	UPROPERTY()
	FString ClassConfigName;

	UPROPERTY()
	TMap<FString, bool> ClassConfigFlags;

	UPROPERTY()
	TMap<FString, FString> ClassDefaultValues;

	UPROPERTY()
	TMap<FString, FString> ClassDefaultValueDelta;

	UPROPERTY()
	TArray<FBS_UserDefinedStructSchema> UserDefinedStructSchemas;

	UPROPERTY()
	TArray<FBS_UserDefinedEnumSchema> UserDefinedEnumSchemas;
	
	// Enhanced detailed information
	UPROPERTY()
	TArray<FBS_VariableInfo> DetailedVariables;
	
	UPROPERTY()
	TArray<FBS_FunctionInfo> DetailedFunctions;

	UPROPERTY()
	TArray<FBS_FunctionInfo> DelegateSignatures;
	
	UPROPERTY()
	TArray<FBS_ComponentInfo> DetailedComponents;

	UPROPERTY()
	TArray<FBS_TimelineData> Timelines;

	UPROPERTY()
	FBS_WidgetBlueprintData WidgetBlueprint;
	
	// Legacy simple arrays (for backward compatibility)
	UPROPERTY()
	TArray<FString> Variables;
	
	UPROPERTY()
	TArray<FString> Functions;
	
	UPROPERTY()
	TArray<FString> Components;
	
	UPROPERTY()
	TArray<FString> EventNodes;
	
	UPROPERTY()
	TArray<FString> GraphNodes;
	
	// Structured graph data
	UPROPERTY()
	TArray<FBS_GraphData_Ext> StructuredGraphsExt;
	
	// Asset references
	UPROPERTY()
	TArray<FString> AssetReferences;

	UPROPERTY()
	TArray<FString> UnsupportedNodeTypes;

	UPROPERTY()
	TArray<FString> PartiallySupportedNodeTypes;
	
	// Blueprint settings
	UPROPERTY()
	bool bIsInterface;
	
	UPROPERTY()
	bool bIsMacroLibrary;
	
	UPROPERTY()
	bool bIsFunctionLibrary;

	UPROPERTY()
	bool bIsAnimBlueprint;
	
	UPROPERTY()
	int32 TotalNodeCount;
	
	UPROPERTY()
	FString ExtractionTimestamp;

	// Animation extraction (AnimBP only)
	UPROPERTY()
	FString TargetSkeletonPath;

	UPROPERTY()
	TArray<FBS_AnimationVariableData> AnimationVariables;

	UPROPERTY()
	FBS_AnimGraphData AnimGraph;

	UPROPERTY()
	TArray<FBS_AnimAssetData> AnimationAssets;

	UPROPERTY()
	TArray<FBS_ControlRigData> ControlRigs;

	UPROPERTY()
	TArray<FBS_GameplayTagStateData> GameplayTags;

	UPROPERTY()
	FBS_FastPathAnalysis FastPathAnalysis;

	UPROPERTY()
	FBS_ExtractionCoverageData Coverage;
};

/**
 * Core Blueprint analysis class that extracts complete internal structure
 * Main analyzer for serializing Blueprints to structured data
 */
class BLUEPRINTSERIALIZER_API UBlueprintAnalyzer
{

public:
	/**
	 * Analyze a single Blueprint and return complete structure data
	 */
	static FBS_BlueprintData AnalyzeBlueprint(UBlueprint* Blueprint);
	
	/**
	 * Analyze all Blueprints in the project
	 */
	static TArray<FBS_BlueprintData> AnalyzeAllProjectBlueprints();
	
	/**
	 * Export Blueprint analysis to JSON format
	 */
	static FString ExportBlueprintDataToJSON(const FBS_BlueprintData& Data);
	
	/**
	 * Export multiple Blueprint analyses to JSON array
	 */
	static FString ExportMultipleBlueprintsToJSON(const TArray<FBS_BlueprintData>& DataArray);
	
	/**
	 * Save Blueprint analysis data to file
	 */
	static bool SaveBlueprintDataToFile(const FString& JsonData, const FString& FilePath);
	
	/**
	 * Export a single Blueprint directly to JSON file (complete workflow)
	 */
	static bool ExportSingleBlueprintToJSON(const FString& BlueprintPath, const FString& OutputDirectory = TEXT(""));

	/**
	 * Convert Blueprint data structure to JSON object
	 */
	static TSharedPtr<FJsonObject> BlueprintDataToJsonObject(const FBS_BlueprintData& Data);

private:
	/**
	 * Extract variable information from Blueprint (legacy)
	 */
	static TArray<FString> ExtractVariables(UBlueprint* Blueprint);
	
	/**
	 * Extract detailed variable information from Blueprint
	 */
	static TArray<FBS_VariableInfo> ExtractDetailedVariables(UBlueprint* Blueprint);
	
	/**
	 * Extract function information from Blueprint (legacy)
	 */
	static TArray<FString> ExtractFunctions(UBlueprint* Blueprint);
	
	/**
	 * Extract detailed function information from Blueprint
	 */
	static TArray<FBS_FunctionInfo> ExtractDetailedFunctions(UBlueprint* Blueprint);
	static TArray<FBS_FunctionInfo> ExtractDelegateSignatures(UBlueprint* Blueprint);
	
	/**
	 * Extract component information from Blueprint (legacy)
	 */
	static TArray<FString> ExtractComponents(UBlueprint* Blueprint);
	
	/**
	 * Extract detailed component information from Blueprint
	 */
	static TArray<FBS_ComponentInfo> ExtractDetailedComponents(UBlueprint* Blueprint);
	
	/**
	 * Extract asset references from Blueprint
	 */
	static TArray<FString> ExtractAssetReferences(UBlueprint* Blueprint);
	
	/**
	 * Extract Blueprint metadata and settings
	 */
	static void ExtractBlueprintMetadata(UBlueprint* Blueprint, FBS_BlueprintData& OutData);
	static void ExtractClassParityData(UBlueprint* Blueprint, FBS_BlueprintData& OutData);
	static void ExtractUserTypeSchemas(UBlueprint* Blueprint, FBS_BlueprintData& OutData);
	static void ExtractTimelineData(UBlueprint* Blueprint, FBS_BlueprintData& OutData);
	static void ExtractWidgetBlueprintData(UBlueprint* Blueprint, FBS_BlueprintData& OutData);
	
	/**
	 * Extract graph node information from Blueprint (legacy)
	 */
	static TArray<FString> ExtractGraphNodes(UBlueprint* Blueprint, int32& OutTotalNodeCount);
	
	/**
	 * Extract structured graph data from Blueprint
	 */
	static TArray<FBS_GraphData_Ext> ExtractStructuredGraphsExt(UBlueprint* Blueprint, int32& OutTotalNodeCount);

	/**
	 * Extract AnimBlueprint-specific data
	 */
	static void ExtractAnimBlueprintData(UBlueprint* Blueprint, FBS_BlueprintData& OutData);
	static void ExtractAnimGraphs(UAnimBlueprint* AnimBP, FBS_AnimGraphData& OutAnimGraphData);
	static void ExtractStateMachine(class UAnimationStateMachineGraph* SMGraph, FBS_AnimGraphData& OutAnimGraphData);
	static void ExtractTransitionRuleGraph(class UAnimStateTransitionNode* TransitionNode, FBS_AnimTransitionData& OutTransData);
	static FBS_AnimNodeData AnalyzeAnimNode(class UAnimGraphNode_Base* AnimNode);
	static void ExtractLinkedLayers(UAnimBlueprint* AnimBP, FBS_AnimGraphData& OutAnimGraphData);
	static void ExtractAnimationVariables(UBlueprint* Blueprint, FBS_BlueprintData& OutData);
	static void UpdateExtractionCoverage(const FBS_AnimGraphData& AnimGraphData, FBS_BlueprintData& OutData);

	// Animation asset extraction
	static void ExtractAnimAssets(const TArray<FString>& AssetPaths, FBS_BlueprintData& OutData);
	static void ExtractAnimSequenceNotifies(class UAnimSequenceBase* Sequence, FBS_AnimAssetData& OutData);
	static void ExtractAnimationCurves(class UAnimSequence* Sequence, FBS_AnimAssetData& OutData);
	static void ExtractMontageSections(class UAnimMontage* Montage, FBS_AnimAssetData& OutData);
	static void ExtractBlendSpaceData(class UBlendSpace* BlendSpace, FBS_AnimAssetData& OutData);

	// Control Rig extraction (Editor-only)
	static void ExtractControlRigData(const TArray<FString>& AssetPaths, FBS_BlueprintData& OutData);
	static void ExtractControlRigGraph(class UControlRigBlueprint* RigBlueprint, FBS_ControlRigData& OutRigData);
	
	/**
	 * Extract event nodes specifically from Blueprint graphs
	 */
	static TArray<FString> ExtractEventNodes(UBlueprint* Blueprint);
	
	/**
	 * Extract implemented interfaces from Blueprint
	 */
	static TArray<FString> ExtractImplementedInterfaces(UBlueprint* Blueprint);
	
	/**
	 * Analyze node with complete pin connections and execution flow (legacy)
	 */
	static FString AnalyzeNodeWithConnections(UEdGraphNode* Node);
	
	/**
	 * Analyze node and return JSON object with enhanced metadata
	 */
	static TSharedPtr<FJsonObject> AnalyzeNodeToJsonObject(UEdGraphNode* Node);

	/** Preferred: analyze node to structured data */
	static FBS_NodeData AnalyzeNodeToStruct(UEdGraphNode* Node);
};
