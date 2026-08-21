#include "BlueprintExtractorBlueprintLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "K2Node_Select.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_GetEnumeratorNameAsString.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_ExecutionSequence.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/UserDefinedEnum.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/UObjectIterator.h"
#include "UObject/TopLevelAssetPath.h"

namespace
{
	FString MakeSafeArtifactComponent(const FString& Input)
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

	FString BuildBlueprintArtifactToken(const FString& BlueprintName, const FString& BlueprintPath)
	{
		const FString SafeName = MakeSafeArtifactComponent(BlueprintName);
		const FString IdentityPath = BlueprintPath.IsEmpty() ? BlueprintName : BlueprintPath;
		const FString PathHash = FMD5::HashAnsiString(*IdentityPath).Left(8);
		return FString::Printf(TEXT("%s_%s"), *SafeName, *PathHash);
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
}

bool UBlueprintSerializerBlueprintLibrary::SerializeAllProjectBlueprints()
{
	return ExportAllProjectBlueprintData();
}

bool UBlueprintSerializerBlueprintLibrary::ExportAllProjectBlueprintData()
{
#if WITH_EDITOR
	UE_LOG(LogTemp, Warning, TEXT("Starting Blueprint data export..."));
	
	try
	{
		// Get all Blueprint data
		TArray<FBS_BlueprintData> BlueprintData = UBlueprintAnalyzer::AnalyzeAllProjectBlueprints();
		
		UE_LOG(LogTemp, Warning, TEXT("Analyzed %d Blueprints"), BlueprintData.Num());
		
		// Export to JSON
		FString JsonData = UBlueprintAnalyzer::ExportMultipleBlueprintsToJSON(BlueprintData);
		
		// Save to file
		FString ExportPath = UDataExportManager::GetDefaultExportPath();
		FString FileName = FString::Printf(TEXT("BP_SLZR_Blueprint_Test_Export_%s.json"), *UDataExportManager::GetTimestamp());
		FString FullPath = FPaths::Combine(ExportPath, FileName);
		
		bool bSuccess = UBlueprintAnalyzer::SaveBlueprintDataToFile(JsonData, FullPath);
		
		if (bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("✅ Export successful! File saved to: %s"), *FullPath);
			
			// Show notification in editor
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, 
					FString::Printf(TEXT("Blueprint export successful! Check: %s"), *FullPath));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("❌ Export failed!"));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("Blueprint export failed!"));
			}
		}
		
		return bSuccess;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogTemp, Error, TEXT("Exception during export: %s"), *FString(e.what()));
		return false;
	}
#else
	UE_LOG(LogTemp, Warning, TEXT("Blueprint analysis only available in editor builds"));
	return false;
#endif
}

int32 UBlueprintSerializerBlueprintLibrary::GetProjectBlueprintCount()
{
#if WITH_EDITOR
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	TArray<FAssetData> BlueprintAssets;
	AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), BlueprintAssets);
	TArray<FAssetData> AnimBlueprintAssets;
	AssetRegistry.GetAssetsByClass(UAnimBlueprint::StaticClass()->GetClassPathName(), AnimBlueprintAssets);

	TSet<FName> SeenPackages;
	int32 TotalCount = 0;
	for (const FAssetData& AssetData : BlueprintAssets)
	{
		if (!SeenPackages.Contains(AssetData.PackageName))
		{
		    SeenPackages.Add(AssetData.PackageName);
			++TotalCount;
		}
	}
	for (const FAssetData& AssetData : AnimBlueprintAssets)
	{
		if (!SeenPackages.Contains(AssetData.PackageName))
		{
		    SeenPackages.Add(AssetData.PackageName);
			++TotalCount;
		}
	}

	return TotalCount;
#else
	return 0;
#endif
}

FString UBlueprintSerializerBlueprintLibrary::AnalyzeSingleBlueprint(UBlueprint* TargetBlueprint)
{
#if WITH_EDITOR
	if (!TargetBlueprint)
	{
		return TEXT("Error: Blueprint is null");
	}
	
	FBS_BlueprintData Data = UBlueprintAnalyzer::AnalyzeBlueprint(TargetBlueprint);
	
	FString Result = FString::Printf(TEXT(
		"Blueprint Analysis Results:\n"
		"Name: %s\n"
		"Path: %s\n"
		"Parent: %s\n"
		"Variables: %d\n"
		"Functions: %d\n"
		"Components: %d\n"
		"Total Nodes: %d\n"
		"Interfaces: %d"
	), 
		*Data.BlueprintName,
		*Data.BlueprintPath,
		*Data.ParentClassName,
		Data.Variables.Num(),
		Data.Functions.Num(),
		Data.Components.Num(),
		Data.TotalNodeCount,
		Data.ImplementedInterfaces.Num()
	);
	
	UE_LOG(LogTemp, Warning, TEXT("Blueprint Analysis:\n%s"), *Result);
	
	return Result;
#else
	return TEXT("Analysis only available in editor builds");
#endif
}

FString UBlueprintSerializerBlueprintLibrary::SerializeSingleBlueprint(UBlueprint* TargetBlueprint)
{
#if WITH_EDITOR
	if (!TargetBlueprint)
	{
		return TEXT("{}");
	}

	const FBS_BlueprintData Data = UBlueprintAnalyzer::AnalyzeBlueprint(TargetBlueprint);
	return UBlueprintAnalyzer::ExportBlueprintDataToJSON(Data);
#else
	return TEXT("{}");
#endif
}

bool UBlueprintSerializerBlueprintLibrary::ExportCompleteProjectData()
{
#if WITH_EDITOR
	return UDataExportManager::ExportCompleteProjectData();
#else
	return false;
#endif
}


FString UBlueprintSerializerBlueprintLibrary::GetExportDirectory()
{
	return UDataExportManager::GetDefaultExportPath();
}

bool UBlueprintSerializerBlueprintLibrary::GenerateLLMContext(UBlueprint* TargetBlueprint)
{
#if WITH_EDITOR
	if (!TargetBlueprint)
	{
		UE_LOG(LogTemp, Warning, TEXT("GenerateLLMContext: TargetBlueprint is null"));
		return false;
	}

	const FBS_BlueprintData Data = UBlueprintAnalyzer::AnalyzeBlueprint(TargetBlueprint);
	const FString SerializedJson = UBlueprintAnalyzer::ExportBlueprintDataToJSON(Data);

	FString ExportDir = UDataExportManager::GetDefaultExportPath() / TEXT("LLMContext");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*ExportDir) && !PlatformFile.CreateDirectoryTree(*ExportDir))
	{
		UE_LOG(LogTemp, Error, TEXT("GenerateLLMContext: Failed to create export directory: %s"), *ExportDir);
		return false;
	}

	FString ContextText;
	ContextText += FString::Printf(TEXT("# Blueprint Context: %s\n\n"), *Data.BlueprintName);
	ContextText += FString::Printf(TEXT("- Blueprint Path: `%s`\n"), *Data.BlueprintPath);
	ContextText += FString::Printf(TEXT("- Parent Class: `%s`\n"), *Data.ParentClassPath);
	ContextText += FString::Printf(TEXT("- Generated Class: `%s`\n"), *Data.GeneratedClassPath);
	ContextText += FString::Printf(TEXT("- Namespace: `%s`\n"), *Data.BlueprintNamespace);
	ContextText += FString::Printf(TEXT("- Interface Count: %d\n"), Data.ImplementedInterfaces.Num());
	ContextText += FString::Printf(TEXT("- Variable Count: %d\n"), Data.DetailedVariables.Num());
	ContextText += FString::Printf(TEXT("- Function Count: %d\n"), Data.DetailedFunctions.Num());
	ContextText += FString::Printf(TEXT("- Delegate Signature Count: %d\n"), Data.DelegateSignatures.Num());
	ContextText += FString::Printf(TEXT("- Component Count: %d\n"), Data.DetailedComponents.Num());
	ContextText += FString::Printf(TEXT("- Timeline Count: %d\n"), Data.Timelines.Num());
	ContextText += FString::Printf(TEXT("- Total Node Count: %d\n"), Data.TotalNodeCount);
	ContextText += FString::Printf(TEXT("- Unsupported Node Types: %d\n"), Data.UnsupportedNodeTypes.Num());
	ContextText += FString::Printf(TEXT("- Partially Supported Node Types: %d\n\n"), Data.PartiallySupportedNodeTypes.Num());

	if (Data.UnsupportedNodeTypes.Num() > 0)
	{
		ContextText += TEXT("## Unsupported Node Types\n");
		for (const FString& NodeType : Data.UnsupportedNodeTypes)
		{
			ContextText += FString::Printf(TEXT("- `%s`\n"), *NodeType);
		}
		ContextText += TEXT("\n");
	}

	if (Data.PartiallySupportedNodeTypes.Num() > 0)
	{
		ContextText += TEXT("## Partially Supported Node Types\n");
		for (const FString& NodeType : Data.PartiallySupportedNodeTypes)
		{
			ContextText += FString::Printf(TEXT("- `%s`\n"), *NodeType);
		}
		ContextText += TEXT("\n");
	}

	ContextText += TEXT("## Serialized Blueprint JSON\n\n");
	ContextText += TEXT("```json\n");
	ContextText += SerializedJson;
	ContextText += TEXT("\n```\n");

	const FString TimeStamp = UDataExportManager::GetTimestamp();
	const FString ArtifactToken = BuildBlueprintArtifactToken(Data.BlueprintName, Data.BlueprintPath);
	const FString ContextFileName = FString::Printf(TEXT("BP_SLZR_Context_%s_%s.md"), *ArtifactToken, *TimeStamp);
	const FString ContextFilePath = FPaths::Combine(ExportDir, ContextFileName);

	if (!FFileHelper::SaveStringToFile(ContextText, *ContextFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Error, TEXT("GenerateLLMContext: Failed to write context file: %s"), *ContextFilePath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("GenerateLLMContext: Created context file: %s"), *ContextFilePath);
	return true;
#else
	return false;
#endif
}

