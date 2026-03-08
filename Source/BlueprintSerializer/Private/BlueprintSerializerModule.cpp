#include "BlueprintSerializerModule.h"
#include "BSSettings.h"
#include "BSDataTypes.h"
#include "BlueprintAnalyzer.h"
#include "BlueprintExtractorBlueprintLibrary.h"

#include "ISettingsModule.h"
#include "HAL/IConsoleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Engine/Blueprint.h"
#include "Animation/AnimBlueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "BlueprintSerializer"

DEFINE_LOG_CATEGORY(LogBlueprintSerializer);

void FBlueprintSerializerModule::StartupModule()
{
	UE_LOG(LogBlueprintSerializer, Log, TEXT("Blueprint Serializer: Module startup"));
	
	// Register settings
	RegisterSettings();
	
	// Register console commands
	RegisterConsoleCommands();
	
	// Register menu extensions (if in editor)
#if WITH_EDITOR
	RegisterMenuExtensions();
#endif
}

void FBlueprintSerializerModule::ShutdownModule()
{
	UE_LOG(LogBlueprintSerializer, Log, TEXT("Blueprint Serializer: Module shutdown"));
	
	// Unregister menu extensions
#if WITH_EDITOR
	UnregisterMenuExtensions();
#endif
	
	// Unregister console commands
	UnregisterConsoleCommands();
	
	// Unregister settings
	UnregisterSettings();
}

void FBlueprintSerializerModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
			"Project",
			"Plugins",
			"BlueprintSerializer",
			LOCTEXT("BlueprintSerializerSettingsName", "Blueprint Serializer"),
			LOCTEXT("BlueprintSerializerSettingsDescription", "Configure Blueprint serialization and export settings"),
			GetMutableDefault<UBlueprintSerializerSettings>()
		);
	}
}

void FBlueprintSerializerModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "BlueprintSerializer");
	}
}

