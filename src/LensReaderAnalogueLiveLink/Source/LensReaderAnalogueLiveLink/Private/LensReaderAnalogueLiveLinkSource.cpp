#include "LensReaderAnalogueLiveLinkSource.h"

#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkCameraTypes.h"

#include "Async/Async.h"
#include "Common/UdpSocketBuilder.h"
#include "HAL/RunnableThread.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

#include "GenericPlatform/GenericPlatformProcess.h"

#include "RenderCore.h"

#define LOCTEXT_NAMESPACE "FLensReaderAnalogueLiveLinkSource"

#include "vrpn_Analog.h"

FLensReaderAnalogueLiveLinkSource::FLensReaderAnalogueLiveLinkSource(FText InEndpoint)
: Stopping(false)
, Client(nullptr)
, Thread(nullptr)
{
	// defaults
	DeviceEndpoint = InEndpoint;

	SourceStatus = LOCTEXT("SourceStatus_DeviceNotFound", "Device Not Found");
    SourceType = LOCTEXT("LensReaderAnalogueLiveLinkSource", "Lens Reader Analogue");
    SourceMachineName = DeviceEndpoint;

    Start();

    SourceStatus = LOCTEXT("SourceStatus_Receiving", "Receiving");
}

FLensReaderAnalogueLiveLinkSource::~FLensReaderAnalogueLiveLinkSource()
{
	Stop();
}

void FLensReaderAnalogueLiveLinkSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;
}


bool FLensReaderAnalogueLiveLinkSource::IsSourceStillValid() const
{
	// Source is valid if we have a valid thread and socket
	bool bIsSourceValid = !Stopping && Thread != nullptr;
	return bIsSourceValid;
}


bool FLensReaderAnalogueLiveLinkSource::RequestSourceShutdown()
{
	Stop();

	return true;
}
// FRunnable interface

void FLensReaderAnalogueLiveLinkSource::Start()
{
    Stopping = false;

    if (Thread == nullptr)
    {
        ThreadName = "LensReaderAnalogue Receiver ";
        ThreadName.AppendInt(FAsyncThreadIndex::GetNext());
        Thread = FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
    }
}

void FLensReaderAnalogueLiveLinkSource::Stop()
{
    Stopping = true;

    if (Thread != nullptr)
    {
        Thread->WaitForCompletion();
        Thread = nullptr;
    }
}

void FLensReaderAnalogueLiveLinkSource::HandleTrackerCallback(void* p)
{
    vrpn_ANALOGCB* t = (vrpn_ANALOGCB*)p;

    if (!IsSourceStillValid())
        return;

    FScopeLock lock(&currentLock);

    currentFocalLength = t->channel[0];

    currentWorldTime = t->msg_time.tv_usec;
    currentWorldTime /= 1000000.0;
    currentWorldTime += t->msg_time.tv_sec;
}

void VRPN_CALLBACK handle_tracker(void* userData, const vrpn_ANALOGCB t)
{
    FLensReaderAnalogueLiveLinkSource* ctx = (FLensReaderAnalogueLiveLinkSource*)userData;

    ctx->HandleTrackerCallback((void*)&t);
}

uint32 FLensReaderAnalogueLiveLinkSource::Run()
{
    vrpn_Analog_Remote *tkr;
    FString conn_fs = DeviceEndpoint.ToString();
    std::string conn_std = "LensReaderAnalogue@";

    conn_std += std::string(TCHAR_TO_UTF8(*conn_fs));
        
    tkr = new vrpn_Analog_Remote(conn_std.c_str());

    tkr->register_change_handler(this, handle_tracker);

    while (!Stopping)
        tkr->mainloop();

    delete tkr;

    return 0;
}

void FLensReaderAnalogueLiveLinkSource::Update()
{
    if (Client)
    {
        double WorldTime, FocalLength;
        FName SubjectName = FName(DeviceEndpoint.ToString());

        {
            FScopeLock lock(&currentLock);
            FocalLength = currentFocalLength;
            WorldTime = currentWorldTime;
        };

        FLiveLinkStaticDataStruct CameraStaticDataStruct = FLiveLinkStaticDataStruct(FLiveLinkCameraStaticData::StaticStruct());
        FLiveLinkCameraStaticData& CameraStaticData = *CameraStaticDataStruct.Cast<FLiveLinkCameraStaticData>();
        CameraStaticData.bIsFocalLengthSupported = true;
        Client->PushSubjectStaticData_AnyThread({ SourceGuid, SubjectName }, ULiveLinkCameraRole::StaticClass(), MoveTemp(CameraStaticDataStruct));

        FTimer timer;
        FLiveLinkFrameDataStruct CameraFrameDataStruct = FLiveLinkFrameDataStruct(FLiveLinkCameraFrameData::StaticStruct());
        FLiveLinkCameraFrameData& CameraFrameData = *CameraFrameDataStruct.Cast<FLiveLinkCameraFrameData>();
        CameraFrameData.FocalLength = FocalLength;
        CameraFrameData.WorldTime = FLiveLinkWorldTime(WorldTime); // (double)(timer.GetCurrentTime()));
        Client->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(CameraFrameDataStruct));

    }
}

#undef LOCTEXT_NAMESPACE
