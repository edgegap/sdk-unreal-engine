// Edgegap SDK: ping beacon latency measurement.

#include "EdgegapBeacons.h"
#include "EdgegapSettings.h"
#include "Generated/EdgegapMMApi.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace
{
	TSharedPtr<FInternetAddr> MakeAddr(ISocketSubsystem* Sockets, const FString& Ip, int32 Port)
	{
		TSharedRef<FInternetAddr> Addr = Sockets->CreateInternetAddr();
		bool bValid = false;
		Addr->SetIp(*Ip, bValid);
		Addr->SetPort(Port);
		return bValid ? TSharedPtr<FInternetAddr>(Addr) : nullptr;
	}

	/** Beacons echo UDP datagrams; send a unique payload and time the echo. */
	double UdpPingMs(const FString& Ip, int32 Port, double Timeout)
	{
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		const TSharedPtr<FInternetAddr> Addr = Sockets ? MakeAddr(Sockets, Ip, Port) : nullptr;
		if (!Addr.IsValid() || Port <= 0) { return -1.0; }

		FSocket* Socket = Sockets->CreateSocket(NAME_DGram, TEXT("EdgegapBeaconUdp"), Addr->GetProtocolType());
		if (!Socket) { return -1.0; }
		Socket->SetNonBlocking(true);

		const FString Payload = FString::Printf(TEXT("EG%08x"), FMath::Rand());
		const FTCHARToUTF8 Bytes(*Payload);
		int32 Sent = 0;
		double Result = -1.0;
		const double Start = FPlatformTime::Seconds();
		if (Socket->SendTo(reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length(), Sent, *Addr))
		{
			uint8 Buffer[256];
			TSharedRef<FInternetAddr> From = Sockets->CreateInternetAddr();
			while (Result < 0.0)
			{
				const double Left = Timeout - (FPlatformTime::Seconds() - Start);
				if (Left <= 0.0 || !Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(Left))) { break; }
				int32 Read = 0;
				while (Socket->RecvFrom(Buffer, sizeof(Buffer), Read, *From))
				{
					if (Read == Bytes.Length() && FMemory::Memcmp(Buffer, Bytes.Get(), Read) == 0)
					{
						Result = (FPlatformTime::Seconds() - Start) * 1000.0;
						break;
					}
				}
			}
		}
		Socket->Close();
		Sockets->DestroySocket(Socket);
		return Result;
	}

	/** TCP connect time; works where UDP is filtered. */
	double TcpPingMs(const FString& Ip, int32 Port, double Timeout)
	{
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		const TSharedPtr<FInternetAddr> Addr = Sockets ? MakeAddr(Sockets, Ip, Port) : nullptr;
		if (!Addr.IsValid() || Port <= 0) { return -1.0; }

		FSocket* Socket = Sockets->CreateSocket(NAME_Stream, TEXT("EdgegapBeaconTcp"), Addr->GetProtocolType());
		if (!Socket) { return -1.0; }
		Socket->SetNonBlocking(true);
		const double Start = FPlatformTime::Seconds();
		Socket->Connect(*Addr);
		const bool bConnected = Socket->Wait(ESocketWaitConditions::WaitForWrite, FTimespan::FromSeconds(Timeout))
			&& Socket->GetConnectionState() == SCS_Connected;
		const double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
		Socket->Close();
		Sockets->DestroySocket(Socket);
		return bConnected ? Ms : -1.0;
	}

	double Average(TFunctionRef<double()> Ping, int32 Samples)
	{
		double Sum = 0.0;
		int32 Count = 0;
		for (int32 i = 0; i < Samples; ++i)
		{
			const double Ms = Ping();
			if (Ms >= 0.0) { Sum += Ms; ++Count; }
		}
		return Count > 0 ? Sum / Count : -1.0;
	}
}

