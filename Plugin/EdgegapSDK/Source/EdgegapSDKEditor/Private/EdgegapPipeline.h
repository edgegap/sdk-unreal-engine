// Edgegap SDK editor: sequential build / push / API steps with log output.

#pragma once

#include "CoreMinimal.h"

class FMonitoredProcess;
class SNotificationItem;

DECLARE_LOG_CATEGORY_EXTERN(LogEdgegapEditor, Log, All);

/** Lines for the Edgegap panel's log box (also written to the Output Log). */
DECLARE_MULTICAST_DELEGATE_OneParam(FEdgegapEditorLogLine, const FString&);

class FEdgegapPipeline : public TSharedFromThis<FEdgegapPipeline>
{
public:
	/** A step calls Next(true) to continue or Next(false) to stop the pipeline. */
	using FStep = TFunction<void(TFunction<void(bool)> Next)>;

	static FEdgegapEditorLogLine OnLogLine;
	static void Log(const FString& Line, bool bError = false);

	explicit FEdgegapPipeline(const FString& InTitle) : Title(InTitle) {}

	void Add(const FString& Name, FStep Step) { Steps.Add({ Name, MoveTemp(Step) }); }

	/** Runs an external program, streaming its output. Env is set only for the child process. */
	FStep Process(const FString& Exe, const FString& Args, const TMap<FString, FString>& Env = TMap<FString, FString>());

	/** Wraps a step so a failure is logged as a warning and the pipeline goes on. */
	static FStep WarnOnFailure(FStep Step)
	{
		return [Step](TFunction<void(bool)> Next)
		{
			Step([Next](bool bOk)
			{
				if (!bOk) { Log(TEXT("Warning: step failed, continuing.")); }
				Next(true);
			});
		};
	}

	void Run(TFunction<void(bool)> OnFinished = nullptr);
	void Cancel();
	bool IsRunning() const { return bRunning; }

	/** Values steps pass to later steps (version name, request id...). */
	TMap<FString, FString> Vars;

private:
	void RunStep(int32 Index);
	void Finish(bool bOk);

	struct FNamedStep
	{
		FString Name;
		FStep Step;
	};

	FString Title;
	TArray<FNamedStep> Steps;
	TFunction<void(bool)> Done;
	TSharedPtr<FMonitoredProcess> CurrentProcess;
	TSharedPtr<SNotificationItem> Notification;
	bool bRunning = false;
	bool bCancelled = false;
};
