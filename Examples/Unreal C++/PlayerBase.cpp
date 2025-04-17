// Fill out your copyright notice in the Description page of Project Settings.

// This is still here because I never remember the syntax for this
//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, FString::Printf(TEXT("Server Ledge Grab")));

#include "Player/PlayerBase.h"
#include "Camera/CameraComponent.h"
#include "Base/BetterSpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include <Kismet/GameplayStatics.h>
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"
#include "Curves/CurveVector.h"
#include "Net/UnrealNetwork.h"
#include "Base/CustomCharacterMovementComponent.h"

DEFINE_LOG_CATEGORY(LogPlayerBase);

// Sets default values
APlayerBase::APlayerBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) :
	// Set CharacterMovementComponent default class to CustomCharacterMovementComponent
	Super(ObjectInitializer
		.SetDefaultSubobjectClass<UCustomCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Character replicates
	bReplicates = true;
	
	// Setup Capsule
	GetCapsuleComponent()->InitCapsuleSize(40.0f, 96.0f);

	// Setup CameraBoom
	CameraBoom = CreateDefaultSubobject<UBetterSpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->TargetArmLength = 0.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bInheritPitch = true;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = true;

	// Setup Camera
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);
	Camera->bUsePawnControlRotation = false;

	// Setup first person mesh
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(Camera);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;
	FirstPersonMesh->SetCollisionProfileName(TEXT("CharacterMesh"));

	// Setup third person mesh
	USkeletalMeshComponent* ThirdPersonMesh = GetMesh();
	ThirdPersonMesh->SetOwnerNoSee(true);
	ThirdPersonMesh->bCastDynamicShadow = true;
	ThirdPersonMesh->CastShadow = true;
	ThirdPersonMesh->bCastHiddenShadow = true;
	ThirdPersonMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));

	// Set up camera pitch angle clamping
	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (CameraManager)
	{
		CameraManager->ViewPitchMax = PitchAngleMax;
		CameraManager->ViewPitchMin = PitchAngleMin;
	}

	// Set up blockers
	for (int i = 0; i < StaticEnum<EPlayerBlocker>()->GetMaxEnumValue(); i++)
	{
		Blockers.Add((EPlayerBlocker)i, TSet<FName>());
	}

	// Cache / Set default values
	DefaultWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
	DefaultCrouchSpeed = GetCharacterMovement()->MaxWalkSpeedCrouched;
	DefaultCameraBoomZ = CameraBoom->GetRelativeLocation().Z;
	DefaultCapsuleHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	CrouchCapsuleResizeOffset = (DefaultCapsuleHalfHeight - GetCharacterMovement()->GetCrouchedHalfHeight());
	SetLocomotionState(EPlayerLocomotionState::Idle);
}

// Called when the game starts or when spawned
void APlayerBase::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void APlayerBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Locomotion State Machine
	UpdateLocomotionState();

	// Crouch camera
	CrouchSpeed = FMath::Max(CrouchSpeed, 0.0001f);
	if (GetCharacterMovement()->IsCrouching())
		CrouchCameraLerpProgress = FMath::Min(CrouchCameraLerpProgress + (DeltaTime * (1.0f / CrouchSpeed)), 1);
	else
		CrouchCameraLerpProgress = FMath::Max(CrouchCameraLerpProgress - (DeltaTime * (1.0f / CrouchSpeed)), 0);

	CameraBoom->SetRelativeLocation(GetCrouchPositionRelativeCameraBoomPosition());

	// Print state to the screen
	if (GEngine)
	{
		FString LocalRoleString = UEnum::GetValueAsString(GetLocalRole());
		FString ActorName = GetName();
		FString LocomotionStateString = UEnum::GetValueAsString(LocomotionState);
		FString MovementModeString = UEnum::GetValueAsString(GetCharacterMovement()->MovementMode);
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, 
			FString::Printf(TEXT("%s | %s | %s | %s"), *ActorName, *LocalRoleString, *LocomotionStateString, *MovementModeString));
	}
}

