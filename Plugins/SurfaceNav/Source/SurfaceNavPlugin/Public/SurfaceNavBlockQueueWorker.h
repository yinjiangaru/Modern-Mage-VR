// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "SurfaceNavCommon.h"

/**
 *
 */
class SurfaceNavBlockQueueWorker : public FRunnable
{
public:

	// Constructor, create the thread by calling this
	SurfaceNavBlockQueueWorker(uint8 InThreadId, FQueueBlocksNearInvokersDelegate& InQueueBlocksNearInvokersDel);

	// Destructor
	virtual ~SurfaceNavBlockQueueWorker();


	// Overriden from FRunnable
	// Do not call these functions youself, that will happen automatically
	virtual bool Init() override; // Do your setup here, allocate memory, ect.
	virtual uint32 Run() override; // Main data processing happens here
	virtual void Stop() override; // Clean up any memory you allocated here

	//void SetInputReady(bool bInInputReady = true);

	bool bInputReady;

private:

	uint8 ThreadId;
	FQueueBlocksNearInvokersDelegate& QueueBlocksNearInvokersDel;

	// Thread handle. Control the thread using this, with operators like Kill and Suspend
	FRunnableThread* Thread;

	// Used to know when the thread should exit, changed in Stop(), read in Run()
	bool bRunThread = true;
};