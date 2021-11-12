// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

struct FConsoleVariablesEditorCommandInfo;

class FConsoleVariablesEditorMainPanel;
class FConsoleVariablesEditorToolkit;
class ISettingsSection;
class UConsoleVariablesAsset;
class UConsoleVariablesEditorProjectSettings;

class FConsoleVariablesEditorModule : public IModuleInterface
{
public:
	
	static FConsoleVariablesEditorModule& Get();

	//~ Begin IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface

	void OpenConsoleVariablesDialogWithAssetSelected(const FAssetData& InAssetData);
	
	static void OpenConsoleVariablesSettings();

	TWeakObjectPtr<UConsoleVariablesEditorProjectSettings> GetConsoleVariablesUserSettings() const
	{
		return ProjectSettingsObjectPtr;
	}

	/** Find all console variables and cache their startup values */
	void QueryAndBeginTrackingConsoleVariables();

	/** Find a tracked console variable by the command string with optional case sensitivity. */
	TWeakPtr<FConsoleVariablesEditorCommandInfo> FindCommandInfoByName(const FString& NameToSearch, ESearchCase::Type InSearchCase = ESearchCase::IgnoreCase);
	
	[[nodiscard]] TObjectPtr<UConsoleVariablesAsset> GetEditingAsset() const;
	void SetEditingAsset(const TObjectPtr<UConsoleVariablesAsset> InEditingAsset);
	
private:

	void PostEngineInit();
	void OnFEngineLoopInitComplete();

	void RegisterMenuItem();
	bool RegisterProjectSettings();
	bool HandleModifiedProjectSettings();

	TObjectPtr<UConsoleVariablesAsset> AllocateTransientPreset();

	TSharedRef<SDockTab> SpawnMainPanelTab(const FSpawnTabArgs& Args);

	void OpenConsoleVariablesEditor();

	void OnConsoleVariableChange();
	
	static const FName ConsoleVariablesToolkitPanelTabId;

	/* Lives for as long as the module is loaded. */
	TSharedPtr<FConsoleVariablesEditorMainPanel> MainPanel;

	// Transient preset that's being edited so we don't affect the reference asset unless we save it
	TObjectPtr<UConsoleVariablesAsset> EditingAsset = nullptr;

	TSharedPtr<ISettingsSection> ProjectSettingsSectionPtr;
	TWeakObjectPtr<UConsoleVariablesEditorProjectSettings> ProjectSettingsObjectPtr;

	/** All tracked variables and their default, startup, and current values */
	TArray<TSharedPtr<FConsoleVariablesEditorCommandInfo>> ConsoleVariablesMasterReference;

	/** A callback registered with the Console Manager that is called when a console variable is changed */
	FConsoleVariableSinkHandle VariableChangedSinkHandle;

	FConsoleCommandDelegate VariableChangedSinkDelegate;
};