// Called to bind functionality to input
void APlayerBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// Bind Input Actions and Value Bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Bind Input Actions
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerBase::InputActionMove);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &APlayerBase::InputActionMove);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerBase::InputActionLook);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerBase::InputActionBeginJump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &APlayerBase::InputActionEndJump);
		EnhancedInputComponent->BindAction(PrimaryFireAction, ETriggerEvent::Started, this, &APlayerBase::InputActionPrimaryFire);
		EnhancedInputComponent->BindAction(AimDownSightsAction, ETriggerEvent::Started, this, &APlayerBase::InputActionBeginAimDownSights);
		EnhancedInputComponent->BindAction(AimDownSightsAction, ETriggerEvent::Completed, this, &APlayerBase::InputActionEndAimDownSights);
		EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &APlayerBase::InputActionReload);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &APlayerBase::InputActionBeginSprint);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &APlayerBase::InputActionEndSprint);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &APlayerBase::InputActionBeginCrouch);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &APlayerBase::InputActionEndCrouch);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &APlayerBase::InputActionInteract);
		EnhancedInputComponent->BindAction(NextWeaponAction, ETriggerEvent::Started, this, &APlayerBase::InputActionNextWeapon);
		EnhancedInputComponent->BindAction(PreviousWeaponAction, ETriggerEvent::Started, this, &APlayerBase::InputActionPreviousWeapon);
		EnhancedInputComponent->BindAction(ToggleFlashlightAction, ETriggerEvent::Started, this, &APlayerBase::InputActionToggleFlashlight);
	}
	else
	{
		UE_LOG(LogPlayerBase, Error, TEXT("'%s' Failed to find an Enhanced Input component!"), *GetNameSafe(this));
	}
}

void APlayerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APlayerBase, bServerLedgeGrabCheckSucceeded);
}

/**
 * --------------------
 * - RPCs
 * --------------------
 */
#pragma region RPCs
void APlayerBase::Server_SetWalkSpeed_Implementation(float Speed)
{
	GetCharacterMovement()->MaxWalkSpeed = Speed;
}

void APlayerBase::Server_PrepareForLedgeGrab_Implementation()
{
	PrepareForLedgeGrab();
}

void APlayerBase::Server_CleanUpLedgeGrab_Implementation()
{
	CleanUpLedgeGrab();
}

void APlayerBase::Server_SetActorLocation_Implementation(FVector Location)
{
	SetActorLocation(Location);
}

void APlayerBase::Server_SetActorRotation_Implementation(FRotator Rotation)
{
	SetActorRotation(Rotation);
}

void APlayerBase::Server_CheckLedgeGrab_Implementation()
{
	FTransform Throwaway;
	bServerLedgeGrabCheckSucceeded = CheckLedgeGrab(Throwaway);
}
#pragma endregion

/**
 * --------------------
 * - Blockers
 * --------------------
 */
#pragma region Blockers
void APlayerBase::AddBlocker(EPlayerBlocker BlockerType, FName BlockerName)
{
	Blockers[BlockerType].Add(BlockerName);
}

int APlayerBase::RemoveBlocker(EPlayerBlocker BlockerType, FName BlockerName)
{
	return Blockers[BlockerType].Remove(BlockerName);
}

bool APlayerBase::HasBlocker(EPlayerBlocker BlockerType, FName BlockerName)
{
	return Blockers[BlockerType].Contains(BlockerName);
}

bool APlayerBase::HasAnyBlocker(EPlayerBlocker BlockerType)
{
	return !Blockers[BlockerType].IsEmpty();
}

int APlayerBase::ClearBlockers(EPlayerBlocker BlockerType)
{
	int NumRemoved = Blockers[BlockerType].Num();
	Blockers[BlockerType].Empty();
	return NumRemoved;
}

int APlayerBase::ClearAllBlockersByName(FName Name)
{
	int NumCleared = 0;
	for (int i = 0; i < StaticEnum<EPlayerBlocker>()->GetMaxEnumValue(); i++)
	{
		EPlayerBlocker Blocker = static_cast<EPlayerBlocker>(i);
		NumCleared += RemoveBlocker(Blocker, Name);
	}
	return NumCleared;
}

void APlayerBase::AddMultiBlocker(const TArray<EPlayerBlocker>& BlockerTypes, FName BlockerName)
{
	for (EPlayerBlocker BlockerType : BlockerTypes)
	{
		Blockers[BlockerType].Add(BlockerName);
	}
}

int APlayerBase::RemoveMultiBlocker(const TArray<EPlayerBlocker>& BlockerTypes, FName BlockerName)
{
	int NumRemoved = 0;
	for (EPlayerBlocker BlockerType : BlockerTypes)
	{
		NumRemoved += Blockers[BlockerType].Remove(BlockerName);
	}
	return NumRemoved;
}
#pragma endregion

