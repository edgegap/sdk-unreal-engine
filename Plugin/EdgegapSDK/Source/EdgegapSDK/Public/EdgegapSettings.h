// Edgegap SDK: project settings (Project Settings > Plugins > Edgegap).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EdgegapSettings.generated.h"

UENUM(BlueprintType)
enum class EEdgegapBeaconProtocol : uint8
{
	/** UDP echo to the beacon's udp_port: closest to game traffic. Falls back to TCP if nothing answers. */
	Udp,
	/** TCP connect time to the beacon's tcp_port: works where UDP is blocked. */
	Tcp,
};

/**
 * Stored in DefaultGame.ini, so everything here ships with clients. Secrets that must not
 * ship (the org API token, the server browser server token) live in UEdgegapUserSecrets or
 * in environment variables instead.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Edgegap"))
class EDGEGAPSDK_API UEdgegapSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	// ---- Matchmaker

	/** Matchmaker URL from the dashboard, e.g. https://om-xxxxxxxxxx.edgegap.net (env EDGEGAP_MM_URL, -EdgegapMMUrl=). */
	UPROPERTY(Config, EditAnywhere, Category = "Matchmaker")
	FString MatchmakerUrl;

	/** Matchmaker auth token (client-facing; env EDGEGAP_MM_TOKEN, -EdgegapMMToken=). */
	UPROPERTY(Config, EditAnywhere, Category = "Matchmaker")
	FString MatchmakerToken;

	/** Seconds between assignment polls. Edgegap rate-limits; keep this at 1 or more. */
	UPROPERTY(Config, EditAnywhere, Category = "Matchmaker", meta = (ClampMin = "1.0"))
	float PollIntervalSeconds = 2.0f;

	/** Give up searching after this many seconds (0 = rely on the matchmaker's ticket expiration). */
	UPROPERTY(Config, EditAnywhere, Category = "Matchmaker", meta = (ClampMin = "0.0"))
	float SearchTimeoutSeconds = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Matchmaker|Beacons")
	EEdgegapBeaconProtocol BeaconProtocol = EEdgegapBeaconProtocol::Udp;

	/** Pings sent to each beacon; the average is reported. */
	UPROPERTY(Config, EditAnywhere, Category = "Matchmaker|Beacons", meta = (ClampMin = "1", ClampMax = "10"))
	int32 BeaconPingSamples = 3;

	/** Seconds to wait for each beacon answer. */
	UPROPERTY(Config, EditAnywhere, Category = "Matchmaker|Beacons", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float BeaconTimeoutSeconds = 1.0f;

	/** Connect to the assigned server as soon as a match is found. */
	UPROPERTY(Config, EditAnywhere, Category = "Matchmaker|Travel")
	bool bTravelOnMatch = true;

	/** Port name from the Edgegap app version used for ClientTravel. */
	UPROPERTY(Config, EditAnywhere, Category = "Matchmaker|Travel")
	FString GamePortName = TEXT("gameport");

	// ---- Server browser

	/** Server browser URL from the dashboard (env EDGEGAP_SB_URL, -EdgegapSBUrl=). */
	UPROPERTY(Config, EditAnywhere, Category = "Server Browser")
	FString ServerBrowserUrl;

	/** Client token (browse, reserve seats). Server token: env EDGEGAP_SB_SERVER_TOKEN on the app version. */
	UPROPERTY(Config, EditAnywhere, Category = "Server Browser")
	FString ServerBrowserClientToken;

	// ---- Edgegap API

	UPROPERTY(Config, EditAnywhere, Category = "Edgegap API")
	FString ApiUrl = TEXT("https://api.edgegap.com");

	/**
	 * Ship the org API token in packaged builds (DefaultGame.ini). Anyone with the build can then
	 * create, stop and delete everything in your Edgegap organization. Leave off unless the build
	 * never leaves your team (internal tools, CI). Normally servers get EDGEGAP_API_TOKEN instead.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Edgegap API", meta = (DisplayName = "Include API Token In Packaged Builds (unsafe)"))
	bool bIncludeApiTokenInPackagedBuilds = false;

	/** Only read when the option above is on. Set it from the editor with Edgegap > Copy API Token To Packaged Builds. */
	UPROPERTY(Config, EditAnywhere, Category = "Edgegap API", meta = (EditCondition = "bIncludeApiTokenInPackagedBuilds", PasswordField = true))
	FString PackagedApiToken;

	// ---- Network

	/** Per-attempt HTTP timeout in seconds. */
	UPROPERTY(Config, EditAnywhere, Category = "Network", meta = (ClampMin = "1.0"))
	float RequestTimeoutSeconds = 10.0f;

	/** Retries after the first attempt. */
	UPROPERTY(Config, EditAnywhere, Category = "Network", meta = (ClampMin = "0", ClampMax = "10"))
	int32 MaxRetries = 3;

	// ---- Dedicated server

	/** On a deployment, call Edgegap's self-stop when the server process exits or crashes, so it is not restarted or billed. */
	UPROPERTY(Config, EditAnywhere, Category = "Dedicated Server")
	bool bSelfStopOnExit = true;

	/**
	 * On servers started by a matchmaker (MM_TICKET_IDS set), only let in players who join with a
	 * ticket the matchmaker assigned here (?edgegap_mm_ticket_id=, added by the client module).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Dedicated Server")
	bool bRequireMatchmakerTicket = true;
};

/**
 * Per-developer secrets, saved in Saved/Config/<Platform>/EditorPerProjectUserSettings.ini:
 * never committed with the project and never staged into packaged builds.
 */
UCLASS(Config = EditorPerProjectUserSettings, meta = (DisplayName = "Edgegap (my secrets)"))
class EDGEGAPSDK_API UEdgegapUserSecrets : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Organization API token from the dashboard (Dashboard > User Settings > Tokens). Without the "token " prefix. */
	UPROPERTY(Config, EditAnywhere, Category = "Edgegap API", meta = (PasswordField = true))
	FString ApiToken;

	/** Server browser server token, for testing server-side calls in the editor. */
	UPROPERTY(Config, EditAnywhere, Category = "Server Browser", meta = (PasswordField = true))
	FString ServerBrowserServerToken;

	/** Container registry user (Dashboard > Container Registry), for Build and Push. */
	UPROPERTY(Config, EditAnywhere, Category = "Container Registry")
	FString RegistryUsername;

	/** Container registry token, for Build and Push and for the app version's private registry login. */
	UPROPERTY(Config, EditAnywhere, Category = "Container Registry", meta = (PasswordField = true))
	FString RegistryToken;
};
