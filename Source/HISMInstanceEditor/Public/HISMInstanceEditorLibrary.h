// Copyright Olkon Games / HISM Instance Editor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HISMInstanceEditorLibrary.generated.h"

class UInstancedStaticMeshComponent;

/**
 * BlueprintCallable helpers to edit individual instances of ISM/HISM components.
 * All world-space transforms; all operations support editor undo (transactions).
 */
UCLASS()
class HISMINSTANCEEDITOR_API UHISMInstanceEditorLibrary : public UObject
{
    GENERATED_BODY()

  public:
    /** Returns instance index nearest to WorldPos within Comp, or -1. */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static int32 FindNearestInstance(UInstancedStaticMeshComponent *Comp, FVector WorldPos);

    /** Returns the ISM/HISM component holding the instance nearest to WorldPos across
     *  all loaded actors in the editor world, plus its index and distance. */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static UInstancedStaticMeshComponent *FindNearestInstanceGlobal(FVector WorldPos, int32 &OutIndex, float &OutDistance);

    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static bool GetInstanceWorldTransform(UInstancedStaticMeshComponent *Comp, int32 Index, FTransform &OutTransform);

    /** Moves instance Index so its origin lands at NewWorldTransform (rotation/scale replaced). */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static bool MoveInstance(UInstancedStaticMeshComponent *Comp, int32 Index, const FTransform &NewWorldTransform);

    /** Moves the instance nearest to FromWorld so its origin lands at ToWorld (keeps its rotation/scale). */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static bool MoveNearestInstance(UInstancedStaticMeshComponent *Comp, FVector FromWorld, const FTransform &ToWorld);

    /** Global (all ISM/HISM in the editor world) version of MoveNearestInstance. */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static bool MoveNearestInstanceGlobal(FVector FromWorld, const FTransform &ToWorld);

    /** Rotates the instance nearest to FromWorld around its own Z by Degrees. */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static bool RotateNearestInstanceYaw(UInstancedStaticMeshComponent *Comp, FVector FromWorld, float Degrees);

    /** Global version of RotateNearestInstanceYaw. */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static bool RotateNearestInstanceGlobal(FVector FromWorld, float Degrees);

    /** Deletes the instance nearest to FromWorld. */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static bool DeleteNearestInstance(UInstancedStaticMeshComponent *Comp, FVector FromWorld);

    /** Global version of DeleteNearestInstance. */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static bool DeleteNearestInstanceGlobal(FVector FromWorld);

    /** Duplicates the instance nearest to FromWorld, placing the copy at ToWorld. */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static bool DuplicateNearestInstance(UInstancedStaticMeshComponent *Comp, FVector FromWorld, const FTransform &ToWorld);

    /** Global version of DuplicateNearestInstance. */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static bool DuplicateNearestInstanceGlobal(FVector FromWorld, const FTransform &ToWorld);

    /** PLA-like edit, step 1 (Unpack): turns every instance of each SELECTED ISM/HISM
     *  host into an individual StaticMeshActor (label EDIT_<host>_<i>, folder
     *  "HISM Edit/<host>") so each snowdrift can be moved with the normal gizmo.
     *  Source hosts are hidden in the editor while editing. Selects the EDIT actors. */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static bool UnpackHISMHostsToEditActors();

    /** PLA-like edit, step 2 (Pack): writes the EDIT actors' transforms back into
     *  their source hosts (in place, hosts are preserved for World Partition),
     *  deletes the EDIT actors and unhides the hosts. Works from selected EDIT
     *  actors, selected hosts, or all EDIT actors in the level. */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static bool RepackHISMHostsFromEditActors();

    /** Creates (or returns true if exists) a Blueprint actor with a Hierarchical
     *  Instanced Static Mesh component as its ROOT, so all component properties stay
     *  editable in the Details panel for placed instances.
     *  AssetPath example: /Game/WinterHunt/Blueprints/Envirement/BP_HISMHost */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static bool CreateHISMHostBlueprint(const FString &AssetPath);

    /** Sets StaticMesh + PhysMaterialOverride on the SCS TEMPLATE HISM component of a
     *  host blueprint (python cannot reach SCS; template values persist reliably).
     *  Pass empty PhysMaterialPath to keep the current one. */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor")
    static bool SetHISMHostTemplateDefaults(const FString &AssetPath, UStaticMesh *Mesh, const FString &PhysMaterialPath);

    /** Converts the given actors (any actor with a StaticMeshComponent carrying a mesh,
     *  typically StaticMeshActors) into BP_HISMHost_<Mesh> instanced hosts, grouped by
     *  (World Partition cell, mesh, phys material). Originals are deleted. Returns the
     *  number of hosts created. Does NOT save the level. */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor", meta = (CallInEditor = "true"))
    static int32 ConvertActorsToHISM(const TArray<AActor *> &Actors);

    /** Convenience wrapper: converts the current editor actor selection. Returns hosts created. */
    UFUNCTION(BlueprintCallable, Category = "HISM Instance Editor", meta = (CallInEditor = "true"))
    static int32 ConvertSelectedActorsToHISM();
};
