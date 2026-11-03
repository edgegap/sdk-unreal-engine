// Edgegap SDK: ticket gate "already in use" tests (Session Frontend > Automation, filter "Edgegap").

#include "Misc/AutomationTest.h"
#include "EdgegapCompat.h"
#include "EdgegapTicketGate.h"
#include "Engine/DemoNetConnection.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace EdgegapTicketGateTests
{
	// Stands in for a client connection; only its open/closed state matters to the gate.
	UNetConnection* OpenConnection(UObject* Outer)
	{
		UNetConnection* Connection = NewObject<UDemoNetConnection>(Outer);
		Connection->SetConnectionState(USOCK_Open);
		return Connection;
	}

	void Close(UNetConnection* Connection) { Connection->SetConnectionState(USOCK_Closed); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEdgegapTicketGateInUseTest, "Edgegap.Server.TicketInUse",
	EDGEGAP_TEST_FLAGS)

bool FEdgegapTicketGateInUseTest::RunTest(const FString& Parameters)
{
	using namespace EdgegapTicketGateTests;
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UEdgegapTicketGate* Gate = NewObject<UEdgegapTicketGate>(GameInstance);
	Gate->SetEnforcing(true);
	Gate->AllowTicket(TEXT("ticketA"));
	Gate->AllowTicket(TEXT("ticketB"));
	FString Ticket, Error;

	UNetConnection* A = OpenConnection(GameInstance);
	UNetConnection* B = OpenConnection(GameInstance);
	Gate->HoldTicket(TEXT("ticketA"), A);
	Gate->HoldTicket(TEXT("ticketB"), B);
	TestFalse(TEXT("held ticket refused"), Gate->CheckJoin(TEXT("?edgegap_mm_ticket_id=ticketA"), TEXT("x"), Ticket, Error));
	TestTrue(TEXT("in-use reason"), Error.Contains(TEXT("already in use")));

	// Logout of a controller the game replaced: its player is still connected (live bug with Lyra).
	Gate->ReleaseTickets(nullptr);
	Error.Reset();
	TestFalse(TEXT("replaced controller keeps the ticket held"), Gate->CheckJoin(TEXT("?edgegap_mm_ticket_id=ticketA"), TEXT("x"), Ticket, Error));

	// Another player disconnects: their connection is closed by the time Logout runs.
	Close(B);
	Gate->ReleaseTickets(nullptr);
	Error.Reset();
	TestFalse(TEXT("other player leaving keeps the ticket held"), Gate->CheckJoin(TEXT("?edgegap_mm_ticket_id=ticketA"), TEXT("x"), Ticket, Error));
	TestTrue(TEXT("leaver's ticket rejoins"), Gate->CheckJoin(TEXT("?edgegap_mm_ticket_id=ticketB"), TEXT("x"), Ticket, Error));

	// Holder's connection closes before any Logout: the ticket is free again.
	Close(A);
	TestTrue(TEXT("ticket rejoins after its connection closed"), Gate->CheckJoin(TEXT("?edgegap_mm_ticket_id=ticketA"), TEXT("x"), Ticket, Error));

	// Kick: Logout names the still-open connection.
	UNetConnection* A2 = OpenConnection(GameInstance);
	Gate->HoldTicket(TEXT("ticketA"), A2);
	Error.Reset();
	TestFalse(TEXT("rejoined ticket held again"), Gate->CheckJoin(TEXT("?edgegap_mm_ticket_id=ticketA"), TEXT("x"), Ticket, Error));
	Gate->ReleaseTickets(A2);
	TestTrue(TEXT("released by its connection"), Gate->CheckJoin(TEXT("?edgegap_mm_ticket_id=ticketA"), TEXT("x"), Ticket, Error));
	return true;
}

#endif
