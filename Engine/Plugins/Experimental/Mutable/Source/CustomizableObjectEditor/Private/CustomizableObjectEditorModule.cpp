// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomizableObjectEditorModule.h"
#include "CustomizableObjectEditor.h"
#include "CustomizableObjectInstanceEditor.h"
#include "CustomizableObjectDebugger.h"
#include "CustomizableObjectInstance.h"
#include "CustomizableSkeletalMeshActor.h"
#include "CustomizableObjectInstanceFactory.h"
#include "CustomizableObjectDetails.h"
#include "CustomizableInstanceDetails.h"
#include "CustomizableObjectSystem.h"		// For defines related to memory function replacements.
#include "CustomizableObjectEditorStyle.h"
#include "Nodes/CustomizableObjectNodeCopyMaterial.h"
#include "Nodes/CustomizableObjectNodeEditMaterial.h"
#include "Nodes/CustomizableObjectNodeExtendMaterial.h"
#include "Nodes/CustomizableObjectNodeLayoutBlocks.h"
#include "Nodes/CustomizableObjectNodeMeshClipMorph.h"
#include "Nodes/CustomizableObjectNodeMeshMorph.h"
#include "Nodes/CustomizableObjectNodeMorphMaterial.h"
#include "Nodes/CustomizableObjectNodeProjectorConstant.h"
#include "Nodes/CustomizableObjectNodeProjectorParameter.h"
#include "Nodes/CustomizableObjectNodeMeshReshape.h"
#include "Nodes/CustomizableObjectNodeLayoutBlocksDetails.h"
#include "Nodes/CustomizableObjectNodeParentedMaterialDetails.h"
#include "Nodes/CustomizableObjectNodeEditMaterialBaseDetails.h"
#include "Nodes/CustomizableObjectNodeMorphMaterialDetails.h"
#include "Nodes/CustomizableObjectNodeObjectDetails.h"
#include "CustomizableObjectNodeObjectGroupDetails.h"
#include "Nodes/CustomizableObjectNodeMeshMorphDetails.h"
#include "Nodes/CustomizableObjectNodeProjectorParameterDetails.h"
#include "Nodes/CustomizableObjectNodeMeshClipMorphDetails.h"
#include "Nodes/CustomizableObjectNodeMeshClipWithMeshDetails.h"
#include "Nodes/CustomizableObjectNodeExternalPinDetails.h"
#include "Nodes/CustomizableObjectNodeCopyMaterialDetails.h"
#include "Nodes/CustomizableObjectNodeSkeletalMeshDetails.h"
#include "Nodes/CustomizableObjectNodePinViewerDetails.h"
#include "Nodes/CustomizableObjectNodeTableDetails.h"
#include "Nodes/CustomizableObjectNodeMeshReshapeSelectionDetails.h"
#include "CustomizableObjectIdentifierCustomization.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_CustomizableObject.h"
#include "AssetTypeActions_CustomizableObjectInstance.h"
#include "Styling/SlateStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "MessageLogModule.h"
#include "Logging/MessageLog.h"
#include "CustomizableObjectEditorSettings.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "CustomizableObjectCustomSettingsDetails.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "CoreGlobals.h"
#include "CustomizableObjectCustomSettings.h"
#include "Settings/ProjectPackagingSettings.h"
#include "UnrealEditorPortabilityHelpers.h"
#include "CookOnTheSide/CookOnTheFlyServer.h" // This is necessary to register the cook delegate.
#include "Nodes/CustomizableObjectNodeStaticMesh.h"
#include "CustomizableObjectDGGUI.h"
#include "Kismet/GameplayStatics.h"


const FName CustomizableObjectEditorAppIdentifier = FName(TEXT("CustomizableObjectEditorApp"));
const FName CustomizableObjectInstanceEditorAppIdentifier = FName(TEXT("CustomizableObjectInstanceEditorApp"));
const FName CustomizableObjectDebuggerAppIdentifier = FName(TEXT("CustomizableObjectDebuggerApp"));

#define LOCTEXT_NAMESPACE "MutableSettings"

static TAutoConsoleVariable<bool> CVarMutableOnCookStartEnabled(
	TEXT("b.OnCookStartEnabled"),
	true,
	TEXT("If enabled, Customizable Objects will be compiled before the actual cook starts. Compiled data will be stored on de DDC and cached during BeginCache.\n"),
	ECVF_Scalability);

