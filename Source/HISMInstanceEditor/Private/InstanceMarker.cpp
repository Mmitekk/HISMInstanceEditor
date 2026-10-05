// Copyright Olkon Games / HISM Instance Editor. All Rights Reserved.

#include "InstanceMarker.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AInstanceMarker::AInstanceMarker()
{
    PrimaryActorTick.bCanEverTick = false;
    SetActorEnableCollision(false);

    USceneComponent *Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    UTextRenderComponent *Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("MarkerText"));
    Text->SetupAttachment(Root);
    Text->SetText(FText::FromString(TEXT("INSTANCE MARKER")));
    Text->SetWorldSize(20.f);
    Text->SetHorizontalAlignment(EHTA_Center);
    Text->SetTextRenderColor(FColor::Yellow);
    Text->SetHiddenInGame(true);

    UStaticMeshComponent *Ball = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerBall"));
    Ball->SetupAttachment(Root);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        Ball->SetStaticMesh(SphereMesh.Object);
        Ball->SetRelativeScale3D(FVector(0.35f));
    }
    Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Ball->SetHiddenInGame(true);
}
