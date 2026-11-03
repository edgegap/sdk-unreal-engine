// Edgegap SDK: model and helper tests (Session Frontend > Automation, filter "Edgegap").

#include "Misc/AutomationTest.h"
#include "EdgegapCompat.h"
#include "EdgegapMatchmakingLibrary.h"
#include "EdgegapServerLibrary.h"
#include "EdgegapTicketGate.h"
#include "Engine/GameInstance.h"
#include "Generated/EdgegapAPIModels.h"
#include "Generated/EdgegapMMModels.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEdgegapMMReadMemberTest, "Edgegap.Matchmaker.Models.ReadPartyMember",
	EDGEGAP_TEST_FLAGS)

bool FEdgegapMMReadMemberTest::RunTest(const FString& Parameters)
{
	const FString Body = TEXT(R"({
		"member_id": "m1", "group_id": "g1", "is_ready": true, "status": "HOST_ASSIGNED", "team_id": null,
		"assignment": {
			"fqdn": "abc.pr.edgegap.net", "public_ip": "1.2.3.4",
			"ports": { "gameport": { "internal": 7777, "external": 31500, "link": "abc.pr.edgegap.net:31500", "protocol": "UDP" } },
			"location": { "city": "Montreal", "country": "Canada", "continent": "North America", "administrative_division": "Quebec", "timezone": "America/Toronto" }
		}
	})");

	FEdgegapMMReadPartyMemberResponse Member;
	const TSharedPtr<FJsonObject> Obj = EdgegapJson::ParseObject(Body);
	if (!TestTrue(TEXT("parses"), Obj.IsValid())) { return false; }
	Member.ReadJson(*Obj);

	TestEqual(TEXT("member_id"), Member.MemberId, TEXT("m1"));
	TestTrue(TEXT("is_ready"), Member.bIsReady);
	TestEqual(TEXT("status enum"), Member.Status, EEdgegapMMTicketSteps::HostAssigned);
	TestTrue(TEXT("has assignment"), Member.bHasAssignment);
	TestEqual(TEXT("city"), Member.Assignment.Location.City, TEXT("Montreal"));
	const FEdgegapMMAssignmentPortResponse* Port = Member.Assignment.Ports.Find(TEXT("gameport"));
	TestTrue(TEXT("gameport"), Port != nullptr && Port->External == 31500);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEdgegapMMWriteTicketTest, "Edgegap.Matchmaker.Models.WriteProfileTicket",
	EDGEGAP_TEST_FLAGS)