/**
 * StaticMesh editor module
 */
class FCustomizableObjectEditorModule : public ICustomizableObjectEditorModule
{
public:
	// IModuleInterface interface
	void StartupModule() override;
	void ShutdownModule() override;

	// ICustomizableObjectEditorModule interface
	TSharedRef<ICustomizableObjectEditor> CreateCustomizableObjectEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UCustomizableObject* CustomizableObject ) override;
	TSharedRef<ICustomizableObjectInstanceEditor> CreateCustomizableObjectInstanceEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UCustomizableObjectInstance* CustomizableObjectInstance ) override;
	TSharedRef<ICustomizableObjectDebugger> CreateCustomizableObjectDebugger(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UCustomizableObject* CustomizableObject) override;
	virtual EAssetTypeCategories::Type GetAssetCategory() const override;
	virtual FCustomizableObjectEditorLogger& GetLogger() override;

	virtual TSharedPtr<FExtensibilityManager> GetCustomizableObjectEditorToolBarExtensibilityManager() override { return CustomizableObjectEditor_ToolBarExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetCustomizableObjectEditorMenuExtensibilityManager() override { return CustomizableObjectEditor_MenuExtensibilityManager; };

public:
	bool HandleSettingsSaved();
	void RegisterSettings();

private:	
	TSharedPtr<FExtensibilityManager> CustomizableObjectEditor_ToolBarExtensibilityManager;
	TSharedPtr<FExtensibilityManager> CustomizableObjectEditor_MenuExtensibilityManager;

	/** List of registered custom details to remove later. */
	TArray<FName> RegisteredCustomDetails;

	/** Custom asset category. */
	EAssetTypeCategories::Type CustomizableObjectAssetCategory;

	/** Register Custom details. Also adds them to RegisteredCustomDetails list. */
	void RegisterCustomDetails(FPropertyEditorModule& PropertyModule, const UClass* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate);

	FCustomizableObjectEditorLogger Logger;

	// Command to look for Customizable Object Instance in the player pawn of the current world and open a DGGUI to edit its parameters
	IConsoleCommand* LaunchDGGUICommand;
	static void ToggleDGGUI(const TArray<FString>& Arguments);

	// Command to look for Customizable Object Instance in the player pawn of the current world and open its Customizable Object Instance Editor
	IConsoleCommand* LaunchCOIECommand;
	static void OpenCOIE(const TArray<FString>& Arguments);
};

IMPLEMENT_MODULE( FCustomizableObjectEditorModule, CustomizableObjectEditor );


static void* CustomMalloc(std::size_t Size_t, uint32_t Alignment)
{
	return FMemory::Malloc(Size_t, Alignment);
}


static void CustomFree(void* mem)
{
	return FMemory::Free(mem);
}


FCustomizableObjectCompilerBase* NewCompiler()
{
	return new FCustomizableObjectCompiler();
}


