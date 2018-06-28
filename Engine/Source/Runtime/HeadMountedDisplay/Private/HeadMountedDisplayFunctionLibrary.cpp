// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "HeadMountedDisplayFunctionLibrary.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "ISpectatorScreenController.h"
#include "IXRSystemAssets.h"
#include "Components/PrimitiveComponent.h"
#include "Features/IModularFeatures.h"
#include "XRMotionControllerBase.h" // for GetHandEnumForSourceName()

DEFINE_LOG_CATEGORY_STATIC(LogUHeadMountedDisplay, Log, All);

/* UHeadMountedDisplayFunctionLibrary
 *****************************************************************************/

UHeadMountedDisplayFunctionLibrary::UHeadMountedDisplayFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled()
{
	return GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed();
}

bool UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayConnected()
{
	return GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice() && GEngine->XRSystem->GetHMDDevice()->IsHMDConnected();
}

bool UHeadMountedDisplayFunctionLibrary::EnableHMD(bool bEnable)
{
	if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
	{
		GEngine->XRSystem->GetHMDDevice()->EnableHMD(bEnable);
		if (GEngine->StereoRenderingDevice.IsValid())
		{
			return GEngine->StereoRenderingDevice->EnableStereo(bEnable) || !bEnable; // EnableStereo returns the actual value. When disabling, we always report success.
		}
		else
		{
			return true; // Assume that if we have a valid HMD but no stereo rendering that the operation succeeded.
		}
	}
	return false;
}

FName UHeadMountedDisplayFunctionLibrary::GetHMDDeviceName()
{
	FName DeviceName(NAME_None);

	if (GEngine->XRSystem.IsValid())
	{
		DeviceName = GEngine->XRSystem->GetSystemName();
	}

	return DeviceName;
}

EHMDWornState::Type UHeadMountedDisplayFunctionLibrary::GetHMDWornState()
{
	if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
	{
		return GEngine->XRSystem->GetHMDDevice()->GetHMDWornState();
	}

	return EHMDWornState::Unknown;
}

void UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(FRotator& DeviceRotation, FVector& DevicePosition)
{
	if(GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed())
	{
		FQuat OrientationAsQuat;
		FVector Position(0.f);

		GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, OrientationAsQuat, Position);

		DeviceRotation = OrientationAsQuat.Rotator();
		DevicePosition = Position;
	}
	else
	{
		DeviceRotation = FRotator::ZeroRotator;
		DevicePosition = FVector::ZeroVector;
	}
}

bool UHeadMountedDisplayFunctionLibrary::HasValidTrackingPosition()
{
	if(GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed())
	{
		return GEngine->XRSystem->HasValidTrackingPosition();
	}

	return false;
}

int32 UHeadMountedDisplayFunctionLibrary::GetNumOfTrackingSensors()
{
	if (GEngine->XRSystem.IsValid())
	{
		return GEngine->XRSystem->CountTrackedDevices(EXRTrackedDeviceType::TrackingReference);
	}
	return 0;
}

void UHeadMountedDisplayFunctionLibrary::GetPositionalTrackingCameraParameters(FVector& CameraOrigin, FRotator& CameraRotation, float& HFOV, float& VFOV, float& CameraDistance, float& NearPlane, float& FarPlane)
{
	bool isActive;
	float LeftFOV;
	float RightFOV;
	float TopFOV;
	float BottomFOV;
	GetTrackingSensorParameters(CameraOrigin, CameraRotation, LeftFOV, RightFOV, TopFOV, BottomFOV, CameraDistance, NearPlane, FarPlane, isActive, 0);
	HFOV = LeftFOV + RightFOV;
	VFOV = TopFOV + BottomFOV;
}

void UHeadMountedDisplayFunctionLibrary::GetTrackingSensorParameters(FVector& Origin, FRotator& Rotation, float& LeftFOV, float& RightFOV, float& TopFOV, float& BottomFOV, float& Distance, float& NearPlane, float& FarPlane, bool& IsActive, int32 Index)
{
	IsActive = false;

	if (Index >= 0 && GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed() && GEngine->XRSystem->DoesSupportPositionalTracking())
	{
		TArray<int32> TrackingSensors;
		GEngine->XRSystem->EnumerateTrackedDevices(TrackingSensors, EXRTrackedDeviceType::TrackingReference);

		FQuat Orientation;
		FXRSensorProperties SensorProperties;
		IsActive = GEngine->XRSystem->GetTrackingSensorProperties(TrackingSensors[Index], Orientation, Origin, SensorProperties);
		Rotation = Orientation.Rotator();
		LeftFOV = SensorProperties.LeftFOV;
		RightFOV = SensorProperties.RightFOV;
		TopFOV = SensorProperties.TopFOV;
		BottomFOV = SensorProperties.BottomFOV;
		Distance = SensorProperties.CameraDistance;
		NearPlane = SensorProperties.NearPlane;
		FarPlane = SensorProperties.FarPlane;
	}
	else
	{
		// No HMD, zero the values
		Origin = FVector::ZeroVector;
		Rotation = FRotator::ZeroRotator;
		LeftFOV = RightFOV = TopFOV = BottomFOV = 0.f;
		NearPlane = 0.f;
		FarPlane = 0.f;
		Distance = 0.f;
	}
}

void UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(float Yaw, EOrientPositionSelector::Type Options)
{
	if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed())
	{
		switch (Options)
		{
		case EOrientPositionSelector::Orientation:
			GEngine->XRSystem->ResetOrientation(Yaw);
			break;
		case EOrientPositionSelector::Position:
			GEngine->XRSystem->ResetPosition();
			break;
		default:
			GEngine->XRSystem->ResetOrientationAndPosition(Yaw);
		}
	}
}

void UHeadMountedDisplayFunctionLibrary::SetClippingPlanes(float Near, float Far)
{
	IHeadMountedDisplay* HMD = GEngine->XRSystem.IsValid() ? GEngine->XRSystem->GetHMDDevice() : nullptr;

	if (HMD)
	{
		HMD->SetClippingPlanes(Near, Far);
	}
}

/** DEPRECATED - Use GetPixelDensity */
float UHeadMountedDisplayFunctionLibrary::GetScreenPercentage()
{
	static const auto ScreenPercentageTCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"));
	return ScreenPercentageTCVar->GetValueOnGameThread();
}

float UHeadMountedDisplayFunctionLibrary::GetPixelDensity()
{
	static const auto PixelDensityTCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("vr.pixeldensity"));
	return PixelDensityTCVar->GetValueOnGameThread();
}

void UHeadMountedDisplayFunctionLibrary::SetWorldToMetersScale(UObject* WorldContext, float NewScale)
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetWorldSettings()->WorldToMeters = NewScale;
	}
}

float UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(UObject* WorldContext)
{
	return WorldContext ? WorldContext->GetWorld()->GetWorldSettings()->WorldToMeters : 0.f;
}

void UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(TEnumAsByte<EHMDTrackingOrigin::Type> InOrigin)
{
	if (GEngine->XRSystem.IsValid())
	{
		EHMDTrackingOrigin::Type Origin = EHMDTrackingOrigin::Eye;
		switch (InOrigin)
		{
		case EHMDTrackingOrigin::Eye:
			Origin = EHMDTrackingOrigin::Eye;
			break;
		case EHMDTrackingOrigin::Floor:
			Origin = EHMDTrackingOrigin::Floor;
			break;
		default:
			break;
		}
		GEngine->XRSystem->SetTrackingOrigin(Origin);
	}
}

TEnumAsByte<EHMDTrackingOrigin::Type> UHeadMountedDisplayFunctionLibrary::GetTrackingOrigin()
{
	EHMDTrackingOrigin::Type Origin = EHMDTrackingOrigin::Eye;

	if (GEngine->XRSystem.IsValid())
	{
		Origin = GEngine->XRSystem->GetTrackingOrigin();
	}

	return Origin;
}

FTransform UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(UObject* WorldContext)
{
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		return TrackingSys->GetTrackingToWorldTransform();
	}
	return FTransform::Identity;
}

void UHeadMountedDisplayFunctionLibrary::GetVRFocusState(bool& bUseFocus, bool& bHasFocus)
{
	IHeadMountedDisplay* HMD = GEngine->XRSystem.IsValid() ? GEngine->XRSystem->GetHMDDevice() : nullptr;
	if (HMD)
	{
		bUseFocus = HMD->DoesAppUseVRFocus();
		bHasFocus = HMD->DoesAppHaveVRFocus();
	}
	else
	{
		bUseFocus = bHasFocus = false;
	}
}

namespace HMDFunctionLibraryHelpers
{
	ISpectatorScreenController* GetSpectatorScreenController()
	{
		IHeadMountedDisplay* HMD = GEngine->XRSystem.IsValid() ? GEngine->XRSystem->GetHMDDevice() : nullptr;
		if (HMD)
		{
			return HMD->GetSpectatorScreenController();
		}
		return nullptr;
	}
}

bool UHeadMountedDisplayFunctionLibrary::IsSpectatorScreenModeControllable()
{
	return HMDFunctionLibraryHelpers::GetSpectatorScreenController() != nullptr;
}

void UHeadMountedDisplayFunctionLibrary::SetSpectatorScreenMode(ESpectatorScreenMode Mode)
{
	ISpectatorScreenController* const Controller = HMDFunctionLibraryHelpers::GetSpectatorScreenController();
	if (Controller)
	{
		Controller->SetSpectatorScreenMode(Mode);
	}
	else
	{
		static FName PSVRName(TEXT("PSVR"));
		if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetSystemName() == PSVRName)
		{
			UE_LOG(LogHMD, Warning, TEXT("SetSpectatorScreenMode called while running PSVR, but the SpectatorScreenController was not found.  Perhaps you need to set the plugin project setting bEnableSocialScreenSeparateMode to true to enable it?  Ignoring this call."));
		}
	}
}

