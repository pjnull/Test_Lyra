// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "AudioParameterInterface.h"
#include "MetasoundEditor.h"
#include "MetasoundEditorGraphNode.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendLiteral.h"
#include "MetasoundSource.h"
#include "MetasoundUObjectRegistry.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/ScriptInterface.h"

#include "MetasoundEditorGraph.generated.h"


// Forward Declarations
struct FMetasoundFrontendDocument;
class ITargetPlatform;
class UMetasoundEditorGraphInputNode;
class UMetasoundEditorGraphNode;


namespace Metasound
{
	namespace Editor
	{
		struct FGraphValidationResults;
	} // namespace Editor
} // namespace Metasound

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMetasoundMemberNameChanged, FGuid /* NodeID */);

UCLASS()
class METASOUNDEDITOR_API UMetasoundEditorGraphInputLiteral : public UObject
{
	GENERATED_BODY()

public:
	virtual void UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterInterface>& InParameterInterface) const
	{
	}

	virtual FMetasoundFrontendLiteral GetDefault() const
	{
		return FMetasoundFrontendLiteral();
	}

	virtual EMetasoundFrontendLiteralType GetLiteralType() const
	{
		return EMetasoundFrontendLiteralType::None;
	}

	virtual void SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral)
	{
	}

	virtual void PostEditUndo() override;
};

UCLASS(Abstract)
class METASOUNDEDITOR_API UMetasoundEditorGraphMember : public UObject
{
	GENERATED_BODY()


public:
	UPROPERTY()
	FName TypeName;

	// Called when the name of the associated Frontend Node is changed
	FOnMetasoundMemberNameChanged NameChanged;

	// TODO: move into implemenation file.
	virtual void SetName(const FName& InNewName) {}
	virtual FName GetMemberName() const { return NAME_None; }
	virtual Metasound::Editor::ENodeSection GetSectionID() const { return Metasound::Editor::ENodeSection::None; }

	virtual bool CanRename(const FText& InNewName, FText& OutError) const { return false; }
	virtual bool IsRequired() const { return false; }
	virtual TArray<UMetasoundEditorGraphNode*> GetNodes() const { return TArray<UMetasoundEditorGraphNode*>(); }
	virtual void SetDataType(FName InNewType, bool bPostTransaction = true, bool bRegisterParentGraph = true) {}
	virtual void SetDescription(const FText& InDescription) {}
	virtual void SetDisplayName(const FText& InNewName) {}

	virtual void OnDataTypeChanged() { }
	virtual const FText& GetGraphMemberLabel() const { return FText::GetEmpty(); }
};

UCLASS()
class METASOUNDEDITOR_API UMetasoundEditorGraphVertex : public UMetasoundEditorGraphMember
{
	GENERATED_BODY()

protected:
	virtual Metasound::Frontend::FNodeHandle AddNodeHandle(const FName& InNodeName, FName InDataType)
	{
		return Metasound::Frontend::INodeController::GetInvalidHandle();
	}


public:
	UPROPERTY()
	FGuid NodeID;

	UPROPERTY()
	FMetasoundFrontendClassName ClassName;

	virtual FName GetMemberName() const override; 

	virtual bool CanRename(const FText& InNewName, FText& OutError) const override;
	virtual bool IsRequired() const override;
	virtual TArray<UMetasoundEditorGraphNode*> GetNodes() const override;
	virtual void SetDescription(const FText& InDescription) override;
	virtual void SetName(const FName& InNewName) override;
	virtual void SetDisplayName(const FText& InNewName) override;
	virtual void SetDataType(FName InNewType, bool bPostTransaction = true, bool bRegisterParentGraph = true) override;
	Metasound::Frontend::FNodeHandle GetNodeHandle() const;
	Metasound::Frontend::FConstNodeHandle GetConstNodeHandle() const;

	virtual EMetasoundFrontendClassType GetClassType() const { return EMetasoundFrontendClassType::Invalid; }
};

UCLASS()
class METASOUNDEDITOR_API UMetasoundEditorGraphInput : public UMetasoundEditorGraphVertex
{
	GENERATED_BODY()

protected:
	virtual Metasound::Frontend::FNodeHandle AddNodeHandle(const FName& InNodeName, FName InDataType) override;
	virtual EMetasoundFrontendClassType GetClassType() const override { return EMetasoundFrontendClassType::Input; }
	virtual const FText& GetGraphMemberLabel() const override;
public:
	UPROPERTY(VisibleAnywhere, Category = DefaultValue)
	UMetasoundEditorGraphInputLiteral* Literal;

	void UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterInterface>& InParameterInterface) const;

	void OnDataTypeChanged() override;

	void UpdateDocumentInput(bool bPostTransaction = true);
	virtual Metasound::Editor::ENodeSection GetSectionID() const override; 

