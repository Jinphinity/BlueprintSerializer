#include "BlueprintExtractorCommands.h"
#include "BlueprintAnalyzer.h"
#include "DataExportManager.h"
#include "AssetReferenceExtractor.h"
#include "Dom/JsonObject.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimCurveMetadata.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/Blueprint.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/IConsoleManager.h"
#endif

#if WITH_EDITOR
// Console command definitions - using static local variables to avoid linker issues
static FAutoConsoleCommand ExportAllBlueprintsCommand(
    TEXT("BP_SLZR.ExportAllBlueprints"),
    TEXT("Export analysis data for all Blueprints in the project"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FBlueprintExtractorCommands::ExportAllBlueprints)
);

static FAutoConsoleCommand ExportCompleteDataCommand(
    TEXT("BP_SLZR.ExportCompleteData"),
    TEXT("Export complete project data (Blueprints + Asset References)"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FBlueprintExtractorCommands::ExportCompleteProjectData)
);

static FAutoConsoleCommand CountBlueprintsCommand(
    TEXT("BP_SLZR.CountBlueprints"),
    TEXT("Count all Blueprints in the project"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FBlueprintExtractorCommands::CountProjectBlueprints)
);

static FAutoConsoleCommand AnalyzeBlueprintCommand(
    TEXT("BP_SLZR.AnalyzeBlueprint"),
    TEXT("Analyze a specific Blueprint. Usage: BP_SLZR.AnalyzeBlueprint /Game/Path/To/Blueprint"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FBlueprintExtractorCommands::AnalyzeSpecificBlueprint)
);

static FAutoConsoleCommand ExportSingleBlueprintCommand(
    TEXT("BP_SLZR.ExportSingleBlueprint"),
    TEXT("Export complete data for a single Blueprint to JSON. Usage: BP_SLZR.ExportSingleBlueprint /Game/Path/To/Blueprint"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FBlueprintExtractorCommands::ExportSingleBlueprint)
);

// Advanced Reference Viewer equivalent commands
static FAutoConsoleCommand ExtractAssetDependenciesCommand(
    TEXT("BP_SLZR.ExtractAssetDependencies"),
    TEXT("Extract asset dependencies with depth/breadth controls. Usage: BP_SLZR.ExtractAssetDependencies <AssetPath> [MaxDepth] [bHardOnly] [bReferencersOnly]"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FBlueprintExtractorCommands::ExtractAssetDependencies)
);

static FAutoConsoleCommand ExtractProjectDependencyMapCommand(
    TEXT("BP_SLZR.ExtractProjectDependencyMap"),
    TEXT("Project-wide dependency mapping with filtering. Usage: BP_SLZR.ExtractProjectDependencyMap [MaxDepth] [AssetClassFilter]"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FBlueprintExtractorCommands::ExtractProjectDependencyMap)
);

static FAutoConsoleCommand MapAssetNetworkCommand(
    TEXT("BP_SLZR.MapAssetNetwork"),
    TEXT("Advanced network mapping with bidirectional depth control. Usage: BP_SLZR.MapAssetNetwork <AssetPath> [MaxDepth] [bBidirectional]"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FBlueprintExtractorCommands::MapAssetNetwork)
);

static FAutoConsoleCommand ExportMultipleBlueprintsCommand(
    TEXT("BP_SLZR.ExportMultipleBlueprints"),
    TEXT("Export multiple Blueprints to JSON. Usage: BP_SLZR.ExportMultipleBlueprints <Path1> <Path2> <Path3> ..."),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FBlueprintExtractorCommands::ExportMultipleBlueprints)
);

static FAutoConsoleCommand ValidateConverterReadyCommand(
    TEXT("BP_SLZR.ValidateConverterReady"),
    TEXT("Validate converter-ready export gates. Usage: BP_SLZR.ValidateConverterReady [ExportDir] [ReportPath]"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FBlueprintExtractorCommands::ValidateConverterReady)
);

static FAutoConsoleCommand AuditAnimationCurvesCommand(
    TEXT("BP_SLZR.AuditAnimationCurves"),
    TEXT("Audit animation curve corpus across all AnimSequence assets. Usage: BP_SLZR.AuditAnimationCurves [ReportPath]"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FBlueprintExtractorCommands::AuditAnimationCurves)
);

static FAutoConsoleCommand RunRegressionSuiteCommand(
    TEXT("BP_SLZR.RunRegressionSuite"),
    TEXT("Run BlueprintSerializer regression suite. Usage: BP_SLZR.RunRegressionSuite [ExportDir] [SkipExport] [BaselinePath]"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&FBlueprintExtractorCommands::RunRegressionSuite)
);
#endif

void FBlueprintExtractorCommands::RegisterCommands()
{
#if WITH_EDITOR
    UE_LOG(LogTemp, Log, TEXT("BP_SLZR commands registered"));
    UE_LOG(LogTemp, Log, TEXT("Available commands:"));
    UE_LOG(LogTemp, Warning, TEXT("  ⚠️ BP_SLZR.ExportAllBlueprints - DANGEROUS: May cause memory overload!"));
    UE_LOG(LogTemp, Warning, TEXT("  ⚠️ BP_SLZR.ExportCompleteData - DANGEROUS: May cause memory overload!"));
    UE_LOG(LogTemp, Log, TEXT("  ✅ BP_SLZR.CountBlueprints - SAFE: Count Blueprints in project"));
    UE_LOG(LogTemp, Log, TEXT("  ✅ BP_SLZR.AnalyzeBlueprint <path> - SAFE: Analyze specific Blueprint"));
    UE_LOG(LogTemp, Log, TEXT("  ✅ BP_SLZR.ExportSingleBlueprint <path> - RECOMMENDED: Export single Blueprint to JSON"));
    UE_LOG(LogTemp, Log, TEXT("  ✅ BP_SLZR.ExportMultipleBlueprints <paths...> - RECOMMENDED: Export multiple Blueprints"));
    UE_LOG(LogTemp, Log, TEXT("  ✅ BP_SLZR.ValidateConverterReady [dir] [report] - Run converter gate checks"));
    UE_LOG(LogTemp, Log, TEXT("  ✅ BP_SLZR.AuditAnimationCurves [report] - Audit AnimSequence curve corpus"));
    UE_LOG(LogTemp, Log, TEXT("  ✅ BP_SLZR.RunRegressionSuite [dir] [skipExport] - Export+validate+curve audit"));
    UE_LOG(LogTemp, Log, TEXT("Advanced Reference Viewer Commands:"));
    UE_LOG(LogTemp, Log, TEXT("  🔍 BP_SLZR.ExtractAssetDependencies <path> [depth] [hardonly] [refs] - Asset dependency analysis"));
    UE_LOG(LogTemp, Log, TEXT("  🌐 BP_SLZR.ExtractProjectDependencyMap [depth] [class] - Project-wide dependency mapping"));
    UE_LOG(LogTemp, Log, TEXT("  🕸️ BP_SLZR.MapAssetNetwork <path> [depth] [bidirectional] - Network dependency mapping"));
#endif
}

void FBlueprintExtractorCommands::UnregisterCommands()
{
#if WITH_EDITOR
    UE_LOG(LogTemp, Log, TEXT("BP_SLZR commands unregistered"));
#endif
}

void FBlueprintExtractorCommands::ExportAllBlueprints(const TArray<FString>& Args)
{
#if WITH_EDITOR
    UE_LOG(LogTemp, Log, TEXT("Starting per-Blueprint export for all project Blueprints..."));

    FString ExportDir;
    if (Args.Num() > 0)
    {
        ExportDir = Args[0];
        int32 SemicolonIndex;
        if (ExportDir.FindChar(';', SemicolonIndex))
        {
            ExportDir = ExportDir.Left(SemicolonIndex);
        }
    }
    if (ExportDir.IsEmpty())
    {
        ExportDir = FPaths::ProjectSavedDir() / TEXT("BlueprintExports") / TEXT("BP_SLZR_All");
    }

    if (FPaths::IsRelative(ExportDir))
    {
        ExportDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ExportDir);
    }
    else
    {
        ExportDir = FPaths::ConvertRelativePathToFull(ExportDir);
    }

    FPaths::NormalizeDirectoryName(ExportDir);
    FPaths::CollapseRelativeDirectories(ExportDir);

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*ExportDir))
    {
        PlatformFile.CreateDirectoryTree(*ExportDir);
    }

    try
    {
        const TArray<FAssetData> AllAssets = UBlueprintAnalyzer::CollectAllProjectBlueprintAssetData();

        UE_LOG(LogTemp, Log, TEXT("Exporting %d Blueprints to: %s"), AllAssets.Num(), *ExportDir);

        int32 SuccessCount = 0;
        int32 FailCount = 0;
        TArray<TSharedPtr<FJsonValue>> Results;
        Results.Reserve(AllAssets.Num());

        for (const FAssetData& AssetData : AllAssets)
        {
            const FString BlueprintAssetPath = AssetData.GetObjectPathString();
            const bool bSuccess = UBlueprintAnalyzer::ExportSingleBlueprintToJSON(BlueprintAssetPath, ExportDir);
            SuccessCount += bSuccess ? 1 : 0;
            FailCount += bSuccess ? 0 : 1;

            TSharedPtr<FJsonObject> Entry = MakeShareable(new FJsonObject);
            Entry->SetStringField(TEXT("blueprintPath"), BlueprintAssetPath);
            Entry->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
            Entry->SetBoolField(TEXT("success"), bSuccess);
            Results.Add(MakeShareable(new FJsonValueObject(Entry)));
        }

        TSharedPtr<FJsonObject> Manifest = MakeShareable(new FJsonObject);
        Manifest->SetStringField(TEXT("exportTimestamp"), FDateTime::Now().ToString());
        Manifest->SetStringField(TEXT("exportDirectory"), ExportDir);
        Manifest->SetNumberField(TEXT("total"), AllAssets.Num());
        Manifest->SetNumberField(TEXT("successCount"), SuccessCount);
        Manifest->SetNumberField(TEXT("failCount"), FailCount);
        Manifest->SetArrayField(TEXT("blueprints"), Results);

        FString ManifestJson;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ManifestJson);
        FJsonSerializer::Serialize(Manifest.ToSharedRef(), Writer);

        const FString ManifestName = FString::Printf(TEXT("BP_SLZR_Manifest_%s.json"), *UDataExportManager::GetTimestamp());
        const FString ManifestPath = FPaths::Combine(ExportDir, ManifestName);
        UBlueprintAnalyzer::SaveBlueprintDataToFile(ManifestJson, ManifestPath);

        UE_LOG(LogTemp, Log, TEXT("Export complete. Success: %d, Failed: %d"), SuccessCount, FailCount);
        UE_LOG(LogTemp, Log, TEXT("Manifest saved: %s"), *ManifestPath);
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("Exception during export"));
    }
#else
    UE_LOG(LogTemp, Warning, TEXT("Blueprint extraction only available in editor builds"));
#endif
}

void FBlueprintExtractorCommands::ExportCompleteProjectData(const TArray<FString>& Args)
{
#if WITH_EDITOR
	// CR-018: Routed through the unified ExportAllBlueprints pipeline.
	// This was previously delegating to UDataExportManager::ExportCompleteProjectData() (legacy monolithic path).
	UE_LOG(LogTemp, Log, TEXT("BP_SLZR.ExportCompleteData: routing through ExportAllBlueprints (unified pipeline)"));
	FBlueprintExtractorCommands::ExportAllBlueprints(Args);
#endif
}

void FBlueprintExtractorCommands::CountProjectBlueprints(const TArray<FString>& Args)
{
#if WITH_EDITOR
    const TArray<FAssetData> AllAssets = UBlueprintAnalyzer::CollectAllProjectBlueprintAssetData();
    
    UE_LOG(LogTemp, Log, TEXT("📊 Found %d Blueprints in project"), AllAssets.Num());
    
    // Show breakdown by location
    TMap<FString, int32> LocationCounts;
    for (const FAssetData& Asset : AllAssets)
    {
        FString Path = Asset.PackageName.ToString();
        FString Location = Path.Contains(TEXT("/Game/")) ? Path.Mid(0, Path.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 6)) : TEXT("Other");
        LocationCounts.FindOrAdd(Location)++;
    }
    
    UE_LOG(LogTemp, Log, TEXT("📋 Blueprint distribution:"));
    for (const auto& Pair : LocationCounts)
    {
        UE_LOG(LogTemp, Log, TEXT("  %s: %d Blueprints"), *Pair.Key, Pair.Value);
    }
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Cyan, 
            FString::Printf(TEXT("📊 Project contains %d Blueprints"), AllAssets.Num()));
    }
#endif
}

// Previously a log-only diagnostic. CR-018: now delegates to ExportSingleBlueprintToJSON
// for full JSON output. Use BP_SLZR.ExportSingleBlueprint directly for the same behavior.
void FBlueprintExtractorCommands::AnalyzeSpecificBlueprint(const TArray<FString>& Args)
{
#if WITH_EDITOR
	if (Args.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: BP_SLZR.AnalyzeBlueprint /Game/Path/To/Blueprint"));
		UE_LOG(LogTemp, Warning, TEXT("Tip: BP_SLZR.ExportSingleBlueprint does the same thing."));
		return;
	}

	FString BlueprintPath = Args[0];

	// Handle semicolon-separated commands (e.g., "path;quit")
	int32 SemicolonIndex;
	if (BlueprintPath.FindChar(';', SemicolonIndex))
	{
		BlueprintPath = BlueprintPath.Left(SemicolonIndex);
	}

	// CR-018: Route through the unified ExportSingleBlueprintToJSON pipeline.
	// Previously called AnalyzeBlueprint() and only printed a log summary.
	UE_LOG(LogTemp, Log, TEXT("BP_SLZR.AnalyzeBlueprint: exporting JSON via unified pipeline (same as ExportSingleBlueprint): %s"), *BlueprintPath);

	const bool bSuccess = UBlueprintAnalyzer::ExportSingleBlueprintToJSON(BlueprintPath);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("Blueprint exported successfully: %s"), *BlueprintPath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Blueprint export failed: %s"), *BlueprintPath);
	}
#endif
}

void FBlueprintExtractorCommands::ExportSingleBlueprint(const TArray<FString>& Args)
{
#if WITH_EDITOR
    if (Args.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ Usage: BP_SLZR.ExportSingleBlueprint /Game/Path/To/Blueprint"));
        return;
    }
    
    FString BlueprintPath = Args[0];
    
    UE_LOG(LogTemp, Log, TEXT("🔍 Original argument: %s"), *BlueprintPath);
    
    // Handle semicolon-separated commands (e.g., "path;quit")
    int32 SemicolonIndex;
    if (BlueprintPath.FindChar(';', SemicolonIndex))
    {
        FString OriginalPath = BlueprintPath;
        BlueprintPath = BlueprintPath.Left(SemicolonIndex);
        UE_LOG(LogTemp, Log, TEXT("🔧 Cleaned path from '%s' to '%s'"), *OriginalPath, *BlueprintPath);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("ℹ️ No semicolon found in path"));
    }
    
    UE_LOG(LogTemp, Log, TEXT("🚀 Starting single Blueprint export: %s"), *BlueprintPath);
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Blue, 
            FString::Printf(TEXT("Exporting Blueprint: %s"), *BlueprintPath));
    }
    
    try
    {
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        
        if (!Blueprint)
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Could not load Blueprint: %s"), *BlueprintPath);
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, 
                    FString::Printf(TEXT("❌ Blueprint not found: %s"), *BlueprintPath));
            }
            return;
        }
        
        // Use the new complete export method that handles everything
        bool bSuccess = UBlueprintAnalyzer::ExportSingleBlueprintToJSON(BlueprintPath);
        
        // The ExportSingleBlueprintToJSON method handles all logging internally
        if (!bSuccess)
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("❌ Blueprint export failed!"));
            }
        }
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Exception during single Blueprint export"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("❌ Exception during export"));
        }
    }
