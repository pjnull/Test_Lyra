// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ISettingsSection.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWidget.h"
#include "Widgets/Docking/SDockTab.h"

class ULevelSnapshotsEditorProjectSettings;
class ULevelSnapshotsEditorDataManagementSettings;
class FToolBarBuilder;
class SLevelSnapshotsEditor;
class ULevelSnapshotsEditorData;

class FLevelSnapshotsEditorModule : public IModuleInterface
{
public:

	static FLevelSnapshotsEditorModule& Get();
	static void OpenLevelSnapshotsSettings();

	//~ Begin IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface

	bool GetUseCreationForm() const;
	void SetUseCreationForm(bool bInUseCreationForm);
	void ToggleUseCreationForm() { SetUseCreationForm(!GetUseCreationForm()); }

	void OpenLevelSnapshotsDialogWithAssetSelected(const FAssetData& InAssetData);
	void OpenSnapshotsEditor();

	TWeakObjectPtr<ULevelSnapshotsEditorProjectSettings> GetLevelSnapshotsUserSettings() const { return ProjectSettingsObjectPtr; }
	TWeakObjectPtr<ULevelSnapshotsEditorDataManagementSettings> GetLevelSnapshotsDataManagementSettings() const { return DataMangementSettingsObjectPtr; }

private:

	void RegisterTabSpawner();
	void UnregisterTabSpawner();

	TSharedRef<SDockTab> SpawnLevelSnapshotsTab(const FSpawnTabArgs& SpawnTabArgs);
	ULevelSnapshotsEditorData* AllocateTransientPreset();
	
	bool RegisterProjectSettings();
	bool HandleModifiedProjectSettings();
	
	void RegisterEditorToolbar();
	void MapEditorToolbarActions();
	TSharedRef<SWidget> FillEditorToolbarComboButtonMenuOptions(TSharedPtr<class FUICommandList> Commands);

	
	/** Command list (for combo button sub menu options) */
	TSharedPtr<FUICommandList> EditorToolbarButtonCommandList;

	/** Lives for as long as the UI is open. */
	TWeakPtr<SLevelSnapshotsEditor> WeakSnapshotEditor;

	TSharedPtr<ISettingsSection> ProjectSettingsSectionPtr;
	TWeakObjectPtr<ULevelSnapshotsEditorProjectSettings> ProjectSettingsObjectPtr;
	
	TSharedPtr<ISettingsSection> DataMangementSettingsSectionPtr;
	TWeakObjectPtr<ULevelSnapshotsEditorDataManagementSettings> DataMangementSettingsObjectPtr;
};
