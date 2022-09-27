// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomizableObjectPopulationClassFactory.h"
#include "CustomizableObjectPopulationClass.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectPopulationClassFactory"

UCustomizableObjectPopulationClassFactory::UCustomizableObjectPopulationClassFactory() : Super()
{
	// Property initialization
	bCreateNew = true;
	SupportedClass = UCustomizableObjectPopulationClass::StaticClass();
	bEditAfterNew = true;
}

 UObject* UCustomizableObjectPopulationClassFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	 UCustomizableObjectPopulationClass* CustomizableObjectPopulation = NewObject<UCustomizableObjectPopulationClass>(InParent, Class, Name, Flags);
	 
	 return CustomizableObjectPopulation;
}

 bool UCustomizableObjectPopulationClassFactory::ShouldShowInNewMenu() const
 {
	 return true;
 }

#undef LOCTEXT_NAMESPACE