// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#ifdef USE_TECHSOFT_SDK

#include "CADData.h"

#include "TechSoftInterface.h"

namespace CADLibrary
{

	// Single-ownership smart TeshSoft object
	// Use this when you need to manage TechSoft object's lifetime.
	//
	// TechSoft give access to void pointers
	// According to the context, the class name of the void pointer is known but the class is unknown
	// i.e. A3DSDKTypes.h defines all type like :
	// 	   typedef void A3DEntity;		
	// 	   typedef void A3DAsmModelFile; ...
	// 
	// From a pointer, TechSoft give access to a copy of the associated structure :
	//
	// const A3DXXXXX* pPointer;
	// A3DXXXXXData sData; // the structure
	// A3D_INITIALIZE_DATA(A3DXXXXXData, sData); // initialization of the structure
	// A3DXXXXXXGet(pPointer, &sData); // Copy of the data of the pointer in the structure
	// ...
	// A3DXXXXXXGet(NULL, &sData); // Free the structure
	//
	// A3D_INITIALIZE_DATA, and all A3DXXXXXXGet methods are TechSoft macro
	//

	template<class ObjectType>
	class TUniqueTSObj
	{
	public:

		/**
		 * Constructor of an initialized ObjectType object
		 */
		explicit TUniqueTSObj()
		{
			InitializeData(Data);
		}

		/**
		 * Constructor of an filled ObjectType object with the data of DataPtr
		 * @param DataPtr: the pointer of the data to copy
		 */
		explicit TUniqueTSObj(const A3DEntity* DataPtr)
		{
			//TechSoftInterfaceImpl::InitializeData(Data);
			InitializeData(Data);

			Status = GetData(DataPtr, Data);
		}

		~TUniqueTSObj()
		{
			GetData(NULL, Data);
		}

		/**
		 * Fill the structure with the data of a new DataPtr
		 */
		A3DStatus Get(const A3DEntity* DataPtr)
		{
			if (IsValid())
			{
				Status = GetData(NULL, Data);
			}
			else
			{
				Status = A3DStatus::A3D_SUCCESS;
			}

			if (!IsValid() || (DataPtr == NULL))
			{
				Status = A3DStatus::A3D_ERROR;
				return Status;
			}

			Status = GetData(DataPtr, Data);
			return Status;
		}

		/**
		 * Empty the structure
		 */
		void Reset()
		{
			Get(NULL);
		}

		/**
		 * Return
		 *  - A3DStatus::A3D_SUCCESS if the data is filled
		 *  - A3DStatus::A3D_ERROR if the data is empty
		 */
		A3DStatus GetStatus()
		{
			return Status;
		}

		/**
		 * Return true if the data is filled
		 */
		const bool IsValid() const
		{
			return Status == A3DStatus::A3D_SUCCESS;
		}

		// Non-copyable
		TUniqueTSObj(const TUniqueTSObj&) = delete;
		TUniqueTSObj& operator=(const TUniqueTSObj&) = delete;

		// Conversion methods

		const ObjectType& operator*() const
		{
			return Data;
		}

		ObjectType& operator*()
		{
			return Data;
		}

		const ObjectType* operator->() const
		{
			check(IsValid());
			return &Data;
		}

		ObjectType* operator->()
		{
			check(IsValid());
			return &Data;
		}

		/**
		 * Return the structure and set the status to filled
		 * The method is used to manage structure filled by UE
		 */
		ObjectType* GetEmptyDataPtr()
		{
			Get(NULL);
			Status = A3DStatus::A3D_SUCCESS;
			return &Data;
		}



	private:
		ObjectType Data;
		A3DStatus Status = A3DStatus::A3D_ERROR;

		void InitializeData(ObjectType& Data)
#ifdef USE_TECHSOFT_SDK
			;
#else
		{
		}
#endif

		A3DStatus GetData(const A3DEntity* AsmModelFilePtr, ObjectType& OutData)
#ifdef USE_TECHSOFT_SDK
			;
#else
		{
			return A3DStatus::A3D_ERROR;
		}
#endif

	};
}

#endif