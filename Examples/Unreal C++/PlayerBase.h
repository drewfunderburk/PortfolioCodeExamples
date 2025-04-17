// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "PlayerBase.generated.h"

class USkeletalMeshComponent;
class UBetterSpringArmComponent;
class UCameraComponent;
class UInputComponent;
class UInputAction;
class UInputMappingContext;
class UCurveFloat;
struct FInputActionValue;
struct FEnhancedInputActionValueBinding;
struct FTimeline;

DECLARE_LOG_CATEGORY_EXTERN(LogPlayerBase, Log, All);

UENUM(BlueprintType)
enum class EPlayerLocomotionState : uint8
{
	Idle,
	Moving,
	Sprinting,
	CrouchIdle,
	CrouchMoving,
	Sliding,
	LedgeGrabbing,
	Falling,
};

UENUM(BlueprintType)
enum class EPlayerBlocker : uint8
{
	Movement,
	Look,
	Sprint,
	Crouch,
	Slide,
	Jump,
	Interact,
	ToggleFlashlight,
	PrimaryFire,
	SecondaryFire,
	Reload,
	AimDownSights,
	WeaponSwap,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLocomotionStateChangedSignature, EPlayerLocomotionState, PreviousState, EPlayerLocomotionState, NewState, APlayerBase*, Player);

UCLASS(config=Game)
class HORDESHOOTER_API APlayerBase : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	APlayerBase(const FObjectInitializer& ObjectInitializer);

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// Blockers
public:
	UFUNCTION(BlueprintCallable, Category = "PlayerBase|Blockers")
	void AddBlocker(EPlayerBlocker BlockerType, FName BlockerName);
	UFUNCTION(BlueprintCallable, Category = "PlayerBase|Blockers")
	int RemoveBlocker(EPlayerBlocker BlockerType, FName BlockerName);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PlayerBase|Blockers")
	bool HasBlocker(EPlayerBlocker BlockerType, FName BlockerName);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PlayerBase|Blockers")
	bool HasAnyBlocker(EPlayerBlocker BlockerType);
	UFUNCTION(BlueprintCallable, Category = "PlayerBase|Blockers")
	int ClearBlockers(EPlayerBlocker BlockerType);
	UFUNCTION(BlueprintCallable, Category = "PlayerBase|Blockers")
	int ClearAllBlockersByName(FName Name);
	UFUNCTION(BlueprintCallable, Category = "PlayerBase|Blockers")
	void AddMultiBlocker(const TArray<EPlayerBlocker>& BlockerTypes, FName BlockerName);
	UFUNCTION(BlueprintCallable, Category = "PlayerBase|Blockers")
	int RemoveMultiBlocker(const TArray<EPlayerBlocker>& BlockerTypes, FName BlockerName);

	void AddMoveSpeedModifier(const FString& Key, float Value);
	void RemoveMoveSpeedModifier(const FString& Key);

	// RPCs
private:
	UFUNCTION(Server, Reliable)
	void Server_SetWalkSpeed(float Speed);
	void Server_SetWalkSpeed_Implementation(float Speed);

	UFUNCTION(Server, Reliable)
	void Server_PrepareForLedgeGrab();
	void Server_PrepareForLedgeGrab_Implementation();
	
	UFUNCTION(Server, Reliable)
	void Server_CleanUpLedgeGrab();
	void Server_CleanUpLedgeGrab_Implementation();
	
	UFUNCTION(Server, Unreliable)
	void Server_SetActorLocation(FVector Location);
	void Server_SetActorLocation_Implementation(FVector Location);

	UFUNCTION(Server, Unreliable)
	void Server_SetActorRotation(FRotator Rotation);
	void Server_SetActorRotation_Implementation(FRotator Rotation);

	UFUNCTION(Server, Unreliable)
	void Server_CheckLedgeGrab();
	void Server_CheckLedgeGrab_Implementation();
	
	// Input actions
protected:
	void InputActionMove(const FInputActionValue& Value);
	void InputActionLook(const FInputActionValue& Value);
	void InputActionBeginJump(const FInputActionValue& Value);
	void InputActionEndJump(const FInputActionValue& Value);
	void InputActionPrimaryFire(const FInputActionValue& Value);
	void InputActionBeginAimDownSights(const FInputActionValue& Value);
	void InputActionEndAimDownSights(const FInputActionValue& Value);
	void InputActionReload(const FInputActionValue& Value);
	void InputActionBeginSprint(const FInputActionValue& Value);
	void InputActionEndSprint(const FInputActionValue& Value);
	void InputActionBeginCrouch(const FInputActionValue& Value);
	void InputActionEndCrouch(const FInputActionValue& Value);
	void InputActionInteract(const FInputActionValue& Value);
	void InputActionNextWeapon(const FInputActionValue& Value);
	void InputActionPreviousWeapon(const FInputActionValue& Value);
	void InputActionToggleFlashlight(const FInputActionValue& Value);

	// Locomotion