void APlayerBase::AddMoveSpeedModifier(const FString& Key, float Value)
{
	if (MoveSpeedModifiers.Contains(Key))
	{
		MoveSpeedModifiers[Key] = Value;
	}
	else
	{
		MoveSpeedModifiers.Add(Key, Value);
	}
}

void APlayerBase::RemoveMoveSpeedModifier(const FString& Key)
{
	MoveSpeedModifiers.Remove(Key);
}

/**
 * --------------------
 * - Input Actions
 * --------------------
 */
#pragma region INPUT_ACTIONS
inline void APlayerBase::InputActionMove(const FInputActionValue& Value)
{
	MoveInput = Value.Get<FVector2D>();
}
void APlayerBase::InputActionLook(const FInputActionValue& Value)
{
	LookInput = Value.Get<FVector2D>();

	// If no controller, no input, or we have a look blocker, return
	if (!Controller || LookInput.SizeSquared() == 0 || HasAnyBlocker(EPlayerBlocker::Look))
		return;

	AddControllerYawInput(LookInput.X * LookSensitivity);
	AddControllerPitchInput(-LookInput.Y * LookSensitivity);
}
void APlayerBase::InputActionBeginJump(const FInputActionValue& Value)
{
	bJumpInput = true;
}
void APlayerBase::InputActionEndJump(const FInputActionValue& Value)
{
	bJumpInput = false;
	StopJumping();
}
void APlayerBase::InputActionPrimaryFire(const FInputActionValue& Value)
{
	bPrimaryFireInput = Value.Get<bool>();
}
void APlayerBase::InputActionBeginAimDownSights(const FInputActionValue& Value)
{
	bAimDownSightsInput = Value.Get<bool>();
}
void APlayerBase::InputActionEndAimDownSights(const FInputActionValue& Value)
{
	bAimDownSightsInput = Value.Get<bool>();
}
void APlayerBase::InputActionReload(const FInputActionValue& Value)
{
	bReloadInput = Value.Get<bool>();
}
void APlayerBase::InputActionBeginSprint(const FInputActionValue& Value)
{
	bSprintInput = true;
}
void APlayerBase::InputActionEndSprint(const FInputActionValue& Value)
{
	bSprintInput = false;
}
void APlayerBase::InputActionBeginCrouch(const FInputActionValue& Value)
{
	bCrouchInput = true;
}
void APlayerBase::InputActionEndCrouch(const FInputActionValue& Value)
{
	bCrouchInput = false;
}
void APlayerBase::InputActionInteract(const FInputActionValue& Value)
{
	bInteractInput = Value.Get<bool>();
}
void APlayerBase::InputActionNextWeapon(const FInputActionValue& Value)
{
	bNextWeaponInput = Value.Get<bool>();
}
void APlayerBase::InputActionPreviousWeapon(const FInputActionValue& Value)
{
	bPreviousWeaponInput = Value.Get<bool>();
}
void APlayerBase::InputActionToggleFlashlight(const FInputActionValue& Value)
{
	bToggleFlashlightInput = Value.Get<bool>();
}
#pragma endregion

/**
 * --------------------
 * - Locomotion
 * --------------------
 */
#pragma region LOCOMOTION
void APlayerBase::UpdateLocomotionState()
{
	// Do not run state machine if this actor is not locally controlled
	if (!IsLocallyControlled())
		return;
	
	switch (LocomotionState)
	{
	case EPlayerLocomotionState::Idle:
		UpdateLocomotionStateIdle();
		break;
	case EPlayerLocomotionState::Moving:
		UpdateLocomotionStateMoving();
		break;
	case EPlayerLocomotionState::Sprinting:
		UpdateLocomotionStateSprinting();
		break;
	case EPlayerLocomotionState::CrouchIdle:
		UpdateLocomotionStateCrouchIdle();
		break;
	case EPlayerLocomotionState::CrouchMoving:
		UpdateLocomotionStateCrouchMoving();
		break;
	case EPlayerLocomotionState::Sliding:
		UpdateLocomotionStateSliding();
		break;
	case EPlayerLocomotionState::Falling:
		UpdateLocomotionStateFalling();
		break;
	case EPlayerLocomotionState::LedgeGrabbing:
		UpdateLocomotionStateLedgeGrabbing();
		break;
	default:
		UE_LOG(LogPlayerBase, Error, TEXT("'%s' Invalid Locomotion State!"), *GetNameSafe(this));
		break;
	}
}

