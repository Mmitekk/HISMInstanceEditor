// Copyright Olkon Games / HISM Instance Editor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstanceMarker.generated.h"

/**
 * Editor-only helper actor for the HISM Instance Editor plugin.
 * Drag it with the normal gizmo to the desired spot, right-click it and pick an
 * action ("Move nearest instance here" etc.). Invisible in game, no collision.
 */
UCLASS(Blueprintable)
class HISMINSTANCEEDITOR_API AInstanceMarker : public AActor
{
    GENERATED_BODY()

  public:
    AInstanceMarker();
};