void FBlueprintSerializerModule::RegisterConsoleCommands()
{
	// BP_SLZR.Serialize - Main serialization command
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("BP_SLZR.Serialize"),
		TEXT("Serialize a Blueprint to JSON. Usage: BP_SLZR.Serialize /Game/Path/To/Blueprint"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogBlueprintSerializer, Warning, TEXT("Usage: BP_SLZR.Serialize /Game/Path/To/Blueprint"));
				return;
			}
			
			FString BlueprintPath = Args[0];
			UE_LOG(LogBlueprintSerializer, Log, TEXT("Serializing Blueprint: %s"), *BlueprintPath);

			int32 SemicolonIndex;
			if (BlueprintPath.FindChar(';', SemicolonIndex))
			{
				BlueprintPath = BlueprintPath.Left(SemicolonIndex);
			}

			if (BlueprintPath.IsEmpty())
			{
				UE_LOG(LogBlueprintSerializer, Warning, TEXT("Empty Blueprint path after sanitization"));
				return;
			}

			const bool bSuccess = UBlueprintAnalyzer::ExportSingleBlueprintToJSON(BlueprintPath);
			if (!bSuccess)
			{
				UE_LOG(LogBlueprintSerializer, Error, TEXT("Failed to serialize Blueprint: %s"), *BlueprintPath);
			}
		}),
		ECVF_Default
	));
	
	// BP_SLZR.Count - Count blueprints in project
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("BP_SLZR.Count"),
		TEXT("Count all Blueprints in the current project"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const int32 TotalCount = UBlueprintAnalyzer::CollectAllProjectBlueprintAssetData().Num();
			
			UE_LOG(LogBlueprintSerializer, Display, TEXT("Found %d Blueprints in project"), TotalCount);
			
			// Show notification
			FNotificationInfo Info(FText::Format(
				LOCTEXT("BlueprintCountNotification", "Found {0} Blueprints"),
				FText::AsNumber(TotalCount)
			));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}),
		ECVF_Default
	));
	
	// BP_SLZR.Settings - Open plugin settings
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("BP_SLZR.Settings"),
		TEXT("Open Blueprint Serializer settings in Project Settings"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
			{
				SettingsModule->ShowViewer("Project", "Plugins", "BlueprintSerializer");
			}
		}),
		ECVF_Default
	));
	
	// BP_SLZR.OpenFolder - Open export folder
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("BP_SLZR.OpenFolder"),
		TEXT("Open the Blueprint Serializer export folder"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const UBlueprintSerializerSettings* Settings = GetDefault<UBlueprintSerializerSettings>();
			FString ExportDir = Settings->GetExportDirectory();
			
			// Ensure directory exists
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			if (!PlatformFile.DirectoryExists(*ExportDir))
			{
				PlatformFile.CreateDirectoryTree(*ExportDir);
			}
			
			// Open folder in system explorer
			FPlatformProcess::ExploreFolder(*ExportDir);
			
			UE_LOG(LogBlueprintSerializer, Log, TEXT("Opened export folder: %s"), *ExportDir);
		}),
		ECVF_Default
	));
	
	// BP_SLZR.Version - Show plugin version
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("BP_SLZR.Version"),
		TEXT("Show Blueprint Serializer plugin version"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UE_LOG(LogBlueprintSerializer, Display, TEXT("Blueprint Serializer v1.0.0"));
			UE_LOG(LogBlueprintSerializer, Display, TEXT("Engine: %d.%d.%d"), 
				ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION);
			
			// Show notification
			FNotificationInfo Info(LOCTEXT("VersionNotification", "Blueprint Serializer v1.0.0"));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}),
		ECVF_Default
	));
	
	// BP_SLZR.GenerateContext - Generate LLM context file
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("BP_SLZR.GenerateContext"),
		TEXT("Generate LLM context file for a Blueprint. Usage: BP_SLZR.GenerateContext /Game/Path/To/Blueprint"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogBlueprintSerializer, Warning, TEXT("Usage: BP_SLZR.GenerateContext /Game/Path/To/Blueprint"));
				return;
			}
			
			const UBlueprintSerializerSettings* Settings = GetDefault<UBlueprintSerializerSettings>();
			if (!Settings->bGenerateLLMContext)
			{
				UE_LOG(LogBlueprintSerializer, Warning, TEXT("LLM context generation is disabled in settings; proceeding because command was invoked explicitly"));
			}
			
			FString BlueprintPath = Args[0];
			UE_LOG(LogBlueprintSerializer, Log, TEXT("Generating LLM context for: %s"), *BlueprintPath);

			int32 SemicolonIndex;
			if (BlueprintPath.FindChar(';', SemicolonIndex))
			{
				BlueprintPath = BlueprintPath.Left(SemicolonIndex);
			}

			if (BlueprintPath.IsEmpty())
			{
				UE_LOG(LogBlueprintSerializer, Warning, TEXT("Empty Blueprint path after sanitization"));
				return;
			}

			UBlueprint* TargetBlueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
			if (!TargetBlueprint)
			{
				UE_LOG(LogBlueprintSerializer, Error, TEXT("Could not load Blueprint: %s"), *BlueprintPath);
				return;
			}

			if (!UBlueprintSerializerBlueprintLibrary::GenerateLLMContext(TargetBlueprint))
			{
				UE_LOG(LogBlueprintSerializer, Error, TEXT("LLM context generation failed: %s"), *BlueprintPath);
			}
		}),
		ECVF_Default
	));
	
	// BP_SLZR.Help - Show help for all commands
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("BP_SLZR.Help"),
		TEXT("Show help for all Blueprint Serializer commands"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UE_LOG(LogBlueprintSerializer, Display, TEXT("=== Blueprint Serializer Commands ==="));
			UE_LOG(LogBlueprintSerializer, Display, TEXT("BP_SLZR.Serialize <path> - Serialize a Blueprint to JSON"));
			UE_LOG(LogBlueprintSerializer, Display, TEXT("BP_SLZR.Count - Count all Blueprints in project"));
			UE_LOG(LogBlueprintSerializer, Display, TEXT("BP_SLZR.Settings - Open plugin settings"));
			UE_LOG(LogBlueprintSerializer, Display, TEXT("BP_SLZR.OpenFolder - Open export folder"));
			UE_LOG(LogBlueprintSerializer, Display, TEXT("BP_SLZR.Version - Show plugin version"));
			UE_LOG(LogBlueprintSerializer, Display, TEXT("BP_SLZR.GenerateContext <path> - Generate LLM context file"));
			UE_LOG(LogBlueprintSerializer, Display, TEXT("BP_SLZR.Help - Show this help"));
		}),
		ECVF_Default
	));
}