void EdgegapBeacons::Measure(TFunction<void(const TMap<FString, double>&, const TArray<FEdgegapBeaconResult>&, const FEdgegapError&)> Done)
{
	FEdgegapMMApi::ListLocationBeacons([Done](const FEdgegapMMBeaconListResponse& List, const FEdgegapError& Error)
	{
		if (Error.bFailed)
		{
			Done({}, {}, Error);
			return;
		}

		const UEdgegapSettings* Settings = GetDefault<UEdgegapSettings>();
		const int32 Samples = Settings->BeaconPingSamples;
		const double Timeout = Settings->BeaconTimeoutSeconds;
		const bool bUdpFirst = Settings->BeaconProtocol == EEdgegapBeaconProtocol::Udp;
		const TArray<FEdgegapMMReadBeaconResponse> Beacons = List.Beacons;

		Async(EAsyncExecution::ThreadPool, [Done, Beacons, Samples, Timeout, bUdpFirst]()
		{
			TArray<FEdgegapBeaconResult> Results;
			Results.SetNum(Beacons.Num());
			ParallelFor(Beacons.Num(), [&](int32 i)
			{
				const FEdgegapMMReadBeaconResponse& Beacon = Beacons[i];
				FEdgegapBeaconResult& R = Results[i];
				R.City = Beacon.Location.City;
				R.Country = Beacon.Location.Country;
				R.PublicIp = Beacon.PublicIp;
				if (bUdpFirst)
				{
					R.LatencyMs = Average([&]() { return UdpPingMs(Beacon.PublicIp, Beacon.UdpPort, Timeout); }, Samples);
					R.Protocol = TEXT("UDP");
				}
				if (R.LatencyMs < 0.0)
				{
					R.LatencyMs = Average([&]() { return TcpPingMs(Beacon.PublicIp, Beacon.TcpPort, Timeout); }, Samples);
					R.Protocol = TEXT("TCP");
				}
				if (R.LatencyMs >= 0.0) { R.LatencyMs = FMath::RoundToDouble(R.LatencyMs * 10.0) / 10.0; }
			});

			TMap<FString, double> Latencies;
			for (const FEdgegapBeaconResult& R : Results)
			{
				if (R.LatencyMs < 0.0 || R.City.IsEmpty()) { continue; }
				const double* Existing = Latencies.Find(R.City);
				Latencies.Add(R.City, Existing ? FMath::Min(*Existing, R.LatencyMs) : R.LatencyMs);
			}

			AsyncTask(ENamedThreads::GameThread, [Done, Latencies, Results]()
			{
				TArray<FString> Parts;
				for (const FEdgegapBeaconResult& R : Results)
				{
					Parts.Add(R.LatencyMs >= 0.0 ? FString::Printf(TEXT("%s=%.0fms/%s"), *R.City, R.LatencyMs, *R.Protocol) : FString::Printf(TEXT("%s=timeout"), *R.City));
				}
				UE_LOG(LogEdgegap, Log, TEXT("Beacons: %s"), *FString::Join(Parts, TEXT(", ")));
				Done(Latencies, Results, Latencies.IsEmpty()
					? FEdgegapError::Make(0, TEXT("No beacon answered (UDP and TCP). Check the network, or pass latencies yourself."))
					: FEdgegapError());
			});
		});
	});
}

UEdgegapMeasureBeaconsNode* UEdgegapMeasureBeaconsNode::MeasureBeacons()
{
	return NewObject<UEdgegapMeasureBeaconsNode>();
}

void UEdgegapMeasureBeaconsNode::Activate()
{
	AddToRoot();
	EdgegapBeacons::Measure([this](const TMap<FString, double>& Latencies, const TArray<FEdgegapBeaconResult>& Results, const FEdgegapError& Error)
	{
		FEdgegapBeaconMeasurement Measurement;
		Measurement.Latencies = Latencies;
		Measurement.Results = Results;
		(Error.bFailed ? OnFailure : OnSuccess).Broadcast(Measurement, Error);
		RemoveFromRoot();
		SetReadyToDestroy();
	});
}