void APlayerBase::UpdateLocomotionStateIdle()
{
	if (MoveInput.SizeSquared() > 0 && !HasAnyBlocker(EPlayerBlocker::Movement))
	{
		SetLocomotionState(EPlayerLocomotionState::Moving);
		return;
	}

	// If we have crouch input and there are no crouch blockers, transition to CrouchIdle
	if (bCrouchInput && !HasAnyBlocker(EPlayerBlocker::Crouch))
	{
		SetLocomotionState(EPlayerLocomotionState::CrouchIdle);
		return;
	}

	// If we are falling, transition to Falling
	if (GetCharacterMovement()->IsFalling())
	{
		SetLocomotionState(EPlayerLocomotionState::Falling);
		return;
	}

	if (bJumpInput && !HasAnyBlocker(EPlayerBlocker::Jump))
		Jump();

	bCurrentLocomotionStateEntered = true;
}

void APlayerBase::UpdateLocomotionStateMoving()
{
	// If we have no movement input or we have a movement blocker, transition to Idle
	if (MoveInput.SizeSquared() == 0 || HasAnyBlocker(EPlayerBlocker::Movement))
	{
		SetLocomotionState(EPlayerLocomotionState::Idle);
		return;
	}

	// If we are falling, transition to falling state
	if (GetCharacterMovement()->IsFalling())
	{
		SetLocomotionState(EPlayerLocomotionState::Falling);
		return;
	}

	// If we have crouch input and there are no crouch blockers, transition to CrouchMoving
	if (bCrouchInput && !HasAnyBlocker(EPlayerBlocker::Crouch))
	{
		SetLocomotionState(EPlayerLocomotionState::CrouchMoving);
		return;
	}

	// If we have sprint input and there are no sprint blockers, transition to Sprinting
	if (bSprintInput && !HasAnyBlocker(EPlayerBlocker::Sprint))
	{
		SetLocomotionState(EPlayerLocomotionState::Sprinting);
		return;
	}

	if (bJumpInput && !HasAnyBlocker(EPlayerBlocker::Jump))
	{
		Jump();
		Server_CheckLedgeGrab();
	}

	bCurrentLocomotionStateEntered = true;

	// Handle movement
	GetCharacterMovement()->MaxWalkSpeed = GetModifiedMoveSpeed(DefaultWalkSpeed);
	Move();
}

void APlayerBase::UpdateLocomotionStateSprinting()
{
	// If we have no movement input, we have a movement blocker, or we have a sprint blocker, transition to Idle
	if (MoveInput.SizeSquared() == 0 ||
		HasAnyBlocker(EPlayerBlocker::Movement) || 
		HasAnyBlocker(EPlayerBlocker::Sprint))
	{
		SetLocomotionState(EPlayerLocomotionState::Idle);
		if (!HasAuthority()) Server_SetWalkSpeed(GetModifiedMoveSpeed(DefaultWalkSpeed));
		return;
	}

	// If we are falling, transition to falling state
	if (GetCharacterMovement()->IsFalling())
	{
		SetLocomotionState(EPlayerLocomotionState::Falling);
		if (!HasAuthority()) Server_SetWalkSpeed(GetModifiedMoveSpeed(DefaultWalkSpeed));
		return;
	}

	// If we have no sprint input or there are sprint blockers, transition to Moving
	if (!bSprintInput || HasAnyBlocker(EPlayerBlocker::Sprint))
	{
		SetLocomotionState(EPlayerLocomotionState::Moving);
		if (!HasAuthority()) Server_SetWalkSpeed(GetModifiedMoveSpeed(DefaultWalkSpeed));
		return;
	}

	// If we have crouch input and there are no slide blockers or crouch blockers, transition to Sliding
	if (bCrouchInput && 
		!HasAnyBlocker(EPlayerBlocker::Slide) && 
		!HasAnyBlocker(EPlayerBlocker::Crouch))
	{
		SetLocomotionState(EPlayerLocomotionState::Sliding);
		if (!HasAuthority()) Server_SetWalkSpeed(GetModifiedMoveSpeed(DefaultWalkSpeed));
		return;
	}

	if (!bCurrentLocomotionStateEntered && !HasAuthority()) Server_SetWalkSpeed(GetModifiedMoveSpeed(DefaultWalkSpeed) * SprintSpeedMultiplier);

	bCurrentLocomotionStateEntered = true;

	if (bJumpInput && !HasAnyBlocker(EPlayerBlocker::Jump))
		Jump();
	
	// Handle movement
	GetCharacterMovement()->MaxWalkSpeed = GetModifiedMoveSpeed(DefaultWalkSpeed) * SprintSpeedMultiplier;
	Move();
}