bool FEdgegapMMWriteTicketTest::RunTest(const FString& Parameters)
{
	FEdgegapMMCreatePartyTicketCasual Ticket;
	Ticket.bIsReady = true;
	Ticket.Attributes.Beacons.Add(TEXT("Montreal"), 12.5);

	const TSharedRef<FJsonObject> Obj = Ticket.WriteJson();
	TestEqual(TEXT("profile constant"), Obj->GetStringField(TEXT("profile")), FString(TEXT("casual")));
	TestFalse(TEXT("empty player_ip omitted"), Obj->HasField(TEXT("player_ip")));
	TestEqual(TEXT("beacon latency is a number"), Obj->GetObjectField(TEXT("attributes"))->GetObjectField(TEXT("beacons"))->GetNumberField(TEXT("Montreal")), 12.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEdgegapAttributesTest, "Edgegap.Matchmaker.Attributes",
	EDGEGAP_TEST_FLAGS)

bool FEdgegapAttributesTest::RunTest(const FString& Parameters)
{
	TMap<FString, double> Latencies;
	Latencies.Add(TEXT("Montreal"), 12.0);
	TArray<FEdgegapAttribute> Attributes;
	Attributes.Add(UEdgegapMatchmakingLibrary::MakeNumberAttribute(TEXT("elo"), 1337.0));
	Attributes.Add(UEdgegapMatchmakingLibrary::MakeStringArrayAttribute(TEXT("maps"), { TEXT("a"), TEXT("b") }));
	Attributes.Add(UEdgegapMatchmakingLibrary::MakeLatenciesAttribute(Latencies));

	FString Json, Error;
	TestTrue(TEXT("builds"), UEdgegapMatchmakingLibrary::AttributesToJson(Attributes, Json, Error));
	const TSharedPtr<FJsonObject> Obj = EdgegapJson::ParseObject(Json);
	TestEqual(TEXT("number"), Obj->GetNumberField(TEXT("elo")), 1337.0);
	TestEqual(TEXT("array"), Obj->GetArrayField(TEXT("maps")).Num(), 2);
	TestEqual(TEXT("latency map"), Obj->GetObjectField(TEXT("beacons"))->GetNumberField(TEXT("Montreal")), 12.0);

	Attributes.Add(UEdgegapMatchmakingLibrary::MakeBooleanAttribute(TEXT("elo"), true));
	TestFalse(TEXT("duplicate name rejected"), UEdgegapMatchmakingLibrary::AttributesToJson(Attributes, Json, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEdgegapServerEnvTest, "Edgegap.Server.Environment",
	EDGEGAP_TEST_FLAGS)

bool FEdgegapServerEnvTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("ticket ids"), UEdgegapServerLibrary::JsonArrayToStringArray(TEXT(R"(["t1","t2"])")).Num(), 2);
	FPlatformMisc::SetEnvironmentVar(TEXT("MM_GROUPS"), TEXT(R"({"g1":["t1"],"g2":["t2","t3"]})"));
	const TMap<FString, FEdgegapIdList> Groups = UEdgegapServerLibrary::GetGroupTicketMapping();
	FPlatformMisc::SetEnvironmentVar(TEXT("MM_GROUPS"), TEXT(""));
	TestEqual(TEXT("groups"), Groups.Num(), 2);
	TestTrue(TEXT("g2 has two tickets"), Groups.Contains(TEXT("g2")) && Groups[TEXT("g2")].Ids.Num() == 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEdgegapApiModelTest, "Edgegap.Api.Models.Deploy",
	EDGEGAP_TEST_FLAGS)

bool FEdgegapApiModelTest::RunTest(const FString& Parameters)
{
	FEdgegapAPIDeployModel Deploy;
	Deploy.AppName = TEXT("my-game");
	Deploy.VersionName = TEXT("20261102-2000");
	Deploy.IpList.Add(TEXT("1.2.3.4"));
	const TSharedRef<FJsonObject> Obj = Deploy.WriteJson();
	TestEqual(TEXT("app_name"), Obj->GetStringField(TEXT("app_name")), FString(TEXT("my-game")));
	TestEqual(TEXT("ip_list"), Obj->GetArrayField(TEXT("ip_list")).Num(), 1);
	TestFalse(TEXT("empty optional list omitted"), Obj->HasField(TEXT("geo_ip_list")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEdgegapTicketGateTest, "Edgegap.Server.TicketGate",
	EDGEGAP_TEST_FLAGS)

bool FEdgegapTicketGateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UEdgegapTicketGate* Gate = NewObject<UEdgegapTicketGate>(GameInstance);
	FString Ticket, Error;
	TestTrue(TEXT("off: anyone joins"), Gate->CheckJoin(TEXT("?Name=x"), TEXT("1.2.3.4"), Ticket, Error));

	Gate->SetEnforcing(true);
	Gate->AllowTicket(TEXT("cusfn10msflc73beiik0"));
	TestTrue(TEXT("assigned ticket joins"), Gate->CheckJoin(TEXT("?Name=x?edgegap_mm_ticket_id=cusfn10msflc73beiik0"), TEXT("a"), Ticket, Error));
	TestEqual(TEXT("ticket parsed"), Ticket, FString(TEXT("cusfn10msflc73beiik0")));
	Error.Reset();
	TestFalse(TEXT("no ticket refused"), Gate->CheckJoin(TEXT("?Name=x"), TEXT("a"), Ticket, Error));
	TestTrue(TEXT("403 reason"), Error.StartsWith(TEXT("403")));
	Error.Reset();
	TestFalse(TEXT("unknown ticket refused"), Gate->CheckJoin(TEXT("?edgegap_mm_ticket_id=other"), TEXT("a"), Ticket, Error));
	Error.Reset();
	TestFalse(TEXT("substring of an assigned ticket refused"), Gate->CheckJoin(TEXT("?edgegap_mm_ticket_id=c"), TEXT("a"), Ticket, Error));

	FEdgegapMMReadBackfillResponse Backfill;
	FEdgegapMMBackfillTicketMember Member;
	Member.Id = TEXT("backfilled1");
	Backfill.Tickets.Add(Member.Id, Member);
	Gate->AllowBackfillTickets(Backfill);
	Error.Reset();
	TestTrue(TEXT("backfill ticket joins"), Gate->CheckJoin(TEXT("?edgegap_mm_ticket_id=backfilled1"), TEXT("a"), Ticket, Error));
	return true;
}

#endif