#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif // WITH_EDITOR
};


UCLASS()
class METASOUNDEDITOR_API UMetasoundEditorGraphOutput : public UMetasoundEditorGraphVertex
{
	GENERATED_BODY()

protected:
	virtual Metasound::Frontend::FNodeHandle AddNodeHandle(const FName& InNodeName, FName InDataType) override;
	virtual EMetasoundFrontendClassType GetClassType() const override { return EMetasoundFrontendClassType::Output; }
	virtual const FText& GetGraphMemberLabel() const override;
	virtual Metasound::Editor::ENodeSection GetSectionID() const override; 
};

UCLASS()
class METASOUNDEDITOR_API UMetasoundEditorGraphVariable : public UMetasoundEditorGraphMember
{
	GENERATED_BODY()

protected:
	virtual const FText& GetGraphMemberLabel() const override;
	Metasound::Frontend::FVariableHandle GetVariableHandle();
	Metasound::Frontend::FConstVariableHandle GetConstVariableHandle() const;
public:
	UPROPERTY()
	FGuid VariableID;

	UPROPERTY(VisibleAnywhere, Category = DefaultValue)
	UMetasoundEditorGraphInputLiteral* Literal;

	virtual FName GetMemberName() const override; 
	virtual Metasound::Editor::ENodeSection GetSectionID() const;

	virtual bool CanRename(const FText& InNewName, FText& OutError) const override;
	virtual bool IsRequired() const override;
	virtual TArray<UMetasoundEditorGraphNode*> GetNodes() const override;
	virtual void SetDescription(const FText& InDescription) override;
	virtual void SetName(const FName& InNewName) override;
	virtual void SetDisplayName(const FText& InNewName) override;
	virtual void SetDataType(FName InNewType, bool bPostTransaction, bool bRegisterParentGraph) override;

	void OnDataTypeChanged() override;

	void UpdateDocumentVariable(bool bPostTransaction = true);

#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif // WITH_EDITOR
};

UCLASS()
class METASOUNDEDITOR_API UMetasoundEditorGraph : public UMetasoundEditorGraphBase
{
	GENERATED_BODY()

public:
	UMetasoundEditorGraphInputNode* CreateInputNode(Metasound::Frontend::FNodeHandle InNodeHandle, bool bInSelectNewNode);

	Metasound::Frontend::FDocumentHandle GetDocumentHandle() const;
	Metasound::Frontend::FGraphHandle GetGraphHandle() const;

	UObject* GetMetasound() const;
	UObject& GetMetasoundChecked() const;

	void IterateInputs(TUniqueFunction<void(UMetasoundEditorGraphInput&)> InFunction) const;
	void IterateOutputs(TUniqueFunction<void(UMetasoundEditorGraphOutput&)> InFunction) const;

	bool ContainsInput(UMetasoundEditorGraphInput* InInput) const;
	bool ContainsOutput(UMetasoundEditorGraphOutput* InOutput) const;

	void SetPreviewID(uint32 InPreviewID);
	bool IsPreviewing() const;

	virtual bool Validate(bool bInClearUpdateNotes) override;

	virtual void RegisterGraphWithFrontend() override;

private:
	bool ValidateInternal(Metasound::Editor::FGraphValidationResults& OutResults, bool bClearUpgradeMessaging = true);

	// Preview ID is the Unique ID provided by the UObject that implements
	// a sound's ParameterInterface when a sound begins playing.
	uint32 PreviewID = INDEX_NONE;

	UPROPERTY()
	TArray<TObjectPtr<UMetasoundEditorGraphInput>> Inputs;

	UPROPERTY()
	TArray<TObjectPtr<UMetasoundEditorGraphOutput>> Outputs;

public:
	UMetasoundEditorGraphInput* FindInput(FGuid InNodeID) const;
	UMetasoundEditorGraphInput* FindInput(FName InName) const;
	UMetasoundEditorGraphInput* FindOrAddInput(Metasound::Frontend::FNodeHandle InNodeHandle);

	UMetasoundEditorGraphOutput* FindOutput(FGuid InNodeID) const;
	UMetasoundEditorGraphOutput* FindOutput(FName InName) const;
	UMetasoundEditorGraphOutput* FindOrAddOutput(Metasound::Frontend::FNodeHandle InNodeHandle);

	UMetasoundEditorGraphMember* FindGraphMember(FGuid InNodeID) const;
	UMetasoundEditorGraphMember* FindAdjacentGraphMember(const UMetasoundEditorGraphMember& InMember);

	bool RemoveGraphMember(UMetasoundEditorGraphMember& InGraphMember);

	friend class UMetaSoundFactory;
	friend class UMetaSoundSourceFactory;
	friend class Metasound::Editor::FGraphBuilder;
};