void APlayerBase::UpdateLocomotionStateCrouchIdle()
{
	// If we have no crouch input or there are crouch blockers, transition to Idle
	if ((!bCrouchInput || HasAnyBlocker(EPlayerBlocker::Crouch)) && !GetCharacterMovement()->IsCrouching())
	{
		SetLocomotionState(EPlayerLocomotionState::Idle);
		return;
	}

	// If we are falling, transition to falling state
	if (GetCharacterMovement()->IsFalling())
	{
		SetLocomotionState(EPlayerLocomotionState::Falling);
		return;
	}

	// If we have movement input and there are no movement blockers, transition to CrouchMoving
	if (MoveInput.SizeSquared() > 0 && !HasAnyBlocker(EPlayerBlocker::Movement))
	{
		SetLocomotionState(EPlayerLocomotionState::CrouchMoving);
		return;
	}

	bCurrentLocomotionStateEntered = true;

	if (bCrouchInput)
		Crouch();
	else
		UnCrouch();
}

void APlayerBase::UpdateLocomotionStateCrouchMoving()
{
	// If we have no crouch input and no movement input, 
	// or we have a movement blocker and a crouch blocker, 
	// and on top of those things the character movement component is not crouching, 
	// transition to Idle
	bool bMoveOrCrouchInput = MoveInput.SizeSquared() > 0 || bCrouchInput;
	bool bHasBlockers = HasAnyBlocker(EPlayerBlocker::Movement) && HasAnyBlocker(EPlayerBlocker::Crouch);
	bool bIsCrouching = GetCharacterMovement()->IsCrouching();
	if ((!bMoveOrCrouchInput || bHasBlockers) && !bIsCrouching)
	{
		SetLocomotionState(EPlayerLocomotionState::Idle);
		return;
	}

	// If we are falling, transition to falling state
	if (GetCharacterMovement()->IsFalling())
	{
		SetLocomotionState(EPlayerLocomotionState::Falling);
		return;
	}

	// If we have no movement input or we have a movement blocker, transition to CrouchIdle
	if (MoveInput.SizeSquared() == 0 || HasAnyBlocker(EPlayerBlocker::Movement))
	{
		SetLocomotionState(EPlayerLocomotionState::CrouchIdle);
		return;
	}

	// If we have no crouch input or there are crouch blockers, 
	// and the character movement component is not crouching, 
	// transition to Moving
	if ((!bCrouchInput || HasAnyBlocker(EPlayerBlocker::Crouch)) && !GetCharacterMovement()->IsCrouching())
	{
		SetLocomotionState(EPlayerLocomotionState::Moving);
		return;
	}

	bCurrentLocomotionStateEntered = true;

	// Handle movement
	if (bCrouchInput)
		Crouch();
	else
		UnCrouch();
	GetCharacterMovement()->MaxWalkSpeedCrouched = GetModifiedMoveSpeed(DefaultCrouchSpeed);
	Move();
}

void APlayerBase::UpdateLocomotionStateSliding()
{
	// If we have no crouch input and no movement input, 
	// or we have a movement blocker and a crouch blocker, 
	// and on top of those things the character movement component is not crouching, 
	// transition to Idle
	bool bMoveOrCrouchInput = MoveInput.SizeSquared() > 0 || bCrouchInput;
	bool bHasBlockers = HasAnyBlocker(EPlayerBlocker::Movement) && HasAnyBlocker(EPlayerBlocker::Crouch);
	bool bIsCrouching = GetCharacterMovement()->IsCrouching();
	if ((!bMoveOrCrouchInput || bHasBlockers) && !bIsCrouching)
	{
		SetLocomotionState(EPlayerLocomotionState::Idle);
		return;
	}

	// If we are falling, transition to falling state
	if (GetCharacterMovement()->IsFalling())
	{
		SetLocomotionState(EPlayerLocomotionState::Falling);
		return;
	}

	// If we have no sprint input or there are crouch blockers, transition to CrouchMoving
	// TODO: Potential bug on leaving slide
	if (!bSprintInput || HasAnyBlocker(EPlayerBlocker::Crouch))
	{
		SetLocomotionState(EPlayerLocomotionState::CrouchMoving);
		return;
	}

	// If we have no movement input or we have a movement blocker, transition to CrouchIdle
	if (MoveInput.SizeSquared() == 0 || HasAnyBlocker(EPlayerBlocker::Movement))
	{
		SetLocomotionState(EPlayerLocomotionState::CrouchIdle);
		return;
	}

	// If no crouch input or there are crouch blockers, transition to Moving
	if ((!bCrouchInput || HasAnyBlocker(EPlayerBlocker::Crouch)) && !GetCharacterMovement()->IsCrouching())
	{
		SetLocomotionState(EPlayerLocomotionState::Moving);
		return;
	}
	
	bCurrentLocomotionStateEntered = true;

	// Handle movement
	Crouch();
	GetCharacterMovement()->MaxWalkSpeedCrouched = GetModifiedMoveSpeed(DefaultCrouchSpeed);
	Move();
}