void FCustomizableObjectEditorModule::StartupModule()
{
	UCustomizableObjectSystem::GetInstance()->SetNewCompilerFunc(NewCompiler);

	// Register the thumbnail renderers
	//UThumbnailManager::Get().RegisterCustomRenderer(UCustomizableObject::StaticClass(), UCustomizableObjectThumbnailRenderer::StaticClass());
	//UThumbnailManager::Get().RegisterCustomRenderer(UCustomizableObjectInstance::StaticClass(), UCustomizableObjectInstanceThumbnailRenderer::StaticClass());
	
	// Property views
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	RegisterCustomDetails(PropertyModule, UCustomizableObject::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectInstance::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableInstanceDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeLayoutBlocks::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeLayoutBlocksDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeExtendMaterial::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeParentedMaterialDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeEditMaterialBase::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeEditMaterialBaseDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeEditMaterial::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodePinViewerDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeMorphMaterial::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeMorphMaterialDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeObject::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeObjectDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeObjectGroup::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeObjectGroupDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeProjectorParameter::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeProjectorParameterDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeProjectorConstant::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeProjectorParameterDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeMeshMorph::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeMeshMorphDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeMeshClipMorph::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeMeshClipMorphDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeMeshClipWithMesh::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeMeshClipWithMeshDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeExternalPin::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeExternalPinDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectEmptyClassForSettings::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectCustomSettingsDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeMaterial::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodePinViewerDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeCopyMaterial::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeCopyMaterialDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeSkeletalMesh::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeSkeletalMeshDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeStaticMesh::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodePinViewerDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeTable::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeTableDetails::MakeInstance));

	//Custom properties
	PropertyModule.RegisterCustomPropertyTypeLayout("CustomizableObjectIdentifier", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCustomizableObjectIdentifierCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(FMeshReshapeBoneReference::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMeshReshapeBonesReferenceCustomization::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();

	// Register factory
	UCustomizableObjectInstanceFactory* CustomizableObjectInstanceFactory = NewObject<UCustomizableObjectInstanceFactory>();
	CustomizableObjectInstanceFactory->NewActorClass = ACustomizableSkeletalMeshActor::StaticClass();
	UEditorEngine* engine = GEditor;
	engine->ActorFactories.Add(CustomizableObjectInstanceFactory);
	
	// New category
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	CustomizableObjectAssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("Mutable")), LOCTEXT("MutableAssetsCategory", "Mutable"));

	// Asset actions
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked< FAssetToolsModule >( "AssetTools" );
	
	TSharedPtr<FAssetTypeActions_CustomizableObject> CustomizableObjectAssetTypeActions = MakeShareable( new FAssetTypeActions_CustomizableObject );	
	AssetToolsModule.Get().RegisterAssetTypeActions( CustomizableObjectAssetTypeActions.ToSharedRef() );

	TSharedPtr<FAssetTypeActions_CustomizableObjectInstance> CustomizableObjectInstanceAssetTypeActions = MakeShareable( new FAssetTypeActions_CustomizableObjectInstance );
	AssetToolsModule.Get().RegisterAssetTypeActions( CustomizableObjectInstanceAssetTypeActions.ToSharedRef() );
	

	// Additional UI style
	FCustomizableObjectEditorStyle::Initialize();

	RegisterSettings();

	// Create the message log category
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.RegisterLogListing(FName("Mutable"), LOCTEXT("MutableLog", "Mutable"));

	CustomizableObjectEditor_ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
	CustomizableObjectEditor_MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);

	LaunchDGGUICommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("mutable.ToggleDGGUI"),
		TEXT("Looks for a Customizable Object Instance within the player pawn and opens a UI to modify its parameters, or closes it if it's open. Specify slot ID to control which component is modified."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FCustomizableObjectEditorModule::ToggleDGGUI));

	LaunchCOIECommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("mutable.OpenCOIE"),
		TEXT("Looks for a Customizable Object Instance within the player pawn and opens its Customizable Object Instance Editor. Specify slot ID to control which component is edited."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FCustomizableObjectEditorModule::OpenCOIE));

}


void FCustomizableObjectEditorModule::ShutdownModule()
{
	if( FModuleManager::Get().IsModuleLoaded( "PropertyEditor" ) )
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		for (const auto& ClassName : RegisteredCustomDetails)
		{
			PropertyModule.UnregisterCustomClassLayout(ClassName);
		}

		PropertyModule.NotifyCustomizationModuleChanged();
	}

	CustomizableObjectEditor_ToolBarExtensibilityManager.Reset();
	CustomizableObjectEditor_MenuExtensibilityManager.Reset();

	FCustomizableObjectEditorStyle::Shutdown();
}


TSharedRef<ICustomizableObjectEditor> FCustomizableObjectEditorModule::CreateCustomizableObjectEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UCustomizableObject* CustomizableObject )
{
	return FCustomizableObjectEditor::Create(Mode, InitToolkitHost,CustomizableObject);
}


TSharedRef<ICustomizableObjectInstanceEditor> FCustomizableObjectEditorModule::CreateCustomizableObjectInstanceEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UCustomizableObjectInstance* CustomizableObjectInstance )
{
	TSharedRef<FCustomizableObjectInstanceEditor> NewCustomizableObjectInstanceEditor(new FCustomizableObjectInstanceEditor());
	NewCustomizableObjectInstanceEditor->InitCustomizableObjectInstanceEditor(Mode, InitToolkitHost,CustomizableObjectInstance);
	return NewCustomizableObjectInstanceEditor;
}


