// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavBlockQueueWorker.h" // Change this to reference the header file above


#pragma region Main Thread Code
// This code will be run on the thread that invoked this thread (i.e. game thread)


SurfaceNavBlockQueueWorker::SurfaceNavBlockQueueWorker(uint8 InThreadId, FQueueBlocksNearInvokersDelegate& InQueueBlocksNearInvokersDel) :
	ThreadId(InThreadId), QueueBlocksNearInvokersDel(InQueueBlocksNearInvokersDel)
{
	// Constructs the actual thread object. It will begin execution immediately
	// If you've passed in any inputs, set them up before calling this.
	FString Msg = "DSN Worker #" + FString::FromInt(ThreadId);
	Thread = FRunnableThread::Create(this, *Msg); // FText::FromString(Msg));
}


SurfaceNavBlockQueueWorker::~SurfaceNavBlockQueueWorker()
{
	if (Thread)
	{
		// Kill() is a blocking call, it waits for the thread to finish.
		// Hopefully that doesn't take too long
		Thread->Kill();
		delete Thread;
	}
}


#pragma endregion
// The code below will run on the new thread.


bool SurfaceNavBlockQueueWorker::Init()
{
	//UE_LOG(LogTemp, Warning, TEXT("Block Queue Worker has been initialized"))

		// Return false if you want to abort the thread
		return true;
}


uint32 SurfaceNavBlockQueueWorker::Run()
{
	// Peform your processor intensive task here. In this example, a neverending
	// task is created, which will only end when Stop is called.
	while (bRunThread)
	{

		/*if (bInputReady)
		{*/
			QueueBlocksNearInvokersDel.ExecuteIfBound(ThreadId);

			// Do your intensive tasks here. You can safely modify all input variables.
			// For the example, I'm just going to convert the input int to a float
			//ExampleFloatOutput = ExampleIntInput;
			//FPlatformProcess::Sleep(1.0f); // Simulate a heavy workload

			// Do this once you've finished using the input/output variables
			// From this point onwards, do not touch them!
			/*bInputReady = false;

		}*/

		// I hear that it's good to let the thread sleep a bit, so the OS can utilise it better or something.
		FPlatformProcess::Sleep(0.005f);
	}

	return 0;
}

//void SurfaceNavBlockQueueWorker::SetInputReady(bool bInInputReady)
//{
//	bInputReady = bInInputReady;
//}


// This function is NOT run on the new thread!
void SurfaceNavBlockQueueWorker::Stop()
{
	// Clean up memory usage here, and make sure the Run() function stops soon
	// The main thread will be stopped until this finishes!

	// For this example, we just need to terminate the while loop
	// It will finish in <= 1 sec, due to the Sleep()
	bRunThread = false;

	/*if (Thread)
	{
		Thread->WaitForCompletion();
	}*/
}