FString UBlueprintSerializerBlueprintLibrary::AuditSingleBlueprintToFile(UBlueprint* TargetBlueprint)
{
#if WITH_EDITOR
	if (!TargetBlueprint)
	{
		return FString();
	}

	const FBS_BlueprintData Data = UBlueprintAnalyzer::AnalyzeBlueprint(TargetBlueprint);

	// Build audit report
	TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject);
	Root->SetStringField(TEXT("schemaVersion"), Data.SchemaVersion.IsEmpty() ? TEXT("1.7") : Data.SchemaVersion);
	Root->SetStringField(TEXT("blueprintName"), Data.BlueprintName);
	Root->SetStringField(TEXT("blueprintPath"), Data.BlueprintPath);
	Root->SetStringField(TEXT("parentClass"), Data.ParentClassName);
	Root->SetBoolField(TEXT("isAnimBlueprint"), Data.bIsAnimBlueprint);
	Root->SetBoolField(TEXT("isInterface"), Data.bIsInterface);
	Root->SetBoolField(TEXT("isMacroLibrary"), Data.bIsMacroLibrary);
	Root->SetBoolField(TEXT("isFunctionLibrary"), Data.bIsFunctionLibrary);

	// Structural counts
	int32 GraphCount = Data.StructuredGraphsExt.Num();
	int32 NodeCount = 0;
	int32 PinCount = 0;
	TMap<FString, int32> NodeTypeCounts;
	for (const FBS_GraphData_Ext& Graph : Data.StructuredGraphsExt)
	{
		for (const FBS_NodeData& Node : Graph.Nodes)
		{
			++NodeCount;
			NodeTypeCounts.FindOrAdd(Node.NodeType)++;
			PinCount += Node.Pins.Num();
		}
	}
	Root->SetNumberField(TEXT("graphCount"), GraphCount);
	Root->SetNumberField(TEXT("nodeCount"), NodeCount);
	Root->SetNumberField(TEXT("pinCount"), PinCount);

	// Node type coverage
	TArray<TSharedPtr<FJsonValue>> NodeTypes;
	for (const TPair<FString, int32>& Pair : NodeTypeCounts)
	{
		TSharedPtr<FJsonObject> Entry = MakeShareable(new FJsonObject);
		Entry->SetStringField(TEXT("type"), Pair.Key);
		Entry->SetNumberField(TEXT("count"), Pair.Value);
		NodeTypes.Add(MakeShareable(new FJsonValueObject(Entry)));
	}
	Root->SetArrayField(TEXT("nodeTypes"), NodeTypes);

	// Raw UFunction::Script fingerprints are intentionally noncanonical. The
	// in-memory buffer embeds process-resolved names and object/field addresses.
	TArray<TSharedPtr<FJsonValue>> RawBytecodeDiagnostics;
	if (UClass* GenClass = TargetBlueprint->GeneratedClass)
	{
		for (TFieldIterator<UFunction> FuncIt(GenClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Func = *FuncIt;
			if (!Func)
			{
				continue;
			}

			const TArray<uint8>& Script = Func->Script;
			const FString Hash = FMD5::HashBytes(Script.GetData(), Script.Num());

			TSharedPtr<FJsonObject> FuncObj = MakeShareable(new FJsonObject);
			FuncObj->SetStringField(TEXT("functionName"), Func->GetName());
			FuncObj->SetNumberField(TEXT("bytecodeSize"), Script.Num());
			FuncObj->SetObjectField(
				TEXT("rawInMemoryBytecodeDiagnostic"),
				BuildRawInMemoryBytecodeDiagnostic(Hash));
			RawBytecodeDiagnostics.Add(MakeShareable(new FJsonValueObject(FuncObj)));
		}
	}
	Root->SetArrayField(TEXT("rawInMemoryBytecodeDiagnostics"), RawBytecodeDiagnostics);

	// Serialize report
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	const FString ExportPath = UDataExportManager::GetDefaultExportPath();
	const FString ArtifactToken = BuildBlueprintArtifactToken(Data.BlueprintName, Data.BlueprintPath);
	const FString FileName = FString::Printf(TEXT("BP_SLZR_AUDIT_%s_%s.json"), *ArtifactToken, *UDataExportManager::GetTimestamp());
	const FString FullPath = FPaths::Combine(ExportPath, FileName);

	if (UBlueprintAnalyzer::SaveBlueprintDataToFile(OutputString, FullPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Audit report saved: %s"), *FullPath);
		return FullPath;
	}

	return FString();
#else
	return FString();
#endif
}

namespace
{
	struct FNodeBuildResult
	{
		UEdGraphNode* Node = nullptr;
		bool bSupported = true;
	};

	static TSharedPtr<FJsonObject> JsonValueAsObject(const TSharedPtr<FJsonValue>& Value);

	// Round-trip reconstruction consumes paths that came from donor-authored Blueprint pins and
	// property text. Never pass those strings directly to UObject loading: malformed legacy values
	// such as "//Server" make CoreUObject fail-fast while attempting to create a package. Preserve
	// the raw string in the exported JSON, but only resolve it when it is a syntactically valid
	// object/package path or a conservative reflection identifier.
	static bool TryNormalizeSafeLoadIdentifier(const FString& RawIdentifier, FString& OutIdentifier)
	{
		OutIdentifier = RawIdentifier;
		OutIdentifier.TrimStartAndEndInline();
		if (OutIdentifier.IsEmpty())
		{
			return false;
		}

		if (!OutIdentifier.StartsWith(TEXT("/")) && OutIdentifier.Contains(TEXT("'")))
		{
			OutIdentifier = FPackageName::ExportTextPathToObjectPath(OutIdentifier);
			OutIdentifier.TrimStartAndEndInline();
		}

		if (OutIdentifier.StartsWith(TEXT("/")))
		{
			if (OutIdentifier.Len() < 3
				|| OutIdentifier.StartsWith(TEXT("//"))
				|| OutIdentifier.Contains(TEXT("//"))
				|| OutIdentifier.Contains(TEXT("\\")))
			{
				return false;
			}

			int32 LastSlashIndex = INDEX_NONE;
			OutIdentifier.FindLastChar(TEXT('/'), LastSlashIndex);
			const int32 ObjectDelimiterIndex = OutIdentifier.Find(
				TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart,
				FMath::Max(LastSlashIndex + 1, 0));

			if (ObjectDelimiterIndex != INDEX_NONE)
			{
				return FPackageName::IsValidObjectPath(OutIdentifier);
			}

			return FPackageName::IsValidLongPackageName(OutIdentifier, true);
		}

		for (const TCHAR Ch : OutIdentifier)
		{
			if (!FChar::IsAlnum(Ch)
				&& Ch != TEXT('_')
				&& Ch != TEXT(':')
				&& Ch != TEXT('.'))
			{
				return false;
			}
		}

		return true;
	}

	template <typename TObjectType>
	static TObjectType* LoadObjectFromValidatedIdentifier(const FString& RawIdentifier)
	{
		FString SafeIdentifier;
		if (!TryNormalizeSafeLoadIdentifier(RawIdentifier, SafeIdentifier))
		{
			return nullptr;
		}

		return LoadObject<TObjectType>(nullptr, *SafeIdentifier);
	}

	static UObject* StaticLoadObjectFromValidatedIdentifier(
		UClass* ExpectedClass,
		const FString& RawIdentifier)
	{
		FString SafeIdentifier;
		if (!ExpectedClass
			|| !TryNormalizeSafeLoadIdentifier(RawIdentifier, SafeIdentifier))
		{
			return nullptr;
		}

		return StaticLoadObject(ExpectedClass, nullptr, *SafeIdentifier);
	}

	template <typename TObjectType>
	static TObjectType* TryFindTypeFromValidatedIdentifier(const FString& RawIdentifier)
	{
		FString SafeIdentifier;
		if (!TryNormalizeSafeLoadIdentifier(RawIdentifier, SafeIdentifier))
		{
			return nullptr;
		}

		return UClass::TryFindTypeSlow<TObjectType>(SafeIdentifier);
	}

	template <typename TObjectType>
	static TObjectType* FindLoadedObjectByNameOrPath(const FString& NameOrPath)
	{
		if (NameOrPath.IsEmpty())
		{
			return nullptr;
		}

		for (TObjectIterator<TObjectType> It; It; ++It)
		{
			TObjectType* Candidate = *It;
			if (!Candidate)
			{
				continue;
			}

			if (Candidate->GetName() == NameOrPath || Candidate->GetPathName() == NameOrPath)
			{
				return Candidate;
			}
		}

		return nullptr;
	}

	static UEnum* FindLoadedEnumByNameOrPath(const FString& EnumName)
	{
		if (EnumName.IsEmpty())
		{
			return nullptr;
		}

		for (TObjectIterator<UEnum> It; It; ++It)
		{
			UEnum* Candidate = *It;
			if (!Candidate)
			{
				continue;
			}

			if (Candidate->GetName() == EnumName || Candidate->GetPathName() == EnumName)
			{
				return Candidate;
			}
		}

		return nullptr;
	}

	static UScriptStruct* FindLoadedStructByNameOrPath(const FString& StructName)
	{
		if (StructName.IsEmpty())
		{
			return nullptr;
		}

		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			UScriptStruct* Candidate = *It;
			if (!Candidate)
			{
				continue;
			}

			if (Candidate->GetName() == StructName || Candidate->GetPathName() == StructName)
			{
				return Candidate;
			}
		}

		return nullptr;
	}

	static UClass* FindClassByNameOrPath(const FString& ClassNameOrPath)
	{
		if (ClassNameOrPath.IsEmpty())
		{
			return nullptr;
		}

		if (UClass* FoundByType = TryFindTypeFromValidatedIdentifier<UClass>(ClassNameOrPath))
		{
			return FoundByType;
		}

		UClass* Found = FindLoadedObjectByNameOrPath<UClass>(ClassNameOrPath);
		if (Found)
		{
			return Found;
		}

		if (ClassNameOrPath.StartsWith(TEXT("/")))
		{
			if (UClass* Loaded = LoadObjectFromValidatedIdentifier<UClass>(ClassNameOrPath))
			{
				return Loaded;
			}
		}

		return LoadObjectFromValidatedIdentifier<UClass>(ClassNameOrPath);
	}

	static UClass* FindClassBySimpleName(const FString& ClassName)
	{
		if (ClassName.IsEmpty())
		{
			return nullptr;
		}

		UClass* Found = FindClassByNameOrPath(ClassName);
		if (Found)
		{
			return Found;
		}

		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetName() == ClassName)
			{
				return *It;
			}
		}

		return nullptr;
	}

	static UClass* FindBlueprintGeneratedClassByName(const FString& ClassName)
	{
		if (ClassName.IsEmpty())
		{
			return nullptr;
		}

		FString AssetName = ClassName;
		AssetName.RemoveFromEnd(TEXT("_C"));

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		TArray<FAssetData> BlueprintAssets;
		AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), BlueprintAssets);

		for (const FAssetData& AssetData : BlueprintAssets)
		{
			if (AssetData.AssetName.ToString() != AssetName)
			{
				continue;
			}

			if (UBlueprint* BlueprintAsset = Cast<UBlueprint>(AssetData.GetAsset()))
			{
				if (BlueprintAsset->GeneratedClass)
				{
					return BlueprintAsset->GeneratedClass;
				}
			}
		}

		return nullptr;
	}

	static UObject* FindAssetByName(const FString& AssetName, const FTopLevelAssetPath& ClassPath)
	{
		if (AssetName.IsEmpty())
		{
			return nullptr;
		}

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByClass(ClassPath, Assets);
		for (const FAssetData& AssetData : Assets)
		{
			if (AssetData.AssetName.ToString() == AssetName)
			{
				return AssetData.GetAsset();
			}
		}

		return nullptr;
	}

	static UObject* ResolveEnumByName(const FString& EnumName)
	{
		if (EnumName.IsEmpty())
		{
			return nullptr;
		}

		if (UEnum* FoundByType = TryFindTypeFromValidatedIdentifier<UEnum>(EnumName))
		{
			return FoundByType;
		}

		if (UEnum* Found = FindLoadedEnumByNameOrPath(EnumName))
		{
			return Found;
		}

		if (EnumName.StartsWith(TEXT("/")))
		{
			if (UObject* Loaded = StaticLoadObjectFromValidatedIdentifier(
				UObject::StaticClass(), EnumName))
			{
				return Loaded;
			}
		}

		return FindAssetByName(EnumName, UUserDefinedEnum::StaticClass()->GetClassPathName());
	}

	static UObject* ResolveStructByName(const FString& StructName)
	{
		if (StructName.IsEmpty())
		{
			return nullptr;
		}

		if (UScriptStruct* FoundByType =
			TryFindTypeFromValidatedIdentifier<UScriptStruct>(StructName))
		{
			return FoundByType;
		}

		if (UScriptStruct* Found = FindLoadedStructByNameOrPath(StructName))
		{
			return Found;
		}

		if (StructName.StartsWith(TEXT("/")))
		{
			if (UObject* Loaded = StaticLoadObjectFromValidatedIdentifier(
				UObject::StaticClass(), StructName))
			{
				return Loaded;
			}
		}

		return FindAssetByName(StructName, UUserDefinedStruct::StaticClass()->GetClassPathName());
	}

	static bool BuildPinTypeFromDetailedVarType(const FString& TypeString, FEdGraphPinType& OutType)
	{
		OutType = FEdGraphPinType();
		if (TypeString.IsEmpty())
		{
			return false;
		}

		FString BaseType = TypeString;
		FString InnerType;
		const int32 ParenIndex = TypeString.Find(TEXT("("));
		if (ParenIndex != INDEX_NONE)
		{
			BaseType = TypeString.Left(ParenIndex).TrimStartAndEnd();
			const int32 CloseIndex = TypeString.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ParenIndex);
			if (CloseIndex != INDEX_NONE)
			{
				InnerType = TypeString.Mid(ParenIndex + 1, CloseIndex - ParenIndex - 1).TrimStartAndEnd();
			}
		}
		else
		{
			BaseType = TypeString.TrimStartAndEnd();
		}

		if (BaseType.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		}
		else if (BaseType.Equals(TEXT("byte"), ESearchCase::IgnoreCase))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_Byte;
			if (!InnerType.IsEmpty())
			{
				if (UObject* EnumObj = ResolveEnumByName(InnerType))
				{
					OutType.PinSubCategoryObject = EnumObj;
				}
			}
		}
		else if (BaseType.Equals(TEXT("int"), ESearchCase::IgnoreCase) || BaseType.Equals(TEXT("int32"), ESearchCase::IgnoreCase))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_Int;
		}
		else if (BaseType.Equals(TEXT("float"), ESearchCase::IgnoreCase))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_Real;
			OutType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		}
		else if (BaseType.Equals(TEXT("double"), ESearchCase::IgnoreCase) || BaseType.Equals(TEXT("real"), ESearchCase::IgnoreCase))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_Real;
			OutType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		}
		else if (BaseType.Equals(TEXT("string"), ESearchCase::IgnoreCase))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_String;
		}
		else if (BaseType.Equals(TEXT("name"), ESearchCase::IgnoreCase))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_Name;
		}
		else if (BaseType.Equals(TEXT("text"), ESearchCase::IgnoreCase))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_Text;
		}
		else if (BaseType.StartsWith(TEXT("struct"), ESearchCase::IgnoreCase))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			if (!InnerType.IsEmpty())
			{
				if (UObject* StructObj = ResolveStructByName(InnerType))
				{
					OutType.PinSubCategoryObject = StructObj;
				}
			}
		}
		else if (BaseType.StartsWith(TEXT("object"), ESearchCase::IgnoreCase))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_Object;
			if (!InnerType.IsEmpty())
			{
				if (UClass* ClassObj = FindClassBySimpleName(InnerType))
				{
					OutType.PinSubCategoryObject = ClassObj;
				}
				else if (InnerType.EndsWith(TEXT("_C")))
				{
					if (UClass* BPClass = FindBlueprintGeneratedClassByName(InnerType))
					{
						OutType.PinSubCategoryObject = BPClass;
					}
				}
			}
		}

		return OutType.PinCategory != NAME_None;
	}

	static bool ParseComponentSummary(const FString& Summary, FString& OutName, FString& OutClass)
	{
		OutName = Summary.TrimStartAndEnd();
		OutClass.Reset();

		const int32 OpenParen = Summary.Find(TEXT(" ("));
		const int32 CloseParen = Summary.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, OpenParen + 2);
		if (OpenParen != INDEX_NONE && CloseParen != INDEX_NONE && CloseParen > OpenParen)
		{
			OutName = Summary.Left(OpenParen).TrimStartAndEnd();
			OutClass = Summary.Mid(OpenParen + 2, CloseParen - (OpenParen + 2)).TrimStartAndEnd();
			return true;
		}

		return false;
	}

	static USCS_Node* FindSCSNodeByName(USimpleConstructionScript* SCS, const FName& VarName)
	{
		if (!SCS)
		{
			return nullptr;
		}

		const TArray<USCS_Node*>& Nodes = SCS->GetAllNodes();
		for (USCS_Node* Node : Nodes)
		{
			if (Node && Node->GetVariableName() == VarName)
			{
				return Node;
			}
		}

		return nullptr;
	}

	static void EnsureComponentsFromJson(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Root)
	{
		if (!Blueprint || !Root.IsValid())
		{
			return;
		}

		USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
		if (!SCS)
		{
			return;
		}

		auto AddComponent = [&](const FString& ComponentName, const FString& ClassNameOrPath, const FString& ParentName)
		{
			if (ComponentName.IsEmpty() || ClassNameOrPath.IsEmpty())
			{
				return;
			}

			const FName VarName(*ComponentName);
			if (FindSCSNodeByName(SCS, VarName))
			{
				return;
			}

			UClass* ComponentClass = FindClassBySimpleName(ClassNameOrPath);
			if (!ComponentClass && ClassNameOrPath.EndsWith(TEXT("_C")))
			{
				ComponentClass = FindBlueprintGeneratedClassByName(ClassNameOrPath);
			}
			if (!ComponentClass)
			{
				UE_LOG(LogTemp, Warning, TEXT("BlueprintSerializer: Could not resolve component class '%s' for component '%s'"), *ClassNameOrPath, *ComponentName);
				return;
			}
			if (!ComponentClass->IsChildOf(UActorComponent::StaticClass()))
			{
				UE_LOG(LogTemp, Warning, TEXT("BlueprintSerializer: Class '%s' is not an ActorComponent for component '%s'"), *ComponentClass->GetName(), *ComponentName);
				return;
			}

			USCS_Node* NewNode = SCS->CreateNode(ComponentClass, VarName);
			if (!NewNode)
			{
				return;
			}
			NewNode->SetVariableName(VarName, true);

			const bool bHasExplicitParent = !ParentName.IsEmpty() && !ParentName.Equals(TEXT("Unknown"), ESearchCase::IgnoreCase);
			if (bHasExplicitParent)
			{
				if (USCS_Node* ParentNode = FindSCSNodeByName(SCS, FName(*ParentName)))
				{
					ParentNode->AddChildNode(NewNode);
					return;
				}
			}

			SCS->AddNode(NewNode);
		};

		const TArray<TSharedPtr<FJsonValue>>* DetailedComponentsJson = nullptr;
		if (Root->TryGetArrayField(TEXT("detailedComponents"), DetailedComponentsJson) && DetailedComponentsJson)
		{
			for (const TSharedPtr<FJsonValue>& CompVal : *DetailedComponentsJson)
			{
			const TSharedPtr<FJsonObject> CompObj = JsonValueAsObject(CompVal);
				if (!CompObj.IsValid())
				{
					continue;
				}

				FString Name;
				FString ClassName;
				FString ParentName;
				CompObj->TryGetStringField(TEXT("name"), Name);
				CompObj->TryGetStringField(TEXT("class"), ClassName);
				CompObj->TryGetStringField(TEXT("parentComponent"), ParentName);

				AddComponent(Name, ClassName, ParentName);
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* ComponentsJson = nullptr;
		if (Root->TryGetArrayField(TEXT("components"), ComponentsJson) && ComponentsJson)
		{
			for (const TSharedPtr<FJsonValue>& CompVal : *ComponentsJson)
			{
				if (!CompVal.IsValid() || CompVal->Type != EJson::String)
				{
					continue;
				}
				const FString Summary = CompVal->AsString();
				FString Name;
				FString ClassName;
				ParseComponentSummary(Summary, Name, ClassName);
				AddComponent(Name, ClassName, FString());
			}
		}
	}

	static UFunction* ResolveFunctionFromJson(const TSharedPtr<FJsonObject>& NodeObj)
	{
		if (!NodeObj.IsValid())
		{
			return nullptr;
		}

		FString FunctionName;
		NodeObj->TryGetStringField(TEXT("functionName"), FunctionName);
		FString FunctionClassName;
		if (NodeObj->HasField(TEXT("functionClass")))
		{
			NodeObj->TryGetStringField(TEXT("functionClass"), FunctionClassName);
		}

		UClass* FunctionClass = nullptr;
		if (!FunctionClassName.IsEmpty())
		{
			FunctionClass = FindClassByNameOrPath(FunctionClassName);
		}

		// Fallback to meta.functionOwner if present
		if (!FunctionClass && NodeObj->HasField(TEXT("nodeProperties")))
		{
			const TSharedPtr<FJsonObject> Props = NodeObj->GetObjectField(TEXT("nodeProperties"));
			if (Props.IsValid() && Props->HasField(TEXT("meta.functionOwner")))
			{
				FunctionClass = FindClassByNameOrPath(Props->GetStringField(TEXT("meta.functionOwner")));
			}
		}

		if (!FunctionClass || FunctionName.IsEmpty())
		{
			return nullptr;
		}

		return FunctionClass->FindFunctionByName(*FunctionName);
	}

	static FEdGraphPinType BuildPinTypeFromJson(const TSharedPtr<FJsonObject>& PinObj);
	static TSharedPtr<FJsonObject> JsonValueAsObject(const TSharedPtr<FJsonValue>& Value);

	static FString ExtractLocalVarField(const FString& Source, const FString& Key)
	{
		const FString Prefix = Key + TEXT("=\"");
		int32 Start = Source.Find(Prefix, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (Start == INDEX_NONE)
		{
			return FString();
		}
		Start += Prefix.Len();
		int32 End = Source.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
		if (End == INDEX_NONE)
		{
			return FString();
		}
		return Source.Mid(Start, End - Start);
	}

	static FString ExtractLocalVarFieldValue(const FString& Source, const FString& Key)
	{
		const FString Prefix = Key + TEXT("=");
		int32 Start = Source.Find(Prefix, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (Start == INDEX_NONE)
		{
			return FString();
		}
		Start += Prefix.Len();
		int32 End = Source.Find(TEXT(","), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
		if (End == INDEX_NONE)
		{
			End = Source.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
		}
		if (End == INDEX_NONE)
		{
			End = Source.Len();
		}
		FString Value = Source.Mid(Start, End - Start);
		Value.ReplaceInline(TEXT("\""), TEXT(""));
		return Value;
	}

	static FString ExtractMemberNameFromReference(const FString& ReferenceString)
	{
		if (ReferenceString.IsEmpty())
		{
			return FString();
		}
		const FString Prefix = TEXT("MemberName=\"");
		int32 Start = ReferenceString.Find(Prefix, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (Start == INDEX_NONE)
		{
			return FString();
		}
		Start += Prefix.Len();
		int32 End = ReferenceString.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
		if (End == INDEX_NONE)
		{
			return FString();
		}
		return ReferenceString.Mid(Start, End - Start);
	}

	static FString ExtractMemberParentFromReference(const FString& ReferenceString)
	{
		if (ReferenceString.IsEmpty())
		{
			return FString();
		}
		const FString Prefix = TEXT("MemberParent=\"");
		int32 Start = ReferenceString.Find(Prefix, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (Start == INDEX_NONE)
		{
			return FString();
		}
		Start += Prefix.Len();
		int32 End = ReferenceString.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
		if (End == INDEX_NONE)
		{
			return FString();
		}
		return ReferenceString.Mid(Start, End - Start);
	}

	static FString ExtractMemberScopeFromReference(const FString& ReferenceString)
	{
		if (ReferenceString.IsEmpty())
		{
			return FString();
		}
		const FString Prefix = TEXT("MemberScope=\"");
		int32 Start = ReferenceString.Find(Prefix, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (Start == INDEX_NONE)
		{
			return FString();
		}
		Start += Prefix.Len();
		int32 End = ReferenceString.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
		if (End == INDEX_NONE)
		{
			return FString();
		}
		return ReferenceString.Mid(Start, End - Start);
	}

	static FString ExtractMemberGuidFromReference(const FString& ReferenceString)
	{
		if (ReferenceString.IsEmpty())
		{
			return FString();
		}
		const FString Prefix = TEXT("MemberGuid=");
		int32 Start = ReferenceString.Find(Prefix, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (Start == INDEX_NONE)
		{
			return FString();
		}
		Start += Prefix.Len();
		int32 End = ReferenceString.Find(TEXT(","), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
		if (End == INDEX_NONE)
		{
			End = ReferenceString.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
		}
		if (End == INDEX_NONE)
		{
			End = ReferenceString.Len();
		}
		return ReferenceString.Mid(Start, End - Start);
	}

	static bool SetStructPropertyByName(UObject* Obj, const FName& PropName, const UScriptStruct* StructType, const void* Value)
	{
		if (!Obj || !StructType || !Value)
		{
			return false;
		}

		FProperty* Property = Obj->GetClass()->FindPropertyByName(PropName);
		FStructProperty* StructProp = CastField<FStructProperty>(Property);
		if (!StructProp || StructProp->Struct != StructType)
		{
			return false;
		}

		void* Dest = StructProp->ContainerPtrToValuePtr<void>(Obj);
		StructType->CopyScriptStruct(Dest, Value);
		return true;
	}

	static bool SetIntPropertyByName(UObject* Obj, const FName& PropName, int32 Value)
	{
		if (!Obj)
		{
			return false;
		}
		FProperty* Property = Obj->GetClass()->FindPropertyByName(PropName);
		FIntProperty* IntProp = CastField<FIntProperty>(Property);
		if (!IntProp)
		{
			return false;
		}
		IntProp->SetPropertyValue_InContainer(Obj, Value);
		return true;
	}

	static bool SetObjectPropertyByName(UObject* Obj, const FName& PropName, UObject* Value)
	{
		if (!Obj)
		{
			return false;
		}
		FProperty* Property = Obj->GetClass()->FindPropertyByName(PropName);
		FObjectProperty* ObjProp = CastField<FObjectProperty>(Property);
		if (!ObjProp)
		{
			return false;
		}
		ObjProp->SetObjectPropertyValue_InContainer(Obj, Value);
		return true;
	}

	static bool TryParsePinTypeString(const FString& InText, FEdGraphPinType& OutType)
	{
		if (InText.IsEmpty())
		{
			return false;
		}

		const UScriptStruct* PinStruct = FEdGraphPinType::StaticStruct();
		if (!PinStruct)
		{
			return false;
		}

		const TCHAR* Result = PinStruct->ImportText(*InText, &OutType, nullptr, PPF_None, nullptr, PinStruct->GetName());
		return Result != nullptr;
	}

	static FGuid ParseGuid(const FString& GuidString);

	static void RestoreCreateDelegateFromJson(UK2Node_CreateDelegate* Node, const TSharedPtr<FJsonObject>& NodeObj)
	{
		if (!Node || !NodeObj.IsValid() || !NodeObj->HasField(TEXT("nodeProperties")))
		{
			return;
		}

		const TSharedPtr<FJsonObject> Props = NodeObj->GetObjectField(TEXT("nodeProperties"));
		if (!Props.IsValid())
		{
			return;
		}

		FString SelectedFunctionName;
		FString SelectedFunctionGuid;
		Props->TryGetStringField(TEXT("SelectedFunctionName"), SelectedFunctionName);
		Props->TryGetStringField(TEXT("SelectedFunctionGuid"), SelectedFunctionGuid);

		if (SelectedFunctionName.IsEmpty())
		{
			return;
		}

		Node->SetFunction(*SelectedFunctionName);
		const FGuid ParsedGuid = ParseGuid(SelectedFunctionGuid);
		if (ParsedGuid.IsValid())
		{
			SetStructPropertyByName(Node, TEXT("SelectedFunctionGuid"), TBaseStructure<FGuid>::Get(), &ParsedGuid);
		}
	}

	static FEdGraphPinType BuildPinTypeFromLocalVarString(const FString& VarTypeString)
	{
		FEdGraphPinType PinType;
		if (VarTypeString.IsEmpty())
		{
			return PinType;
		}

		const FString Category = ExtractLocalVarField(VarTypeString, TEXT("PinCategory"));
		const FString SubCategory = ExtractLocalVarField(VarTypeString, TEXT("PinSubCategory"));
		PinType.PinCategory = Category.IsEmpty() ? NAME_None : FName(*Category);
		PinType.PinSubCategory = SubCategory.IsEmpty() ? NAME_None : FName(*SubCategory);

		const FString SubCategoryObjectPath = ExtractLocalVarField(VarTypeString, TEXT("PinSubCategoryObject"));
		if (!SubCategoryObjectPath.IsEmpty())
		{
			if (UObject* Obj = StaticLoadObjectFromValidatedIdentifier(
				UObject::StaticClass(), SubCategoryObjectPath))
			{
				PinType.PinSubCategoryObject = Obj;
			}
		}

		const FString ContainerType = ExtractLocalVarFieldValue(VarTypeString, TEXT("ContainerType"));
		if (ContainerType.Equals(TEXT("Array"), ESearchCase::IgnoreCase))
		{
			PinType.ContainerType = EPinContainerType::Array;
		}
		else if (ContainerType.Equals(TEXT("Set"), ESearchCase::IgnoreCase))
		{
			PinType.ContainerType = EPinContainerType::Set;
		}
		else if (ContainerType.Equals(TEXT("Map"), ESearchCase::IgnoreCase))
		{
			PinType.ContainerType = EPinContainerType::Map;
		}

		return PinType;
	}

	static void ApplyDefaultValueToPin(UEdGraphPin* Pin, const TSharedPtr<FJsonObject>& PinObj)
	{
		if (!Pin || !PinObj.IsValid())
		{
			return;
		}

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		if (!Schema)
		{
			return;
		}

		const bool bIsSelfPin = (Pin->PinName == UEdGraphSchema_K2::PN_Self) ||
			Pin->PinName.ToString().Equals(TEXT("self"), ESearchCase::IgnoreCase);
		const bool bIsDelegatePin = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Delegate);

		// Align pin type metadata (needed for enum pins and structs)
		{
			if (!bIsSelfPin && !bIsDelegatePin)
			{
				const FEdGraphPinType PinType = BuildPinTypeFromJson(PinObj);
				if (PinType.PinCategory != NAME_None && PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
				{
					Pin->PinType = PinType;
				}
			}
		}
		if (!bIsSelfPin && !Pin->PinType.PinSubCategoryObject.IsValid())
		{
			FString EnumPath;
			if (PinObj->TryGetStringField(TEXT("objectPath"), EnumPath) && !EnumPath.IsEmpty())
			{
				if (UEnum* EnumObj = LoadObjectFromValidatedIdentifier<UEnum>(EnumPath))
				{
					Pin->PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
					Pin->PinType.PinSubCategoryObject = EnumObj;
				}
			}
		}

		FString ObjPath;
		if (PinObj->TryGetStringField(TEXT("defaultObjectPath"), ObjPath) && !ObjPath.IsEmpty())
		{
			UObject* Obj = StaticLoadObjectFromValidatedIdentifier(
				UObject::StaticClass(), ObjPath);
			if (Obj)
			{
				Pin->DefaultObject = Obj;
			}
		}

		FString DefaultValue;
		PinObj->TryGetStringField(TEXT("defaultValue"), DefaultValue);

		if (UEnum* EnumObj = Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get()))
		{
			if (DefaultValue.IsEmpty() || DefaultValue.Equals(TEXT("(INVALID)"), ESearchCase::IgnoreCase))
			{
				const FString FirstEnum = EnumObj->GetNameStringByIndex(0);
				if (!FirstEnum.IsEmpty())
				{
					Schema->TrySetDefaultValue(*Pin, FirstEnum);
					return;
				}
			}
		}

		if (!DefaultValue.IsEmpty())
		{
			Schema->TrySetDefaultValue(*Pin, DefaultValue);
		}
	}

	static FGuid ParseGuid(const FString& GuidString)
	{
		FGuid Guid;
		FGuid::Parse(GuidString, Guid);
		return Guid;
	}

	static TSharedPtr<FJsonObject> JsonValueAsObject(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> Obj;
		if (Value->Type == EJson::Object)
		{
			Obj = Value->AsObject();
		}
		else if (Value->Type == EJson::String)
		{
			const FString JsonString = Value->AsString();
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
			TSharedPtr<FJsonObject> Parsed;
			if (FJsonSerializer::Deserialize(Reader, Parsed))
			{
				Obj = Parsed;
			}
		}

		return Obj;
	}

	static bool TryGetPinName(const TSharedPtr<FJsonObject>& PinObj, FString& OutName)
	{
		if (!PinObj.IsValid())
		{
			return false;
		}

		if (PinObj->TryGetStringField(TEXT("pinName"), OutName))
		{
			return true;
		}

		if (PinObj->TryGetStringField(TEXT("name"), OutName))
		{
			return true;
		}

		return false;
	}

	static FNodeBuildResult BuildNodeFromJson(UBlueprint* Blueprint, UEdGraph* Graph, const TSharedPtr<FJsonObject>& NodeObj, const FString& GraphType, TSet<FString>& Unsupported)
	{
		FNodeBuildResult Result;
		if (!Blueprint || !Graph || !NodeObj.IsValid())
		{
			Result.bSupported = false;
			return Result;
		}

		FString NodeType;
		FString NodeGuidStr;
		NodeObj->TryGetStringField(TEXT("nodeType"), NodeType);
		NodeObj->TryGetStringField(TEXT("nodeGuid"), NodeGuidStr);
		int32 PosX = 0;
		int32 PosY = 0;
		NodeObj->TryGetNumberField(TEXT("posX"), PosX);
		NodeObj->TryGetNumberField(TEXT("posY"), PosY);

		// Function entry/result are created by AddFunctionGraph; just find and update
		if (NodeType == TEXT("K2Node_FunctionEntry") || NodeType == TEXT("K2Node_FunctionResult"))
		{
			for (UEdGraphNode* Existing : Graph->Nodes)
			{
				if (!Existing)
				{
					continue;
				}
				if ((NodeType == TEXT("K2Node_FunctionEntry") && Existing->IsA<UK2Node_FunctionEntry>()) ||
					(NodeType == TEXT("K2Node_FunctionResult") && Existing->IsA<UK2Node_FunctionResult>()))
				{
					Existing->NodePosX = PosX;
					Existing->NodePosY = PosY;
					if (!NodeGuidStr.IsEmpty())
					{
						Existing->NodeGuid = ParseGuid(NodeGuidStr);
					}
					Result.Node = Existing;
					return Result;
				}
			}
		}

		UClass* NodeClass = FindClassByNameOrPath(NodeType);
		if (!NodeClass)
		{
			Unsupported.Add(NodeType);
			Result.bSupported = false;
			return Result;
		}

		UEdGraphNode* NewNode = NewObject<UEdGraphNode>(Graph, NodeClass, NAME_None, RF_Transactional);
		Graph->AddNode(NewNode, true, false);
		NewNode->NodePosX = PosX;
		NewNode->NodePosY = PosY;
		if (!NodeGuidStr.IsEmpty())
		{
			NewNode->NodeGuid = ParseGuid(NodeGuidStr);
		}

		bool bDidAllocate = false;
		bool bSkipReconstruct = false;
		bool bRestoreCreateDelegate = false;
		FName RestoreCreateDelegateName = NAME_None;
		FGuid RestoreCreateDelegateGuid;

		if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(NewNode))
		{
			if (UFunction* Func = ResolveFunctionFromJson(NodeObj))
			{
				Call->SetFromFunction(Func);
				Call->AllocateDefaultPins();
				bDidAllocate = true;
			}
			else
			{
				FString FunctionName;
				NodeObj->TryGetStringField(TEXT("functionName"), FunctionName);
				if (!FunctionName.IsEmpty())
				{
					Call->FunctionReference.SetSelfMember(*FunctionName);
				}
			}
		}
		else if (UK2Node_SwitchEnum* SwitchEnum = Cast<UK2Node_SwitchEnum>(NewNode))
		{
			if (NodeObj->HasField(TEXT("nodeProperties")))
			{
				const TSharedPtr<FJsonObject> Props = NodeObj->GetObjectField(TEXT("nodeProperties"));
				if (Props.IsValid() && Props->HasField(TEXT("Enum")))
				{
					const FString EnumPath = Props->GetStringField(TEXT("Enum"));
					if (!EnumPath.IsEmpty())
					{
						if (UEnum* EnumObj = LoadObjectFromValidatedIdentifier<UEnum>(EnumPath))
						{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6
							SwitchEnum->Enum = EnumObj;
#else
							SwitchEnum->SetEnum(EnumObj);
#endif
							SwitchEnum->AllocateDefaultPins();
							bDidAllocate = true;
						}
					}
				}
			}
		}
		else if (UK2Node_GetEnumeratorNameAsString* EnumToString = Cast<UK2Node_GetEnumeratorNameAsString>(NewNode))
		{
			EnumToString->AllocateDefaultPins();
			bDidAllocate = true;
		}
		else if (UK2Node_PromotableOperator* Op = Cast<UK2Node_PromotableOperator>(NewNode))
		{
			if (UFunction* Func = ResolveFunctionFromJson(NodeObj))
			{
				Op->SetFromFunction(Func);
			}
		}
		else if (UK2Node_CommutativeAssociativeBinaryOperator* Op2 = Cast<UK2Node_CommutativeAssociativeBinaryOperator>(NewNode))
		{
			if (UFunction* Func = ResolveFunctionFromJson(NodeObj))
			{
				Op2->SetFromFunction(Func);
			}
		}
		else if (UK2Node_ExecutionSequence* Sequence = Cast<UK2Node_ExecutionSequence>(NewNode))
		{
			int32 DesiredThenPins = 0;
			const TArray<TSharedPtr<FJsonValue>>* PinsJson = nullptr;
			if (NodeObj->TryGetArrayField(TEXT("pins"), PinsJson) && PinsJson)
			{
				for (const TSharedPtr<FJsonValue>& PinVal : *PinsJson)
				{
					const TSharedPtr<FJsonObject> PinObj = JsonValueAsObject(PinVal);
					if (!PinObj.IsValid())
					{
						continue;
					}
					FString PinName;
					if (!TryGetPinName(PinObj, PinName))
					{
						continue;
					}
					if (PinName.StartsWith(TEXT("then_"), ESearchCase::IgnoreCase))
					{
						DesiredThenPins++;
					}
				}
			}

			Sequence->AllocateDefaultPins();
			bDidAllocate = true;

			if (DesiredThenPins > 0)
			{
				int32 ExistingThenPins = 0;
				for (UEdGraphPin* Pin : Sequence->Pins)
				{
					if (Pin && Pin->PinName.ToString().StartsWith(TEXT("then_"), ESearchCase::IgnoreCase))
					{
						ExistingThenPins++;
					}
				}

				while (ExistingThenPins < DesiredThenPins)
				{
					Sequence->AddInputPin();
					ExistingThenPins++;
				}
			}
		}
		else if (UK2Node_Select* SelectNode = Cast<UK2Node_Select>(NewNode))
		{
			int32 NumOptionPins = 0;
			bool bHasNumOptionPins = false;
			FEdGraphPinType IndexPinType;
			bool bHasIndexPinType = false;
			UEnum* EnumObj = nullptr;

			if (NodeObj->HasField(TEXT("nodeProperties")))
			{
				const TSharedPtr<FJsonObject> Props = NodeObj->GetObjectField(TEXT("nodeProperties"));
				if (Props.IsValid())
				{
					FString EnumPath;
					if (Props->TryGetStringField(TEXT("Enum"), EnumPath) && !EnumPath.IsEmpty() && !EnumPath.Equals(TEXT("None"), ESearchCase::IgnoreCase))
					{
						EnumObj = LoadObjectFromValidatedIdentifier<UEnum>(EnumPath);
					}

					FString IndexPinTypeStr;
					if (Props->TryGetStringField(TEXT("IndexPinType"), IndexPinTypeStr))
					{
						bHasIndexPinType = TryParsePinTypeString(IndexPinTypeStr, IndexPinType);
					}

					FString NumOptionStr;
					if (Props->TryGetStringField(TEXT("NumOptionPins"), NumOptionStr))
					{
						NumOptionPins = FCString::Atoi(*NumOptionStr);
						bHasNumOptionPins = NumOptionPins > 0;
					}
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* PinsJson = nullptr;
			if (NodeObj->TryGetArrayField(TEXT("pins"), PinsJson) && PinsJson)
			{
				for (const TSharedPtr<FJsonValue>& PinVal : *PinsJson)
				{
					const TSharedPtr<FJsonObject> PinObj = JsonValueAsObject(PinVal);
					if (!PinObj.IsValid())
					{
						continue;
					}
					FString PinName;
					if (!TryGetPinName(PinObj, PinName))
					{
						continue;
					}
					if (PinName.Equals(TEXT("Index"), ESearchCase::IgnoreCase))
					{
						if (!bHasIndexPinType)
						{
							IndexPinType = BuildPinTypeFromJson(PinObj);
							bHasIndexPinType = IndexPinType.PinCategory != NAME_None;
						}
						if (!EnumObj)
						{
							EnumObj = Cast<UEnum>(IndexPinType.PinSubCategoryObject.Get());
							if (!EnumObj)
							{
								FString EnumPath;
								if (PinObj->TryGetStringField(TEXT("objectPath"), EnumPath) && !EnumPath.IsEmpty())
								{
									EnumObj = LoadObjectFromValidatedIdentifier<UEnum>(EnumPath);
								}
							}
						}
					}
				}

				if (!bHasNumOptionPins)
				{
					int32 Count = 0;
					for (const TSharedPtr<FJsonValue>& PinVal : *PinsJson)
					{
						const TSharedPtr<FJsonObject> PinObj = JsonValueAsObject(PinVal);
						if (!PinObj.IsValid())
						{
							continue;
						}
						FString PinName;
						if (!TryGetPinName(PinObj, PinName))
						{
							continue;
						}
						if (PinName.Equals(TEXT("Index"), ESearchCase::IgnoreCase) ||
							PinName.Equals(UEdGraphSchema_K2::PN_ReturnValue.ToString(), ESearchCase::IgnoreCase))
						{
							continue;
						}
						Count++;
					}
					if (Count > 0)
					{
						NumOptionPins = Count;
						bHasNumOptionPins = true;
					}
				}
			}

			if (EnumObj)
			{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6
				SetObjectPropertyByName(SelectNode, TEXT("Enum"), EnumObj);
#else
				SelectNode->SetEnum(EnumObj, true);
#endif
			}
			if (bHasIndexPinType)
			{
				SetStructPropertyByName(SelectNode, TEXT("IndexPinType"), FEdGraphPinType::StaticStruct(), &IndexPinType);
			}
			if (bHasNumOptionPins)
			{
				SetIntPropertyByName(SelectNode, TEXT("NumOptionPins"), NumOptionPins);
			}

			SelectNode->AllocateDefaultPins();
			bDidAllocate = true;
		}
		else if (UK2Node_BreakStruct* BreakStruct = Cast<UK2Node_BreakStruct>(NewNode))
		{
			if (NodeObj->HasField(TEXT("nodeProperties")))
			{
				const TSharedPtr<FJsonObject> Props = NodeObj->GetObjectField(TEXT("nodeProperties"));
				if (Props.IsValid() && Props->HasField(TEXT("StructType")))
				{
					const FString StructPath = Props->GetStringField(TEXT("StructType"));
					if (!StructPath.IsEmpty())
					{
						if (UScriptStruct* StructObj =
							LoadObjectFromValidatedIdentifier<UScriptStruct>(StructPath))
						{
							SetObjectPropertyByName(BreakStruct, TEXT("StructType"), StructObj);
						}
					}
				}
			}
			BreakStruct->AllocateDefaultPins();
			bDidAllocate = true;
		}
		else if (UK2Node_VariableGet* VarGet = Cast<UK2Node_VariableGet>(NewNode))
		{
			FString VarName;
			NodeObj->TryGetStringField(TEXT("variableName"), VarName);
			FString OwnerPath;
			FString VarReferenceString;
			if (NodeObj->HasField(TEXT("nodeProperties")))
			{
				const TSharedPtr<FJsonObject> Props = NodeObj->GetObjectField(TEXT("nodeProperties"));
				if (Props.IsValid() && Props->HasField(TEXT("meta.variableOwner")))
				{
					OwnerPath = Props->GetStringField(TEXT("meta.variableOwner"));
				}
				if (Props.IsValid() && Props->HasField(TEXT("VariableReference")))
				{
					VarReferenceString = Props->GetStringField(TEXT("VariableReference"));
					const FString MemberName = ExtractMemberNameFromReference(VarReferenceString);
					if (!MemberName.IsEmpty())
					{
						VarName = MemberName;
					}
				}
			}
			FMemberReference Ref;
			if (!VarReferenceString.IsEmpty())
			{
				const FString MemberScope = ExtractMemberScopeFromReference(VarReferenceString);
				if (!MemberScope.IsEmpty())
				{
					const FString MemberGuidStr = ExtractMemberGuidFromReference(VarReferenceString);
					const FName VarFName(*VarName);
					FGuid LocalGuid = FBlueprintEditorUtils::FindLocalVariableGuidByName(Blueprint, Graph, VarFName);
					if (!LocalGuid.IsValid())
					{
						LocalGuid = ParseGuid(MemberGuidStr);
					}
					Ref.SetLocalMember(*VarName, MemberScope, LocalGuid);
					VarGet->VariableReference = Ref;
					VarGet->AllocateDefaultPins();
					bDidAllocate = true;
				}
			}
			if (bDidAllocate)
			{
				// Local variable handled
			}
			else
			if (!OwnerPath.IsEmpty())
			{
				if (UClass* OwnerClass = FindClassByNameOrPath(OwnerPath))
				{
					Ref.SetExternalMember(*VarName, OwnerClass);
				}
				else
				{
					Ref.SetSelfMember(*VarName);
				}
			}
			else
			{
				Ref.SetSelfMember(*VarName);
			}
			VarGet->VariableReference = Ref;
			VarGet->AllocateDefaultPins();
			bDidAllocate = true;
		}
		else if (UK2Node_VariableSet* VarSet = Cast<UK2Node_VariableSet>(NewNode))
		{
			FString VarName;
			NodeObj->TryGetStringField(TEXT("variableName"), VarName);
			FString OwnerPath;
			FString VarReferenceString;
			if (NodeObj->HasField(TEXT("nodeProperties")))
			{
				const TSharedPtr<FJsonObject> Props = NodeObj->GetObjectField(TEXT("nodeProperties"));
				if (Props.IsValid() && Props->HasField(TEXT("meta.variableOwner")))
				{
					OwnerPath = Props->GetStringField(TEXT("meta.variableOwner"));
				}
				if (Props.IsValid() && Props->HasField(TEXT("VariableReference")))
				{
					VarReferenceString = Props->GetStringField(TEXT("VariableReference"));
					const FString MemberName = ExtractMemberNameFromReference(VarReferenceString);
					if (!MemberName.IsEmpty())
					{
						VarName = MemberName;
					}
				}
			}
			FMemberReference Ref;
			if (!VarReferenceString.IsEmpty())
			{
				const FString MemberScope = ExtractMemberScopeFromReference(VarReferenceString);
				if (!MemberScope.IsEmpty())
				{
					const FString MemberGuidStr = ExtractMemberGuidFromReference(VarReferenceString);
					const FName VarFName(*VarName);
					FGuid LocalGuid = FBlueprintEditorUtils::FindLocalVariableGuidByName(Blueprint, Graph, VarFName);
					if (!LocalGuid.IsValid())
					{
						LocalGuid = ParseGuid(MemberGuidStr);
					}
					Ref.SetLocalMember(*VarName, MemberScope, LocalGuid);
					VarSet->VariableReference = Ref;
					VarSet->AllocateDefaultPins();
					bDidAllocate = true;
				}
			}
			if (bDidAllocate)
			{
				// Local variable handled
			}
			else
			if (!OwnerPath.IsEmpty())
			{
				if (UClass* OwnerClass = FindClassByNameOrPath(OwnerPath))
				{
					Ref.SetExternalMember(*VarName, OwnerClass);
				}
				else
				{
					Ref.SetSelfMember(*VarName);
				}
			}
			else
			{
				Ref.SetSelfMember(*VarName);
			}
			VarSet->VariableReference = Ref;
			VarSet->AllocateDefaultPins();
			bDidAllocate = true;
		}
		else if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(NewNode))
		{
			FString CustomEventName;
			if (NodeObj->TryGetStringField(TEXT("customEventName"), CustomEventName))
			{
				CustomEvent->CustomFunctionName = *CustomEventName;
			}
		}
		else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(NewNode))
		{
			FString EventName;
			if (NodeObj->TryGetStringField(TEXT("eventName"), EventName))
			{
				FMemberReference Ref;
				Ref.SetSelfMember(*EventName);
				EventNode->EventReference = Ref;
			}
		}
		else if (UK2Node_CreateDelegate* CreateDelegate = Cast<UK2Node_CreateDelegate>(NewNode))
		{
			FString SelectedFunctionName;
			FString SelectedFunctionGuid;
			if (NodeObj->HasField(TEXT("nodeProperties")))
			{
				const TSharedPtr<FJsonObject> Props = NodeObj->GetObjectField(TEXT("nodeProperties"));
				if (Props.IsValid())
				{
					Props->TryGetStringField(TEXT("SelectedFunctionName"), SelectedFunctionName);
					Props->TryGetStringField(TEXT("SelectedFunctionGuid"), SelectedFunctionGuid);
				}
			}
			if (!SelectedFunctionName.IsEmpty())
			{
				RestoreCreateDelegateName = *SelectedFunctionName;
				bRestoreCreateDelegate = true;
				CreateDelegate->SetFunction(RestoreCreateDelegateName);
				const FGuid ParsedGuid = ParseGuid(SelectedFunctionGuid);
				if (ParsedGuid.IsValid())
				{
					RestoreCreateDelegateGuid = ParsedGuid;
					SetStructPropertyByName(CreateDelegate, TEXT("SelectedFunctionGuid"), TBaseStructure<FGuid>::Get(), &RestoreCreateDelegateGuid);
				}
			}
		}
		else if (UK2Node_AssignDelegate* AssignDelegate = Cast<UK2Node_AssignDelegate>(NewNode))
		{
			FString DelegateReferenceStr;
			if (NodeObj->HasField(TEXT("nodeProperties")))
			{
				const TSharedPtr<FJsonObject> Props = NodeObj->GetObjectField(TEXT("nodeProperties"));
				if (Props.IsValid())
				{
					Props->TryGetStringField(TEXT("DelegateReference"), DelegateReferenceStr);
				}
			}
			if (!DelegateReferenceStr.IsEmpty())
			{
				const FString MemberName = ExtractMemberNameFromReference(DelegateReferenceStr);
				const FString MemberParent = ExtractMemberParentFromReference(DelegateReferenceStr);
				const FString MemberGuidStr = ExtractMemberGuidFromReference(DelegateReferenceStr);
				const FGuid MemberGuid = ParseGuid(MemberGuidStr);
				UClass* OwnerClass = nullptr;
				if (!MemberParent.IsEmpty())
				{
					OwnerClass = FindClassByNameOrPath(MemberParent);
				}

				FMemberReference DelegateRef;
				if (OwnerClass && !MemberName.IsEmpty())
				{
					DelegateRef.SetDirect(*MemberName, MemberGuid, OwnerClass, false);
				}
				else if (!MemberName.IsEmpty())
				{
					DelegateRef.SetSelfMember(*MemberName);
				}

				SetStructPropertyByName(AssignDelegate, TEXT("DelegateReference"), FMemberReference::StaticStruct(), &DelegateRef);
			}
		}

		// Apply raw node properties (best-effort)
		if (NodeObj->HasField(TEXT("nodeProperties")))
		{
			const TSharedPtr<FJsonObject> Props = NodeObj->GetObjectField(TEXT("nodeProperties"));
			if (Props.IsValid())
			{
				for (const auto& Pair : Props->Values)
				{
					const FString PropName(*Pair.Key);
					const FString PropValue = Pair.Value.IsValid() ? Pair.Value->AsString() : FString();
					if (PropName.IsEmpty())
					{
						continue;
					}
					FProperty* Property = NewNode->GetClass()->FindPropertyByName(*PropName);
					if (!Property || Property->HasAnyPropertyFlags(CPF_Transient))
					{
						continue;
					}
					void* ValuePtr = Property->ContainerPtrToValuePtr<void>(NewNode);
					Property->ImportText_Direct(*PropValue, ValuePtr, NewNode, PPF_None);
				}
			}
		}

		if (!bDidAllocate)
		{
			NewNode->AllocateDefaultPins();
		}
		if (!bSkipReconstruct)
		{
			NewNode->ReconstructNode();
		}
		if (bRestoreCreateDelegate)
		{
			if (UK2Node_CreateDelegate* CreateDelegate = Cast<UK2Node_CreateDelegate>(NewNode))
			{
				CreateDelegate->SetFunction(RestoreCreateDelegateName);
				if (RestoreCreateDelegateGuid.IsValid())
				{
					SetStructPropertyByName(CreateDelegate, TEXT("SelectedFunctionGuid"), TBaseStructure<FGuid>::Get(), &RestoreCreateDelegateGuid);
				}
			}
		}
		Result.Node = NewNode;
		return Result;
	}

	static void ApplyPinIdsAndSplits(UEdGraphNode* Node, const TArray<TSharedPtr<FJsonValue>>& PinsJson)
	{
		if (!Node)
		{
			return;
		}

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		if (!Schema)
		{
			return;
		}

		// Split pins with subPinIds first
		for (const TSharedPtr<FJsonValue>& PinVal : PinsJson)
		{
			const TSharedPtr<FJsonObject> PinObj = JsonValueAsObject(PinVal);
			if (!PinObj.IsValid() || !PinObj->HasField(TEXT("subPinIds")))
			{
				continue;
			}

			FString PinName;
			if (!TryGetPinName(PinObj, PinName))
			{
				continue;
			}
			UEdGraphPin* Pin = Node->FindPin(*PinName);
			if (Pin && Pin->SubPins.Num() == 0)
			{
				Schema->SplitPin(Pin);
			}
		}

		// Apply PinId mappings
		for (const TSharedPtr<FJsonValue>& PinVal : PinsJson)
		{
			const TSharedPtr<FJsonObject> PinObj = JsonValueAsObject(PinVal);
			if (!PinObj.IsValid())
			{
				continue;
			}

			FString PinName;
			FString PinIdStr;
			TryGetPinName(PinObj, PinName);
			PinObj->TryGetStringField(TEXT("pinId"), PinIdStr);
			if (PinIdStr.IsEmpty())
			{
				continue;
			}

			UEdGraphPin* Pin = Node->FindPin(*PinName);
			if (Pin)
			{
				Pin->PinId = ParseGuid(PinIdStr);
			}
		}
	}

	static FEdGraphPinType BuildPinTypeFromJson(const TSharedPtr<FJsonObject>& PinObj)
	{
		FEdGraphPinType PinType;
		if (!PinObj.IsValid())
		{
			return PinType;
		}

		FString Category;
		FString SubCategory;
		PinObj->TryGetStringField(TEXT("pinType"), Category);
		PinObj->TryGetStringField(TEXT("pinSubType"), SubCategory);
		if (Category.IsEmpty())
		{
			PinObj->TryGetStringField(TEXT("category"), Category);
		}
		if (SubCategory.IsEmpty())
		{
			PinObj->TryGetStringField(TEXT("subCategory"), SubCategory);
		}

		if (Category.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			Category.Reset();
		}
		if (SubCategory.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			SubCategory.Reset();
		}

		PinType.PinCategory = Category.IsEmpty() ? NAME_None : FName(*Category);
		PinType.PinSubCategory = SubCategory.IsEmpty() ? NAME_None : FName(*SubCategory);

		FString ObjectPath;
		PinObj->TryGetStringField(TEXT("objectPath"), ObjectPath);
		if (!ObjectPath.IsEmpty())
		{
			if (UObject* Obj = StaticLoadObjectFromValidatedIdentifier(
				UObject::StaticClass(), ObjectPath))
			{
				PinType.PinSubCategoryObject = Obj;
			}
		}
		else
		{
			FString ObjectType;
			PinObj->TryGetStringField(TEXT("objectType"), ObjectType);
			if (!ObjectType.IsEmpty())
			{
				if (UClass* ObjClass = TryFindTypeFromValidatedIdentifier<UClass>(ObjectType))
				{
					PinType.PinSubCategoryObject = ObjClass;
				}
				else if (UEnum* ObjEnum = TryFindTypeFromValidatedIdentifier<UEnum>(ObjectType))
				{
					PinType.PinSubCategoryObject = ObjEnum;
				}
				else if (UScriptStruct* ObjStruct =
					TryFindTypeFromValidatedIdentifier<UScriptStruct>(ObjectType))
				{
					PinType.PinSubCategoryObject = ObjStruct;
				}
				else if (UObject* Obj = FindLoadedObjectByNameOrPath<UClass>(ObjectType))
				{
					PinType.PinSubCategoryObject = Obj;
				}
				else if (ObjectType.StartsWith(TEXT("/")))
				{
					if (UObject* ObjFromPath = StaticLoadObjectFromValidatedIdentifier(
						UObject::StaticClass(), ObjectType))
					{
						PinType.PinSubCategoryObject = ObjFromPath;
					}
				}
			}
		}

		bool bIsArray = false, bIsMap = false, bIsSet = false;
		PinObj->TryGetBoolField(TEXT("isArray"), bIsArray);
		PinObj->TryGetBoolField(TEXT("isMap"), bIsMap);
		PinObj->TryGetBoolField(TEXT("isSet"), bIsSet);
		if (bIsArray) PinType.ContainerType = EPinContainerType::Array;
		else if (bIsMap) PinType.ContainerType = EPinContainerType::Map;
		else if (bIsSet) PinType.ContainerType = EPinContainerType::Set;

		bool bIsRef = false, bIsConst = false;
		PinObj->TryGetBoolField(TEXT("isReference"), bIsRef);
		PinObj->TryGetBoolField(TEXT("isConst"), bIsConst);
		PinType.bIsReference = bIsRef;
		PinType.bIsConst = bIsConst;

		return PinType;
	}

	static void AddUserPinsFromJson(UK2Node_EditablePinBase* EditableNode, const TArray<TSharedPtr<FJsonValue>>& PinsJson)
	{
		if (!EditableNode)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& PinVal : PinsJson)
		{
			const TSharedPtr<FJsonObject> PinObj = JsonValueAsObject(PinVal);
			if (!PinObj.IsValid())
			{
				continue;
			}

			FString PinName;
			TryGetPinName(PinObj, PinName);
			if (PinName.IsEmpty())
			{
				continue;
			}

			FString Direction;
			PinObj->TryGetStringField(TEXT("direction"), Direction);
			const bool bIsExec = PinObj->HasField(TEXT("isExec")) && PinObj->GetBoolField(TEXT("isExec"));
			if (bIsExec || PinName == TEXT("self"))
			{
				continue;
			}

			if (EditableNode->FindPin(*PinName))
			{
				continue;
			}

			const EEdGraphPinDirection PinDir = (Direction == TEXT("Input")) ? EGPD_Input : EGPD_Output;
			const FEdGraphPinType PinType = BuildPinTypeFromJson(PinObj);
			EditableNode->CreateUserDefinedPin(*PinName, PinType, PinDir);
		}
	}

	static void EnsureVariableFromNodeJson(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& NodeObj)
	{
		if (!Blueprint || !NodeObj.IsValid())
		{
			return;
		}

		FString NodeType;
		NodeObj->TryGetStringField(TEXT("nodeType"), NodeType);
		if (NodeType != TEXT("K2Node_VariableGet") && NodeType != TEXT("K2Node_VariableSet"))
		{
			return;
		}

		FString VarName;
		FString VarReferenceString;
		NodeObj->TryGetStringField(TEXT("variableName"), VarName);
		if (VarName.IsEmpty() && NodeObj->HasField(TEXT("nodeProperties")))
		{
			const TSharedPtr<FJsonObject> Props = NodeObj->GetObjectField(TEXT("nodeProperties"));
			if (Props.IsValid() && Props->HasField(TEXT("meta.variableName")))
			{
				VarName = Props->GetStringField(TEXT("meta.variableName"));
			}
			if (Props.IsValid() && Props->HasField(TEXT("VariableReference")))
			{
				VarReferenceString = Props->GetStringField(TEXT("VariableReference"));
				const FString MemberName = ExtractMemberNameFromReference(VarReferenceString);
				if (!MemberName.IsEmpty())
				{
					VarName = MemberName;
				}
			}
		}
		if (VarName.IsEmpty())
		{
			return;
		}

		if (!VarReferenceString.IsEmpty())
		{
			const FString MemberScope = ExtractMemberScopeFromReference(VarReferenceString);
			if (!MemberScope.IsEmpty())
			{
				// Local variable; created elsewhere from FunctionEntry
				return;
			}
		}

		// NOTE: allow creation even when owner metadata is ambiguous to avoid missing pins

		TSet<FName> ExistingVars;
		FBlueprintEditorUtils::GetClassVariableList(Blueprint, ExistingVars);

		const TArray<TSharedPtr<FJsonValue>>* PinsJson = nullptr;
		if (!NodeObj->TryGetArrayField(TEXT("pins"), PinsJson) || !PinsJson)
		{
			return;
		}

		TSharedPtr<FJsonObject> VarPinObj;
		for (const TSharedPtr<FJsonValue>& PinVal : *PinsJson)
		{
			const TSharedPtr<FJsonObject> PinObj = JsonValueAsObject(PinVal);
			if (!PinObj.IsValid())
			{
				continue;
			}
			FString PinName;
			TryGetPinName(PinObj, PinName);
			if (PinName == VarName)
			{
				VarPinObj = PinObj;
				break;
			}
		}

		if (!VarPinObj.IsValid())
		{
			for (const TSharedPtr<FJsonValue>& PinVal : *PinsJson)
			{
				const TSharedPtr<FJsonObject> PinObj = JsonValueAsObject(PinVal);
				if (!PinObj.IsValid())
				{
					continue;
				}
				const bool bIsExec = PinObj->HasField(TEXT("isExec")) && PinObj->GetBoolField(TEXT("isExec"));
				FString PinName;
				TryGetPinName(PinObj, PinName);
				FString Direction;
				PinObj->TryGetStringField(TEXT("direction"), Direction);

				if (bIsExec || PinName == TEXT("self") || PinName == TEXT("Output_Get"))
				{
					continue;
				}

				const bool bIsSet = (NodeType == TEXT("K2Node_VariableSet"));
				if ((bIsSet && Direction == TEXT("Input")) || (!bIsSet && Direction == TEXT("Output")))
				{
					VarPinObj = PinObj;
					break;
				}
			}
		}

		if (!VarPinObj.IsValid())
		{
			return;
		}

		const FEdGraphPinType PinType = BuildPinTypeFromJson(VarPinObj);
		if (PinType.PinCategory == NAME_None)
		{
			return;
		}

		if (ExistingVars.Contains(*VarName))
		{
			FBlueprintEditorUtils::ChangeMemberVariableType(Blueprint, *VarName, PinType);
			return;
		}

		FBlueprintEditorUtils::AddMemberVariable(Blueprint, *VarName, PinType);
	}

	static void EnsureStubFunctionFromCallNodeJson(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& NodeObj)
	{
		if (!Blueprint || !NodeObj.IsValid())
		{
			return;
		}

		FString NodeType;
		NodeObj->TryGetStringField(TEXT("nodeType"), NodeType);
		if (NodeType != TEXT("K2Node_CallFunction"))
		{
			return;
		}

		FString FunctionName;
		NodeObj->TryGetStringField(TEXT("functionName"), FunctionName);
		if (FunctionName.IsEmpty())
		{
			return;
		}

		FString FunctionClassName;
		NodeObj->TryGetStringField(TEXT("functionClass"), FunctionClassName);
		if (!FunctionClassName.IsEmpty())
		{
			// External function, do not create stub
			return;
		}

		if (Blueprint->ParentClass && Blueprint->ParentClass->FindFunctionByName(*FunctionName))
		{
			return;
		}

		if (NodeObj->HasField(TEXT("nodeProperties")))
		{
			const TSharedPtr<FJsonObject> Props = NodeObj->GetObjectField(TEXT("nodeProperties"));
			if (Props.IsValid() && Props->HasField(TEXT("FunctionReference")))
			{
				const FString FunctionRef = Props->GetStringField(TEXT("FunctionReference"));
				if (FunctionRef.Contains(TEXT("MemberParent=")))
				{
					return;
				}
			}
		}

		for (UEdGraph* Existing : Blueprint->FunctionGraphs)
		{
			if (Existing && Existing->GetName() == FunctionName)
			{
				return;
			}
		}

		UEdGraph* FuncGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, *FunctionName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		if (!FuncGraph)
		{
			return;
		}
		FBlueprintEditorUtils::AddFunctionGraph(Blueprint, FuncGraph, true, (UFunction*)nullptr);

		UK2Node_FunctionEntry* EntryNode = nullptr;
		UK2Node_FunctionResult* ResultNode = nullptr;
		for (UEdGraphNode* Node : FuncGraph->Nodes)
		{
			if (!Node)
			{
				continue;
			}
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			{
				EntryNode = Entry;
			}
			else if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
			{
				ResultNode = Result;
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* PinsJson = nullptr;
		if (!NodeObj->TryGetArrayField(TEXT("pins"), PinsJson) || !PinsJson)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& PinVal : *PinsJson)
		{
			const TSharedPtr<FJsonObject> PinObj = JsonValueAsObject(PinVal);
			if (!PinObj.IsValid())
			{
				continue;
			}

			FString PinName;
			TryGetPinName(PinObj, PinName);
			if (PinName.IsEmpty() || PinName == TEXT("self"))
			{
				continue;
			}

			const bool bIsExec = PinObj->HasField(TEXT("isExec")) && PinObj->GetBoolField(TEXT("isExec"));
			if (bIsExec)
			{
				continue;
			}

			FString Direction;
			PinObj->TryGetStringField(TEXT("direction"), Direction);
			const FEdGraphPinType PinType = BuildPinTypeFromJson(PinObj);

			if (Direction == TEXT("Input") && EntryNode)
			{
				EntryNode->CreateUserDefinedPin(*PinName, PinType, EGPD_Output);
			}
			else if (Direction == TEXT("Output") && ResultNode)
			{
				ResultNode->CreateUserDefinedPin(*PinName, PinType, EGPD_Input);
			}
		}
	}

	static void EnsureLocalVariablesFromFunctionEntry(UBlueprint* Blueprint, UEdGraph* Graph, const TSharedPtr<FJsonObject>& NodeObj)
	{
		if (!Blueprint || !Graph || !NodeObj.IsValid())
		{
			return;
		}

		FString NodeType;
		NodeObj->TryGetStringField(TEXT("nodeType"), NodeType);
		if (NodeType != TEXT("K2Node_FunctionEntry"))
		{
			return;
		}

		if (!NodeObj->HasField(TEXT("nodeProperties")))
		{
			return;
		}

		const TSharedPtr<FJsonObject> Props = NodeObj->GetObjectField(TEXT("nodeProperties"));
		if (!Props.IsValid())
		{
			return;
		}

		FString LocalVars;
		if (!Props->TryGetStringField(TEXT("LocalVariables"), LocalVars) || LocalVars.IsEmpty())
		{
			return;
		}

		int32 SearchIndex = 0;
		while (true)
		{
			const int32 VarStart = LocalVars.Find(TEXT("VarName=\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchIndex);
			if (VarStart == INDEX_NONE)
			{
				break;
			}

			int32 BlockEnd = LocalVars.Find(TEXT("),(VarName=\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, VarStart);
			if (BlockEnd == INDEX_NONE)
			{
				BlockEnd = LocalVars.Find(TEXT("))"), ESearchCase::IgnoreCase, ESearchDir::FromStart, VarStart);
			}
			const FString Block = (BlockEnd == INDEX_NONE) ? LocalVars.Mid(VarStart) : LocalVars.Mid(VarStart, BlockEnd - VarStart);

			const FString VarName = ExtractLocalVarField(Block, TEXT("VarName"));
			if (VarName.IsEmpty())
			{
				SearchIndex = (BlockEnd == INDEX_NONE) ? LocalVars.Len() : BlockEnd + 1;
				continue;
			}

			FString VarTypeString;
			{
				const int32 TypeStart = Block.Find(TEXT("VarType=("), ESearchCase::IgnoreCase, ESearchDir::FromStart);
				if (TypeStart != INDEX_NONE)
				{
					const int32 TypeEnd = Block.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, TypeStart);
					if (TypeEnd != INDEX_NONE)
					{
						VarTypeString = Block.Mid(TypeStart + FString(TEXT("VarType=(")).Len(), TypeEnd - (TypeStart + FString(TEXT("VarType=(")).Len()));
					}
				}
			}

			const FEdGraphPinType VarPinType = BuildPinTypeFromLocalVarString(VarTypeString);
			if (VarPinType.PinCategory == NAME_None)
			{
				SearchIndex = (BlockEnd == INDEX_NONE) ? LocalVars.Len() : BlockEnd + 1;
				continue;
			}

			if (FBlueprintEditorUtils::FindLocalVariable(Blueprint, Graph, *VarName) == nullptr)
			{
				const FString DefaultValue = ExtractLocalVarField(Block, TEXT("DefaultValue"));
				FBlueprintEditorUtils::AddLocalVariable(Blueprint, Graph, *VarName, VarPinType, DefaultValue);
			}

			SearchIndex = (BlockEnd == INDEX_NONE) ? LocalVars.Len() : BlockEnd + 1;
		}

		// Also seed local variables for function input pins (parameters)
		const TArray<TSharedPtr<FJsonValue>>* PinsJson = nullptr;
		if (NodeObj->TryGetArrayField(TEXT("pins"), PinsJson) && PinsJson)
		{
			for (const TSharedPtr<FJsonValue>& PinVal : *PinsJson)
			{
				const TSharedPtr<FJsonObject> PinObj = JsonValueAsObject(PinVal);
				if (!PinObj.IsValid())
				{
					continue;
				}
				FString PinName;
				if (!TryGetPinName(PinObj, PinName) || PinName.IsEmpty())
				{
					continue;
				}
				if (PinName.Equals(TEXT("then"), ESearchCase::IgnoreCase) || PinName.Equals(TEXT("self"), ESearchCase::IgnoreCase))
				{
					continue;
				}
				const bool bIsExec = PinObj->HasField(TEXT("isExec")) && PinObj->GetBoolField(TEXT("isExec"));
				if (bIsExec)
				{
					continue;
				}
				FString Direction;
				PinObj->TryGetStringField(TEXT("direction"), Direction);
				if (Direction != TEXT("Output"))
				{
					continue;
				}

				if (FBlueprintEditorUtils::FindLocalVariable(Blueprint, Graph, *PinName) != nullptr)
				{
					continue;
				}

				const FEdGraphPinType PinType = BuildPinTypeFromJson(PinObj);
				if (PinType.PinCategory == NAME_None)
				{
					continue;
				}

				FString DefaultValue;
				PinObj->TryGetStringField(TEXT("defaultValue"), DefaultValue);
				FBlueprintEditorUtils::AddLocalVariable(Blueprint, Graph, *PinName, PinType, DefaultValue);
			}
		}
	}

	static void AddMemberVariableFromSummary(UBlueprint* Blueprint, const FString& Summary)
	{
		if (!Blueprint || Summary.IsEmpty())
		{
			return;
		}

		if (Summary.Contains(TEXT("[Inherited]")))
		{
			return;
		}

		int32 ParenIndex = INDEX_NONE;
		if (!Summary.FindLastChar(TEXT('('), ParenIndex))
		{
			return;
		}

		const FString NamePart = Summary.Left(ParenIndex).TrimEnd();
		const int32 CloseIndex = Summary.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ParenIndex);
		if (CloseIndex == INDEX_NONE)
		{
			return;
		}

		const FString TypePart = Summary.Mid(ParenIndex + 1, CloseIndex - ParenIndex - 1).TrimStartAndEnd();
		if (NamePart.IsEmpty() || TypePart.IsEmpty())
		{
			return;
		}

		TSet<FName> ExistingVars;
		FBlueprintEditorUtils::GetClassVariableList(Blueprint, ExistingVars);
		if (ExistingVars.Contains(*NamePart))
		{
			return;
		}

		FEdGraphPinType PinType;
		if (TypePart.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		}
		else if (TypePart.Equals(TEXT("byte"), ESearchCase::IgnoreCase))
		{
			// Skip raw byte in summary list to avoid stomping enum-typed variables
			return;
		}
		else if (TypePart.Equals(TEXT("int"), ESearchCase::IgnoreCase) || TypePart.Equals(TEXT("int32"), ESearchCase::IgnoreCase))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		}
		else if (TypePart.Equals(TEXT("float"), ESearchCase::IgnoreCase))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
			PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		}
		else if (TypePart.Equals(TEXT("double"), ESearchCase::IgnoreCase) || TypePart.Equals(TEXT("real"), ESearchCase::IgnoreCase))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
			PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		}
		else if (TypePart.Equals(TEXT("string"), ESearchCase::IgnoreCase))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_String;
		}
		else if (TypePart.Equals(TEXT("name"), ESearchCase::IgnoreCase))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		}
		else if (TypePart.Equals(TEXT("text"), ESearchCase::IgnoreCase))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
		}
		else if (TypePart.Equals(TEXT("struct"), ESearchCase::IgnoreCase))
		{
			// Skip struct types without explicit object path
			return;
		}
		else if (TypePart.Equals(TEXT("object"), ESearchCase::IgnoreCase))
		{
			// Skip object types without explicit class
			return;
		}

		if (PinType.PinCategory == NAME_None)
		{
			return;
		}

		FBlueprintEditorUtils::AddMemberVariable(Blueprint, *NamePart, PinType);
	}
}

FString UBlueprintSerializerBlueprintLibrary::RoundTripAuditSingleBlueprint(UBlueprint* TargetBlueprint)
{
#if WITH_EDITOR
	if (!TargetBlueprint)
	{
		return FString();
	}

	// Serialize original blueprint to JSON
	const FString JsonData = UBlueprintSerializerBlueprintLibrary::SerializeSingleBlueprint(TargetBlueprint);
	TSharedPtr<FJsonObject> Root;
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return FString();
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* GraphsJson = nullptr;
	if (!Root->TryGetArrayField(TEXT("structuredGraphs"), GraphsJson) || !GraphsJson)
	{
		return FString();
	}

	TMap<FGuid, TSharedPtr<FJsonObject>> NodeDetailsByGuid;
	const TArray<TSharedPtr<FJsonValue>>* GraphNodesJson = nullptr;
	if (Root->TryGetArrayField(TEXT("graphNodes"), GraphNodesJson) && GraphNodesJson)
	{
		for (const TSharedPtr<FJsonValue>& NodeVal : *GraphNodesJson)
		{
			if (!NodeVal.IsValid())
			{
				continue;
			}

			TSharedPtr<FJsonObject> NodeObj = JsonValueAsObject(NodeVal);
			if (!NodeObj.IsValid())
			{
				continue;
			}

			FString GuidString;
			if (NodeObj->TryGetStringField(TEXT("nodeGuid"), GuidString))
			{
				const FGuid Guid = ParseGuid(GuidString);
				if (Guid.IsValid())
				{
					NodeDetailsByGuid.Add(Guid, NodeObj);
				}
			}
		}
	}

	// Create transient blueprint
	UClass* ParentClass = TargetBlueprint->ParentClass;
	if (!ParentClass)
	{
		ParentClass = AActor::StaticClass();
	}
	UBlueprint* TempBP = FKismetEditorUtilities::CreateBlueprint(
		ParentClass,
		GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("BP_SLZR_RoundTrip")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		FName(TEXT("BlueprintSerializer"))
	);

	if (!TempBP)
	{
		return FString();
	}

	// Implement interfaces before graph reconstruction
	const TArray<TSharedPtr<FJsonValue>>* InterfacePathsJson = nullptr;
	if (Root->TryGetArrayField(TEXT("implementedInterfacePaths"), InterfacePathsJson) && InterfacePathsJson)
	{
		for (const TSharedPtr<FJsonValue>& IfaceVal : *InterfacePathsJson)
		{
			if (!IfaceVal.IsValid() || IfaceVal->Type != EJson::String)
			{
				continue;
			}

			const FString InterfacePathString = IfaceVal->AsString();
			if (InterfacePathString.IsEmpty())
			{
				continue;
			}

			const FTopLevelAssetPath InterfaceAssetPath(InterfacePathString);
			if (InterfaceAssetPath.IsValid())
			{
				FBlueprintEditorUtils::ImplementNewInterface(TempBP, InterfaceAssetPath);
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* InterfacesJson = nullptr;
	if (Root->TryGetArrayField(TEXT("implementedInterfaces"), InterfacesJson) && InterfacesJson)
	{
		for (const TSharedPtr<FJsonValue>& IfaceVal : *InterfacesJson)
		{
			if (!IfaceVal.IsValid() || IfaceVal->Type != EJson::String)
			{
				continue;
			}
			const FString IfaceName = IfaceVal->AsString();
			if (!IfaceName.IsEmpty())
			{
				if (UClass* InterfaceClass = FindClassBySimpleName(IfaceName))
				{
					FBlueprintEditorUtils::ImplementNewInterface(TempBP, InterfaceClass->GetClassPathName());
				}
			}
		}
	}

	EnsureComponentsFromJson(TempBP, Root);

	// Pre-seed member variables from summary list (enriched with pin-derived types)
	const TArray<TSharedPtr<FJsonValue>>* VariablesJson = nullptr;
	if (Root->TryGetArrayField(TEXT("variables"), VariablesJson) && VariablesJson)
	{
		TMap<FString, FEdGraphPinType> VarTypeByName;
		const TArray<TSharedPtr<FJsonValue>>* DetailedVarsJson = nullptr;
		if (Root->TryGetArrayField(TEXT("detailedVariables"), DetailedVarsJson) && DetailedVarsJson)
		{
			for (const TSharedPtr<FJsonValue>& VarVal : *DetailedVarsJson)
			{
			const TSharedPtr<FJsonObject> VarObj = JsonValueAsObject(VarVal);
				if (!VarObj.IsValid())
				{
					continue;
				}

				FString VarName;
				FString TypeString;
				VarObj->TryGetStringField(TEXT("name"), VarName);
				VarObj->TryGetStringField(TEXT("type"), TypeString);
				if (VarName.IsEmpty() || TypeString.IsEmpty())
				{
					continue;
				}

				FEdGraphPinType PinType;
				if (BuildPinTypeFromDetailedVarType(TypeString, PinType))
				{
					VarTypeByName.Add(VarName, PinType);
				}
			}
		}

		if (NodeDetailsByGuid.Num() > 0)
		{
			for (const TPair<FGuid, TSharedPtr<FJsonObject>>& Pair : NodeDetailsByGuid)
			{
				const TSharedPtr<FJsonObject> NodeObj = Pair.Value;
				if (!NodeObj.IsValid())
				{
					continue;
				}

				FString NodeType;
				NodeObj->TryGetStringField(TEXT("nodeType"), NodeType);
				if (NodeType != TEXT("K2Node_VariableGet") && NodeType != TEXT("K2Node_VariableSet"))
				{
					continue;
				}

				FString VarName;
				NodeObj->TryGetStringField(TEXT("variableName"), VarName);
				if (NodeObj->HasField(TEXT("nodeProperties")))
				{
					const TSharedPtr<FJsonObject> Props = NodeObj->GetObjectField(TEXT("nodeProperties"));
					if (Props.IsValid() && Props->HasField(TEXT("VariableReference")))
					{
						const FString MemberName = ExtractMemberNameFromReference(Props->GetStringField(TEXT("VariableReference")));
						if (!MemberName.IsEmpty())
						{
							VarName = MemberName;
						}
					}
				}
				if (VarName.IsEmpty())
				{
					continue;
				}

				const TArray<TSharedPtr<FJsonValue>>* PinsJson = nullptr;
				if (!NodeObj->TryGetArrayField(TEXT("pins"), PinsJson) || !PinsJson)
				{
					continue;
				}

				TSharedPtr<FJsonObject> VarPinObj;
				for (const TSharedPtr<FJsonValue>& PinVal : *PinsJson)
				{
					const TSharedPtr<FJsonObject> PinObj = JsonValueAsObject(PinVal);
					if (!PinObj.IsValid())
					{
						continue;
					}
					FString PinName;
					TryGetPinName(PinObj, PinName);
					if (PinName == VarName)
					{
						VarPinObj = PinObj;
						break;
					}
				}

				if (!VarPinObj.IsValid())
				{
					for (const TSharedPtr<FJsonValue>& PinVal : *PinsJson)
					{
						const TSharedPtr<FJsonObject> PinObj = JsonValueAsObject(PinVal);
						if (!PinObj.IsValid())
						{
							continue;
						}
						const bool bIsExec = PinObj->HasField(TEXT("isExec")) && PinObj->GetBoolField(TEXT("isExec"));
						FString PinName;
						TryGetPinName(PinObj, PinName);
						FString Direction;
						PinObj->TryGetStringField(TEXT("direction"), Direction);
						if (bIsExec || PinName == TEXT("self") || PinName == TEXT("Output_Get"))
						{
							continue;
						}
						const bool bIsSet = (NodeType == TEXT("K2Node_VariableSet"));
						if ((bIsSet && Direction == TEXT("Input")) || (!bIsSet && Direction == TEXT("Output")))
						{
							VarPinObj = PinObj;
							break;
						}
					}
				}

				if (VarPinObj.IsValid())
				{
					const FEdGraphPinType PinType = BuildPinTypeFromJson(VarPinObj);
					if (PinType.PinCategory != NAME_None)
					{
						VarTypeByName.Add(VarName, PinType);
					}
				}
			}
		}

		for (const TSharedPtr<FJsonValue>& VarVal : *VariablesJson)
		{
			if (!VarVal.IsValid() || VarVal->Type != EJson::String)
			{
				continue;
			}
			const FString Summary = VarVal->AsString();
			FString VarName = Summary;
			const int32 SpaceIndex = Summary.Find(TEXT(" ("));
			if (SpaceIndex != INDEX_NONE)
			{
				VarName = Summary.Left(SpaceIndex);
			}
			if (const FEdGraphPinType* FoundType = VarTypeByName.Find(VarName))
			{
				TSet<FName> ExistingVars;
				FBlueprintEditorUtils::GetClassVariableList(TempBP, ExistingVars);
				if (!ExistingVars.Contains(*VarName))
				{
					FBlueprintEditorUtils::AddMemberVariable(TempBP, *VarName, *FoundType);
				}
			}
			else
			{
				AddMemberVariableFromSummary(TempBP, Summary);
			}
		}
	}

	// Ensure skeleton reflects components and member variables before graph construction
	FKismetEditorUtilities::GenerateBlueprintSkeleton(TempBP, true);

	TSet<FString> UnsupportedNodes;
	TMap<FGuid, UEdGraphNode*> NodeByGuid;
	TMap<FGuid, FString> NodeTypeByGuid;
	const bool bAllowStubFunctions = false;

	// Build graphs and nodes
	for (const TSharedPtr<FJsonValue>& GraphVal : *GraphsJson)
	{
		const TSharedPtr<FJsonObject> GraphObj = JsonValueAsObject(GraphVal);
		if (!GraphObj.IsValid())
		{
			continue;
		}

		FString GraphName;
		FString GraphType;
		GraphObj->TryGetStringField(TEXT("name"), GraphName);
		GraphObj->TryGetStringField(TEXT("graphType"), GraphType);
		UEdGraph* Graph = nullptr;

		if (GraphType == TEXT("Ubergraph"))
		{
			const FName Name = GraphName.IsEmpty() ? FName(TEXT("EventGraph")) : FName(*GraphName);
			Graph = FBlueprintEditorUtils::CreateNewGraph(TempBP, Name, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
			FBlueprintEditorUtils::AddUbergraphPage(TempBP, Graph);
		}
		else if (GraphType == TEXT("Function"))
		{
			const FName Name = GraphName.IsEmpty() ? FName(TEXT("FunctionGraph")) : FName(*GraphName);
			Graph = FBlueprintEditorUtils::CreateNewGraph(TempBP, Name, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
			FBlueprintEditorUtils::AddFunctionGraph(TempBP, Graph, true, (UFunction*)nullptr);
		}
		else if (GraphType == TEXT("Macro"))
		{
			const FName Name = GraphName.IsEmpty() ? FName(TEXT("MacroGraph")) : FName(*GraphName);
			Graph = FBlueprintEditorUtils::CreateNewGraph(TempBP, Name, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
			FBlueprintEditorUtils::AddMacroGraph(TempBP, Graph, true, nullptr);
		}

		if (!Graph)
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* NodesJson = nullptr;
		if (!GraphObj->TryGetArrayField(TEXT("nodes"), NodesJson) || !NodesJson)
		{
			continue;
		}

		// Ensure member variables and stub functions exist before node creation
		for (const TSharedPtr<FJsonValue>& NodeVal : *NodesJson)
		{
			TSharedPtr<FJsonObject> NodeObj = JsonValueAsObject(NodeVal);
			if (!NodeObj.IsValid())
			{
				continue;
			}

			FString NodeGuidString;
			if (NodeObj->TryGetStringField(TEXT("nodeGuid"), NodeGuidString))
			{
				const FGuid NodeGuid = ParseGuid(NodeGuidString);
				if (NodeGuid.IsValid())
				{
					if (const TSharedPtr<FJsonObject>* FullNode = NodeDetailsByGuid.Find(NodeGuid))
					{
						if (FullNode->IsValid())
						{
							NodeObj = *FullNode;
						}
					}
				}
			}

			EnsureVariableFromNodeJson(TempBP, NodeObj);
			if (bAllowStubFunctions)
			{
				EnsureStubFunctionFromCallNodeJson(TempBP, NodeObj);
			}
			EnsureLocalVariablesFromFunctionEntry(TempBP, Graph, NodeObj);
		}

		// Create nodes
		for (const TSharedPtr<FJsonValue>& NodeVal : *NodesJson)
		{
			TSharedPtr<FJsonObject> NodeObj = JsonValueAsObject(NodeVal);
			if (!NodeObj.IsValid() && NodeVal->Type == EJson::String)
			{
				// Legacy: node stored as JSON string
				const FString NodeString = NodeVal->AsString();
				TSharedRef<TJsonReader<>> NodeReader = TJsonReaderFactory<>::Create(NodeString);
				TSharedPtr<FJsonObject> Parsed;
				if (FJsonSerializer::Deserialize(NodeReader, Parsed))
				{
					NodeObj = Parsed;
				}
			}

			if (!NodeObj.IsValid())
			{
				continue;
			}

			FString NodeGuidString;
			if (NodeObj->TryGetStringField(TEXT("nodeGuid"), NodeGuidString))
			{
				const FGuid NodeGuid = ParseGuid(NodeGuidString);
				if (NodeGuid.IsValid())
				{
					if (const TSharedPtr<FJsonObject>* FullNode = NodeDetailsByGuid.Find(NodeGuid))
					{
						if (FullNode->IsValid())
						{
							NodeObj = *FullNode;
						}
					}
				}
			}

			FNodeBuildResult Built = BuildNodeFromJson(TempBP, Graph, NodeObj, GraphType, UnsupportedNodes);
			if (Built.Node)
			{
				const TArray<TSharedPtr<FJsonValue>>* PinsJson = nullptr;
				if (NodeObj->TryGetArrayField(TEXT("pins"), PinsJson) && PinsJson)
				{
					if (UK2Node_EditablePinBase* Editable = Cast<UK2Node_EditablePinBase>(Built.Node))
					{
						AddUserPinsFromJson(Editable, *PinsJson);
					}

					// Apply pin ids and splits based on JSON
					ApplyPinIdsAndSplits(Built.Node, *PinsJson);
				}

				NodeByGuid.Add(Built.Node->NodeGuid, Built.Node);
				FString NodeType;
				NodeObj->TryGetStringField(TEXT("nodeType"), NodeType);
				NodeTypeByGuid.Add(Built.Node->NodeGuid, NodeType);
			}
		}
	}

	// Apply defaults + connect pins
	TSet<FString> ConnectionKeys;
	for (const TPair<FGuid, UEdGraphNode*>& Pair : NodeByGuid)
	{
		UEdGraphNode* Node = Pair.Value;
		if (!Node)
		{
			continue;
		}
		const FString NodeGuidStr = Pair.Key.ToString();

		// Find original node JSON (prefer full node details with connections)
		TSharedPtr<FJsonObject> NodeObj;
		if (const TSharedPtr<FJsonObject>* FullNode = NodeDetailsByGuid.Find(Pair.Key))
		{
			if (FullNode->IsValid())
			{
				NodeObj = *FullNode;
			}
		}

		if (!NodeObj.IsValid())
		{
			for (const TSharedPtr<FJsonValue>& GraphVal : *GraphsJson)
			{
				const TSharedPtr<FJsonObject> GraphObj = JsonValueAsObject(GraphVal);
				if (!GraphObj.IsValid())
				{
					continue;
				}
				const TArray<TSharedPtr<FJsonValue>>* NodesJson = nullptr;
				if (!GraphObj->TryGetArrayField(TEXT("nodes"), NodesJson) || !NodesJson)
				{
					continue;
				}
				for (const TSharedPtr<FJsonValue>& NodeVal : *NodesJson)
				{
					TSharedPtr<FJsonObject> Tmp = JsonValueAsObject(NodeVal);
					if (!Tmp.IsValid() && NodeVal->Type == EJson::String)
					{
						const FString NodeString = NodeVal->AsString();
						TSharedRef<TJsonReader<>> NodeReader = TJsonReaderFactory<>::Create(NodeString);
						TSharedPtr<FJsonObject> Parsed;
						if (FJsonSerializer::Deserialize(NodeReader, Parsed))
						{
							Tmp = Parsed;
						}
					}
					FString TmpGuid;
					if (Tmp.IsValid() && Tmp->TryGetStringField(TEXT("nodeGuid"), TmpGuid) && TmpGuid == NodeGuidStr)
					{
						NodeObj = Tmp;
						break;
					}
				}
				if (NodeObj.IsValid())
				{
					break;
				}
			}
		}

		if (!NodeObj.IsValid())
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* PinsJson = nullptr;
		if (!NodeObj->TryGetArrayField(TEXT("pins"), PinsJson) || !PinsJson)
		{
			continue;
		}

		for (const TSharedPtr<FJsonValue>& PinVal : *PinsJson)
		{
			TSharedPtr<FJsonObject> PinObj = JsonValueAsObject(PinVal);
			if (!PinObj.IsValid())
			{
				continue;
			}

			FString PinName;
			TryGetPinName(PinObj, PinName);
			UEdGraphPin* Pin = Node->FindPin(*PinName);
			if (!Pin)
			{
				continue;
			}

			ApplyDefaultValueToPin(Pin, PinObj);

			const TArray<TSharedPtr<FJsonValue>>* Connections = nullptr;
			if (!PinObj->TryGetArrayField(TEXT("connections"), Connections) || !Connections)
			{
				continue;
			}

			for (const TSharedPtr<FJsonValue>& ConnVal : *Connections)
			{
				const TSharedPtr<FJsonObject> ConnObj = JsonValueAsObject(ConnVal);
				if (!ConnObj.IsValid())
				{
					continue;
				}

				FString OtherGuidStr;
				FString OtherPinName;
				ConnObj->TryGetStringField(TEXT("connectedNodeGuid"), OtherGuidStr);
				ConnObj->TryGetStringField(TEXT("connectedPinName"), OtherPinName);
				const FGuid OtherGuid = ParseGuid(OtherGuidStr);

				UEdGraphNode* OtherNode = NodeByGuid.FindRef(OtherGuid);
				if (!OtherNode)
				{
					continue;
				}
				UEdGraphPin* OtherPin = OtherNode->FindPin(*OtherPinName);
				if (!OtherPin)
				{
					continue;
				}
				UEdGraph* PinGraph = Pin->GetOwningNode() ? Pin->GetOwningNode()->GetGraph() : nullptr;
				UEdGraph* OtherGraph = OtherPin->GetOwningNode() ? OtherPin->GetOwningNode()->GetGraph() : nullptr;
				if (!PinGraph || !OtherGraph || PinGraph != OtherGraph)
				{
					continue;
				}

				const FString Key = NodeGuidStr + TEXT("|") + PinName + TEXT("|") + OtherGuidStr + TEXT("|") + OtherPinName;
				const FString ReverseKey = OtherGuidStr + TEXT("|") + OtherPinName + TEXT("|") + NodeGuidStr + TEXT("|") + PinName;
				if (ConnectionKeys.Contains(Key) || ConnectionKeys.Contains(ReverseKey))
				{
					continue;
				}
				ConnectionKeys.Add(Key);

				// Align pin types when one side lost metadata (common for containers)
				if (Pin->PinType.PinCategory == OtherPin->PinType.PinCategory)
				{
					if (Pin->PinType.PinSubCategory.IsNone() && !OtherPin->PinType.PinSubCategory.IsNone())
					{
						Pin->PinType.PinSubCategory = OtherPin->PinType.PinSubCategory;
					}
					else if (OtherPin->PinType.PinSubCategory.IsNone() && !Pin->PinType.PinSubCategory.IsNone())
					{
						OtherPin->PinType.PinSubCategory = Pin->PinType.PinSubCategory;
					}

					if (!Pin->PinType.PinSubCategoryObject.IsValid() && OtherPin->PinType.PinSubCategoryObject.IsValid())
					{
						Pin->PinType.PinSubCategoryObject = OtherPin->PinType.PinSubCategoryObject;
					}
					else if (!OtherPin->PinType.PinSubCategoryObject.IsValid() && Pin->PinType.PinSubCategoryObject.IsValid())
					{
						OtherPin->PinType.PinSubCategoryObject = Pin->PinType.PinSubCategoryObject;
					}

					if (Pin->PinType.ContainerType == EPinContainerType::None && OtherPin->PinType.ContainerType != EPinContainerType::None)
					{
						Pin->PinType.ContainerType = OtherPin->PinType.ContainerType;
					}
					else if (OtherPin->PinType.ContainerType == EPinContainerType::None && Pin->PinType.ContainerType != EPinContainerType::None)
					{
						OtherPin->PinType.ContainerType = Pin->PinType.ContainerType;
					}
				}

				UEdGraphSchema_K2 const* Schema = GetDefault<UEdGraphSchema_K2>();
				if (Schema)
				{
					const bool bConnected = Schema->TryCreateConnection(Pin, OtherPin);
					if (!bConnected)
					{
						const bool bIsContainer = (Pin->PinType.ContainerType != EPinContainerType::None) ||
							(OtherPin->PinType.ContainerType != EPinContainerType::None);
						if (bIsContainer)
						{
							Pin->MakeLinkTo(OtherPin);
						}
					}
				}
				else
				{
					Pin->MakeLinkTo(OtherPin);
				}
			}
		}
	}

	// Refresh create-delegate nodes after connections to preserve selected function/signature
	for (const TPair<FGuid, UEdGraphNode*>& Pair : NodeByGuid)
	{
		if (UK2Node_CreateDelegate* CreateDelegate = Cast<UK2Node_CreateDelegate>(Pair.Value))
		{
			CreateDelegate->HandleAnyChangeWithoutNotifying();
		}
	}

	// Normalize enum pin subcategories on connected byte pins
	for (const TPair<FGuid, UEdGraphNode*>& Pair : NodeByGuid)
	{
		UEdGraphNode* Node = Pair.Value;
		if (!Node)
		{
			continue;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Byte)
			{
				continue;
			}
			if (Pin->PinType.PinSubCategoryObject.IsValid())
			{
				continue;
			}
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked && Cast<UEnum>(Linked->PinType.PinSubCategoryObject.Get()))
				{
					Pin->PinType.PinSubCategoryObject = Linked->PinType.PinSubCategoryObject;
					break;
				}
			}
		}
	}

	// Fix enum defaults after normalization
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	for (const TPair<FGuid, UEdGraphNode*>& Pair : NodeByGuid)
	{
		UEdGraphNode* Node = Pair.Value;
		if (!Node)
		{
			continue;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}
			if (UEnum* EnumObj = Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get()))
			{
				if (Pin->DefaultValue.IsEmpty() || Pin->DefaultValue.Equals(TEXT("(INVALID)"), ESearchCase::IgnoreCase))
				{
					const FString FirstEnum = EnumObj->GetNameStringByIndex(0);
					if (!FirstEnum.IsEmpty() && Schema)
					{
						Schema->TrySetDefaultValue(*Pin, FirstEnum);
					}
				}
			}
		}
	}

	// Final restore of Create Delegate selections right before compile
	for (const TPair<FGuid, UEdGraphNode*>& Pair : NodeByGuid)
	{
		if (UK2Node_CreateDelegate* CreateDelegate = Cast<UK2Node_CreateDelegate>(Pair.Value))
		{
			if (const TSharedPtr<FJsonObject>* FullNode = NodeDetailsByGuid.Find(Pair.Key))
			{
				RestoreCreateDelegateFromJson(CreateDelegate, *FullNode);
				CreateDelegate->HandleAnyChangeWithoutNotifying();
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(TempBP);
	FCompilerResultsLog CompileLog;
	FKismetEditorUtilities::CompileBlueprint(TempBP, EBlueprintCompileOptions::None, &CompileLog);

	// Collect raw in-memory fingerprints for diagnostics only. These values are
	// not a semantic round-trip gate because Script embeds process-local IDs and
	// addresses, and reconstructed objects necessarily have different identities.
	TMap<FString, FString> OriginalRawBytecodeMd5;
	TMap<FString, FString> RoundTripRawBytecodeMd5;
	auto CollectRawBytecodeMd5 = [](UBlueprint* BP, TMap<FString, FString>& Out)
	{
		if (!BP || !BP->GeneratedClass)
		{
			return;
		}
		for (TFieldIterator<UFunction> FuncIt(BP->GeneratedClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Func = *FuncIt;
			if (!Func)
			{
				continue;
			}
			const TArray<uint8>& Script = Func->Script;
			const FString Hash = FMD5::HashBytes(Script.GetData(), Script.Num());
			Out.Add(Func->GetName(), Hash);
		}
	};
	CollectRawBytecodeMd5(TargetBlueprint, OriginalRawBytecodeMd5);
	CollectRawBytecodeMd5(TempBP, RoundTripRawBytecodeMd5);

	TArray<TSharedPtr<FJsonValue>> RawDiagnosticDifferences;
	for (const TPair<FString, FString>& Pair : OriginalRawBytecodeMd5)
	{
		const FString* NewHash = RoundTripRawBytecodeMd5.Find(Pair.Key);
		if (!NewHash || *NewHash != Pair.Value)
		{
			TSharedPtr<FJsonObject> Entry = MakeShareable(new FJsonObject);
			Entry->SetStringField(TEXT("functionName"), Pair.Key);
			Entry->SetObjectField(
				TEXT("original"),
				BuildRawInMemoryBytecodeDiagnostic(Pair.Value));
			Entry->SetObjectField(
				TEXT("roundTrip"),
				BuildRawInMemoryBytecodeDiagnostic(NewHash ? *NewHash : TEXT("")));
			RawDiagnosticDifferences.Add(MakeShareable(new FJsonValueObject(Entry)));
		}
	}

	// Report
	TSharedPtr<FJsonObject> Report = MakeShareable(new FJsonObject);
	Report->SetStringField(TEXT("schemaVersion"), TEXT("1.7"));
	Report->SetStringField(TEXT("blueprintName"), TargetBlueprint->GetName());
	Report->SetStringField(TEXT("blueprintPath"), TargetBlueprint->GetPathName());
	Report->SetNumberField(TEXT("originalFunctionCount"), OriginalRawBytecodeMd5.Num());
	Report->SetNumberField(TEXT("roundTripFunctionCount"), RoundTripRawBytecodeMd5.Num());
	Report->SetArrayField(TEXT("rawInMemoryBytecodeDiagnosticDifferences"), RawDiagnosticDifferences);
	Report->SetBoolField(TEXT("rawBytecodeDifferencesAffectRoundTripResult"), false);
	Report->SetNumberField(TEXT("compileErrorCount"), CompileLog.NumErrors);
	Report->SetNumberField(TEXT("compileWarningCount"), CompileLog.NumWarnings);
	if (CompileLog.Messages.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Messages;
		for (const TSharedRef<FTokenizedMessage>& Msg : CompileLog.Messages)
		{
			TSharedPtr<FJsonObject> MsgObj = MakeShareable(new FJsonObject);
			MsgObj->SetStringField(TEXT("severity"), Msg->GetSeverity() == EMessageSeverity::Error ? TEXT("Error") :
				(Msg->GetSeverity() == EMessageSeverity::Warning ? TEXT("Warning") : TEXT("Info")));
			MsgObj->SetStringField(TEXT("message"), Msg->ToText().ToString());
			Messages.Add(MakeShareable(new FJsonValueObject(MsgObj)));
		}
		Report->SetArrayField(TEXT("compileMessages"), Messages);
	}

	TArray<TSharedPtr<FJsonValue>> Unsupported;
	for (const FString& Name : UnsupportedNodes)
	{
		Unsupported.Add(MakeShareable(new FJsonValueString(Name)));
	}
	Report->SetArrayField(TEXT("unsupportedNodeTypes"), Unsupported);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);

	const FString ExportPath = UDataExportManager::GetDefaultExportPath();
	const FString ArtifactToken = BuildBlueprintArtifactToken(TargetBlueprint->GetName(), TargetBlueprint->GetPathName());
	const FString FileName = FString::Printf(TEXT("BP_SLZR_ROUNDTRIP_%s_%s.json"), *ArtifactToken, *UDataExportManager::GetTimestamp());
	const FString FullPath = FPaths::Combine(ExportPath, FileName);
	if (UBlueprintAnalyzer::SaveBlueprintDataToFile(Output, FullPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Round-trip audit saved: %s"), *FullPath);
		return FullPath;
	}

	return FString();
#else
	return FString();
#endif
}