TSharedRef<ICustomizableObjectDebugger> FCustomizableObjectEditorModule::CreateCustomizableObjectDebugger(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UCustomizableObject* CustomizableObject)
{
	TSharedRef<FCustomizableObjectDebugger> NewCustomizableObjectDebugger(new FCustomizableObjectDebugger());
	NewCustomizableObjectDebugger->InitCustomizableObjectDebugger(Mode, InitToolkitHost, CustomizableObject);
	return NewCustomizableObjectDebugger;
}


FCustomizableObjectEditorLogger& FCustomizableObjectEditorModule::GetLogger()
{
	return Logger;
}


bool FCustomizableObjectEditorModule::HandleSettingsSaved()
{
	UCustomizableObjectEditorSettings* CustomizableObjectSettings = GetMutableDefault<UCustomizableObjectEditorSettings>();

	if (CustomizableObjectSettings != nullptr)
	{
		CustomizableObjectSettings->SaveConfig();
		
		FEditorCompileSettings CompileSettings;
		CompileSettings.bDisableCompilation = CustomizableObjectSettings->bDisableMutableCompileInEditor;
		CompileSettings.bEnableAutomaticCompilation = CustomizableObjectSettings->bEnableAutomaticCompilation;
		CompileSettings.bCompileObjectsSynchronously = CustomizableObjectSettings->bCompileObjectsSynchronously;
		CompileSettings.bCompileRootObjectsOnStartPIE = CustomizableObjectSettings->bCompileRootObjectsOnStartPIE;
		
		UCustomizableObjectSystem::GetInstance()->EditorSettingsChanged(CompileSettings);
	}

    return true;
}


void FCustomizableObjectEditorModule::RegisterSettings()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
    {
        ISettingsSectionPtr SettingsSectionPtr = SettingsModule->RegisterSettings("Project", "Plugins", "CustomizableObjectSettings",
            LOCTEXT("MutableSettings_Setting", "Mutable"),
            LOCTEXT("MutableSettings_Setting_Desc", "Mutable Settings"),
            GetMutableDefault<UCustomizableObjectEditorSettings>()
        );

        if (SettingsSectionPtr.IsValid())
        {
            SettingsSectionPtr->OnModified().BindRaw(this, &FCustomizableObjectEditorModule::HandleSettingsSaved);
        }

		if (UCustomizableObjectSystem::GetInstance() != nullptr)
		{
			UCustomizableObjectEditorSettings* CustomizableObjectSettings = GetMutableDefault<UCustomizableObjectEditorSettings>();
			if (CustomizableObjectSettings != nullptr)
			{
				FEditorCompileSettings CompileSettings;
				CompileSettings.bDisableCompilation = CustomizableObjectSettings->bDisableMutableCompileInEditor;
				CompileSettings.bEnableAutomaticCompilation = CustomizableObjectSettings->bEnableAutomaticCompilation;
				CompileSettings.bCompileObjectsSynchronously = CustomizableObjectSettings->bCompileObjectsSynchronously;
				CompileSettings.bCompileRootObjectsOnStartPIE = CustomizableObjectSettings->bCompileRootObjectsOnStartPIE;
				
				UCustomizableObjectSystem::GetInstance()->EditorSettingsChanged(CompileSettings);
			}
		}
    }
}


void FCustomizableObjectEditorModule::RegisterCustomDetails(FPropertyEditorModule& PropertyModule, const UClass* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate)
{
	const FName ClassName = FName(Class->GetName());
	PropertyModule.RegisterCustomClassLayout(ClassName, DetailLayoutDelegate);

	RegisteredCustomDetails.Add(ClassName);
}


const UWorld* GetWorldForCurrentCOI()
{
	UWorld* CurrentWorld = nullptr;
	const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
	for (const FWorldContext& Context : WorldContexts)
	{
		if ((Context.WorldType == EWorldType::Game) && (Context.World() != NULL))
		{
			CurrentWorld = Context.World();
		}
	}
	// Fall back to GWorld if we don't actually have a world.
	if (CurrentWorld == nullptr)
	{
		CurrentWorld = GWorld;
	}
	return CurrentWorld;
}


