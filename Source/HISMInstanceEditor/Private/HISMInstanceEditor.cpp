// Copyright Olkon Games / HISM Instance Editor. All Rights Reserved.

#include "HISMInstanceEditor.h"
#include "HISMInstanceEditorLibrary.h"
#include "InstanceMarker.h"

#include "Components/TextRenderComponent.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "ToolMenus.h"

DEFINE_LOG_CATEGORY(LogHISMInstanceEditor);

void FHISMInstanceEditorModule::StartupModule()
{
    // Canonical path: RegisterStartupCallback BEFORE menus exist defers to ToolMenus' own
    // startup moment (after LevelEditor registered its menus). Calling this later (e.g. from
    // OnPostEngineInit) creates stub menus that get replaced by real registrations - sections
    // silently vanish. In commandlets IsToolMenuUIEnabled() is false and this is a no-op.
    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateStatic(&FHISMInstanceEditorModule::ExtendActorContextMenu));
    UE_LOG(LogHISMInstanceEditor, Log, TEXT("HISM Instance Editor module started"));
}

void FHISMInstanceEditorModule::ShutdownModule()
{
    UToolMenus::UnregisterOwner(this);
}

void FHISMInstanceEditorModule::ExtendActorContextMenu()
{
    if (!UToolMenus::Get())
        return;

    // The same section in BOTH menus: 3D viewport right-click AND the outliner right-click.
    const TCHAR *MenuNames[] = {
        TEXT("LevelViewportContextMenu.ActorOptions"), // viewport actor context menu
        TEXT("LevelEditor.ActorContextMenu"),          // outliner actor context menu
    };
    for (const TCHAR *MenuName : MenuNames)
    {
        UToolMenu *Menu = UToolMenus::Get()->ExtendMenu(MenuName);
        if (!Menu)
        {
            UE_LOG(LogHISMInstanceEditor, Warning, TEXT("ExtendMenu('%s') returned null"), MenuName);
            continue;
        }

        FToolMenuSection &Section =
            Menu->AddSection("HISMInstanceEditor", FText::FromString(TEXT("HISM Instance Editor")));

        Section.AddMenuEntry(
            "HISMIE_SpawnMarker",
            FText::FromString(TEXT("HISM: Spawn instance marker here")),
            FText::FromString(TEXT("Spawns an editor-only marker; drag it and right-click it to edit the nearest instance.")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateStatic(&FHISMInstanceEditorModule::ExecuteSpawnMarkerAtSelection)));

        Section.AddMenuEntry(
            "HISMIE_Unpack",
            FText::FromString(TEXT("HISM: Unpack instances for editing (like PLA)")),
            FText::FromString(TEXT("Turns every instance of the selected HISM hosts into EDIT_ actors so each drift moves with the normal gizmo. Hosts are hidden while editing.")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateStatic(&FHISMInstanceEditorModule::ExecuteUnpackSelectedHosts)));

        Section.AddMenuEntry(
            "HISMIE_Repack",
            FText::FromString(TEXT("HISM: Pack edited instances back (like PLA)")),
            FText::FromString(TEXT("Writes EDIT_ actors back into their HISM hosts, deletes the EDIT_ actors and unhides the hosts. Select EDIT_ actors and/or hosts first.")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateStatic(&FHISMInstanceEditorModule::ExecuteRepackFromEditActors)));

        Section.AddMenuEntry(
            "HISMIE_Convert",
            FText::FromString(TEXT("HISM: Convert selected actors to HISM (grouped)")),
            FText::FromString(TEXT("Turns selected static mesh actors (rocks, ice, etc.) into instanced hosts grouped by World Partition cell + mesh + phys material. Originals are deleted. Does not save the level.")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateStatic(&FHISMInstanceEditorModule::ExecuteConvertSelectedActors)));

        Section.AddDynamicEntry(
            "HISMIE_MarkerActions",
            FNewToolMenuSectionDelegate::CreateStatic(&FHISMInstanceEditorModule::AddMarkerActions));
    }
    UE_LOG(LogHISMInstanceEditor, Log, TEXT("HISM menus extended (viewport + outliner)"));
}

void FHISMInstanceEditorModule::AddMarkerActions(FToolMenuSection &Section)
{
    if (!GEditor)
        return;

    AInstanceMarker *Marker = nullptr;
    USelection *Sel = GEditor->GetSelectedActors();
    const int32 Num = Sel ? Sel->Num() : 0;
    for (int32 i = 0; i < Num; ++i)
    {
        Marker = Cast<AInstanceMarker>(Sel->GetSelectedObject(i));
        if (Marker)
            break;
    }
    if (!Marker)
        return;

    TWeakObjectPtr<AInstanceMarker> Weak(Marker);

    Section.AddMenuEntry(
        "HISMIE_Move",
        FText::FromString(TEXT("HISM: Move nearest instance here")),
        FText::FromString(TEXT("Moves the nearest ISM/HISM instance so its origin lands at the marker.")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(&FHISMInstanceEditorModule::ExecuteMoveNearestToMarker, Weak)));

    Section.AddMenuEntry(
        "HISMIE_Duplicate",
        FText::FromString(TEXT("HISM: Duplicate nearest instance here")),
        FText::FromString(TEXT("Copies the nearest ISM/HISM instance to the marker location.")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(&FHISMInstanceEditorModule::ExecuteDuplicateNearestToMarker, Weak)));

    Section.AddMenuEntry(
        "HISMIE_Rotate15",
        FText::FromString(TEXT("HISM: Rotate nearest instance 15\u00b0")),
        FText(),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(&FHISMInstanceEditorModule::ExecuteRotateNearestByMarker, Weak, 15.f)));

    Section.AddMenuEntry(
        "HISMIE_Rotate90",
        FText::FromString(TEXT("HISM: Rotate nearest instance 90\u00b0")),
        FText(),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(&FHISMInstanceEditorModule::ExecuteRotateNearestByMarker, Weak, 90.f)));

    Section.AddMenuEntry(
        "HISMIE_Delete",
        FText::FromString(TEXT("HISM: Delete nearest instance")),
        FText::FromString(TEXT("Removes the nearest ISM/HISM instance. Undoable with Ctrl+Z.")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(&FHISMInstanceEditorModule::ExecuteDeleteNearestToMarker, Weak)));
}