void UHeadMountedDisplayFunctionLibrary::SetSpectatorScreenTexture(UTexture* InTexture)
{
	ISpectatorScreenController* const Controller = HMDFunctionLibraryHelpers::GetSpectatorScreenController();
	if (Controller)
	{
		if (!InTexture)
		{
			UE_LOG(LogHMD, Warning, TEXT("SetSpectatorScreenTexture blueprint function called with null Texture!"));
		}

		Controller->SetSpectatorScreenTexture(InTexture);
	}
}

void UHeadMountedDisplayFunctionLibrary::SetSpectatorScreenModeTexturePlusEyeLayout(FVector2D EyeRectMin, FVector2D EyeRectMax, FVector2D TextureRectMin, FVector2D TextureRectMax, bool bDrawEyeFirst /* = true */, bool bClearBlack /* = false */)
{
	ISpectatorScreenController* const Controller = HMDFunctionLibraryHelpers::GetSpectatorScreenController();
	if (Controller)
	{
		Controller->SetSpectatorScreenModeTexturePlusEyeLayout(FSpectatorScreenModeTexturePlusEyeLayout(EyeRectMin, EyeRectMax, TextureRectMin, TextureRectMax, bDrawEyeFirst, bClearBlack));
	}
}

TArray<FXRDeviceId> UHeadMountedDisplayFunctionLibrary::EnumerateTrackedDevices(const FName SystemId, EXRTrackedDeviceType DeviceType)
{
	TArray<FXRDeviceId> DeviceListOut;

	// @TODO: It seems certain IXRTrackingSystem's aren't registering themselves with the modular feature framework. Ideally we'd be loop over them instead of picking just one.
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		if (SystemId.IsNone() || TrackingSys->GetSystemName() == SystemId)
		{
			TArray<int32> DeviceIds;
			TrackingSys->EnumerateTrackedDevices(DeviceIds, DeviceType);

			DeviceListOut.Reserve(DeviceListOut.Num() + DeviceIds.Num());
			for (const int32& DeviceId : DeviceIds)
			{
				DeviceListOut.Add(FXRDeviceId(TrackingSys, DeviceId));
			}
		}			
	}

	return DeviceListOut;
}

void UHeadMountedDisplayFunctionLibrary::GetDevicePose(const FXRDeviceId& XRDeviceId, bool& bIsTracked, FRotator& Orientation, bool& bHasPositionalTracking, FVector& Position)
{
	bIsTracked = false;
	bHasPositionalTracking = false;

	// @TODO: It seems certain IXRTrackingSystem's aren't registering themselves with the modular feature framework. Ideally we'd be loop over them instead of picking just one.
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		if (XRDeviceId.IsOwnedBy(TrackingSys))
		{
			FQuat QuatRotation;
			if (TrackingSys->GetCurrentPose(XRDeviceId.DeviceId, QuatRotation, Position))
			{
				bIsTracked = true;
				bHasPositionalTracking = TrackingSys->HasValidTrackingPosition();

				Orientation = FRotator(QuatRotation);
			}
			else
			{
				Position = FVector::ZeroVector;
				Orientation = FRotator::ZeroRotator;
			}
		}
	}
}

void UHeadMountedDisplayFunctionLibrary::GetDeviceWorldPose(UObject* WorldContext, const FXRDeviceId& XRDeviceId, bool& bIsTracked, FRotator& Orientation, bool& bHasPositionalTracking, FVector& Position)
{
	GetDevicePose(XRDeviceId, bIsTracked, Orientation, bHasPositionalTracking, Position);

	const FTransform TrackingToWorld = GetTrackingToWorldTransform(WorldContext);
	Position = TrackingToWorld.TransformPosition(Position);

	FQuat WorldOrientation = TrackingToWorld.TransformRotation(Orientation.Quaternion());
	Orientation = WorldOrientation.Rotator();
}

bool UHeadMountedDisplayFunctionLibrary::IsDeviceTracking(const FXRDeviceId& XRDeviceId)
{
	bool bIsTracked = false;

	// @TODO: It seems certain IXRTrackingSystem's aren't registering themselves with the modular feature framework. Ideally we'd be loop over them instead of picking just one.
	IXRTrackingSystem* TrackingSys = GEngine->XRSystem.Get();
	if (TrackingSys)
	{
		if (XRDeviceId.IsOwnedBy(TrackingSys))
		{
			bIsTracked = TrackingSys->IsTracking(XRDeviceId.DeviceId);
		}
	}

	return bIsTracked;
}
