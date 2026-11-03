// Edgegap SDK editor: build, push and deploy settings (shared with the team in DefaultEditor.ini).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EdgegapBuildSettings.generated.h"

UENUM()
enum class EEdgegapBuildMode : uint8
{
	/** Build the Linux server inside Epic's Unreal container image. Needs Docker and a GitHub account linked to Epic (ghcr.io access). */
	Docker,
	/** Build with this machine's engine. Needs a source-built engine (launcher engines cannot build Server targets) and the Linux toolchain on Windows. */
	Local,
};

UENUM()
enum class EEdgegapServerConfig : uint8
{
	Development,
	Shipping,
};

UCLASS(Config = Editor, DefaultConfig, meta = (DisplayName = "Edgegap Build"))
class UEdgegapBuildSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Edgegap application name (created on first push if missing). */
	UPROPERTY(Config, EditAnywhere, Category = "Application")
	FString AppName;

	/** Version name and image tag. Empty = UTC timestamp YYYYMMDD-HHMM at build time. */
	UPROPERTY(EditAnywhere, Transient, Category = "Application")
	FString VersionName;

	/** vCPU units (1024 = 1 vCPU). */
	UPROPERTY(Config, EditAnywhere, Category = "Application", meta = (ClampMin = "128"))
	int32 ReqCpu = 1024;

	/** Memory in MB. */
	UPROPERTY(Config, EditAnywhere, Category = "Application", meta = (ClampMin = "128"))
	int32 ReqMemory = 2048;

	/** Minutes before Edgegap stops a deployment (0 = Edgegap default). */
	UPROPERTY(Config, EditAnywhere, Category = "Application")
	int32 MaxDurationMinutes = 60;

	UPROPERTY(Config, EditAnywhere, Category = "Application|Port")
	FString PortName = TEXT("gameport");

	UPROPERTY(Config, EditAnywhere, Category = "Application|Port")
	int32 Port = 7777;

	/** UDP for the default IpNetDriver; TCP or WS for others. */
	UPROPERTY(Config, EditAnywhere, Category = "Application|Port", meta = (GetOptions = "GetProtocols"))
	FString Protocol = TEXT("UDP");

	UPROPERTY(Config, EditAnywhere, Category = "Build")
	EEdgegapBuildMode BuildMode = EEdgegapBuildMode::Docker;

	/** Image for Docker mode. Empty = ghcr.io/epicgames/unreal-engine:dev-slim-<this engine's version>. */
	UPROPERTY(Config, EditAnywhere, Category = "Build", meta = (EditCondition = "BuildMode == EEdgegapBuildMode::Docker"))
	FString UnrealImage;

	UPROPERTY(Config, EditAnywhere, Category = "Build")
	EEdgegapServerConfig ServerConfig = EEdgegapServerConfig::Development;

	/** Server target name. Empty = <Project>Server. */
	UPROPERTY(Config, EditAnywhere, Category = "Build")
	FString ServerTarget;

	/** Map the server opens. Empty = the project's server default map. */
	UPROPERTY(Config, EditAnywhere, Category = "Build")
	FString ServerMap;

	/** Extra server command-line arguments baked into the image. */
	UPROPERTY(Config, EditAnywhere, Category = "Build")
	FString ExtraServerArguments;

	UPROPERTY(Config, EditAnywhere, Category = "Container Registry")
	FString RegistryUrl = TEXT("registry.edgegap.com");

	/** Your registry project (Dashboard > Container Registry), e.g. edgegap-abcd1234. Image: <url>/<project>/<app>:<version>. */
	UPROPERTY(Config, EditAnywhere, Category = "Container Registry")
	FString RegistryProject;

	UFUNCTION()
	static TArray<FString> GetProtocols() { return { TEXT("UDP"), TEXT("TCP"), TEXT("TCP/UDP"), TEXT("HTTP"), TEXT("HTTPS"), TEXT("WS"), TEXT("WSS") }; }
};