#else
    UE_LOG(LogTemp, Warning, TEXT("Blueprint extraction only available in editor builds"));
#endif
}

void FBlueprintExtractorCommands::ExportMultipleBlueprints(const TArray<FString>& Args)
{
#if WITH_EDITOR
    if (Args.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ Usage: BP_SLZR.ExportMultipleBlueprints <Path1> <Path2> <Path3> ..."));
        UE_LOG(LogTemp, Warning, TEXT("Example: BP_SLZR.ExportMultipleBlueprints /Game/BP_Character /Game/BP_Enemy /Game/BP_Weapon"));
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("🚀 Starting export of %d Blueprints..."), Args.Num());
    
    int32 SuccessCount = 0;
    int32 FailCount = 0;
    
    for (const FString& BlueprintPath : Args)
    {
        FString CleanPath = BlueprintPath;
        
        // Handle semicolon-separated commands
        int32 SemicolonIndex;
        if (CleanPath.FindChar(';', SemicolonIndex))
        {
            CleanPath = CleanPath.Left(SemicolonIndex);
        }
        
        // Skip empty paths
        if (CleanPath.IsEmpty())
        {
            continue;
        }
        
        UE_LOG(LogTemp, Log, TEXT("📘 Exporting: %s"), *CleanPath);
        
        // Try to load and export the blueprint
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *CleanPath);
        
        if (Blueprint)
        {
            bool bSuccess = UBlueprintAnalyzer::ExportSingleBlueprintToJSON(CleanPath);
            if (bSuccess)
            {
                SuccessCount++;
                UE_LOG(LogTemp, Log, TEXT("✅ Successfully exported: %s"), *CleanPath);
            }
            else
            {
                FailCount++;
                UE_LOG(LogTemp, Warning, TEXT("⚠️ Failed to export: %s"), *CleanPath);
            }
        }
        else
        {
            FailCount++;
            UE_LOG(LogTemp, Warning, TEXT("❌ Could not load Blueprint: %s"), *CleanPath);
        }
    }
    
    // Summary
    UE_LOG(LogTemp, Log, TEXT("🎯 Export complete: %d successful, %d failed out of %d total"), 
        SuccessCount, FailCount, Args.Num());
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, 
            SuccessCount > 0 ? FColor::Green : FColor::Red,
            FString::Printf(TEXT("Exported %d/%d Blueprints"), SuccessCount, Args.Num()));
    }
#else
    UE_LOG(LogTemp, Warning, TEXT("Blueprint extraction only available in editor builds"));
#endif
}

void FBlueprintExtractorCommands::AuditAnimationCurves(const TArray<FString>& Args)
{
#if WITH_EDITOR
    auto CleanConsoleArg = [](const FString& InArg) -> FString
    {
        FString Cleaned = InArg;
        int32 SemicolonIndex;
        if (Cleaned.FindChar(';', SemicolonIndex))
        {
            Cleaned = Cleaned.Left(SemicolonIndex);
        }
        Cleaned.TrimStartAndEndInline();
        return Cleaned;
    };

    const FString AbsoluteProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    const FString AbsoluteSavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());

    auto ResolveAbsolutePath = [&AbsoluteProjectDir](const FString& MaybeRelative) -> FString
    {
        FString OutPath = MaybeRelative;
        if (OutPath.IsEmpty())
        {
            return OutPath;
        }

        if (FPaths::IsRelative(OutPath))
        {
            OutPath = FPaths::ConvertRelativePathToFull(AbsoluteProjectDir, OutPath);
        }
        else
        {
            OutPath = FPaths::ConvertRelativePathToFull(OutPath);
        }

        FPaths::NormalizeFilename(OutPath);
        FPaths::CollapseRelativeDirectories(OutPath);
        return OutPath;
    };

    FString ReportPath;
    if (Args.Num() > 0)
    {
        ReportPath = ResolveAbsolutePath(CleanConsoleArg(Args[0]));
    }
    else
    {
        ReportPath = ResolveAbsolutePath(
            FPaths::Combine(
                AbsoluteSavedDir,
                TEXT("BlueprintExports"),
                FString::Printf(TEXT("BP_SLZR_AnimCurveAudit_%s.json"), *UDataExportManager::GetTimestamp())));
    }

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> AnimSequenceAssets;
    AssetRegistry.GetAssetsByClass(UAnimSequence::StaticClass()->GetClassPathName(), AnimSequenceAssets);
    AnimSequenceAssets.Sort([](const FAssetData& A, const FAssetData& B)
    {
        return A.PackageName.LexicalLess(B.PackageName);
    });

    int32 LoadedAnimSequences = 0;
    int32 LoadFailures = 0;
    int32 MissingDataModelCount = 0;

    int32 SequencesWithAnyCurves = 0;
    int32 SequencesWithFloatCurves = 0;
    int32 SequencesWithTransformCurves = 0;
    int32 SequencesWithMaterialCurves = 0;
    int32 SequencesWithMorphCurves = 0;

    int32 FloatCurvesTotal = 0;
    int32 TransformCurvesTotal = 0;
    int32 MaterialCurvesTotal = 0;
    int32 MorphCurvesTotal = 0;

    TArray<TSharedPtr<FJsonValue>> TransformCurveAssets;
    TArray<TSharedPtr<FJsonValue>> MaterialCurveAssets;
    TArray<TSharedPtr<FJsonValue>> MorphCurveAssets;
    TSet<FString> CandidateSequencePaths;

    auto BuildStringArray = [](const TArray<FString>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        Out.Reserve(Values.Num());
        for (const FString& Value : Values)
        {
            Out.Add(MakeShareable(new FJsonValueString(Value)));
        }
        return Out;
    };

    for (const FAssetData& AssetData : AnimSequenceAssets)
    {
        UAnimSequence* Sequence = Cast<UAnimSequence>(AssetData.GetAsset());
        if (!Sequence)
        {
            LoadFailures++;
            continue;
        }

        LoadedAnimSequences++;

        const IAnimationDataModel* DataModel = Sequence->GetDataModel();
        if (!DataModel)
        {
            MissingDataModelCount++;
            continue;
        }

        const TArray<FFloatCurve>& FloatCurves = DataModel->GetFloatCurves();
        const TArray<FTransformCurve>& TransformCurves = DataModel->GetTransformCurves();
        const int32 FloatCurveCount = FloatCurves.Num();
        const int32 TransformCurveCount = TransformCurves.Num();

        FloatCurvesTotal += FloatCurveCount;
        TransformCurvesTotal += TransformCurveCount;

        if (FloatCurveCount > 0 || TransformCurveCount > 0)
        {
            SequencesWithAnyCurves++;
        }
        if (FloatCurveCount > 0)
        {
            SequencesWithFloatCurves++;
        }
        if (TransformCurveCount > 0)
        {
            SequencesWithTransformCurves++;

            TArray<FString> TransformCurveNames;
            TransformCurveNames.Reserve(TransformCurves.Num());
            for (const FTransformCurve& TransformCurve : TransformCurves)
            {
                TransformCurveNames.Add(TransformCurve.GetName().ToString());
            }
            TransformCurveNames.Sort();

            TSharedPtr<FJsonObject> AssetObj = MakeShareable(new FJsonObject);
            AssetObj->SetStringField(TEXT("assetPath"), AssetData.GetObjectPathString());
            AssetObj->SetStringField(TEXT("assetName"), Sequence->GetName());
            AssetObj->SetNumberField(TEXT("floatCurveCount"), FloatCurveCount);
            AssetObj->SetNumberField(TEXT("transformCurveCount"), TransformCurveCount);
            AssetObj->SetArrayField(TEXT("transformCurveNames"), BuildStringArray(TransformCurveNames));
            TransformCurveAssets.Add(MakeShareable(new FJsonValueObject(AssetObj)));

            CandidateSequencePaths.Add(AssetData.GetObjectPathString());
        }

        TArray<FString> MaterialCurveNames;
        TArray<FString> MorphCurveNames;

        if (const USkeleton* Skeleton = Sequence->GetSkeleton())
        {
            auto AccumulateCurveMeta = [&Skeleton, &MaterialCurveNames, &MorphCurveNames](const FName& CurveName)
            {
                if (const FCurveMetaData* CurveMeta = Skeleton->GetCurveMetaData(CurveName))
                {
                    if (CurveMeta->Type.bMaterial)
                    {
                        MaterialCurveNames.AddUnique(CurveName.ToString());
                    }
                    if (CurveMeta->Type.bMorphtarget)
                    {
                        MorphCurveNames.AddUnique(CurveName.ToString());
                    }
                }
            };

            for (const FFloatCurve& FloatCurve : FloatCurves)
            {
                AccumulateCurveMeta(FloatCurve.GetName());
            }
            for (const FTransformCurve& TransformCurve : TransformCurves)
            {
                AccumulateCurveMeta(TransformCurve.GetName());
            }
        }

        MaterialCurveNames.Sort();
        MorphCurveNames.Sort();
        MaterialCurvesTotal += MaterialCurveNames.Num();
        MorphCurvesTotal += MorphCurveNames.Num();

        if (MaterialCurveNames.Num() > 0)
        {
            SequencesWithMaterialCurves++;

            TSharedPtr<FJsonObject> AssetObj = MakeShareable(new FJsonObject);
            AssetObj->SetStringField(TEXT("assetPath"), AssetData.GetObjectPathString());
            AssetObj->SetStringField(TEXT("assetName"), Sequence->GetName());
            AssetObj->SetNumberField(TEXT("materialCurveCount"), MaterialCurveNames.Num());
            AssetObj->SetArrayField(TEXT("materialCurveNames"), BuildStringArray(MaterialCurveNames));
            MaterialCurveAssets.Add(MakeShareable(new FJsonValueObject(AssetObj)));

            CandidateSequencePaths.Add(AssetData.GetObjectPathString());
        }

        if (MorphCurveNames.Num() > 0)
        {
            SequencesWithMorphCurves++;

            TSharedPtr<FJsonObject> AssetObj = MakeShareable(new FJsonObject);
            AssetObj->SetStringField(TEXT("assetPath"), AssetData.GetObjectPathString());
            AssetObj->SetStringField(TEXT("assetName"), Sequence->GetName());
            AssetObj->SetNumberField(TEXT("morphCurveCount"), MorphCurveNames.Num());
            AssetObj->SetArrayField(TEXT("morphCurveNames"), BuildStringArray(MorphCurveNames));
            MorphCurveAssets.Add(MakeShareable(new FJsonValueObject(AssetObj)));

            CandidateSequencePaths.Add(AssetData.GetObjectPathString());
        }
    }

    TArray<FString> CandidatePathsArray = CandidateSequencePaths.Array();
    CandidatePathsArray.Sort();

    TSharedPtr<FJsonObject> Report = MakeShareable(new FJsonObject);
    Report->SetStringField(TEXT("analysisType"), TEXT("AnimationCurveAudit"));
    Report->SetStringField(TEXT("auditTimestamp"), FDateTime::Now().ToString());
    Report->SetStringField(TEXT("projectDir"), AbsoluteProjectDir);

    TSharedPtr<FJsonObject> Metrics = MakeShareable(new FJsonObject);
    Metrics->SetNumberField(TEXT("animSequenceAssetsTotal"), AnimSequenceAssets.Num());
    Metrics->SetNumberField(TEXT("loadedAnimSequences"), LoadedAnimSequences);
    Metrics->SetNumberField(TEXT("loadFailures"), LoadFailures);
    Metrics->SetNumberField(TEXT("missingDataModelCount"), MissingDataModelCount);
    Metrics->SetNumberField(TEXT("sequencesWithAnyCurves"), SequencesWithAnyCurves);
    Metrics->SetNumberField(TEXT("sequencesWithFloatCurves"), SequencesWithFloatCurves);
    Metrics->SetNumberField(TEXT("sequencesWithTransformCurves"), SequencesWithTransformCurves);
    Metrics->SetNumberField(TEXT("sequencesWithMaterialCurves"), SequencesWithMaterialCurves);
    Metrics->SetNumberField(TEXT("sequencesWithMorphCurves"), SequencesWithMorphCurves);
    Metrics->SetNumberField(TEXT("floatCurvesTotal"), FloatCurvesTotal);
    Metrics->SetNumberField(TEXT("transformCurvesTotal"), TransformCurvesTotal);
    Metrics->SetNumberField(TEXT("materialCurvesTotal"), MaterialCurvesTotal);
    Metrics->SetNumberField(TEXT("morphCurvesTotal"), MorphCurvesTotal);
    Report->SetObjectField(TEXT("metrics"), Metrics);

    Report->SetArrayField(TEXT("transformCurveAssets"), TransformCurveAssets);
    Report->SetArrayField(TEXT("materialCurveAssets"), MaterialCurveAssets);
    Report->SetArrayField(TEXT("morphCurveAssets"), MorphCurveAssets);
    Report->SetArrayField(TEXT("candidateSequencePaths"), BuildStringArray(CandidatePathsArray));

    FString ReportJson;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ReportJson);
    FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);

    const bool bSaved = FFileHelper::SaveStringToFile(ReportJson, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    UE_LOG(LogTemp, Log, TEXT("✅ Animation curve audit completed. Sequences=%d loaded=%d failures=%d missingDataModel=%d"), AnimSequenceAssets.Num(), LoadedAnimSequences, LoadFailures, MissingDataModelCount);
    UE_LOG(LogTemp, Log, TEXT("   Curves: any=%d floatSeq=%d transformSeq=%d floatTotal=%d transformTotal=%d"), SequencesWithAnyCurves, SequencesWithFloatCurves, SequencesWithTransformCurves, FloatCurvesTotal, TransformCurvesTotal);
    UE_LOG(LogTemp, Log, TEXT("   Curve meta: materialSeq=%d morphSeq=%d materialTotal=%d morphTotal=%d candidates=%d"), SequencesWithMaterialCurves, SequencesWithMorphCurves, MaterialCurvesTotal, MorphCurvesTotal, CandidatePathsArray.Num());
    UE_LOG(LogTemp, Log, TEXT("   Curve audit report: %s%s"), *ReportPath, bSaved ? TEXT("") : TEXT(" (save failed)"));

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green,
            FString::Printf(TEXT("Anim curve audit complete: %d candidate assets"), CandidatePathsArray.Num()));
    }
#else
    UE_LOG(LogTemp, Warning, TEXT("Blueprint extraction only available in editor builds"));
#endif
}