protected:
	void UpdateLocomotionState();
	void UpdateLocomotionStateIdle();
	void UpdateLocomotionStateMoving();
	void UpdateLocomotionStateSprinting();
	void UpdateLocomotionStateCrouchIdle();
	void UpdateLocomotionStateCrouchMoving();
	void UpdateLocomotionStateSliding();
	void UpdateLocomotionStateFalling();
	void UpdateLocomotionStateLedgeGrabbing();

	void SetLocomotionState(EPlayerLocomotionState NewState, bool bBroadcast = true);

private:
	void Move();
	float GetModifiedMoveSpeed(float StartingMoveSpeed);
	FVector GetCrouchPositionRelativeCameraBoomPosition();
	bool CheckLedgeGrab(FTransform& OutLedgeTransform);
	void PrepareForLedgeGrab();
	void CleanUpLedgeGrab();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Components
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UBetterSpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Mesh, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	// Events
public:
	UPROPERTY(BlueprintAssignable, Category = "PlayerBase | Events")
	FOnLocomotionStateChangedSignature OnLocomotionStateChanged;
	
	// Properties
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	float LookSensitivity = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	float PitchAngleMin = -80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	float PitchAngleMax = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	float CameraBoomCrouchedZ = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Sprint", meta = (AllowPrivateAccess = "true"))
	float SprintSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Crouch", meta = (AllowPrivateAccess = "true", Units = "Seconds", ClampMin=0.00001f))
	float CrouchSpeed = 0.2f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Crouch", meta = (AllowPrivateAccess = "true"))
	UCurveFloat* CrouchSpeedCurve;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Slide", meta = (AllowPrivateAccess = "true"))
	UCurveFloat* SlideSpeedCurve;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Ledge Grab", meta = (AllowPrivateAccess = "true"))
	UCurveVector* LedgeGrabMovementCurve;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Ledge Grab", meta = (AllowPrivateAccess = "true", Units = "Seconds", ClampMin = 0.001f))
	float LedgeGrabSpeed = 0.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Ledge Grab", meta = (AllowPrivateAccess = "true"))
	float LedgeGrabDestinationForwardOffset = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ledge Grab", meta = (AllowPrivateAccess = "true", MakeEditWidget = "true"))
	FVector LedgeGrabTraceStart = FVector(80.0f, 0.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ledge Grab", meta = (AllowPrivateAccess = "true", MakeEditWidget = "true"))
	FVector LedgeGrabTraceEnd = FVector(80.0f, 0.0f, 20.0f);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ledge Grab", meta = (AllowPrivateAccess = "true", MakeEditWidget = "true"))
	float LedgeGrabTraceSize = 10.0f;
	
	// Input
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* PrimaryFireAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* AimDownSightsAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* ReloadAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* CrouchAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* InteractAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* NextWeaponAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* PreviousWeaponAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* ToggleFlashlightAction;

	// Input values
protected:
	FVector2D MoveInput;
	FVector2D LookInput;
	bool bJumpInput;
	bool bPrimaryFireInput;
	bool bAimDownSightsInput;
	bool bReloadInput;
	bool bSprintInput;
	bool bCrouchInput;
	bool bInteractInput;
	bool bNextWeaponInput;
	bool bPreviousWeaponInput;
	bool bToggleFlashlightInput;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	EPlayerLocomotionState LocomotionState;

private:
	TMap<EPlayerBlocker, TSet<FName>> Blockers;
	TMap<FString, float> MoveSpeedModifiers;

	bool bCurrentLocomotionStateEntered = false;

	// Ledge Grabbing
	bool bIsLedgeGrabbing = false;
	EMovementMode MovementModeBeforeLedgeGrab;
	UPROPERTY(Replicated)
	bool bServerLedgeGrabCheckSucceeded = false;
	bool bClientLedgeGrabCheckSucceeded = false;
	float LedgeGrabProgress;
	FTransform LedgeGrabLedgeTransform;
	FTransform LedgeGrabCapsuleDestination;
	FTransform LedgeGrabStartTransform;

	// Sliding
	float SlideProgress;
	
	// Defaults
	float DefaultWalkSpeed;
	float DefaultCrouchSpeed;
	float DefaultCapsuleHalfHeight;
	float DefaultCameraBoomZ;
	float CrouchCapsuleResizeOffset;

	// Crouch camera smoothing
	float CrouchCameraLerpProgress;
};