void FHISMInstanceEditorModule::ExecuteSpawnMarkerAtSelection()
{
    if (!GEditor)
        return;

    FVector Loc = FVector::ZeroVector;
    bool bFound = false;
    USelection *Sel = GEditor->GetSelectedActors();
    const int32 Num = Sel ? Sel->Num() : 0;
    for (int32 i = 0; i < Num; ++i)
    {
        if (AActor *A = Cast<AActor>(Sel->GetSelectedObject(i)))
        {
            Loc = A->GetActorLocation();
            bFound = true;
            break;
        }
    }
    if (!bFound)
    {
        if (FViewport *Viewport = GEditor->GetActiveViewport())
            if (FEditorViewportClient *Client = static_cast<FEditorViewportClient *>(Viewport->GetClient()))
                Loc = Client->GetViewLocation();
    }

    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World)
        return;

    AInstanceMarker *Marker =
        World->SpawnActor<AInstanceMarker>(AInstanceMarker::StaticClass(), Loc, FRotator::ZeroRotator);
    if (Marker)
    {
        GEditor->SelectActor(Marker, true, true);
        UE_LOG(LogHISMInstanceEditor, Log, TEXT("Instance marker spawned at %s"), *Loc.ToString());
    }
}

void FHISMInstanceEditorModule::ExecuteMoveNearestToMarker(TWeakObjectPtr<AInstanceMarker> Marker)
{
    if (!Marker.IsValid())
        return;
    const bool bOk = UHISMInstanceEditorLibrary::MoveNearestInstanceGlobal(Marker->GetActorLocation(),
                                                                           Marker->GetActorTransform());
    UE_LOG(LogHISMInstanceEditor, Log, TEXT("[MoveNearest] ok=%d"), bOk ? 1 : 0);
}

void FHISMInstanceEditorModule::ExecuteDeleteNearestToMarker(TWeakObjectPtr<AInstanceMarker> Marker)
{
    if (!Marker.IsValid())
        return;
    const bool bOk = UHISMInstanceEditorLibrary::DeleteNearestInstanceGlobal(Marker->GetActorLocation());
    UE_LOG(LogHISMInstanceEditor, Log, TEXT("[DeleteNearest] ok=%d"), bOk ? 1 : 0);
}

void FHISMInstanceEditorModule::ExecuteDuplicateNearestToMarker(TWeakObjectPtr<AInstanceMarker> Marker)
{
    if (!Marker.IsValid())
        return;
    const bool bOk = UHISMInstanceEditorLibrary::DuplicateNearestInstanceGlobal(Marker->GetActorLocation(),
                                                                                Marker->GetActorTransform());
    UE_LOG(LogHISMInstanceEditor, Log, TEXT("[DuplicateNearest] ok=%d"), bOk ? 1 : 0);
}

void FHISMInstanceEditorModule::ExecuteRotateNearestByMarker(TWeakObjectPtr<AInstanceMarker> Marker, float Degrees)
{
    if (!Marker.IsValid())
        return;
    const bool bOk = UHISMInstanceEditorLibrary::RotateNearestInstanceGlobal(Marker->GetActorLocation(), Degrees);
    UE_LOG(LogHISMInstanceEditor, Log, TEXT("[RotateNearest] ok=%d deg=%.0f"), bOk ? 1 : 0, Degrees);
}

void FHISMInstanceEditorModule::ExecuteUnpackSelectedHosts()
{
    const bool bOk = UHISMInstanceEditorLibrary::UnpackHISMHostsToEditActors();
    UE_LOG(LogHISMInstanceEditor, Log, TEXT("[Unpack] ok=%d"), bOk ? 1 : 0);
}

void FHISMInstanceEditorModule::ExecuteRepackFromEditActors()
{
    const bool bOk = UHISMInstanceEditorLibrary::RepackHISMHostsFromEditActors();
    UE_LOG(LogHISMInstanceEditor, Log, TEXT("[Pack] ok=%d"), bOk ? 1 : 0);
}

IMPLEMENT_MODULE(FHISMInstanceEditorModule, HISMInstanceEditor)

void FHISMInstanceEditorModule::ExecuteConvertSelectedActors()
{
    const int32 Hosts = UHISMInstanceEditorLibrary::ConvertSelectedActorsToHISM();
    UE_LOG(LogHISMInstanceEditor, Log, TEXT("[ConvertSelected] hosts created: %d"), Hosts);
}
