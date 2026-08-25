#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"

#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#endif

/**
 * Console commands for Blueprint Serializer
 * These can be run directly in the Editor console without modifying any Blueprints
 * 
 * Commands use the BP_SLZR.* namespace:
 * - BP_SLZR.ExportSingleBlueprint <path> - Export one Blueprint to JSON
 * - BP_SLZR.ExportMultipleBlueprints <paths...> - Export multiple Blueprints
 * - BP_SLZR.ExportBlueprintsFromManifest <file> [output-dir] - Export an exact path-list batch
 * - BP_SLZR.AnalyzeBlueprint <path> - Analyze a Blueprint (log only)
 * - BP_SLZR.CountBlueprints - Count all Blueprints in project
 * - BP_SLZR.ExportAllBlueprints - Export all Blueprints (dangerous)
 * - BP_SLZR.ExportCompleteData - Export project data + references (dangerous)
 * - BP_SLZR.ExtractAssetDependencies - Dependency analysis
 * - BP_SLZR.ExtractProjectDependencyMap - Project-wide dependency map
 * - BP_SLZR.MapAssetNetwork - Asset network mapping
 * - BP_SLZR.ValidateConverterReady - Validate converter-ready export gates
 * - BP_SLZR.AuditAnimationCurves - Audit project anim curve corpus
 * - BP_SLZR.RunRegressionSuite - Run full regression workflow
 */
class BLUEPRINTSERIALIZER_API FBlueprintExtractorCommands
{
public:
    static void RegisterCommands();
    static void UnregisterCommands();

    // Console callbacks must be public because the delegates are constructed at
    // namespace scope, outside the class's access context.
    static void ExportSingleBlueprint(const TArray<FString>& Args);
    static void ExportAllBlueprints(const TArray<FString>& Args);
    static void ExportCompleteProjectData(const TArray<FString>& Args);
    static void AnalyzeSpecificBlueprint(const TArray<FString>& Args);
    static void ExportMultipleBlueprints(const TArray<FString>& Args);
    static void ExportBlueprintsFromManifest(const TArray<FString>& Args);
    static void CountProjectBlueprints(const TArray<FString>& Args);
    static void ValidateConverterReady(const TArray<FString>& Args);
    static void AuditAnimationCurves(const TArray<FString>& Args);
    static void RunRegressionSuite(const TArray<FString>& Args);

    static void ExtractAssetDependencies(const TArray<FString>& Args);
    static void ExtractProjectDependencyMap(const TArray<FString>& Args);
    static void MapAssetNetwork(const TArray<FString>& Args);

#if WITH_EDITOR
    // Console command objects - managed internally
#endif
};