void APlayerBase::UpdateLocomotionStateFalling()
{
	// If we are no longer falling, transition to Idle
	if (!GetCharacterMovement()->IsFalling())
	{
		bClientLedgeGrabCheckSucceeded = false;
		LedgeGrabLedgeTransform = FTransform();
		SetLocomotionState(EPlayerLocomotionState::Idle);
		return;
	}

	if (!bCurrentLocomotionStateEntered)
	{
		bClientLedgeGrabCheckSucceeded = false;
		bServerLedgeGrabCheckSucceeded = false;
		LedgeGrabLedgeTransform = FTransform();
		LedgeGrabCapsuleDestination = FTransform();
	}
	
	if (bJumpInput)
	{
		// TODO: Continue here
		// Assume server ledge grab check succeeded if standalone
		if (IsNetMode(NM_Standalone))
		{
			bServerLedgeGrabCheckSucceeded = true;
			bClientLedgeGrabCheckSucceeded = CheckLedgeGrab(LedgeGrabLedgeTransform);
		}
		bServerLedgeGrabCheckSucceeded = true;
		bool bLedgeGrabAgreed = bClientLedgeGrabCheckSucceeded && bServerLedgeGrabCheckSucceeded;
		
		if (bLedgeGrabAgreed)
		{
			SetLocomotionState(EPlayerLocomotionState::LedgeGrabbing);
			return;
		}
		else if (!HasAuthority())
		{
			bClientLedgeGrabCheckSucceeded = CheckLedgeGrab(LedgeGrabLedgeTransform);
			if (bClientLedgeGrabCheckSucceeded)
			{
				Server_CheckLedgeGrab();
			}
		}
	}
	
	bCurrentLocomotionStateEntered = true;

	// Handle movement
	GetCharacterMovement()->MaxWalkSpeed = GetModifiedMoveSpeed(DefaultWalkSpeed);
	Move();
}

void APlayerBase::UpdateLocomotionStateLedgeGrabbing()
{
	// If either the LedgeTransform is invalid or the LedgeGrabCapsuleDestination is invalid, transition to idle
	if (!LedgeGrabLedgeTransform.IsValid() || !LedgeGrabCapsuleDestination.IsValid())
	{
		if (!HasAuthority() && IsNetMode(NM_Client))
			Server_CleanUpLedgeGrab();
		CleanUpLedgeGrab();
		SetLocomotionState(EPlayerLocomotionState::Idle);
		return;
	}

	// Set up ledge grab
	if (!bCurrentLocomotionStateEntered)
	{
		if (!HasAuthority() && IsNetMode(NM_Client))
			Server_PrepareForLedgeGrab();
		PrepareForLedgeGrab();
	}

	// If the ledge grab is completed, transition to idle
	if (LedgeGrabProgress >= 1.0f)
	{
		if (!HasAuthority() && IsNetMode(NM_Client))
			Server_CleanUpLedgeGrab();
		CleanUpLedgeGrab();
		SetLocomotionState(EPlayerLocomotionState::Idle);
		return;
	}
	
	bCurrentLocomotionStateEntered = true;
	
	// Perform Ledge Grab
	if (!IsValid(LedgeGrabMovementCurve)) return;
	
	FVector CurveSample = LedgeGrabMovementCurve->GetVectorValue(LedgeGrabProgress);
	FVector NewLocation = FVector::ZeroVector;
	NewLocation.X = FMath::Lerp(LedgeGrabStartTransform.GetLocation().X, LedgeGrabCapsuleDestination.GetLocation().X, CurveSample.X);
	NewLocation.Y = FMath::Lerp(LedgeGrabStartTransform.GetLocation().Y, LedgeGrabCapsuleDestination.GetLocation().Y, CurveSample.Y);
	NewLocation.Z = FMath::Lerp(LedgeGrabStartTransform.GetLocation().Z, LedgeGrabCapsuleDestination.GetLocation().Z, CurveSample.Z);
	FVector DeltaMovement = NewLocation - GetActorLocation();
	float Scale = DeltaMovement.Length() / GetCharacterMovement()->MaxAcceleration;
	AddMovementInput(DeltaMovement, Scale);

	// Increment LedgeGrabProgress progress
	LedgeGrabProgress += UGameplayStatics::GetWorldDeltaSeconds(this) * (1 / FMath::Max(0.0001f, LedgeGrabSpeed));
	LedgeGrabProgress = FMath::Clamp(LedgeGrabProgress, 0.0f, 1.0f);
}

