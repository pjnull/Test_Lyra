// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE_NP {

class IAsyncReconcileService
{
public:
	virtual ~IAsyncReconcileService() = default;
	virtual int32 Reconcile(const int32 LastCompletedStep) = 0;
};

template<typename AsyncModelDef>
class TAsyncReconcileService : public IAsyncReconcileService
{
public:

	HOIST_ASYNCMODELDEF_TYPES()

	TAsyncReconcileService(TAsyncModelDataStore_Internal<AsyncModelDef>* InDataStore)
	{
		npCheck(InDataStore != nullptr);
		DataStore = InDataStore;
	}

	int32 Reconcile(const int32 LastCompletedStep) final override
	{
		if (!DataStore->NetRecvQueue.Dequeue(DataStore->NetRecvData))
		{
			return INDEX_NONE;
		}
		npEnsure(DataStore->NetRecvQueue.IsEmpty()); // We should enqueue exactly one NetRecv snapshot per marshalled input, which should be exactly one call to Reconcile
		DataStore->NetRecvData.PendingCorrectionMask.Reset();
		const int32 LocalFrameOffset = DataStore->NetRecvData.LocalFrameOffset;

		int32 RewindFrame = INDEX_NONE;
		for (TConstSetBitIterator<> BitIt(DataStore->NetRecvData.NetRecvDirtyMask); BitIt; ++BitIt)
		{
			const int32 idx = BitIt.GetIndex();
			const typename TAsyncNetRecvData<AsyncModelDef>::FInstance& RecvData = DataStore->NetRecvData.NetRecvInstances[idx];
			RecvData.Frame += LocalFrameOffset; // Convert server-frame to local frame

			if (RecvData.Frame >= 0)
			{
				const TAsncFrameSnapshot<AsyncModelDef>& Snapshot = DataStore->Frames[RecvData.Frame];

				// This is probably not enough, we will need to check some alive flag to make sure this state was active at 
				// this time or something?
				bool bShouldReconcile = false;
				if (Snapshot.InputCmds.IsValidIndex(idx) && Snapshot.NetStates.IsValidIndex(idx))
				{
					const InputCmdType& LocalInputCmd = Snapshot.InputCmds[idx];
					const NetStateType& LocalNetState = Snapshot.NetStates[idx];

					if (LocalInputCmd.ShouldReconcile(RecvData.InputCmd))
					{
						UE_LOG(LogNetworkPrediction, Log, TEXT("Reconcile on InputCmd. %s. Instance: %d. Frame: %d"), AsyncModelDef::GetName(), idx, RecvData.Frame);
						bShouldReconcile = true;
					}
					if (LocalNetState.ShouldReconcile(RecvData.NetState))
					{
						UE_LOG(LogNetworkPrediction, Log, TEXT("Reconcile on NetState. %s. Instance: %d. Frame: %d"), AsyncModelDef::GetName(), idx, RecvData.Frame);
						bShouldReconcile = true;
					}
				}
				else
				{
					UE_LOG(LogNetworkPrediction, Log, TEXT("Reconcile due to invalid state. %s. Instance: %d. Frame: %d"), AsyncModelDef::GetName(), idx, RecvData.Frame);
					bShouldReconcile = true;
				}

				if (bShouldReconcile)
				{
					NpResizeAndSetBit(DataStore->NetRecvData.PendingCorrectionMask, idx);
					RewindFrame = RewindFrame == INDEX_NONE ? RecvData.Frame : FMath::Min<int32>(RewindFrame, RecvData.Frame);
				}
			}

			// -------------------------------------------------------
			// Apply all Input corrections for future frames if non-locally controlled.
			//	doing this in a second pass maybe more cache coherent, but this is simpler for now.
			// -------------------------------------------------------
			if ((RecvData.Flags & (uint8)EInstanceFlags::LocallyControlled) == 0)
			{
				for (int32 Frame= FMath::Max<int32>(0, RecvData.Frame+1); Frame <= LastCompletedStep+1; ++Frame)
				{
					TAsncFrameSnapshot<AsyncModelDef>& FutureSnapshot = DataStore->Frames[Frame];
					FutureSnapshot.InputCmds[idx] = RecvData.LatestInputCmd;
				}
			}
		}

		DataStore->NetRecvData.NetRecvDirtyMask.Reset();
		return RewindFrame;
	}

private:
	TAsyncModelDataStore_Internal<AsyncModelDef>* DataStore = nullptr;
};


} // namespace UE_NP