void FBlueprintSerializerModule::UnregisterConsoleCommands()
{
	for (IConsoleCommand* Command : ConsoleCommands)
	{
		if (Command)
		{
			IConsoleManager::Get().UnregisterConsoleObject(Command);
		}
	}
	ConsoleCommands.Empty();
}

void FBlueprintSerializerModule::RegisterMenuExtensions()
{
#if WITH_EDITOR
	// Add Tools menu entry
	if (UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		if (Menu)
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("Programming");
			
			Section.AddSubMenu(
				"BlueprintSerializer",
				LOCTEXT("BlueprintSerializerMenu", "Blueprint Serializer"),
				LOCTEXT("BlueprintSerializerMenuTooltip", "Serialize Blueprints to JSON for analysis and code generation"),
				FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
				{
					FToolMenuSection& SubSection = SubMenu->AddSection("BlueprintSerializerActions", 
						LOCTEXT("SerializationActions", "Serialization Actions"));
					
					SubSection.AddMenuEntry(
						"SerializeSelected",
						LOCTEXT("SerializeSelected", "Serialize Selected Blueprint"),
						LOCTEXT("SerializeSelectedTooltip", "Serialize the currently selected Blueprint to JSON"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([]() 
						{
							FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
							IContentBrowserSingleton& ContentBrowser = ContentBrowserModule.Get();
							TArray<FAssetData> SelectedAssets;
							ContentBrowser.GetSelectedAssets(SelectedAssets);

							for (const FAssetData& AssetData : SelectedAssets)
							{
								if (Cast<UBlueprint>(AssetData.GetAsset()))
								{
									const FString BlueprintPath = AssetData.GetObjectPathString();
									const bool bSuccess = UBlueprintAnalyzer::ExportSingleBlueprintToJSON(BlueprintPath);
									UE_LOG(LogBlueprintSerializer, Log, TEXT("Serialize selected Blueprint (%s): %s"), *BlueprintPath, bSuccess ? TEXT("Success") : TEXT("Failed"));
									return;
								}
							}

							UE_LOG(LogBlueprintSerializer, Warning, TEXT("No Blueprint selected in Content Browser"));
						}))
					);
					
					SubSection.AddMenuEntry(
						"OpenSettings",
						LOCTEXT("OpenSettings", "Settings..."),
						LOCTEXT("OpenSettingsTooltip", "Open Blueprint Serializer settings"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([]() 
						{
							if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
							{
								SettingsModule->ShowViewer("Project", "Plugins", "BlueprintSerializer");
							}
						}))
					);
					
					SubSection.AddMenuEntry(
						"OpenExportFolder",
						LOCTEXT("OpenExportFolder", "Open Export Folder"),
						LOCTEXT("OpenExportFolderTooltip", "Open the folder where serialized files are saved"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([]() 
						{
							const UBlueprintSerializerSettings* Settings = GetDefault<UBlueprintSerializerSettings>();
							FString ExportDir = Settings->GetExportDirectory();
							FPlatformProcess::ExploreFolder(*ExportDir);
						}))
					);
				})
			);
		}
	}
#endif
}

void FBlueprintSerializerModule::UnregisterMenuExtensions()
{
#if WITH_EDITOR
	if (UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::Get()->RemoveSection("LevelEditor.MainMenu.Tools", "BlueprintSerializer");
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintSerializerModule, BlueprintSerializer)