void APlayerBase::SetLocomotionState(EPlayerLocomotionState NewState, bool bBroadcast)
{
	if (bBroadcast)
		OnLocomotionStateChanged.Broadcast(LocomotionState, NewState, this);
	LocomotionState = NewState;
	bCurrentLocomotionStateEntered = false;
}

void APlayerBase::Move()
{
	FVector2D MoveDirection = MoveInput;
	MoveDirection.Normalize();

	AddMovementInput(GetActorForwardVector(), MoveDirection.Y);
	AddMovementInput(GetActorRightVector(), MoveDirection.X);
}

float APlayerBase::GetModifiedMoveSpeed(float StartingMoveSpeed)
{
	float ModifiedMoveSpeed = StartingMoveSpeed;
	for (TPair<FString, float>& Pair : MoveSpeedModifiers)
	{
		ModifiedMoveSpeed *= Pair.Value;
	}
	return ModifiedMoveSpeed;
}

FVector APlayerBase::GetCrouchPositionRelativeCameraBoomPosition()
{
	if (!IsValid(CrouchSpeedCurve)) return FVector::ZeroVector;
	
	float Value = CrouchSpeedCurve->GetFloatValue(CrouchCameraLerpProgress);
	FVector TopPosition;
	FVector BottomPosition;

	if (GetCharacterMovement()->IsCrouching())
	{
		TopPosition = {0, 0, CrouchCapsuleResizeOffset + DefaultCameraBoomZ};
		BottomPosition = {0, 0, CameraBoomCrouchedZ};
	}
	else
	{
		TopPosition = {0, 0, DefaultCameraBoomZ};
		BottomPosition = {0, 0, -CrouchCapsuleResizeOffset + CameraBoomCrouchedZ};
	}
	
	return FMath::Lerp(TopPosition, BottomPosition, Value);
}

