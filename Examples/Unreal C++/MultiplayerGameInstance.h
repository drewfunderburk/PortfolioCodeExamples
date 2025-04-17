// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MultiplayerGameInstance.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMultiplayerGameInstance, Log, All);

/**
 * 
 */
UCLASS()
class HORDESHOOTER_API UMultiplayerGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	UMultiplayerGameInstance();
	virtual ~UMultiplayerGameInstance();

	virtual void Init() override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnCreateSessionComplete")
	void OnCreateSessionCompleteBlueprint(FName SessionName);

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnDestroySessionComplete")
	void OnDestroySessionCompleteBlueprint(FName SessionName);

	UFUNCTION(Exec, BlueprintCallable)
	virtual void Host(const FString& MapPath);

	UFUNCTION(Exec, BlueprintCallable)
	virtual void ShutdownSession();

private:
	void CreateSession();
	void DestroySession();

	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

private:
	const FName SESSION_NAME = TEXT("MySession");
	IOnlineSubsystem* Subsystem;
	IOnlineSessionPtr SessionInterface;
};
