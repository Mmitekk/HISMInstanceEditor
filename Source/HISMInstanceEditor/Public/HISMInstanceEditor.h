// Copyright Olkon Games / HISM Instance Editor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

struct FToolMenuSection;
class AInstanceMarker;

DECLARE_LOG_CATEGORY_EXTERN(LogHISMInstanceEditor, Log, All);

class FHISMInstanceEditorModule : public IModuleInterface
{
  public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

  private:
    /** Builds the actor-context-menu entries (marker actions). */
    static void ExtendActorContextMenu();
    static void AddMarkerActions(FToolMenuSection &Section);

    /** Menu actions. */
    static void ExecuteSpawnMarkerAtSelection();
    static void ExecuteUnpackSelectedHosts();
    static void ExecuteRepackFromEditActors();
    static void ExecuteConvertSelectedActors();
    static void ExecuteMoveNearestToMarker(TWeakObjectPtr<AInstanceMarker> Marker);
    static void ExecuteDeleteNearestToMarker(TWeakObjectPtr<AInstanceMarker> Marker);
    static void ExecuteDuplicateNearestToMarker(TWeakObjectPtr<AInstanceMarker> Marker);
    static void ExecuteRotateNearestByMarker(TWeakObjectPtr<AInstanceMarker> Marker, float Degrees);
};
