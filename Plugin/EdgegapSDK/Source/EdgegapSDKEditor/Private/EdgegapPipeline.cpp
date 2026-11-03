// Edgegap SDK editor: sequential build / push / API steps with log output.

#include "EdgegapPipeline.h"
#include "Async/Async.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/MonitoredProcess.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY(LogEdgegapEditor);

FEdgegapEditorLogLine FEdgegapPipeline::OnLogLine;

void FEdgegapPipeline::Log(const FString& Line, bool bError)
{
	if (bError) { UE_LOG(LogEdgegapEditor, Error, TEXT("%s"), *Line); }
	else { UE_LOG(LogEdgegapEditor, Log, TEXT("%s"), *Line); }
	if (IsInGameThread()) { OnLogLine.Broadcast(Line); }
	else { AsyncTask(ENamedThreads::GameThread, [Line]() { OnLogLine.Broadcast(Line); }); }
}

FEdgegapPipeline::FStep FEdgegapPipeline::Process(const FString& Exe, const FString& Args, const TMap<FString, FString>& Env)
{
	TWeakPtr<FEdgegapPipeline> WeakThis = AsShared();
	return [WeakThis, Exe, Args, Env](TFunction<void(bool)> Next)
	{
		TSharedPtr<FEdgegapPipeline> Self = WeakThis.Pin();
		if (!Self) { return; }
		Log(FString::Printf(TEXT("> %s %s"), *Exe, *Args));

		TSharedPtr<FMonitoredProcess> Proc = MakeShared<FMonitoredProcess>(Exe, Args, true, true);
		Proc->OnOutput().BindLambda([](const FString& Output) { Log(Output); });
		Proc->OnCompleted().BindLambda([WeakThis, Next](int32 Code)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Next, Code]()
			{
				if (TSharedPtr<FEdgegapPipeline> Pinned = WeakThis.Pin()) { Pinned->CurrentProcess.Reset(); }
				if (Code != 0) { Log(FString::Printf(TEXT("Exited with code %d."), Code), true); }
				Next(Code == 0);
			});
		});
		Proc->OnCanceled().BindLambda([WeakThis, Next]()
		{
			AsyncTask(ENamedThreads::GameThread, [Next]() { Next(false); });
		});

		// The child inherits the environment at launch; secrets are removed again right after.
		for (const TPair<FString, FString>& Pair : Env) { FPlatformMisc::SetEnvironmentVar(*Pair.Key, *Pair.Value); }
		const bool bLaunched = Proc->Launch();
		for (const TPair<FString, FString>& Pair : Env) { FPlatformMisc::SetEnvironmentVar(*Pair.Key, TEXT("")); }

		if (!bLaunched)
		{
			Log(FString::Printf(TEXT("Could not start %s (is it installed and on PATH?)."), *Exe), true);
			Next(false);
			return;
		}
		Self->CurrentProcess = Proc;
	};
}

void FEdgegapPipeline::Run(TFunction<void(bool)> OnFinished)
{
	Done = MoveTemp(OnFinished);
	bRunning = true;
	bCancelled = false;

	FNotificationInfo Info(FText::FromString(Title));
	Info.bFireAndForget = false;
	Info.ExpireDuration = 5.0f;
	Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid()) { Notification->SetCompletionState(SNotificationItem::CS_Pending); }

	Log(FString::Printf(TEXT("== %s"), *Title));
	RunStep(0);
}

void FEdgegapPipeline::RunStep(int32 Index)
{
	if (bCancelled) { Finish(false); return; }
	if (Index >= Steps.Num()) { Finish(true); return; }

	const FString& Name = Steps[Index].Name;
	Log(FString::Printf(TEXT("-- %s"), *Name));
	if (Notification.IsValid())
	{
		Notification->SetText(FText::FromString(FString::Printf(TEXT("%s: %s (%d/%d)"), *Title, *Name, Index + 1, Steps.Num())));
	}

	TSharedRef<FEdgegapPipeline> Self = AsShared();
	Steps[Index].Step([Self, Index](bool bOk)
	{
		if (!bOk) { Self->Finish(false); return; }
		Self->RunStep(Index + 1);
	});
}

void FEdgegapPipeline::Cancel()
{
	bCancelled = true;
	if (CurrentProcess.IsValid()) { CurrentProcess->Cancel(true); }
}

void FEdgegapPipeline::Finish(bool bOk)
{
	if (!bRunning) { return; }
	bRunning = false;
	Log(FString::Printf(TEXT("== %s: %s"), *Title, bOk ? TEXT("done") : bCancelled ? TEXT("cancelled") : TEXT("failed (see log above)")), !bOk);
	if (Notification.IsValid())
	{
		Notification->SetText(FText::FromString(FString::Printf(TEXT("%s %s"), *Title, bOk ? TEXT("succeeded") : TEXT("failed"))));
		Notification->SetCompletionState(bOk ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		Notification->ExpireAndFadeout();
		Notification.Reset();
	}
	if (Done) { Done(bOk); }
}