UCustomizableSkeletalComponent* GetPlayerCustomizableSkeletalComponent(const int32 SlotID, const UWorld* CurrentWorld, const int32 PlayerIndex = 0)
{
	// Get customizable skeletal component attached to player pawn
	UCustomizableSkeletalComponent* SelectedCustomizableSkeletalComponent = nullptr;
	{
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(CurrentWorld, PlayerIndex);
		int32 IndexFound = INDEX_NONE;
		for (TObjectIterator<UCustomizableSkeletalComponent> CustomizableSkeletalComponent; CustomizableSkeletalComponent; ++CustomizableSkeletalComponent)
		{
			if (CustomizableSkeletalComponent->IsValidLowLevel() && !CustomizableSkeletalComponent->IsTemplate())
			{
				AActor* CustomizableActor = CustomizableSkeletalComponent->GetAttachmentRootActor();
				if (CustomizableActor && PlayerPawn == CustomizableActor)
				{
					++IndexFound;
					SelectedCustomizableSkeletalComponent = *CustomizableSkeletalComponent;
					if (IndexFound == SlotID)
					{
						break;
					}
				}
			}
		}
	}


	// If none found, try getting a component without caring about the actor
	if (!SelectedCustomizableSkeletalComponent)
	{
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(CurrentWorld, PlayerIndex);
		int32 IndexFound = INDEX_NONE;
		for (TObjectIterator<UCustomizableSkeletalComponent> CustomizableSkeletalComponent; CustomizableSkeletalComponent; ++CustomizableSkeletalComponent)
		{
			if (CustomizableSkeletalComponent->IsValidLowLevel() && !CustomizableSkeletalComponent->IsTemplate())
			{
				++IndexFound;
				SelectedCustomizableSkeletalComponent = *CustomizableSkeletalComponent;
				if (IndexFound == SlotID)
				{
					break;
				}
			}
		}
	}

	return SelectedCustomizableSkeletalComponent;
}


void FCustomizableObjectEditorModule::ToggleDGGUI(const TArray<FString>& Arguments)
{
	int32 SlotID = INDEX_NONE;
	if (Arguments.Num() >= 1)
	{
		SlotID = FCString::Atoi(*Arguments[0]);
	}
	const UWorld* CurrentWorld = GetWorldForCurrentCOI();
	const int32 PlayerIndex = 0;
	if (UDGGUI::CloseExistingDGGUI(CurrentWorld))
	{
		return;
	}
	else if (UCustomizableSkeletalComponent* SelectedCustomizableSkeletalComponent = GetPlayerCustomizableSkeletalComponent(SlotID, CurrentWorld, PlayerIndex))
	{
		UDGGUI::OpenDGGUI(SlotID, SelectedCustomizableSkeletalComponent, CurrentWorld, PlayerIndex);
	}
}


void FCustomizableObjectEditorModule::OpenCOIE(const TArray<FString>& Arguments)
{
	int32 SlotID = INDEX_NONE;
	if (Arguments.Num() >= 1)
	{
		SlotID = FCString::Atoi(*Arguments[0]);
	}
	const UWorld* CurrentWorld = GetWorldForCurrentCOI();
	const int32 PlayerIndex = 0;

	// Open the Customizable Object Instance Editor
	if (UCustomizableSkeletalComponent* SelectedCustomizableSkeletalComponent = GetPlayerCustomizableSkeletalComponent(SlotID, CurrentWorld, PlayerIndex))
	{
		if (UCustomizableObjectInstance* COInstance = SelectedCustomizableSkeletalComponent->CustomizableObjectInstance)
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
			TWeakPtr<IAssetTypeActions> WeakAssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(UCustomizableObjectInstance::StaticClass());

			if (TSharedPtr<IAssetTypeActions> AssetTypeActions = WeakAssetTypeActions.Pin())
			{
				TArray<UObject*> AssetsToEdit;
				AssetsToEdit.Add(COInstance);
				AssetTypeActions->OpenAssetEditor(AssetsToEdit);
			}
		}
	}
}


EAssetTypeCategories::Type FCustomizableObjectEditorModule::GetAssetCategory() const
{
	return CustomizableObjectAssetCategory;
}


#undef LOCTEXT_NAMESPACE