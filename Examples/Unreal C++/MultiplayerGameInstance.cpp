// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiplayerGameInstance.h"
#include "OnlineSubsystemSteam.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "OnlineSessionSettings.h"
#include <AssetRegistry/AssetRegistryModule.h>
#include "Engine/Console.h"

DEFINE_LOG_CATEGORY(LogMultiplayerGameInstance);

UMultiplayerGameInstance::UMultiplayerGameInstance()
{

}

UMultiplayerGameInstance::~UMultiplayerGameInstance()
{

}

void UMultiplayerGameInstance::Init()
{
	Super::Init();

	// Get the online subsystem
	Subsystem = IOnlineSubsystem::Get();
	if (!Subsystem)
	{
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("Failed to find online subsystem"));
		return;
	}

	// Get the session interface
	SessionInterface = Subsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("Failed to find session interface"));
		return;
	}

	// Bind session interface delegates
	SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UMultiplayerGameInstance::OnCreateSessionComplete);
	SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UMultiplayerGameInstance::OnDestroySessionComplete);
	SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UMultiplayerGameInstance::OnJoinSessionComplete);
	SessionInterface->OnSessionUserInviteAcceptedDelegates.AddUObject(this, &UMultiplayerGameInstance::OnInviteAccepted);
}

void UMultiplayerGameInstance::Host(const FString& MapPath)
{
	UEngine* Engine = GetEngine();
	UWorld* World = GetWorld();
	UGameViewportClient* GameViewport = nullptr;
	TObjectPtr<UConsole> ViewportConsole = nullptr;
	if(World)
		GameViewport = World->GetGameViewport();
	if (GameViewport)
		ViewportConsole = GameViewport->ViewportConsole;

	// Ensure mapPath is not empty
	if (MapPath.IsEmpty())
	{
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("No map path"));
		if (ViewportConsole)
			ViewportConsole->OutputText(TEXT("No map path"));
		return;
	}

	// Ensure mapPath is a path to a valid asset
	FString MapName = FPaths::GetBaseFilename(MapPath);
	FString ParsedMapPath = FString::Printf(TEXT("/Game/_HordeShooter/Maps/%s"), *MapName);
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*ParsedMapPath);
	if (!AssetData.IsValid())
	{
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("Map not found"));
		if (ViewportConsole)
			ViewportConsole->OutputText(TEXT("Map not found"));
		return;
	}

	UE_LOG(LogMultiplayerGameInstance, Display, TEXT("Hosting..."));
	if (Engine)
		Engine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Hosting..."));

	// If there is an existing session under SESSION_NAME, destroy it
	if (SessionInterface->GetNamedSession(SESSION_NAME) != nullptr)
	{
		UE_LOG(LogMultiplayerGameInstance, Display, TEXT("Destroying existing session..."));
		SessionInterface->DestroySession(SESSION_NAME);
	}
	CreateSession();

	// Server travel
	UE_LOG(LogMultiplayerGameInstance, Display, TEXT("ServerTraveling to %s"), *ParsedMapPath);
	if (Engine)
		Engine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("ServerTraveling to %s"), *ParsedMapPath));
	if (ViewportConsole)
		ViewportConsole->OutputText(FString::Printf(TEXT("ServerTraveling to %s"), *ParsedMapPath));
	FString URL = FString::Printf(TEXT("%s?listen"), *ParsedMapPath);
	GetWorld()->ServerTravel(URL);
}

void UMultiplayerGameInstance::ShutdownSession()
{
	UE_LOG(LogMultiplayerGameInstance, Display, TEXT("Shutting down..."));
	UEngine* Engine = GetEngine();
	if (Engine)
		Engine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Shutting down..."));
	DestroySession();
}

void UMultiplayerGameInstance::CreateSession()
{
	if (!SessionInterface.IsValid())
		return;
	
	UE_LOG(LogMultiplayerGameInstance, Display, TEXT("Creating session"));

	FOnlineSessionSettings SessionSettings;
	SessionSettings.bIsLANMatch = false;
	SessionSettings.NumPublicConnections = 4;
	SessionSettings.bUsesPresence = true;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.bAllowJoinViaPresenceFriendsOnly = true;
	SessionSettings.bUseLobbiesIfAvailable = true;
	SessionInterface->CreateSession(0, SESSION_NAME, SessionSettings);

	OnCreateSessionCompleteBlueprint(SESSION_NAME);
}

void UMultiplayerGameInstance::DestroySession()
{
	if (!SessionInterface.IsValid())
		return;

	if (SessionInterface->GetNamedSession(SESSION_NAME) != nullptr)
	{
		UE_LOG(LogMultiplayerGameInstance, Display, TEXT("Destroying session"));
		SessionInterface->DestroySession(SESSION_NAME);
		OnDestroySessionCompleteBlueprint(SESSION_NAME);
	}
	else
	{
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("Session not found"));
	}
}

void UMultiplayerGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogMultiplayerGameInstance, Display, TEXT("Session created successfully"));
	}
	else
	{
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("Failed to create session"));
	}
}

void UMultiplayerGameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogMultiplayerGameInstance, Display, TEXT("Session destroyed successfully"));
	}
	else
	{
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("Failed to destroy session"));
	}
}

void UMultiplayerGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (!SessionInterface.IsValid())
		return;

	switch (Result)
	{
	case EOnJoinSessionCompleteResult::Success:
		UE_LOG(LogMultiplayerGameInstance, Display, TEXT("Join session successful"));
		break;
	case EOnJoinSessionCompleteResult::SessionIsFull:
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("Session is full"));
		return;
	case EOnJoinSessionCompleteResult::SessionDoesNotExist:
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("Session does not exist"));
		return;
	case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("Could not retrieve address"));
		return;
	case EOnJoinSessionCompleteResult::AlreadyInSession:
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("Already in session"));
		return;
	case EOnJoinSessionCompleteResult::UnknownError:
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("Unknown error"));
		return;
	}

	FString URL;
	if (!SessionInterface->GetResolvedConnectString(SESSION_NAME, URL))
	{
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("Failed to get resolved connect string"));
		return;
	}

	APlayerController* PlayerController = GetFirstLocalPlayerController();
	if (!PlayerController)
	{
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("Failed to find player controller"));
		return;
	}

	PlayerController->ClientTravel(URL, ETravelType::TRAVEL_Absolute);
}

void UMultiplayerGameInstance::OnInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (!SessionInterface.IsValid())
		return;

	if (bWasSuccessful)
	{
		UE_LOG(LogMultiplayerGameInstance, Warning, TEXT("Invite accepted successfully"));
	}
	else
	{
		UE_LOG(LogMultiplayerGameInstance, Error, TEXT("Failed to accept invite"));
		return;
	}

	SessionInterface->JoinSession(0, SESSION_NAME, InviteResult);
}