bool APlayerBase::CheckLedgeGrab(FTransform& OutLedgeTransform)
{
	bool bDebug = false;
	FColor Color = HasAuthority() ? FColor::Red : FColor::Blue;
	EDrawDebugTrace::Type DebugTraceType = bDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;
	float Duration = HasAuthority() ? 5 : 2;
	
	// BoxTrace Down
	FHitResult Hit;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);
	UKismetSystemLibrary::BoxTraceSingleByProfile(
		GetWorld(),
		GetActorTransform().TransformPosition(LedgeGrabTraceStart),
		GetActorTransform().TransformPosition(LedgeGrabTraceEnd),
		FVector(LedgeGrabTraceSize / 2),
		GetActorRotation(),
		UEnum::GetValueAsName(ECC_Visibility),
		true,
		ActorsToIgnore,
		DebugTraceType,
		Hit,
		true,
		Color,
		FLinearColor::Green,
		Duration);

	// Return if we did not hit a valid actor
	if (!(Hit.bBlockingHit && IsValid(Hit.GetActor()))) return false;
	
	// Return if the floor is not a walkable angle
	float LedgeAngle = abs(acos(GetActorUpVector().Dot(Hit.ImpactNormal)));
	if (LedgeAngle >= GetCharacterMovement()->GetWalkableFloorAngle()) return false;

	// Cache initial hit location
	FVector InitialHitLocation = Hit.ImpactPoint;

	// Get oriented height of InitialHitLocation;
	FVector TransformedInitialHitLocation = GetActorTransform().InverseTransformPositionNoScale(InitialHitLocation);
	FVector InitialHitOffset = FVector(0, 0, TransformedInitialHitLocation.Z);
	FVector FollowUpTraceStart = GetActorTransform().TransformPosition(InitialHitOffset);
	
	// BoxTrace from player at height of initial hit
	UKismetSystemLibrary::BoxTraceSingleByProfile(
	GetWorld(),
	FollowUpTraceStart,
	InitialHitLocation,
	FVector(LedgeGrabTraceSize / 2),
	GetActorRotation(),
	UEnum::GetValueAsName(ECC_Visibility),
	true,
	ActorsToIgnore,
	DebugTraceType,
	Hit,
	true,
	Color,
	FLinearColor::Green,
	Duration);
	
	// Calculate ledge location
	FVector TransformedFollowUpHitLocation = GetActorTransform().InverseTransformPositionNoScale(Hit.ImpactPoint);
	FVector FollowUpHitOffset = FVector(TransformedFollowUpHitLocation.X, TransformedFollowUpHitLocation.Y, TransformedInitialHitLocation.Z);
	FVector FollowUpHitPosition = GetActorTransform().TransformPosition(FollowUpHitOffset);

	// Set OutLedgeTransform
	FTransform LedgeTransform;
	LedgeTransform.SetLocation(FollowUpHitPosition);
	LedgeTransform.SetRotation(Hit.ImpactNormal.ToOrientationQuat());
	LedgeTransform.SetScale3D(FVector::One());

	// Calculate capsule destination offset from ledge grab point
	FVector LedgePositionRelativeOffset = FVector(-LedgeGrabDestinationForwardOffset, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 10);
	FVector LedgePositionOffset = LedgeTransform.TransformPosition(LedgePositionRelativeOffset);
	FVector CapsuleDestination = LedgePositionOffset;

	if (bDebug)
	{
		UKismetSystemLibrary::DrawDebugCapsule(
			GetWorld(),
			CapsuleDestination,
			GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
			GetCapsuleComponent()->GetScaledCapsuleRadius(),
			OutLedgeTransform.GetRotation().Rotator(),
			FLinearColor::Green,
			5);
	}
	
	// Capsule Overlap at destination
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(TEnumAsByte<EObjectTypeQuery>(ECC_WorldStatic));
	TArray<AActor*> OverlappedActors;
	UKismetSystemLibrary::CapsuleOverlapActors(
		GetWorld(),
		CapsuleDestination,
		GetCapsuleComponent()->GetUnscaledCapsuleRadius(),
		GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),
		ObjectTypes,
		AActor::StaticClass(),
		ActorsToIgnore,
		OverlappedActors);

	// Return if overlapped any WorldStatic objects at destination
	if (!OverlappedActors.IsEmpty()) return false;

	OutLedgeTransform = LedgeTransform;
	LedgeGrabCapsuleDestination.SetLocation(CapsuleDestination);
	LedgeGrabCapsuleDestination.SetRotation(OutLedgeTransform.GetRotation());
	LedgeGrabCapsuleDestination.SetScale3D(FVector::One());
	
	if (bDebug)
	{
		UKismetSystemLibrary::DrawDebugSphere(
			GetWorld(),
			OutLedgeTransform.GetLocation(),
			10,
			12,
			Color,
			Duration + 5);

		UKismetSystemLibrary::DrawDebugArrow(
			GetWorld(),
			OutLedgeTransform.GetLocation(),
			OutLedgeTransform.GetLocation() + OutLedgeTransform.GetRotation().GetForwardVector() * 100,
			1,
			Color,
			Duration + 5);
	}
	
	return true;
}

void APlayerBase::PrepareForLedgeGrab()
{
	LedgeGrabProgress = 0.0f;
	LedgeGrabStartTransform = GetActorTransform();
	MovementModeBeforeLedgeGrab = GetCharacterMovement()->MovementMode;
	bIsLedgeGrabbing = true;
	
	StopJumping();
	GetCharacterMovement()->Velocity = FVector::Zero();
	GetCharacterMovement()->SetMovementMode(MOVE_Custom);
	FRotator NewRotation = LedgeGrabCapsuleDestination.GetRotation().Rotator();
	NewRotation.Yaw += 180;
	UGameplayStatics::GetPlayerController(this, 0)->SetControlRotation(NewRotation);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	AddBlocker(EPlayerBlocker::Look, "LedgeGrab");
}

void APlayerBase::CleanUpLedgeGrab()
{
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	GetCharacterMovement()->Velocity = FVector::Zero();
	GetCharacterMovement()->SetMovementMode(MovementModeBeforeLedgeGrab);
	RemoveBlocker(EPlayerBlocker::Look, "LedgeGrab");
	bIsLedgeGrabbing = false;
}
#pragma endregion