void FBlueprintExtractorCommands::RunRegressionSuite(const TArray<FString>& Args)
{
#if WITH_EDITOR
    auto CleanConsoleArg = [](const FString& InArg) -> FString
    {
        FString Cleaned = InArg;
        int32 SemicolonIndex;
        if (Cleaned.FindChar(';', SemicolonIndex))
        {
            Cleaned = Cleaned.Left(SemicolonIndex);
        }
        Cleaned.TrimStartAndEndInline();
        return Cleaned;
    };

    auto ParseBoolArg = [](const FString& InArg, bool bDefaultValue) -> bool
    {
        FString Lower = InArg;
        Lower.TrimStartAndEndInline();
        Lower.ToLowerInline();

        if (Lower.IsEmpty())
        {
            return bDefaultValue;
        }

        if (Lower == TEXT("1") || Lower == TEXT("true") || Lower == TEXT("yes") || Lower == TEXT("y") || Lower == TEXT("on") || Lower == TEXT("skip") || Lower == TEXT("skipexport"))
        {
            return true;
        }

        if (Lower == TEXT("0") || Lower == TEXT("false") || Lower == TEXT("no") || Lower == TEXT("n") || Lower == TEXT("off"))
        {
            return false;
        }

        return bDefaultValue;
    };

    auto LoadJsonObjectFile = [](const FString& FilePath, TSharedPtr<FJsonObject>& OutObject) -> bool
    {
        OutObject.Reset();

        FString JsonText;
        if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
        {
            return false;
        }

        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    };

    const FString AbsoluteProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    const FString AbsoluteSavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());

    auto ResolveAbsolutePath = [&AbsoluteProjectDir](const FString& MaybeRelative) -> FString
    {
        FString OutPath = MaybeRelative;
        if (OutPath.IsEmpty())
        {
            return OutPath;
        }

        if (FPaths::IsRelative(OutPath))
        {
            OutPath = FPaths::ConvertRelativePathToFull(AbsoluteProjectDir, OutPath);
        }
        else
        {
            OutPath = FPaths::ConvertRelativePathToFull(OutPath);
        }

        FPaths::NormalizeFilename(OutPath);
        FPaths::CollapseRelativeDirectories(OutPath);
        return OutPath;
    };

    const FString Timestamp = UDataExportManager::GetTimestamp();

    FString ExportDir;
    if (Args.Num() > 0)
    {
        ExportDir = ResolveAbsolutePath(CleanConsoleArg(Args[0]));
    }
    else
    {
        ExportDir = ResolveAbsolutePath(FPaths::Combine(AbsoluteSavedDir, TEXT("BlueprintExports"), FString::Printf(TEXT("BP_SLZR_All_%s"), *Timestamp)));
    }

    bool bSkipExport = false;
    if (Args.Num() > 1)
    {
        bSkipExport = ParseBoolArg(CleanConsoleArg(Args[1]), false);
    }

    FString BaselinePath = ResolveAbsolutePath(FPaths::Combine(AbsoluteProjectDir, TEXT("Plugins"), TEXT("BlueprintSerializer"), TEXT("REGRESSION_BASELINE.json")));
    if (Args.Num() > 2)
    {
        BaselinePath = ResolveAbsolutePath(CleanConsoleArg(Args[2]));
    }

    const FString ValidationReportPath = ResolveAbsolutePath(FPaths::Combine(ExportDir, FString::Printf(TEXT("BP_SLZR_ValidationReport_%s.json"), *Timestamp)));
    const FString CurveAuditPath = ResolveAbsolutePath(FPaths::Combine(AbsoluteSavedDir, TEXT("BlueprintExports"), FString::Printf(TEXT("BP_SLZR_AnimCurveAudit_%s.json"), *Timestamp)));
    const FString SuiteReportPath = ResolveAbsolutePath(FPaths::Combine(AbsoluteSavedDir, TEXT("BlueprintExports"), FString::Printf(TEXT("BP_SLZR_RegressionRun_%s.json"), *Timestamp)));

    UE_LOG(LogTemp, Log, TEXT("🧪 Running BP serializer regression suite"));
    UE_LOG(LogTemp, Log, TEXT("   ExportDir=%s"), *ExportDir);
    UE_LOG(LogTemp, Log, TEXT("   SkipExport=%s"), bSkipExport ? TEXT("true") : TEXT("false"));
    UE_LOG(LogTemp, Log, TEXT("   BaselinePath=%s"), *BaselinePath);
    UE_LOG(LogTemp, Log, TEXT("   ValidationReport=%s"), *ValidationReportPath);
    UE_LOG(LogTemp, Log, TEXT("   CurveAuditReport=%s"), *CurveAuditPath);

    if (!bSkipExport)
    {
        TArray<FString> ExportArgs;
        ExportArgs.Add(ExportDir);
        ExportAllBlueprints(ExportArgs);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("   Skipping export step, reusing existing export directory."));
    }

    {
        TArray<FString> ValidateArgs;
        ValidateArgs.Add(ExportDir);
        ValidateArgs.Add(ValidationReportPath);
        ValidateConverterReady(ValidateArgs);
    }

    {
        TArray<FString> CurveAuditArgs;
        CurveAuditArgs.Add(CurveAuditPath);
        AuditAnimationCurves(CurveAuditArgs);
    }

    TArray<TSharedPtr<FJsonValue>> FailureArray;
    auto AddFailure = [&FailureArray](const FString& Message)
    {
        FailureArray.Add(MakeShareable(new FJsonValueString(Message)));
        UE_LOG(LogTemp, Error, TEXT("   ❌ %s"), *Message);
    };

    TSharedPtr<FJsonObject> ValidationReportObj;
    TSharedPtr<FJsonObject> CurveAuditObj;
    TSharedPtr<FJsonObject> BaselineObj;

    const bool bValidationLoaded = LoadJsonObjectFile(ValidationReportPath, ValidationReportObj);
    if (!bValidationLoaded)
    {
        AddFailure(FString::Printf(TEXT("Unable to load validation report: %s"), *ValidationReportPath));
    }

    const bool bCurveAuditLoaded = LoadJsonObjectFile(CurveAuditPath, CurveAuditObj);
    if (!bCurveAuditLoaded)
    {
        AddFailure(FString::Printf(TEXT("Unable to load curve audit report: %s"), *CurveAuditPath));
    }

    const bool bBaselineExists = IFileManager::Get().FileExists(*BaselinePath);
    if (!bBaselineExists)
    {
        AddFailure(FString::Printf(TEXT("Baseline file missing: %s"), *BaselinePath));
    }

    const bool bBaselineLoaded = bBaselineExists && LoadJsonObjectFile(BaselinePath, BaselineObj);
    if (bBaselineExists && !bBaselineLoaded)
    {
        AddFailure(FString::Printf(TEXT("Unable to parse baseline file: %s"), *BaselinePath));
    }

    if (ValidationReportObj.IsValid())
    {
        bool bOverallPass = false;
        if (!ValidationReportObj->TryGetBoolField(TEXT("overallPass"), bOverallPass) || !bOverallPass)
        {
            AddFailure(TEXT("Validation report overallPass is false"));
        }
    }

    auto GetChildObject = [](const TSharedPtr<FJsonObject>& Parent, const FString& FieldName) -> TSharedPtr<FJsonObject>
    {
        if (!Parent.IsValid())
        {
            return nullptr;
        }

        const TSharedPtr<FJsonObject>* ChildPtr = nullptr;
        if (Parent->TryGetObjectField(FieldName, ChildPtr) && ChildPtr && ChildPtr->IsValid())
        {
            return *ChildPtr;
        }

        return nullptr;
    };

    auto RequireMetricAtLeast = [&AddFailure](const TSharedPtr<FJsonObject>& MetricsObject, const FString& MetricName, double MinValue, const FString& Context)
    {
        double ActualValue = 0.0;
        if (!MetricsObject.IsValid() || !MetricsObject->TryGetNumberField(MetricName, ActualValue))
        {
            AddFailure(FString::Printf(TEXT("%s missing metric '%s'"), *Context, *MetricName));
            return;
        }

        if (ActualValue < MinValue)
        {
            AddFailure(FString::Printf(TEXT("%s metric '%s' below minimum (actual=%.2f min=%.2f)"), *Context, *MetricName, ActualValue, MinValue));
        }
    };

    auto RequireGateTrue = [&AddFailure](const TSharedPtr<FJsonObject>& GatesObject, const FString& GateName)
    {
        bool bGateValue = false;
        if (!GatesObject.IsValid() || !GatesObject->TryGetBoolField(GateName, bGateValue) || !bGateValue)
        {
            AddFailure(FString::Printf(TEXT("Validation gate failed or missing: %s"), *GateName));
        }
    };

    if (bBaselineLoaded)
    {
        const TSharedPtr<FJsonObject> ValidationGates = GetChildObject(ValidationReportObj, TEXT("gates"));
        const TSharedPtr<FJsonObject> ValidationMetrics = GetChildObject(ValidationReportObj, TEXT("metrics"));
        const TSharedPtr<FJsonObject> CurveMetrics = GetChildObject(CurveAuditObj, TEXT("metrics"));

        const TArray<TSharedPtr<FJsonValue>>* RequiredGateValues = nullptr;
        if (!BaselineObj->TryGetArrayField(TEXT("requiredValidationGates"), RequiredGateValues) || !RequiredGateValues)
        {
            AddFailure(TEXT("Baseline missing requiredValidationGates array"));
        }
        else
        {
            for (const TSharedPtr<FJsonValue>& GateValue : *RequiredGateValues)
            {
                FString GateName;
                if (!GateValue.IsValid() || !GateValue->TryGetString(GateName) || GateName.IsEmpty())
                {
                    continue;
                }
                RequireGateTrue(ValidationGates, GateName);
            }
        }

        const TSharedPtr<FJsonObject> BaselineValidationMinMetrics = GetChildObject(BaselineObj, TEXT("validationMinMetrics"));
        if (!BaselineValidationMinMetrics.IsValid())
        {
            AddFailure(TEXT("Baseline missing validationMinMetrics object"));
        }
        else
        {
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : BaselineValidationMinMetrics->Values)
            {
                double MinValue = 0.0;
                if (!Pair.Value.IsValid() || !Pair.Value->TryGetNumber(MinValue))
                {
                    continue;
                }
                RequireMetricAtLeast(ValidationMetrics, Pair.Key, MinValue, TEXT("validation"));
            }
        }

        const TSharedPtr<FJsonObject> BaselineCurveMinMetrics = GetChildObject(BaselineObj, TEXT("curveAuditMinMetrics"));
        if (!BaselineCurveMinMetrics.IsValid())
        {
            AddFailure(TEXT("Baseline missing curveAuditMinMetrics object"));
        }
        else
        {
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : BaselineCurveMinMetrics->Values)
            {
                double MinValue = 0.0;
                if (!Pair.Value.IsValid() || !Pair.Value->TryGetNumber(MinValue))
                {
                    continue;
                }
                RequireMetricAtLeast(CurveMetrics, Pair.Key, MinValue, TEXT("curveAudit"));
            }
        }
    }

    const bool bSuitePass = FailureArray.Num() == 0;

    TSharedPtr<FJsonObject> SuiteReport = MakeShareable(new FJsonObject);
    SuiteReport->SetStringField(TEXT("analysisType"), TEXT("RegressionSuiteRun"));
    SuiteReport->SetStringField(TEXT("runTimestamp"), FDateTime::Now().ToString());
    SuiteReport->SetStringField(TEXT("projectDir"), AbsoluteProjectDir);
    SuiteReport->SetStringField(TEXT("exportDir"), ExportDir);
    SuiteReport->SetBoolField(TEXT("skipExport"), bSkipExport);
    SuiteReport->SetStringField(TEXT("baselinePath"), BaselinePath);
    SuiteReport->SetBoolField(TEXT("suitePass"), bSuitePass);
    SuiteReport->SetArrayField(TEXT("failures"), FailureArray);
    SuiteReport->SetStringField(TEXT("validationReportPath"), ValidationReportPath);
    SuiteReport->SetStringField(TEXT("curveAuditReportPath"), CurveAuditPath);

    FString SuiteReportJson;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SuiteReportJson);
    FJsonSerializer::Serialize(SuiteReport.ToSharedRef(), Writer);

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(SuiteReportPath), true);
    const bool bSaved = FFileHelper::SaveStringToFile(SuiteReportJson, *SuiteReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    UE_LOG(LogTemp, Log, TEXT("%s Regression suite complete"), bSuitePass ? TEXT("✅") : TEXT("❌"));
    UE_LOG(LogTemp, Log, TEXT("   suitePass=%s failures=%d"), bSuitePass ? TEXT("true") : TEXT("false"), FailureArray.Num());
    UE_LOG(LogTemp, Log, TEXT("   Suite report: %s%s"), *SuiteReportPath, bSaved ? TEXT("") : TEXT(" (save failed)"));

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, bSuitePass ? FColor::Green : FColor::Red,
            FString::Printf(TEXT("BP_SLZR regression %s (skipExport=%s)"), bSuitePass ? TEXT("PASS") : TEXT("FAIL"), bSkipExport ? TEXT("true") : TEXT("false")));
    }
#else
    UE_LOG(LogTemp, Warning, TEXT("Blueprint extraction only available in editor builds"));
#endif
}

void FBlueprintExtractorCommands::ValidateConverterReady(const TArray<FString>& Args)
{
#if WITH_EDITOR
    auto CleanConsoleArg = [](const FString& InArg) -> FString
    {
        FString Cleaned = InArg;
        int32 SemicolonIndex;
        if (Cleaned.FindChar(';', SemicolonIndex))
        {
            Cleaned = Cleaned.Left(SemicolonIndex);
        }
        Cleaned.TrimStartAndEndInline();
        return Cleaned;
    };

    auto ResolveLatestBatchDir = [](const FString& BaseDir) -> FString
    {
        TArray<FString> CandidateDirs;
        IFileManager::Get().FindFiles(CandidateDirs, *FPaths::Combine(BaseDir, TEXT("BP_SLZR_All_*")), false, true);
        CandidateDirs.Sort(TGreater<FString>());

        for (const FString& Candidate : CandidateDirs)
        {
            const FString FullPath = FPaths::Combine(BaseDir, Candidate);
            if (IFileManager::Get().DirectoryExists(*FullPath))
            {
                return FullPath;
            }
        }

        return FString();
    };

    const FString AbsoluteProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

    auto ResolveAbsolutePath = [&AbsoluteProjectDir](const FString& MaybeRelative) -> FString
    {
        FString OutPath = MaybeRelative;
        if (OutPath.IsEmpty())
        {
            return OutPath;
        }

        if (FPaths::IsRelative(OutPath))
        {
            OutPath = FPaths::ConvertRelativePathToFull(AbsoluteProjectDir, OutPath);
        }
        else
        {
            OutPath = FPaths::ConvertRelativePathToFull(OutPath);
        }

        FPaths::NormalizeDirectoryName(OutPath);
        FPaths::CollapseRelativeDirectories(OutPath);
        return OutPath;
    };

    FString ExportDir;
    if (Args.Num() > 0)
    {
        ExportDir = CleanConsoleArg(Args[0]);
    }

    if (ExportDir.IsEmpty())
    {
        const FString AbsoluteSavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
        const FString ExportRoot = ResolveAbsolutePath(FPaths::Combine(AbsoluteSavedDir, TEXT("BlueprintExports")));
        ExportDir = ResolveLatestBatchDir(ExportRoot);
        if (ExportDir.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("❌ ValidateConverterReady: no BP_SLZR_All_* export directory found under %s"), *ExportRoot);
            return;
        }
    }
    else
    {
        ExportDir = ResolveAbsolutePath(ExportDir);

        if (!IFileManager::Get().DirectoryExists(*ExportDir))
        {
            UE_LOG(LogTemp, Error, TEXT("❌ ValidateConverterReady: export directory does not exist: %s"), *ExportDir);
            return;
        }

        TArray<FString> TopLevelJsonFiles;
        IFileManager::Get().FindFiles(TopLevelJsonFiles, *FPaths::Combine(ExportDir, TEXT("*.json")), true, false);

        if (TopLevelJsonFiles.Num() == 0)
        {
            const FString MaybeBatchDir = ResolveLatestBatchDir(ExportDir);
            if (!MaybeBatchDir.IsEmpty())
            {
                ExportDir = MaybeBatchDir;
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("🔎 Validating converter-ready batch: %s"), *ExportDir);

    TArray<FString> JsonFileNames;
    IFileManager::Get().FindFiles(JsonFileNames, *FPaths::Combine(ExportDir, TEXT("*.json")), true, false);
    JsonFileNames.Sort();

    TArray<FString> ManifestFileNames;
    TArray<FString> BlueprintFileNames;
    for (const FString& FileName : JsonFileNames)
    {
        if (FileName.Contains(TEXT("Manifest"), ESearchCase::IgnoreCase))
        {
            ManifestFileNames.Add(FileName);
        }
        else if (FileName.Contains(TEXT("ValidationReport"), ESearchCase::IgnoreCase))
        {
            continue;
        }
        else
        {
            BlueprintFileNames.Add(FileName);
        }
    }

    ManifestFileNames.Sort(TGreater<FString>());
    const FString ManifestPath = ManifestFileNames.Num() > 0 ? FPaths::Combine(ExportDir, ManifestFileNames[0]) : FString();

    int32 ManifestTotal = -1;
    int32 ManifestSuccessCount = -1;
    int32 ManifestFailCount = -1;
    bool bManifestParsed = false;
    if (!ManifestPath.IsEmpty())
    {
        FString ManifestContents;
        if (FFileHelper::LoadFileToString(ManifestContents, *ManifestPath))
        {
            TSharedPtr<FJsonObject> ManifestJson;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ManifestContents);
            if (FJsonSerializer::Deserialize(Reader, ManifestJson) && ManifestJson.IsValid())
            {
                double NumericField = 0.0;
                if (ManifestJson->TryGetNumberField(TEXT("total"), NumericField)) ManifestTotal = static_cast<int32>(NumericField);
                if (ManifestJson->TryGetNumberField(TEXT("successCount"), NumericField)) ManifestSuccessCount = static_cast<int32>(NumericField);
                if (ManifestJson->TryGetNumberField(TEXT("failCount"), NumericField)) ManifestFailCount = static_cast<int32>(NumericField);
                bManifestParsed = true;
            }
        }
    }

    const TArray<FString> RequiredTopLevelKeys = {
        TEXT("classSpecifiers"),
        TEXT("classConfigName"),
        TEXT("classConfigFlags"),
        TEXT("classDefaultValues"),
        TEXT("classDefaultValueDelta"),
        TEXT("dependencyClosure"),
        TEXT("userDefinedStructSchemas"),
        TEXT("userDefinedEnumSchemas")
    };

    const TArray<FString> RequiredClassConfigFlagKeys = {
        TEXT("isConfigClass"),
        TEXT("isDefaultConfig"),
        TEXT("isPerObjectConfig"),
        TEXT("isGlobalUserConfig"),
        TEXT("isProjectUserConfig"),
        TEXT("isPerPlatformConfig"),
        TEXT("configDoesNotCheckDefaults"),
        TEXT("hasConfigName")
    };

    const TArray<FString> RequiredComponentOverrideKeys = {
        TEXT("hasInheritableOverride"),
        TEXT("inheritableOwnerClassPath"),
        TEXT("inheritableOwnerClassName"),
        TEXT("inheritableComponentGuid"),
        TEXT("inheritableSourceTemplatePath"),
        TEXT("inheritableOverrideTemplatePath"),
        TEXT("inheritableOverrideProperties"),
        TEXT("inheritableOverrideValues"),
        TEXT("inheritableParentValues")
    };

    auto HasNonEmptyArrayField = [](const TSharedPtr<FJsonObject>& Obj, const FString& FieldName) -> bool
    {
        const TArray<TSharedPtr<FJsonValue>>* ArrayField = nullptr;
        return Obj.IsValid() && Obj->TryGetArrayField(FieldName, ArrayField) && ArrayField && ArrayField->Num() > 0;
    };

    auto HasNonEmptyStringField = [](const TSharedPtr<FJsonObject>& Obj, const FString& FieldName) -> bool
    {
        FString Value;
        return Obj.IsValid() && Obj->TryGetStringField(FieldName, Value) && !Value.IsEmpty();
    };

    auto HasMeaningfulStringField = [](const TSharedPtr<FJsonObject>& Obj, const FString& FieldName) -> bool
    {
        FString Value;
        if (!Obj.IsValid() || !Obj->TryGetStringField(FieldName, Value))
        {
            return false;
        }

        Value.TrimStartAndEndInline();
        return !Value.IsEmpty() && !Value.Equals(TEXT("None"), ESearchCase::IgnoreCase);
    };

    auto ExtractBlueprintPath = [](const TSharedPtr<FJsonObject>& Json) -> FString
    {
        if (!Json.IsValid())
        {
            return FString();
        }

        FString BlueprintPath;
        if (Json->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) && !BlueprintPath.IsEmpty())
        {
            return BlueprintPath;
        }

        const TSharedPtr<FJsonObject>* BlueprintInfoObj = nullptr;
        if (Json->TryGetObjectField(TEXT("blueprintInfo"), BlueprintInfoObj)
            && BlueprintInfoObj
            && BlueprintInfoObj->IsValid()
            && (*BlueprintInfoObj)->TryGetStringField(TEXT("blueprintPath"), BlueprintPath)
            && !BlueprintPath.IsEmpty())
        {
            return BlueprintPath;
        }

        return FString();
    };

    int32 ParseErrors = 0;
    int32 MissingRequiredExports = 0;
    int32 DependencyClosureNonEmpty = 0;
    int32 IncludeHintsNonEmpty = 0;
    int32 NativeIncludeHintsNonEmpty = 0;
    int32 ExportsWithCompilerIRFallbackShape = 0;
    int32 ExportsMissingCompilerIRFallbackShape = 0;
    int32 ExportsWithUnsupportedNodeFallback = 0;
    int32 ExportsWithBytecodeFallback = 0;
    int32 CompilerIRFallbackBytecodeFunctionCountTotal = 0;
    int32 ExportsWithMacroDependencyClosureShape = 0;
    int32 ExportsMissingMacroDependencyClosureShape = 0;
    int32 ExportsWithMacroGraphPaths = 0;
    int32 ExportsWithMacroBlueprintPaths = 0;
    int32 MacroGraphPathsTotal = 0;
    int32 MacroBlueprintPathsTotal = 0;
    int32 ExportsWithDefaultObjectIncludeHints = 0;
    int32 ExportsWithDefaultObjectClassPaths = 0;
    int32 UserStructSchemaNonEmpty = 0;
    int32 UserEnumSchemaNonEmpty = 0;
    int32 UserStructFieldCount = 0;
    int32 UserEnumValueCount = 0;
    int32 ExportsWithGameplayTags = 0;
    int32 GameplayTagsTotal = 0;
    int32 GameplayTagsWithSourceGraphMetadata = 0;
    int32 GameplayTagsWithSourceNodeTypeMetadata = 0;

    int32 VariableTotal = 0;
    int32 VariableWithDeclarationSpecifiers = 0;
    int32 VariablesWithReplicationShape = 0;
    int32 VariablesReplicated = 0;
    int32 VariablesWithRepNotifyFunction = 0;
    int32 VariablesReplicatedWithRepNotifyFunction = 0;
    int32 FunctionTotal = 0;
    int32 FunctionWithDeclarationSpecifiers = 0;
    int32 FunctionsWithNetworkShape = 0;
    int32 FunctionsNet = 0;
    int32 FunctionsNetServer = 0;
    int32 FunctionsNetClient = 0;
    int32 FunctionsNetMulticast = 0;
    int32 FunctionsReliable = 0;
    int32 FunctionsBlueprintAuthorityOnly = 0;
    int32 FunctionsBlueprintCosmetic = 0;
    int32 FunctionsWithAnyNetworkSemantic = 0;

    int32 ComponentTotal = 0;
    int32 ComponentInheritedCount = 0;
    int32 ComponentHasOverrideCount = 0;
    int32 ComponentWithOverrideDeltaCount = 0;
    int32 ExportsWithOverrideCount = 0;

    int32 ExportsWithControlRigs = 0;
    int32 ControlRigsTotal = 0;
    int32 ControlRigsWithControls = 0;
    int32 ControlRigsWithBones = 0;
    int32 ControlRigsWithControlToBoneMap = 0;

    int32 AnimationAssetsTotal = 0;
    int32 AnimationNotifiesTotal = 0;
    int32 AnimationNotifiesWithTrackName = 0;
    int32 AnimationNotifiesWithTriggeredEventName = 0;
    int32 AnimationNotifiesWithEventParameters = 0;

    int32 AnimationCurvesTotal = 0;
    int32 AnimationCurvesWithCurveAssetPath = 0;
    int32 AnimationCurvesWithInterpolationMode = 0;
    int32 AnimationCurvesWithAxisType = 0;
    int32 AnimationCurvesWithVectorValues = 0;
    int32 AnimationCurvesWithTransformValues = 0;
    int32 AnimationCurvesWithAffectedMaterials = 0;
    int32 AnimationCurvesWithAffectedMorphTargets = 0;
    int32 AnimationCurvesFloatType = 0;
    int32 AnimationCurvesVectorType = 0;
    int32 AnimationCurvesTransformType = 0;

    int32 AnimationTransitionsTotal = 0;
    int32 AnimationTransitionsWithBlendMode = 0;
    int32 AnimationTransitionsWithPriority = 0;
    int32 AnimationTransitionsWithBlendOutTriggerTime = 0;
    int32 AnimationTransitionsWithNotifyStateIndex = 0;

    int32 ClassConfigFlagsPresentCount = 0;
    int32 ExportsMissingClassConfigFlagKeys = 0;
    int32 ComponentsMissingOverrideShape = 0;

    // Task 20: baseline metrics for Tasks 15-19 new CR fields
    int32 ExportsWithStructuredGraphs = 0;
    int32 StructuredGraphsTotal = 0;
    int32 StructuredGraphNodesTotal = 0;
    int32 GraphsWithInputPins = 0;
    int32 GraphsWithOutputPins = 0;
    int32 NodesWithSwitchCasePinIds = 0;
    int32 NodesWithSelectPinIds = 0;
    int32 NodesWithLoopMacro = 0;
    int32 NodesWithCollapsedGraph = 0;
    int32 ExportsWithBytecodeNodeTraces = 0;
    int32 BytecodeNodeTracesTotal = 0;
    // Tasks 22-26: baseline metrics for Branch/Sequence/Reroute/Self/SpawnActor/Timeline
    int32 NodesWithBranch = 0;
    int32 NodesWithSequence = 0;
    int32 NodesWithReroute = 0;
    int32 NodesWithSelf = 0;
    int32 NodesWithSpawnActor = 0;
    int32 NodesWithTimelineExecPins = 0;
    // Tasks 28-32: new high-frequency node type metrics
    int32 NodesWithPromotableOp = 0;
    int32 NodesWithGetArrayItem = 0;
    int32 NodesWithMakeArray = 0;
    int32 NodesWithMakeMap = 0;
    int32 NodesWithMakeSet = 0;
    int32 NodesWithTunnel = 0;
    int32 NodesWithEnumEquality = 0;
    int32 NodesWithEnumInequality = 0;
    int32 NodesWithSetPersistentFrame = 0;
    // Task 33-50: new node handler metric counters
    int32 NodesWithCallMaterialParamCollection = 0;
    int32 NodesWithGetSubsystem = 0;
    int32 NodesWithBaseAsyncTask = 0;
    int32 NodesWithAddComponent = 0;
    int32 NodesWithSetFieldsInStruct = 0;
    int32 NodesWithAsyncAction = 0;
    int32 NodesWithDelegateOp = 0;
    int32 NodesWithInterfaceMessage = 0;
    int32 NodesWithCreateObject = 0;
    int32 NodesWithFormatText = 0;
    int32 NodesWithInputKey = 0;
    int32 NodesWithMathExpression = 0;
    int32 NodesWithEnhancedInputAction = 0;
    int32 NodesWithAssignmentStatement = 0;
    int32 NodesWithTemporaryVariable = 0;
    int32 NodesWithPropertyAccess = 0;
    int32 NodesWithGetDataTableRow = 0;
    int32 NodesWithGetEnumeratorName = 0;
    // Task 51: new node type counters
    int32 NodesWithMultiGate = 0;
    int32 NodesWithListenForGameplayMessages = 0;

    const int32 RawBlueprintFileCount = BlueprintFileNames.Num();
    TMap<FString, FString> SelectedFileByBlueprintPath;
    TMap<FString, FString> SelectedTimestampByBlueprintPath;

    for (const FString& FileName : BlueprintFileNames)
    {
        const FString FullPath = FPaths::Combine(ExportDir, FileName);
        FString Contents;
        if (!FFileHelper::LoadFileToString(Contents, *FullPath))
        {
            ParseErrors++;
            continue;
        }

        TSharedPtr<FJsonObject> Json;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
        if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
        {
            ParseErrors++;
            continue;
        }

        FString BlueprintPath = ExtractBlueprintPath(Json);
        if (BlueprintPath.IsEmpty())
        {
            BlueprintPath = FString::Printf(TEXT("__file__%s"), *FileName);
        }

        FString ExtractionTimestamp;
        Json->TryGetStringField(TEXT("extractionTimestamp"), ExtractionTimestamp);

        const FString* ExistingFile = SelectedFileByBlueprintPath.Find(BlueprintPath);
        const FString* ExistingTimestamp = SelectedTimestampByBlueprintPath.Find(BlueprintPath);

        bool bReplace = false;
        if (!ExistingFile)
        {
            bReplace = true;
        }
        else if (!ExistingTimestamp)
        {
            bReplace = !ExtractionTimestamp.IsEmpty() || FileName > *ExistingFile;
        }
        else if (ExtractionTimestamp > *ExistingTimestamp)
        {
            bReplace = true;
        }
        else if (ExtractionTimestamp == *ExistingTimestamp && FileName > *ExistingFile)
        {
            bReplace = true;
        }

        if (bReplace)
        {
            SelectedFileByBlueprintPath.Add(BlueprintPath, FileName);
            SelectedTimestampByBlueprintPath.Add(BlueprintPath, ExtractionTimestamp);
        }
    }

    TArray<FString> SelectedBlueprintFileNames;
    SelectedFileByBlueprintPath.GenerateValueArray(SelectedBlueprintFileNames);
    SelectedBlueprintFileNames.Sort();
    const int32 DuplicateBlueprintExportsIgnored = FMath::Max(0, RawBlueprintFileCount - SelectedBlueprintFileNames.Num());

    TSet<FString> MissingTopLevelKeyNames;
    TSet<FString> MissingClassConfigFlagKeyNames;
    TSet<FString> MissingComponentOverrideKeyNames;

    for (const FString& FileName : SelectedBlueprintFileNames)
    {
        const FString FullPath = FPaths::Combine(ExportDir, FileName);
        FString Contents;
        if (!FFileHelper::LoadFileToString(Contents, *FullPath))
        {
            ParseErrors++;
            continue;
        }

        TSharedPtr<FJsonObject> Json;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
        if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
        {
            ParseErrors++;
            continue;
        }

        bool bMissingTopLevelInThisExport = false;
        for (const FString& Key : RequiredTopLevelKeys)
        {
            if (!Json->HasField(Key))
            {
                bMissingTopLevelInThisExport = true;
                MissingTopLevelKeyNames.Add(Key);
            }
        }
        if (bMissingTopLevelInThisExport)
        {
            MissingRequiredExports++;
        }

        const TSharedPtr<FJsonObject>* DependencyClosureObj = nullptr;
        if (Json->TryGetObjectField(TEXT("dependencyClosure"), DependencyClosureObj) && DependencyClosureObj && DependencyClosureObj->IsValid())
        {
            const TSharedPtr<FJsonObject> Closure = *DependencyClosureObj;
            const bool bHasClosure =
                HasNonEmptyArrayField(Closure, TEXT("classPaths")) ||
                HasNonEmptyArrayField(Closure, TEXT("structPaths")) ||
                HasNonEmptyArrayField(Closure, TEXT("enumPaths")) ||
                HasNonEmptyArrayField(Closure, TEXT("interfacePaths")) ||
                HasNonEmptyArrayField(Closure, TEXT("assetPaths")) ||
                HasNonEmptyArrayField(Closure, TEXT("controlRigPaths")) ||
                HasNonEmptyArrayField(Closure, TEXT("moduleNames"));

            if (bHasClosure)
            {
                DependencyClosureNonEmpty++;
            }
            if (HasNonEmptyArrayField(Closure, TEXT("includeHints")))
            {
                IncludeHintsNonEmpty++;

                const TArray<TSharedPtr<FJsonValue>>* IncludeHints = nullptr;
                if (Closure->TryGetArrayField(TEXT("includeHints"), IncludeHints) && IncludeHints)
                {
                    bool bHasDefaultObjectInclude = false;
                    for (const TSharedPtr<FJsonValue>& IncludeValue : *IncludeHints)
                    {
                        FString IncludeText;
                        if (IncludeValue.IsValid() && IncludeValue->TryGetString(IncludeText) && IncludeText.Contains(TEXT("Default__")))
                        {
                            bHasDefaultObjectInclude = true;
                            break;
                        }
                    }

                    if (bHasDefaultObjectInclude)
                    {
                        ExportsWithDefaultObjectIncludeHints++;
                    }
                }
            }

            if (HasNonEmptyArrayField(Closure, TEXT("nativeIncludeHints")))
            {
                NativeIncludeHintsNonEmpty++;
            }

            const bool bHasMacroClosureShape =
                Closure->HasField(TEXT("macroGraphPaths")) &&
                Closure->HasField(TEXT("macroBlueprintPaths"));
            if (bHasMacroClosureShape)
            {
                ExportsWithMacroDependencyClosureShape++;
            }
            else
            {
                ExportsMissingMacroDependencyClosureShape++;
            }

            const TArray<TSharedPtr<FJsonValue>>* MacroGraphPaths = nullptr;
            if (Closure->TryGetArrayField(TEXT("macroGraphPaths"), MacroGraphPaths) && MacroGraphPaths)
            {
                MacroGraphPathsTotal += MacroGraphPaths->Num();
                if (MacroGraphPaths->Num() > 0)
                {
                    ExportsWithMacroGraphPaths++;
                }
            }

            const TArray<TSharedPtr<FJsonValue>>* MacroBlueprintPaths = nullptr;
            if (Closure->TryGetArrayField(TEXT("macroBlueprintPaths"), MacroBlueprintPaths) && MacroBlueprintPaths)
            {
                MacroBlueprintPathsTotal += MacroBlueprintPaths->Num();
                if (MacroBlueprintPaths->Num() > 0)
                {
                    ExportsWithMacroBlueprintPaths++;
                }
            }

            const TArray<TSharedPtr<FJsonValue>>* ClassPaths = nullptr;
            if (Closure->TryGetArrayField(TEXT("classPaths"), ClassPaths) && ClassPaths)
            {
                bool bHasDefaultObjectClassPath = false;
                for (const TSharedPtr<FJsonValue>& ClassPathValue : *ClassPaths)
                {
                    FString ClassPathText;
                    if (ClassPathValue.IsValid()
                        && ClassPathValue->TryGetString(ClassPathText)
                        && ClassPathText.StartsWith(TEXT("/Script/"))
                        && ClassPathText.Contains(TEXT("Default__")))
                    {
                        bHasDefaultObjectClassPath = true;
                        break;
                    }
                }

                if (bHasDefaultObjectClassPath)
                {
                    ExportsWithDefaultObjectClassPaths++;
                }
            }
        }

        const TSharedPtr<FJsonObject>* CompilerIRFallbackObj = nullptr;
        if (Json->TryGetObjectField(TEXT("compilerIRFallback"), CompilerIRFallbackObj) && CompilerIRFallbackObj && CompilerIRFallbackObj->IsValid())
        {
            const TSharedPtr<FJsonObject> CompilerIRFallback = *CompilerIRFallbackObj;

            const bool bHasCompilerFallbackShape =
                CompilerIRFallback->HasField(TEXT("hasUnsupportedNodes")) &&
                CompilerIRFallback->HasField(TEXT("unsupportedNodeTypeCount")) &&
                CompilerIRFallback->HasField(TEXT("unsupportedNodeTypes")) &&
                CompilerIRFallback->HasField(TEXT("partiallySupportedNodeTypeCount")) &&
                CompilerIRFallback->HasField(TEXT("partiallySupportedNodeTypes")) &&
                CompilerIRFallback->HasField(TEXT("hasBytecodeFallback")) &&
                CompilerIRFallback->HasField(TEXT("bytecodeBackedFunctionCount")) &&
                CompilerIRFallback->HasField(TEXT("bytecodeBackedFunctions"));

            if (bHasCompilerFallbackShape)
            {
                ExportsWithCompilerIRFallbackShape++;
            }
            else
            {
                ExportsMissingCompilerIRFallbackShape++;
            }

            bool bHasUnsupportedNodes = false;
            if (CompilerIRFallback->TryGetBoolField(TEXT("hasUnsupportedNodes"), bHasUnsupportedNodes) && bHasUnsupportedNodes)
            {
                ExportsWithUnsupportedNodeFallback++;
            }

            bool bHasBytecodeFallback = false;
            if (CompilerIRFallback->TryGetBoolField(TEXT("hasBytecodeFallback"), bHasBytecodeFallback) && bHasBytecodeFallback)
            {
                ExportsWithBytecodeFallback++;
            }

            double BytecodeFunctionCount = 0.0;
            if (CompilerIRFallback->TryGetNumberField(TEXT("bytecodeBackedFunctionCount"), BytecodeFunctionCount))
            {
                CompilerIRFallbackBytecodeFunctionCountTotal += static_cast<int32>(BytecodeFunctionCount);
            }
        }
        else
        {
            ExportsMissingCompilerIRFallbackShape++;
        }

        const TArray<TSharedPtr<FJsonValue>>* StructSchemas = nullptr;
        if (Json->TryGetArrayField(TEXT("userDefinedStructSchemas"), StructSchemas) && StructSchemas)
        {
            if (StructSchemas->Num() > 0)
            {
                UserStructSchemaNonEmpty++;
            }

            for (const TSharedPtr<FJsonValue>& StructValue : *StructSchemas)
            {
                const TSharedPtr<FJsonObject> StructObj = StructValue.IsValid() ? StructValue->AsObject() : nullptr;
                if (!StructObj.IsValid())
                {
                    continue;
                }

                const TArray<TSharedPtr<FJsonValue>>* Fields = nullptr;
                if (StructObj->TryGetArrayField(TEXT("fields"), Fields) && Fields)
                {
                    UserStructFieldCount += Fields->Num();
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* EnumSchemas = nullptr;
        if (Json->TryGetArrayField(TEXT("userDefinedEnumSchemas"), EnumSchemas) && EnumSchemas)
        {
            if (EnumSchemas->Num() > 0)
            {
                UserEnumSchemaNonEmpty++;
            }

            for (const TSharedPtr<FJsonValue>& EnumValue : *EnumSchemas)
            {
                const TSharedPtr<FJsonObject> EnumObj = EnumValue.IsValid() ? EnumValue->AsObject() : nullptr;
                if (!EnumObj.IsValid())
                {
                    continue;
                }

                const TArray<TSharedPtr<FJsonValue>>* Enumerators = nullptr;
                if (EnumObj->TryGetArrayField(TEXT("enumerators"), Enumerators) && Enumerators)
                {
                    UserEnumValueCount += Enumerators->Num();
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* GameplayTagArray = nullptr;
        if (Json->TryGetArrayField(TEXT("gameplayTags"), GameplayTagArray) && GameplayTagArray)
        {
            if (GameplayTagArray->Num() > 0)
            {
                ExportsWithGameplayTags++;
            }

            for (const TSharedPtr<FJsonValue>& GameplayTagValue : *GameplayTagArray)
            {
                const TSharedPtr<FJsonObject> GameplayTagObj = GameplayTagValue.IsValid() ? GameplayTagValue->AsObject() : nullptr;
                if (!GameplayTagObj.IsValid())
                {
                    continue;
                }

                GameplayTagsTotal++;

                const TSharedPtr<FJsonObject>* TagMetadataObj = nullptr;
                bool bHasTagMetadata = GameplayTagObj->TryGetObjectField(TEXT("tagMetadata"), TagMetadataObj);
                if ((!bHasTagMetadata || !TagMetadataObj || !TagMetadataObj->IsValid()))
                {
                    // Backward-compat fallback for older exports prior to tagMetadata rename.
                    bHasTagMetadata = GameplayTagObj->TryGetObjectField(TEXT("metadata"), TagMetadataObj);
                }

                if (bHasTagMetadata && TagMetadataObj && TagMetadataObj->IsValid())
                {
                    if (HasNonEmptyStringField(*TagMetadataObj, TEXT("sourceGraph")))
                    {
                        GameplayTagsWithSourceGraphMetadata++;
                    }
                    if (HasNonEmptyStringField(*TagMetadataObj, TEXT("sourceNodeType")))
                    {
                        GameplayTagsWithSourceNodeTypeMetadata++;
                    }
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* VariableArray = nullptr;
        if (Json->TryGetArrayField(TEXT("detailedVariables"), VariableArray) && VariableArray)
        {
            for (const TSharedPtr<FJsonValue>& VariableValue : *VariableArray)
            {
                const TSharedPtr<FJsonObject> VariableObj = VariableValue.IsValid() ? VariableValue->AsObject() : nullptr;
                if (!VariableObj.IsValid())
                {
                    continue;
                }

                VariableTotal++;
                if (HasNonEmptyArrayField(VariableObj, TEXT("declarationSpecifiers")))
                {
                    VariableWithDeclarationSpecifiers++;
                }

                const bool bHasReplicationShape =
                    VariableObj->HasField(TEXT("isReplicated")) &&
                    VariableObj->HasField(TEXT("replicationCondition")) &&
                    VariableObj->HasField(TEXT("repNotifyFunction"));
                if (bHasReplicationShape)
                {
                    VariablesWithReplicationShape++;
                }

                bool bIsReplicated = false;
                if (VariableObj->TryGetBoolField(TEXT("isReplicated"), bIsReplicated) && bIsReplicated)
                {
                    VariablesReplicated++;
                }

                const bool bHasRepNotifyFunction = HasMeaningfulStringField(VariableObj, TEXT("repNotifyFunction"));
                if (bHasRepNotifyFunction)
                {
                    VariablesWithRepNotifyFunction++;
                }
                if (bIsReplicated && bHasRepNotifyFunction)
                {
                    VariablesReplicatedWithRepNotifyFunction++;
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* FunctionArray = nullptr;
        if (Json->TryGetArrayField(TEXT("detailedFunctions"), FunctionArray) && FunctionArray)
        {
            for (const TSharedPtr<FJsonValue>& FunctionValue : *FunctionArray)
            {
                const TSharedPtr<FJsonObject> FunctionObj = FunctionValue.IsValid() ? FunctionValue->AsObject() : nullptr;
                if (!FunctionObj.IsValid())
                {
                    continue;
                }

                FunctionTotal++;
                if (HasNonEmptyArrayField(FunctionObj, TEXT("declarationSpecifiers")))
                {
                    FunctionWithDeclarationSpecifiers++;
                }

                const bool bHasNetworkShape =
                    FunctionObj->HasField(TEXT("isNet")) &&
                    FunctionObj->HasField(TEXT("isNetServer")) &&
                    FunctionObj->HasField(TEXT("isNetClient")) &&
                    FunctionObj->HasField(TEXT("isNetMulticast")) &&
                    FunctionObj->HasField(TEXT("isReliable")) &&
                    FunctionObj->HasField(TEXT("blueprintAuthorityOnly")) &&
                    FunctionObj->HasField(TEXT("blueprintCosmetic"));
                if (bHasNetworkShape)
                {
                    FunctionsWithNetworkShape++;
                }

                bool bIsNet = false;
                bool bIsNetServer = false;
                bool bIsNetClient = false;
                bool bIsNetMulticast = false;
                bool bIsReliable = false;
                bool bBlueprintAuthorityOnly = false;
                bool bBlueprintCosmetic = false;

                if (FunctionObj->TryGetBoolField(TEXT("isNet"), bIsNet) && bIsNet)
                {
                    FunctionsNet++;
                }
                if (FunctionObj->TryGetBoolField(TEXT("isNetServer"), bIsNetServer) && bIsNetServer)
                {
                    FunctionsNetServer++;
                }
                if (FunctionObj->TryGetBoolField(TEXT("isNetClient"), bIsNetClient) && bIsNetClient)
                {
                    FunctionsNetClient++;
                }
                if (FunctionObj->TryGetBoolField(TEXT("isNetMulticast"), bIsNetMulticast) && bIsNetMulticast)
                {
                    FunctionsNetMulticast++;
                }
                if (FunctionObj->TryGetBoolField(TEXT("isReliable"), bIsReliable) && bIsReliable)
                {
                    FunctionsReliable++;
                }
                if (FunctionObj->TryGetBoolField(TEXT("blueprintAuthorityOnly"), bBlueprintAuthorityOnly) && bBlueprintAuthorityOnly)
                {
                    FunctionsBlueprintAuthorityOnly++;
                }
                if (FunctionObj->TryGetBoolField(TEXT("blueprintCosmetic"), bBlueprintCosmetic) && bBlueprintCosmetic)
                {
                    FunctionsBlueprintCosmetic++;
                }

                if (bIsNet || bIsNetServer || bIsNetClient || bIsNetMulticast || bIsReliable || bBlueprintAuthorityOnly || bBlueprintCosmetic)
                {
                    FunctionsWithAnyNetworkSemantic++;
                }
            }
        }

        const TSharedPtr<FJsonObject>* ClassConfigFlagsObj = nullptr;
        if (Json->TryGetObjectField(TEXT("classConfigFlags"), ClassConfigFlagsObj) && ClassConfigFlagsObj && ClassConfigFlagsObj->IsValid())
        {
            ClassConfigFlagsPresentCount++;

            bool bMissingClassConfigShape = false;
            for (const FString& FlagKey : RequiredClassConfigFlagKeys)
            {
                if (!(*ClassConfigFlagsObj)->HasField(FlagKey))
                {
                    bMissingClassConfigShape = true;
                    MissingClassConfigFlagKeyNames.Add(FlagKey);
                }
            }

            if (bMissingClassConfigShape)
            {
                ExportsMissingClassConfigFlagKeys++;
            }
        }
        else
        {
            ExportsMissingClassConfigFlagKeys++;
            for (const FString& FlagKey : RequiredClassConfigFlagKeys)
            {
                MissingClassConfigFlagKeyNames.Add(FlagKey);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* ComponentArray = nullptr;
        if (Json->TryGetArrayField(TEXT("detailedComponents"), ComponentArray) && ComponentArray)
        {
            bool bExportHasOverride = false;

            for (const TSharedPtr<FJsonValue>& ComponentValue : *ComponentArray)
            {
                const TSharedPtr<FJsonObject> ComponentObj = ComponentValue.IsValid() ? ComponentValue->AsObject() : nullptr;
                if (!ComponentObj.IsValid())
                {
                    continue;
                }

                ComponentTotal++;

                bool bIsInherited = false;
                if (ComponentObj->TryGetBoolField(TEXT("isInherited"), bIsInherited) && bIsInherited)
                {
                    ComponentInheritedCount++;
                }

                bool bHasOverride = false;
                if (ComponentObj->TryGetBoolField(TEXT("hasInheritableOverride"), bHasOverride) && bHasOverride)
                {
                    ComponentHasOverrideCount++;
                    bExportHasOverride = true;
                }

                if (HasNonEmptyArrayField(ComponentObj, TEXT("inheritableOverrideProperties")))
                {
                    ComponentWithOverrideDeltaCount++;
                }

                bool bMissingComponentShape = false;
                for (const FString& ComponentKey : RequiredComponentOverrideKeys)
                {
                    if (!ComponentObj->HasField(ComponentKey))
                    {
                        bMissingComponentShape = true;
                        MissingComponentOverrideKeyNames.Add(ComponentKey);
                    }
                }

                if (bMissingComponentShape)
                {
                    ComponentsMissingOverrideShape++;
                }
            }

            if (bExportHasOverride)
            {
                ExportsWithOverrideCount++;
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* ControlRigArray = nullptr;
        if (Json->TryGetArrayField(TEXT("controlRigs"), ControlRigArray) && ControlRigArray && ControlRigArray->Num() > 0)
        {
            ExportsWithControlRigs++;

            for (const TSharedPtr<FJsonValue>& RigValue : *ControlRigArray)
            {
                const TSharedPtr<FJsonObject> RigObj = RigValue.IsValid() ? RigValue->AsObject() : nullptr;
                if (!RigObj.IsValid())
                {
                    continue;
                }

                ControlRigsTotal++;
                if (HasNonEmptyArrayField(RigObj, TEXT("controls")))
                {
                    ControlRigsWithControls++;
                }
                if (HasNonEmptyArrayField(RigObj, TEXT("bones")))
                {
                    ControlRigsWithBones++;
                }

                const TSharedPtr<FJsonObject>* ControlToBoneMapObj = nullptr;
                if (RigObj->TryGetObjectField(TEXT("controlToBoneMap"), ControlToBoneMapObj)
                    && ControlToBoneMapObj
                    && ControlToBoneMapObj->IsValid()
                    && (*ControlToBoneMapObj)->Values.Num() > 0)
                {
                    ControlRigsWithControlToBoneMap++;
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* AnimationAssetArray = nullptr;
        if (Json->TryGetArrayField(TEXT("animationAssets"), AnimationAssetArray) && AnimationAssetArray)
        {
            for (const TSharedPtr<FJsonValue>& AnimationAssetValue : *AnimationAssetArray)
            {
                const TSharedPtr<FJsonObject> AnimationAssetObj = AnimationAssetValue.IsValid() ? AnimationAssetValue->AsObject() : nullptr;
                if (!AnimationAssetObj.IsValid())
                {
                    continue;
                }

                AnimationAssetsTotal++;

                const TArray<TSharedPtr<FJsonValue>>* NotifyArray = nullptr;
                if (AnimationAssetObj->TryGetArrayField(TEXT("notifies"), NotifyArray) && NotifyArray)
                {
                    for (const TSharedPtr<FJsonValue>& NotifyValue : *NotifyArray)
                    {
                        const TSharedPtr<FJsonObject> NotifyObj = NotifyValue.IsValid() ? NotifyValue->AsObject() : nullptr;
                        if (!NotifyObj.IsValid())
                        {
                            continue;
                        }

                        AnimationNotifiesTotal++;
                        if (HasNonEmptyStringField(NotifyObj, TEXT("trackName")))
                        {
                            AnimationNotifiesWithTrackName++;
                        }
                        if (HasNonEmptyStringField(NotifyObj, TEXT("triggeredEventName")))
                        {
                            AnimationNotifiesWithTriggeredEventName++;
                        }
                        if (HasNonEmptyArrayField(NotifyObj, TEXT("eventParameters")))
                        {
                            AnimationNotifiesWithEventParameters++;
                        }
                    }
                }

                const TArray<TSharedPtr<FJsonValue>>* CurveArray = nullptr;
                if (AnimationAssetObj->TryGetArrayField(TEXT("curves"), CurveArray) && CurveArray)
                {
                    for (const TSharedPtr<FJsonValue>& CurveValue : *CurveArray)
                    {
                        const TSharedPtr<FJsonObject> CurveObj = CurveValue.IsValid() ? CurveValue->AsObject() : nullptr;
                        if (!CurveObj.IsValid())
                        {
                            continue;
                        }

                        AnimationCurvesTotal++;

                        if (HasNonEmptyStringField(CurveObj, TEXT("curveAssetPath")))
                        {
                            AnimationCurvesWithCurveAssetPath++;
                        }
                        if (HasNonEmptyStringField(CurveObj, TEXT("interpolationMode")))
                        {
                            AnimationCurvesWithInterpolationMode++;
                        }
                        if (HasNonEmptyStringField(CurveObj, TEXT("axisType")))
                        {
                            AnimationCurvesWithAxisType++;
                        }
                        if (HasNonEmptyArrayField(CurveObj, TEXT("vectorValues")))
                        {
                            AnimationCurvesWithVectorValues++;
                        }
                        if (HasNonEmptyArrayField(CurveObj, TEXT("transformValues")))
                        {
                            AnimationCurvesWithTransformValues++;
                        }
                        if (HasNonEmptyArrayField(CurveObj, TEXT("affectedMaterials")))
                        {
                            AnimationCurvesWithAffectedMaterials++;
                        }
                        if (HasNonEmptyArrayField(CurveObj, TEXT("affectedMorphTargets")))
                        {
                            AnimationCurvesWithAffectedMorphTargets++;
                        }

                        FString CurveType;
                        if (CurveObj->TryGetStringField(TEXT("curveType"), CurveType))
                        {
                            if (CurveType.Equals(TEXT("FloatCurve"), ESearchCase::CaseSensitive))
                            {
                                AnimationCurvesFloatType++;
                            }
                            else if (CurveType.Equals(TEXT("VectorCurve"), ESearchCase::CaseSensitive))
                            {
                                AnimationCurvesVectorType++;
                            }
                            else if (CurveType.Equals(TEXT("TransformCurve"), ESearchCase::CaseSensitive))
                            {
                                AnimationCurvesTransformType++;
                            }
                        }
                    }
                }
            }
        }

        const TSharedPtr<FJsonObject>* AnimGraphObj = nullptr;
        if (Json->TryGetObjectField(TEXT("animGraph"), AnimGraphObj) && AnimGraphObj && AnimGraphObj->IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* StateMachineArray = nullptr;
            if ((*AnimGraphObj)->TryGetArrayField(TEXT("stateMachines"), StateMachineArray) && StateMachineArray)
            {
                for (const TSharedPtr<FJsonValue>& StateMachineValue : *StateMachineArray)
                {
                    const TSharedPtr<FJsonObject> StateMachineObj = StateMachineValue.IsValid() ? StateMachineValue->AsObject() : nullptr;
                    if (!StateMachineObj.IsValid())
                    {
                        continue;
                    }

                    const TArray<TSharedPtr<FJsonValue>>* TransitionArray = nullptr;
                    if (!StateMachineObj->TryGetArrayField(TEXT("transitions"), TransitionArray) || !TransitionArray)
                    {
                        continue;
                    }

                    for (const TSharedPtr<FJsonValue>& TransitionValue : *TransitionArray)
                    {
                        const TSharedPtr<FJsonObject> TransitionObj = TransitionValue.IsValid() ? TransitionValue->AsObject() : nullptr;
                        if (!TransitionObj.IsValid())
                        {
                            continue;
                        }

                        AnimationTransitionsTotal++;

                        if (HasNonEmptyStringField(TransitionObj, TEXT("blendMode")))
                        {
                            AnimationTransitionsWithBlendMode++;
                        }

                        double NumericField = 0.0;
                        if (TransitionObj->TryGetNumberField(TEXT("priority"), NumericField))
                        {
                            AnimationTransitionsWithPriority++;
                        }
                        if (TransitionObj->TryGetNumberField(TEXT("blendOutTriggerTime"), NumericField))
                        {
                            AnimationTransitionsWithBlendOutTriggerTime++;
                        }
                        if (HasNonEmptyStringField(TransitionObj, TEXT("notifyStateIndex")))
                        {
                            AnimationTransitionsWithNotifyStateIndex++;
                        }
                    }
                }
            }
        }

        // Task 20: count structuredGraphs features (Tasks 15-19 new fields)
        const TArray<TSharedPtr<FJsonValue>>* StructuredGraphsArr = nullptr;
        if (Json->TryGetArrayField(TEXT("structuredGraphs"), StructuredGraphsArr) && StructuredGraphsArr && StructuredGraphsArr->Num() > 0)
        {
            ExportsWithStructuredGraphs++;
            StructuredGraphsTotal += StructuredGraphsArr->Num();
            for (const TSharedPtr<FJsonValue>& GraphValue : *StructuredGraphsArr)
            {
                const TSharedPtr<FJsonObject> GraphObj = GraphValue.IsValid() ? GraphValue->AsObject() : nullptr;
                if (!GraphObj.IsValid()) continue;

                if (HasNonEmptyArrayField(GraphObj, TEXT("graphInputPins")))  GraphsWithInputPins++;
                if (HasNonEmptyArrayField(GraphObj, TEXT("graphOutputPins"))) GraphsWithOutputPins++;

                const TArray<TSharedPtr<FJsonValue>>* NodesArr = nullptr;
                if (!GraphObj->TryGetArrayField(TEXT("nodes"), NodesArr) || !NodesArr) continue;
                StructuredGraphNodesTotal += NodesArr->Num();

                for (const TSharedPtr<FJsonValue>& NodeValue : *NodesArr)
                {
                    const TSharedPtr<FJsonObject> NodeObj = NodeValue.IsValid() ? NodeValue->AsObject() : nullptr;
                    if (!NodeObj.IsValid()) continue;

                    const TSharedPtr<FJsonObject>* NodePropsPtr = nullptr;
                    if (!NodeObj->TryGetObjectField(TEXT("nodeProperties"), NodePropsPtr) || !NodePropsPtr) continue;
                    const TSharedPtr<FJsonObject>& NodeProps = *NodePropsPtr;

                    if (NodeProps->HasField(TEXT("meta.switchCasePinIds"))) NodesWithSwitchCasePinIds++;
                    if (NodeProps->HasField(TEXT("meta.selectIndexPinId"))) NodesWithSelectPinIds++;
                    if (NodeProps->HasField(TEXT("meta.isLoopMacro")))      NodesWithLoopMacro++;
                    if (NodeProps->HasField(TEXT("meta.isCollapsedGraph"))) NodesWithCollapsedGraph++;
                    // Tasks 22-26: count new control-flow node enrichment fields
                    if (NodeProps->HasField(TEXT("meta.isBranch")))          NodesWithBranch++;
                    if (NodeProps->HasField(TEXT("meta.isSequence")))        NodesWithSequence++;
                    if (NodeProps->HasField(TEXT("meta.isReroute")))         NodesWithReroute++;
                    if (NodeProps->HasField(TEXT("meta.isSelf")))            NodesWithSelf++;
                    if (NodeProps->HasField(TEXT("meta.isSpawnActor")))      NodesWithSpawnActor++;
                    if (NodeProps->HasField(TEXT("meta.timelinePlayPinId"))) NodesWithTimelineExecPins++;
                    // Tasks 28-32: count new high-frequency node type enrichment fields
                    if (NodeProps->HasField(TEXT("meta.isPromotableOp")))        NodesWithPromotableOp++;
                    if (NodeProps->HasField(TEXT("meta.isGetArrayItem")))         NodesWithGetArrayItem++;
                    if (NodeProps->HasField(TEXT("meta.isMakeArray")))            NodesWithMakeArray++;
                    if (NodeProps->HasField(TEXT("meta.isMakeMap")))              NodesWithMakeMap++;
                    if (NodeProps->HasField(TEXT("meta.isMakeSet")))              NodesWithMakeSet++;
                    if (NodeProps->HasField(TEXT("meta.isTunnel")))               NodesWithTunnel++;
                    if (NodeProps->HasField(TEXT("meta.isEnumEquality")))         NodesWithEnumEquality++;
                    if (NodeProps->HasField(TEXT("meta.isEnumInequality")))       NodesWithEnumInequality++;
                    if (NodeProps->HasField(TEXT("meta.isSetPersistentFrame")))   NodesWithSetPersistentFrame++;
                    // Task 33-50: new node handler counters
                    if (NodeProps->HasField(TEXT("meta.isCallMaterialParamCollection"))) NodesWithCallMaterialParamCollection++;
                    if (NodeProps->HasField(TEXT("meta.isGetSubsystem")))         NodesWithGetSubsystem++;
                    if (NodeProps->HasField(TEXT("meta.isBaseAsyncTask")))        NodesWithBaseAsyncTask++;
                    if (NodeProps->HasField(TEXT("meta.isAddComponent")))         NodesWithAddComponent++;
                    if (NodeProps->HasField(TEXT("meta.isSetFieldsInStruct")))    NodesWithSetFieldsInStruct++;
                    if (NodeProps->HasField(TEXT("meta.isAsyncAction")))          NodesWithAsyncAction++;
                    if (NodeProps->HasField(TEXT("meta.delegateOp")))             NodesWithDelegateOp++;
                    if (NodeProps->HasField(TEXT("meta.isInterfaceMessage")))     NodesWithInterfaceMessage++;
                    if (NodeProps->HasField(TEXT("meta.isGenericCreateObject")) ||
                        NodeProps->HasField(TEXT("meta.isCreateWidget")))         NodesWithCreateObject++;
                    if (NodeProps->HasField(TEXT("meta.isFormatText")))           NodesWithFormatText++;
                    if (NodeProps->HasField(TEXT("meta.isInputKey")))             NodesWithInputKey++;
                    if (NodeProps->HasField(TEXT("meta.isMathExpression")))       NodesWithMathExpression++;
                    if (NodeProps->HasField(TEXT("meta.isEnhancedInputAction")))  NodesWithEnhancedInputAction++;
                    if (NodeProps->HasField(TEXT("meta.isAssignmentStatement")))  NodesWithAssignmentStatement++;
                    if (NodeProps->HasField(TEXT("meta.isTemporaryVariable")))    NodesWithTemporaryVariable++;
                    if (NodeProps->HasField(TEXT("meta.isPropertyAccess")))       NodesWithPropertyAccess++;
                    if (NodeProps->HasField(TEXT("meta.isGetDataTableRow")))      NodesWithGetDataTableRow++;
                    if (NodeProps->HasField(TEXT("meta.isGetEnumeratorName")))    NodesWithGetEnumeratorName++;
                    // Task 51: new node type counters
                    if (NodeProps->HasField(TEXT("meta.isMultiGate")))            NodesWithMultiGate++;
                    if (NodeProps->HasField(TEXT("meta.isAsyncAction")) &&
                        NodeProps->HasField(TEXT("meta.proxyClass")))
                    {
                        // K2Node_AsyncAction_ListenForGameplayMessages: AsyncAction subclass with
                        // proxy class path containing "ListenForGameplayMessages"
                        FString ProxyClass;
                        if (NodeProps->TryGetStringField(TEXT("meta.proxyClass"), ProxyClass) &&
                            ProxyClass.Contains(TEXT("ListenForGameplayMessages")))
                            NodesWithListenForGameplayMessages++;
                    }
                }
            }
        }

        // Task 20: count bytecode node traces from compilerIRFallback (Task 10 field)
        const TSharedPtr<FJsonObject>* IRFallbackPtr2 = nullptr;
        if (Json->TryGetObjectField(TEXT("compilerIRFallback"), IRFallbackPtr2) && IRFallbackPtr2 && IRFallbackPtr2->IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* FuncsArr = nullptr;
            if ((*IRFallbackPtr2)->TryGetArrayField(TEXT("bytecodeBackedFunctions"), FuncsArr) && FuncsArr)
            {
                bool bExportHasTraces = false;
                for (const TSharedPtr<FJsonValue>& FuncValue : *FuncsArr)
                {
                    const TSharedPtr<FJsonObject> FuncObj = FuncValue.IsValid() ? FuncValue->AsObject() : nullptr;
                    if (!FuncObj.IsValid()) continue;
                    const TArray<TSharedPtr<FJsonValue>>* TracesArr = nullptr;
                    if (FuncObj->TryGetArrayField(TEXT("nodeTraces"), TracesArr) && TracesArr && TracesArr->Num() > 0)
                    {
                        bExportHasTraces = true;
                        BytecodeNodeTracesTotal += TracesArr->Num();
                    }
                }
                if (bExportHasTraces) ExportsWithBytecodeNodeTraces++;
            }
        }
    }

    const int32 BlueprintFileCount = SelectedBlueprintFileNames.Num();
    const bool bGateManifestPresent = bManifestParsed;
    const bool bGateManifestCounts = bManifestParsed && ManifestTotal == BlueprintFileCount && ManifestSuccessCount == BlueprintFileCount && ManifestFailCount == 0;
    const bool bGateParse = ParseErrors == 0;
    const bool bGateRequiredKeys = MissingRequiredExports == 0;
    const bool bGateVariableDeclarations = VariableTotal > 0 && VariableTotal == VariableWithDeclarationSpecifiers;
    const bool bGateFunctionDeclarations = FunctionTotal > 0 && FunctionTotal == FunctionWithDeclarationSpecifiers;
    const bool bGateVariableReplicationShape = VariableTotal > 0 && VariableTotal == VariablesWithReplicationShape;
    const bool bGateFunctionNetworkShape = FunctionTotal > 0 && FunctionTotal == FunctionsWithNetworkShape;
    const bool bGateClassConfigShape = ExportsMissingClassConfigFlagKeys == 0;
    const bool bGateComponentShape = ComponentsMissingOverrideShape == 0;
    const bool bGateDependencyClosureCoverage = DependencyClosureNonEmpty == BlueprintFileCount;
    const bool bGateIncludeHintsCoverage = IncludeHintsNonEmpty == BlueprintFileCount;
    const bool bGateCompilerIRFallbackShape = ExportsMissingCompilerIRFallbackShape == 0;
    const bool bGateMacroDependencyClosureShape = ExportsMissingMacroDependencyClosureShape == 0;

    const bool bOverallPass =
        bGateManifestPresent &&
        bGateManifestCounts &&
        bGateParse &&
        bGateRequiredKeys &&
        bGateVariableDeclarations &&
        bGateFunctionDeclarations &&
        bGateVariableReplicationShape &&
        bGateFunctionNetworkShape &&
        bGateClassConfigShape &&
        bGateComponentShape &&
        bGateDependencyClosureCoverage &&
        bGateIncludeHintsCoverage &&
        bGateCompilerIRFallbackShape &&
        bGateMacroDependencyClosureShape;

    TArray<FString> MissingTopLevelKeys = MissingTopLevelKeyNames.Array();
    MissingTopLevelKeys.Sort();
    TArray<FString> MissingClassConfigKeys = MissingClassConfigFlagKeyNames.Array();
    MissingClassConfigKeys.Sort();
    TArray<FString> MissingComponentShapeKeys = MissingComponentOverrideKeyNames.Array();
    MissingComponentShapeKeys.Sort();

    TSharedPtr<FJsonObject> Report = MakeShareable(new FJsonObject);
    Report->SetStringField(TEXT("analysisType"), TEXT("ConverterReadyValidation"));
    Report->SetStringField(TEXT("validationTimestamp"), FDateTime::Now().ToString());
    Report->SetStringField(TEXT("exportDirectory"), ExportDir);
    Report->SetStringField(TEXT("manifestPath"), ManifestPath);
    Report->SetBoolField(TEXT("overallPass"), bOverallPass);
    Report->SetNumberField(TEXT("blueprintFileCount"), BlueprintFileCount);
    Report->SetNumberField(TEXT("rawBlueprintFileCount"), RawBlueprintFileCount);
    Report->SetNumberField(TEXT("duplicateBlueprintExportsIgnored"), DuplicateBlueprintExportsIgnored);
    Report->SetNumberField(TEXT("parseErrors"), ParseErrors);
    Report->SetNumberField(TEXT("missingRequiredExports"), MissingRequiredExports);

    TSharedPtr<FJsonObject> Gates = MakeShareable(new FJsonObject);
    Gates->SetBoolField(TEXT("manifestPresent"), bGateManifestPresent);
    Gates->SetBoolField(TEXT("manifestCountsMatch"), bGateManifestCounts);
    Gates->SetBoolField(TEXT("parseErrorsZero"), bGateParse);
    Gates->SetBoolField(TEXT("requiredKeysPresent"), bGateRequiredKeys);
    Gates->SetBoolField(TEXT("variableDeclarationCoverage"), bGateVariableDeclarations);
    Gates->SetBoolField(TEXT("functionDeclarationCoverage"), bGateFunctionDeclarations);
    Gates->SetBoolField(TEXT("variableReplicationShape"), bGateVariableReplicationShape);
    Gates->SetBoolField(TEXT("functionNetworkShape"), bGateFunctionNetworkShape);
    Gates->SetBoolField(TEXT("classConfigFlagShape"), bGateClassConfigShape);
    Gates->SetBoolField(TEXT("componentOverrideShape"), bGateComponentShape);
    Gates->SetBoolField(TEXT("dependencyClosureCoverage"), bGateDependencyClosureCoverage);
    Gates->SetBoolField(TEXT("includeHintsCoverage"), bGateIncludeHintsCoverage);
    Gates->SetBoolField(TEXT("compilerIRFallbackShape"), bGateCompilerIRFallbackShape);
    Gates->SetBoolField(TEXT("macroDependencyClosureShape"), bGateMacroDependencyClosureShape);
    Report->SetObjectField(TEXT("gates"), Gates);

    TSharedPtr<FJsonObject> ManifestSummary = MakeShareable(new FJsonObject);
    ManifestSummary->SetNumberField(TEXT("total"), ManifestTotal);
    ManifestSummary->SetNumberField(TEXT("successCount"), ManifestSuccessCount);
    ManifestSummary->SetNumberField(TEXT("failCount"), ManifestFailCount);
    Report->SetObjectField(TEXT("manifestSummary"), ManifestSummary);

    TSharedPtr<FJsonObject> Metrics = MakeShareable(new FJsonObject);
    Metrics->SetNumberField(TEXT("dependencyClosureNonEmpty"), DependencyClosureNonEmpty);
    Metrics->SetNumberField(TEXT("includeHintsNonEmpty"), IncludeHintsNonEmpty);
    Metrics->SetNumberField(TEXT("nativeIncludeHintsNonEmpty"), NativeIncludeHintsNonEmpty);
    Metrics->SetNumberField(TEXT("exportsWithMacroDependencyClosureShape"), ExportsWithMacroDependencyClosureShape);
    Metrics->SetNumberField(TEXT("exportsMissingMacroDependencyClosureShape"), ExportsMissingMacroDependencyClosureShape);
    Metrics->SetNumberField(TEXT("exportsWithMacroGraphPaths"), ExportsWithMacroGraphPaths);
    Metrics->SetNumberField(TEXT("exportsWithMacroBlueprintPaths"), ExportsWithMacroBlueprintPaths);
    Metrics->SetNumberField(TEXT("macroGraphPathsTotal"), MacroGraphPathsTotal);
    Metrics->SetNumberField(TEXT("macroBlueprintPathsTotal"), MacroBlueprintPathsTotal);
    Metrics->SetNumberField(TEXT("exportsWithCompilerIRFallbackShape"), ExportsWithCompilerIRFallbackShape);
    Metrics->SetNumberField(TEXT("exportsMissingCompilerIRFallbackShape"), ExportsMissingCompilerIRFallbackShape);
    Metrics->SetNumberField(TEXT("exportsWithUnsupportedNodeFallback"), ExportsWithUnsupportedNodeFallback);
    Metrics->SetNumberField(TEXT("exportsWithBytecodeFallback"), ExportsWithBytecodeFallback);
    Metrics->SetNumberField(TEXT("compilerIRFallbackBytecodeFunctionCountTotal"), CompilerIRFallbackBytecodeFunctionCountTotal);
    Metrics->SetNumberField(TEXT("rawBlueprintFileCount"), RawBlueprintFileCount);
    Metrics->SetNumberField(TEXT("duplicateBlueprintExportsIgnored"), DuplicateBlueprintExportsIgnored);
    Metrics->SetNumberField(TEXT("exportsWithDefaultObjectIncludeHints"), ExportsWithDefaultObjectIncludeHints);
    Metrics->SetNumberField(TEXT("exportsWithDefaultObjectClassPaths"), ExportsWithDefaultObjectClassPaths);
    Metrics->SetNumberField(TEXT("userStructSchemaNonEmpty"), UserStructSchemaNonEmpty);
    Metrics->SetNumberField(TEXT("userEnumSchemaNonEmpty"), UserEnumSchemaNonEmpty);
    Metrics->SetNumberField(TEXT("userStructFieldCount"), UserStructFieldCount);
    Metrics->SetNumberField(TEXT("userEnumValueCount"), UserEnumValueCount);
    Metrics->SetNumberField(TEXT("exportsWithGameplayTags"), ExportsWithGameplayTags);
    Metrics->SetNumberField(TEXT("gameplayTagsTotal"), GameplayTagsTotal);
    Metrics->SetNumberField(TEXT("gameplayTagsWithSourceGraphMetadata"), GameplayTagsWithSourceGraphMetadata);
    Metrics->SetNumberField(TEXT("gameplayTagsWithSourceNodeTypeMetadata"), GameplayTagsWithSourceNodeTypeMetadata);
    Metrics->SetNumberField(TEXT("variablesWithDeclarationSpecifiers"), VariableWithDeclarationSpecifiers);
    Metrics->SetNumberField(TEXT("variablesTotal"), VariableTotal);
    Metrics->SetNumberField(TEXT("variablesWithReplicationShape"), VariablesWithReplicationShape);
    Metrics->SetNumberField(TEXT("variablesReplicated"), VariablesReplicated);
    Metrics->SetNumberField(TEXT("variablesWithRepNotifyFunction"), VariablesWithRepNotifyFunction);
    Metrics->SetNumberField(TEXT("variablesReplicatedWithRepNotifyFunction"), VariablesReplicatedWithRepNotifyFunction);
    Metrics->SetNumberField(TEXT("functionsWithDeclarationSpecifiers"), FunctionWithDeclarationSpecifiers);
    Metrics->SetNumberField(TEXT("functionsTotal"), FunctionTotal);
    Metrics->SetNumberField(TEXT("functionsWithNetworkShape"), FunctionsWithNetworkShape);
    Metrics->SetNumberField(TEXT("functionsNet"), FunctionsNet);
    Metrics->SetNumberField(TEXT("functionsNetServer"), FunctionsNetServer);
    Metrics->SetNumberField(TEXT("functionsNetClient"), FunctionsNetClient);
    Metrics->SetNumberField(TEXT("functionsNetMulticast"), FunctionsNetMulticast);
    Metrics->SetNumberField(TEXT("functionsReliable"), FunctionsReliable);
    Metrics->SetNumberField(TEXT("functionsBlueprintAuthorityOnly"), FunctionsBlueprintAuthorityOnly);
    Metrics->SetNumberField(TEXT("functionsBlueprintCosmetic"), FunctionsBlueprintCosmetic);
    Metrics->SetNumberField(TEXT("functionsWithAnyNetworkSemantic"), FunctionsWithAnyNetworkSemantic);
    Metrics->SetNumberField(TEXT("componentsTotal"), ComponentTotal);
    Metrics->SetNumberField(TEXT("componentsInherited"), ComponentInheritedCount);
    Metrics->SetNumberField(TEXT("componentsHasInheritableOverride"), ComponentHasOverrideCount);
    Metrics->SetNumberField(TEXT("componentsWithOverrideDelta"), ComponentWithOverrideDeltaCount);
    Metrics->SetNumberField(TEXT("exportsWithInheritableOverride"), ExportsWithOverrideCount);
    Metrics->SetNumberField(TEXT("exportsWithControlRigs"), ExportsWithControlRigs);
    Metrics->SetNumberField(TEXT("controlRigsTotal"), ControlRigsTotal);
    Metrics->SetNumberField(TEXT("controlRigsWithControls"), ControlRigsWithControls);
    Metrics->SetNumberField(TEXT("controlRigsWithBones"), ControlRigsWithBones);
    Metrics->SetNumberField(TEXT("controlRigsWithControlToBoneMap"), ControlRigsWithControlToBoneMap);
    Metrics->SetNumberField(TEXT("animationAssetsTotal"), AnimationAssetsTotal);
    Metrics->SetNumberField(TEXT("animationNotifiesTotal"), AnimationNotifiesTotal);
    Metrics->SetNumberField(TEXT("animationNotifiesWithTrackName"), AnimationNotifiesWithTrackName);
    Metrics->SetNumberField(TEXT("animationNotifiesWithTriggeredEventName"), AnimationNotifiesWithTriggeredEventName);
    Metrics->SetNumberField(TEXT("animationNotifiesWithEventParameters"), AnimationNotifiesWithEventParameters);
    Metrics->SetNumberField(TEXT("animationCurvesTotal"), AnimationCurvesTotal);
    Metrics->SetNumberField(TEXT("animationCurvesWithCurveAssetPath"), AnimationCurvesWithCurveAssetPath);
    Metrics->SetNumberField(TEXT("animationCurvesWithInterpolationMode"), AnimationCurvesWithInterpolationMode);
    Metrics->SetNumberField(TEXT("animationCurvesWithAxisType"), AnimationCurvesWithAxisType);
    Metrics->SetNumberField(TEXT("animationCurvesWithVectorValues"), AnimationCurvesWithVectorValues);
    Metrics->SetNumberField(TEXT("animationCurvesWithTransformValues"), AnimationCurvesWithTransformValues);
    Metrics->SetNumberField(TEXT("animationCurvesWithAffectedMaterials"), AnimationCurvesWithAffectedMaterials);
    Metrics->SetNumberField(TEXT("animationCurvesWithAffectedMorphTargets"), AnimationCurvesWithAffectedMorphTargets);
    Metrics->SetNumberField(TEXT("animationCurvesFloatType"), AnimationCurvesFloatType);
    Metrics->SetNumberField(TEXT("animationCurvesVectorType"), AnimationCurvesVectorType);
    Metrics->SetNumberField(TEXT("animationCurvesTransformType"), AnimationCurvesTransformType);
    Metrics->SetNumberField(TEXT("animationTransitionsTotal"), AnimationTransitionsTotal);
    Metrics->SetNumberField(TEXT("animationTransitionsWithBlendMode"), AnimationTransitionsWithBlendMode);
    Metrics->SetNumberField(TEXT("animationTransitionsWithPriority"), AnimationTransitionsWithPriority);
    Metrics->SetNumberField(TEXT("animationTransitionsWithBlendOutTriggerTime"), AnimationTransitionsWithBlendOutTriggerTime);
    Metrics->SetNumberField(TEXT("animationTransitionsWithNotifyStateIndex"), AnimationTransitionsWithNotifyStateIndex);
    Metrics->SetNumberField(TEXT("classConfigFlagsPresent"), ClassConfigFlagsPresentCount);
    Metrics->SetNumberField(TEXT("exportsMissingClassConfigFlagKeys"), ExportsMissingClassConfigFlagKeys);
    Metrics->SetNumberField(TEXT("componentsMissingOverrideShape"), ComponentsMissingOverrideShape);
    // Task 20: baseline metrics for Tasks 15-19
    Metrics->SetNumberField(TEXT("exportsWithStructuredGraphs"), ExportsWithStructuredGraphs);
    Metrics->SetNumberField(TEXT("structuredGraphsTotal"), StructuredGraphsTotal);
    Metrics->SetNumberField(TEXT("structuredGraphNodesTotal"), StructuredGraphNodesTotal);
    Metrics->SetNumberField(TEXT("graphsWithInputPins"), GraphsWithInputPins);
    Metrics->SetNumberField(TEXT("graphsWithOutputPins"), GraphsWithOutputPins);
    Metrics->SetNumberField(TEXT("nodesWithSwitchCasePinIds"), NodesWithSwitchCasePinIds);
    Metrics->SetNumberField(TEXT("nodesWithSelectPinIds"), NodesWithSelectPinIds);
    Metrics->SetNumberField(TEXT("nodesWithLoopMacro"), NodesWithLoopMacro);
    Metrics->SetNumberField(TEXT("nodesWithCollapsedGraph"), NodesWithCollapsedGraph);
    Metrics->SetNumberField(TEXT("exportsWithBytecodeNodeTraces"), ExportsWithBytecodeNodeTraces);
    Metrics->SetNumberField(TEXT("bytecodeNodeTracesTotal"), BytecodeNodeTracesTotal);
    // Tasks 22-26: control-flow node enrichment baseline metrics
    Metrics->SetNumberField(TEXT("nodesWithBranch"),           NodesWithBranch);
    Metrics->SetNumberField(TEXT("nodesWithSequence"),         NodesWithSequence);
    Metrics->SetNumberField(TEXT("nodesWithReroute"),          NodesWithReroute);
    Metrics->SetNumberField(TEXT("nodesWithSelf"),             NodesWithSelf);
    Metrics->SetNumberField(TEXT("nodesWithSpawnActor"),       NodesWithSpawnActor);
    Metrics->SetNumberField(TEXT("nodesWithTimelineExecPins"), NodesWithTimelineExecPins);
    // Tasks 28-32: new high-frequency node type enrichment metrics
    Metrics->SetNumberField(TEXT("nodesWithPromotableOp"),       NodesWithPromotableOp);
    Metrics->SetNumberField(TEXT("nodesWithGetArrayItem"),        NodesWithGetArrayItem);
    Metrics->SetNumberField(TEXT("nodesWithMakeArray"),           NodesWithMakeArray);
    Metrics->SetNumberField(TEXT("nodesWithMakeMap"),             NodesWithMakeMap);
    Metrics->SetNumberField(TEXT("nodesWithMakeSet"),             NodesWithMakeSet);
    Metrics->SetNumberField(TEXT("nodesWithTunnel"),              NodesWithTunnel);
    Metrics->SetNumberField(TEXT("nodesWithEnumEquality"),        NodesWithEnumEquality);
    Metrics->SetNumberField(TEXT("nodesWithEnumInequality"),      NodesWithEnumInequality);
    Metrics->SetNumberField(TEXT("nodesWithSetPersistentFrame"),  NodesWithSetPersistentFrame);
    // Task 33-50: new node handler metrics
    Metrics->SetNumberField(TEXT("nodesWithCallMaterialParamCollection"), NodesWithCallMaterialParamCollection);
    Metrics->SetNumberField(TEXT("nodesWithGetSubsystem"),        NodesWithGetSubsystem);
    Metrics->SetNumberField(TEXT("nodesWithBaseAsyncTask"),       NodesWithBaseAsyncTask);
    Metrics->SetNumberField(TEXT("nodesWithAddComponent"),        NodesWithAddComponent);
    Metrics->SetNumberField(TEXT("nodesWithSetFieldsInStruct"),   NodesWithSetFieldsInStruct);
    Metrics->SetNumberField(TEXT("nodesWithAsyncAction"),         NodesWithAsyncAction);
    Metrics->SetNumberField(TEXT("nodesWithDelegateOp"),          NodesWithDelegateOp);
    Metrics->SetNumberField(TEXT("nodesWithInterfaceMessage"),    NodesWithInterfaceMessage);
    Metrics->SetNumberField(TEXT("nodesWithCreateObject"),        NodesWithCreateObject);
    Metrics->SetNumberField(TEXT("nodesWithFormatText"),          NodesWithFormatText);
    Metrics->SetNumberField(TEXT("nodesWithInputKey"),            NodesWithInputKey);
    Metrics->SetNumberField(TEXT("nodesWithMathExpression"),      NodesWithMathExpression);
    Metrics->SetNumberField(TEXT("nodesWithEnhancedInputAction"), NodesWithEnhancedInputAction);
    Metrics->SetNumberField(TEXT("nodesWithAssignmentStatement"), NodesWithAssignmentStatement);
    Metrics->SetNumberField(TEXT("nodesWithTemporaryVariable"),   NodesWithTemporaryVariable);
    Metrics->SetNumberField(TEXT("nodesWithPropertyAccess"),      NodesWithPropertyAccess);
    Metrics->SetNumberField(TEXT("nodesWithGetDataTableRow"),     NodesWithGetDataTableRow);
    Metrics->SetNumberField(TEXT("nodesWithGetEnumeratorName"),   NodesWithGetEnumeratorName);
    // Task 51: new node type metrics
    Metrics->SetNumberField(TEXT("nodesWithMultiGate"),                     NodesWithMultiGate);
    Metrics->SetNumberField(TEXT("nodesWithListenForGameplayMessages"),      NodesWithListenForGameplayMessages);
    Report->SetObjectField(TEXT("metrics"), Metrics);

    Report->SetArrayField(TEXT("missingTopLevelKeys"), [&MissingTopLevelKeys]()
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        for (const FString& Key : MissingTopLevelKeys)
        {
            Out.Add(MakeShareable(new FJsonValueString(Key)));
        }
        return Out;
    }());

    Report->SetArrayField(TEXT("missingClassConfigFlagKeys"), [&MissingClassConfigKeys]()
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        for (const FString& Key : MissingClassConfigKeys)
        {
            Out.Add(MakeShareable(new FJsonValueString(Key)));
        }
        return Out;
    }());

    Report->SetArrayField(TEXT("missingComponentOverrideShapeKeys"), [&MissingComponentShapeKeys]()
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        for (const FString& Key : MissingComponentShapeKeys)
        {
            Out.Add(MakeShareable(new FJsonValueString(Key)));
        }
        return Out;
    }());

    FString ReportPath;
    if (Args.Num() > 1)
    {
        ReportPath = CleanConsoleArg(Args[1]);
        if (FPaths::IsRelative(ReportPath))
        {
            ReportPath = FPaths::ConvertRelativePathToFull(AbsoluteProjectDir, ReportPath);
        }
        else
        {
            ReportPath = FPaths::ConvertRelativePathToFull(ReportPath);
        }
    }
    else
    {
        ReportPath = FPaths::Combine(ExportDir, FString::Printf(TEXT("BP_SLZR_ValidationReport_%s.json"), *UDataExportManager::GetTimestamp()));
    }

    FPaths::NormalizeFilename(ReportPath);
    FPaths::CollapseRelativeDirectories(ReportPath);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);

    FString ReportJson;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ReportJson);
    FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);

    const bool bSaved = FFileHelper::SaveStringToFile(ReportJson, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    UE_LOG(LogTemp, Log, TEXT("✅ Converter-ready validation completed. OverallPass=%s"), bOverallPass ? TEXT("true") : TEXT("false"));
    UE_LOG(LogTemp, Log, TEXT("   Blueprints=%d (raw=%d, duplicatesIgnored=%d) ParseErrors=%d MissingRequired=%d"), BlueprintFileCount, RawBlueprintFileCount, DuplicateBlueprintExportsIgnored, ParseErrors, MissingRequiredExports);
    UE_LOG(LogTemp, Log, TEXT("   Declarations vars=%d/%d funcs=%d/%d repShape=%d/%d netShape=%d/%d"), VariableWithDeclarationSpecifiers, VariableTotal, FunctionWithDeclarationSpecifiers, FunctionTotal, VariablesWithReplicationShape, VariableTotal, FunctionsWithNetworkShape, FunctionTotal);
    UE_LOG(LogTemp, Log, TEXT("   GameplayTags exports=%d tags=%d sourceGraph=%d sourceNodeType=%d"), ExportsWithGameplayTags, GameplayTagsTotal, GameplayTagsWithSourceGraphMetadata, GameplayTagsWithSourceNodeTypeMetadata);
    UE_LOG(LogTemp, Log, TEXT("   Networking varsReplicated=%d repNotify=%d rep+notify=%d funcsNet=%d server=%d client=%d multicast=%d reliable=%d authOnly=%d cosmetic=%d any=%d"), VariablesReplicated, VariablesWithRepNotifyFunction, VariablesReplicatedWithRepNotifyFunction, FunctionsNet, FunctionsNetServer, FunctionsNetClient, FunctionsNetMulticast, FunctionsReliable, FunctionsBlueprintAuthorityOnly, FunctionsBlueprintCosmetic, FunctionsWithAnyNetworkSemantic);
    UE_LOG(LogTemp, Log, TEXT("   Components total=%d inherited=%d overrides=%d deltas=%d"), ComponentTotal, ComponentInheritedCount, ComponentHasOverrideCount, ComponentWithOverrideDeltaCount);
    UE_LOG(LogTemp, Log, TEXT("   ControlRigs exports=%d rigs=%d controls=%d bones=%d mapped=%d"), ExportsWithControlRigs, ControlRigsTotal, ControlRigsWithControls, ControlRigsWithBones, ControlRigsWithControlToBoneMap);
    UE_LOG(LogTemp, Log, TEXT("   AnimAssets=%d Notifies=%d trackName=%d triggeredEvent=%d eventParams=%d"), AnimationAssetsTotal, AnimationNotifiesTotal, AnimationNotifiesWithTrackName, AnimationNotifiesWithTriggeredEventName, AnimationNotifiesWithEventParameters);
    UE_LOG(LogTemp, Log, TEXT("   Curves=%d assetPath=%d interp=%d axis=%d vectorValues=%d transformValues=%d mat=%d morph=%d float=%d vector=%d transform=%d"), AnimationCurvesTotal, AnimationCurvesWithCurveAssetPath, AnimationCurvesWithInterpolationMode, AnimationCurvesWithAxisType, AnimationCurvesWithVectorValues, AnimationCurvesWithTransformValues, AnimationCurvesWithAffectedMaterials, AnimationCurvesWithAffectedMorphTargets, AnimationCurvesFloatType, AnimationCurvesVectorType, AnimationCurvesTransformType);
    UE_LOG(LogTemp, Log, TEXT("   Transitions=%d blendMode=%d priority=%d blendOut=%d notifyState=%d"), AnimationTransitionsTotal, AnimationTransitionsWithBlendMode, AnimationTransitionsWithPriority, AnimationTransitionsWithBlendOutTriggerTime, AnimationTransitionsWithNotifyStateIndex);
    UE_LOG(LogTemp, Log, TEXT("   Compiler fallback shape=%d/%d unsupportedExports=%d bytecodeExports=%d bytecodeFunctions=%d"), ExportsWithCompilerIRFallbackShape, BlueprintFileCount, ExportsWithUnsupportedNodeFallback, ExportsWithBytecodeFallback, CompilerIRFallbackBytecodeFunctionCountTotal);
    UE_LOG(LogTemp, Log, TEXT("   Dependency closure non-empty=%d includeHints non-empty=%d nativeIncludeHints non-empty=%d"), DependencyClosureNonEmpty, IncludeHintsNonEmpty, NativeIncludeHintsNonEmpty);
    UE_LOG(LogTemp, Log, TEXT("   Macro closure shape=%d/%d graphPathsExports=%d blueprintPathsExports=%d graphPathsTotal=%d blueprintPathsTotal=%d"), ExportsWithMacroDependencyClosureShape, BlueprintFileCount, ExportsWithMacroGraphPaths, ExportsWithMacroBlueprintPaths, MacroGraphPathsTotal, MacroBlueprintPathsTotal);
    UE_LOG(LogTemp, Log, TEXT("   Exports with Default__ classPaths=%d includeHints=%d"), ExportsWithDefaultObjectClassPaths, ExportsWithDefaultObjectIncludeHints);
    UE_LOG(LogTemp, Log, TEXT("   Validation report: %s%s"), *ReportPath, bSaved ? TEXT("") : TEXT(" (save failed)"));

    if (GEngine)
    {
        const FColor StatusColor = bOverallPass ? FColor::Green : FColor::Yellow;
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, StatusColor,
            FString::Printf(TEXT("Converter validation: %s (%d blueprints)"), bOverallPass ? TEXT("PASS") : TEXT("WARN"), BlueprintFileCount));
    }
#else
    UE_LOG(LogTemp, Warning, TEXT("Blueprint extraction only available in editor builds"));
#endif
}

// ExtractProjectDependencyMap, ExtractAssetDependencies, and MapAssetNetwork implementations
// are in BlueprintExtractorCommands_Extensions.cpp
