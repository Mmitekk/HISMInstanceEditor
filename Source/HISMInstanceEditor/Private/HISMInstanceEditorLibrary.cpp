// Copyright Olkon Games / HISM Instance Editor. All Rights Reserved.

#include "HISMInstanceEditorLibrary.h"
#include "HISMInstanceEditor.h"
#include "InstanceMarker.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Selection.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInterface.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "HISMInstanceEditor"

namespace HISMIE
{
    static UWorld *GetEditorWorld()
    {
        return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    }

    static int32 FindNearestInComp(UInstancedStaticMeshComponent *Comp, const FVector &WorldPos, float &OutDistSq)
    {
        OutDistSq = TNumericLimits<float>::Max();
        if (!Comp || Comp->GetInstanceCount() <= 0)
            return -1;

        int32 Best = -1;
        for (int32 i = 0; i < Comp->GetInstanceCount(); ++i)
        {
            FTransform T;
            if (!Comp->GetInstanceTransform(i, T, /*bWorldSpace*/ true))
                continue;
            const float DistSq = static_cast<float>(FVector::DistSquared(T.GetTranslation(), WorldPos));
            if (DistSq < OutDistSq)
            {
                OutDistSq = DistSq;
                Best = i;
            }
        }
        return Best;
    }

    static bool ApplyInstanceTransform(UInstancedStaticMeshComponent *Comp, int32 Index, const FTransform &NewWorld,
                                       const TCHAR *OpName)
    {
        if (!Comp || !Comp->IsRegistered())
        {
            UE_LOG(LogHISMInstanceEditor, Warning, TEXT("[%s] component is null or not registered"), OpName);
            return false;
        }
        FScopedTransaction Transaction(LOCTEXT("HISMIE_Transaction", "HISM Instance Editor"));
        Comp->Modify();
        const bool bOk = Comp->UpdateInstanceTransform(Index, NewWorld, /*bWorldSpace*/ true,
                                                       /*bMarkRenderStateDirty*/ true, /*bTeleport*/ true);
        if (bOk)
            Comp->MarkPackageDirty();
        return bOk;
    }
} // namespace HISMIE

//----------------------------------------------------------------------
// Per-component queries
//----------------------------------------------------------------------

int32 UHISMInstanceEditorLibrary::FindNearestInstance(UInstancedStaticMeshComponent *Comp, FVector WorldPos)
{
    float DistSq;
    return HISMIE::FindNearestInComp(Comp, WorldPos, DistSq);
}

UInstancedStaticMeshComponent *UHISMInstanceEditorLibrary::FindNearestInstanceGlobal(FVector WorldPos, int32 &OutIndex,
                                                                                     float &OutDistance)
{
    OutIndex = -1;
    OutDistance = TNumericLimits<float>::Max();
    UInstancedStaticMeshComponent *BestComp = nullptr;

    UWorld *World = HISMIE::GetEditorWorld();
    if (!World)
        return nullptr;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TArray<UInstancedStaticMeshComponent *> Comps;
        It->GetComponents<UInstancedStaticMeshComponent, FDefaultAllocator>(Comps);
        for (UInstancedStaticMeshComponent *Comp : Comps)
        {
            float DistSq;
            const int32 Idx = HISMIE::FindNearestInComp(Comp, WorldPos, DistSq);
            if (Idx >= 0 && DistSq < OutDistance)
            {
                OutDistance = DistSq;
                OutIndex = Idx;
                BestComp = Comp;
            }
        }
    }
    OutDistance = FMath::Sqrt(OutDistance);
    return BestComp;
}

bool UHISMInstanceEditorLibrary::GetInstanceWorldTransform(UInstancedStaticMeshComponent *Comp, int32 Index,
                                                           FTransform &OutTransform)
{
    if (!Comp || Index < 0 || Index >= Comp->GetInstanceCount())
        return false;
    return Comp->GetInstanceTransform(Index, OutTransform, /*bWorldSpace*/ true);
}

//----------------------------------------------------------------------
// Per-component operations
//----------------------------------------------------------------------

bool UHISMInstanceEditorLibrary::MoveInstance(UInstancedStaticMeshComponent *Comp, int32 Index,
                                              const FTransform &NewWorldTransform)
{
    if (!Comp || Index < 0 || Index >= Comp->GetInstanceCount())
        return false;
    return HISMIE::ApplyInstanceTransform(Comp, Index, NewWorldTransform, TEXT("MoveInstance"));
}

bool UHISMInstanceEditorLibrary::MoveNearestInstance(UInstancedStaticMeshComponent *Comp, FVector FromWorld,
                                                     const FTransform &ToWorld)
{
    float DistSq;
    const int32 Idx = HISMIE::FindNearestInComp(Comp, FromWorld, DistSq);
    if (Idx < 0)
        return false;

    FTransform Old;
    Comp->GetInstanceTransform(Idx, Old, true);
    FTransform New(ToWorld.GetRotation(), ToWorld.GetTranslation(), Old.GetScale3D());
    return MoveInstance(Comp, Idx, New);
}

bool UHISMInstanceEditorLibrary::RotateNearestInstanceYaw(UInstancedStaticMeshComponent *Comp, FVector FromWorld,
                                                          float Degrees)
{
    float DistSq;
    const int32 Idx = HISMIE::FindNearestInComp(Comp, FromWorld, DistSq);
    if (Idx < 0)
        return false;

    FTransform Old;
    Comp->GetInstanceTransform(Idx, Old, true);
    const FQuat DeltaYaw(FVector::UpVector, FMath::DegreesToRadians(Degrees));
    FTransform New(DeltaYaw * Old.GetRotation(), Old.GetTranslation(), Old.GetScale3D());
    return MoveInstance(Comp, Idx, New);
}

bool UHISMInstanceEditorLibrary::DeleteNearestInstance(UInstancedStaticMeshComponent *Comp, FVector FromWorld)
{
    float DistSq;
    const int32 Idx = HISMIE::FindNearestInComp(Comp, FromWorld, DistSq);
    if (Idx < 0 || !Comp)
        return false;

    FScopedTransaction Transaction(LOCTEXT("HISMIE_Delete", "HISM Instance Editor: Delete instance"));
    Comp->Modify();
    const bool bOk = Comp->RemoveInstance(Idx);
    if (bOk)
    {
        Comp->MarkRenderStateDirty();
        Comp->MarkPackageDirty();
    }
    return bOk;
}

bool UHISMInstanceEditorLibrary::DuplicateNearestInstance(UInstancedStaticMeshComponent *Comp, FVector FromWorld,
                                                          const FTransform &ToWorld)
{
    float DistSq;
    const int32 Idx = HISMIE::FindNearestInComp(Comp, FromWorld, DistSq);
    if (Idx < 0 || !Comp)
        return false;

    FTransform Old;
    Comp->GetInstanceTransform(Idx, Old, true);
    FTransform New(ToWorld.GetRotation(), ToWorld.GetTranslation(), Old.GetScale3D());

    FScopedTransaction Transaction(LOCTEXT("HISMIE_Duplicate", "HISM Instance Editor: Duplicate instance"));
    Comp->Modify();
    const int32 NewIndex = Comp->AddInstance(New, /*bWorldSpace*/ true);
    const bool bOk = NewIndex >= 0;
    if (bOk)
    {
        Comp->MarkRenderStateDirty();
        Comp->MarkPackageDirty();
    }
    return bOk;
}

//----------------------------------------------------------------------
// Global operations
//----------------------------------------------------------------------

bool UHISMInstanceEditorLibrary::MoveNearestInstanceGlobal(FVector FromWorld, const FTransform &ToWorld)
{
    int32 Idx;
    float Dist;
    UInstancedStaticMeshComponent *Comp = FindNearestInstanceGlobal(FromWorld, Idx, Dist);
    if (!Comp || Idx < 0)
        return false;

    FTransform Old;
    Comp->GetInstanceTransform(Idx, Old, true);
    FTransform New(ToWorld.GetRotation(), ToWorld.GetTranslation(), Old.GetScale3D());
    const bool bOk = MoveInstance(Comp, Idx, New);
    UE_LOG(LogHISMInstanceEditor, Log, TEXT("[MoveNearestGlobal] ok=%d idx=%d dist=%.0f owner=%s"),
           bOk ? 1 : 0, Idx, Dist,
           Comp->GetOwner() ? *Comp->GetOwner()->GetActorLabel() : TEXT("?"));
    return bOk;
}

bool UHISMInstanceEditorLibrary::RotateNearestInstanceGlobal(FVector FromWorld, float Degrees)
{
    int32 Idx;
    float Dist;
    UInstancedStaticMeshComponent *Comp = FindNearestInstanceGlobal(FromWorld, Idx, Dist);
    if (!Comp || Idx < 0)
        return false;
    return RotateNearestInstanceYaw(Comp, FromWorld, Degrees);
}

bool UHISMInstanceEditorLibrary::DeleteNearestInstanceGlobal(FVector FromWorld)
{
    int32 Idx;
    float Dist;
    UInstancedStaticMeshComponent *Comp = FindNearestInstanceGlobal(FromWorld, Idx, Dist);
    if (!Comp || Idx < 0)
        return false;
    return DeleteNearestInstance(Comp, FromWorld);
}

bool UHISMInstanceEditorLibrary::DuplicateNearestInstanceGlobal(FVector FromWorld, const FTransform &ToWorld)
{
    int32 Idx;
    float Dist;
    UInstancedStaticMeshComponent *Comp = FindNearestInstanceGlobal(FromWorld, Idx, Dist);
    if (!Comp || Idx < 0)
        return false;
    return DuplicateNearestInstance(Comp, FromWorld, ToWorld);
}

//----------------------------------------------------------------------
// BP_HISMHost factory
//----------------------------------------------------------------------

bool UHISMInstanceEditorLibrary::CreateHISMHostBlueprint(const FString &AssetPath)
{
    if (AssetPath.IsEmpty())
        return false;
    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
        return true;

    FString AssetName;
    AssetPath.Split(TEXT("/"), nullptr, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
    UPackage *Pkg = CreatePackage(*AssetPath);

    UBlueprint *BP = FKismetEditorUtilities::CreateBlueprint(
        AActor::StaticClass(), Pkg, FName(*AssetName), BPTYPE_Normal, UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(), FName("HISMInstanceEditor"));
    if (!BP)
    {
        UE_LOG(LogHISMInstanceEditor, Error, TEXT("[CreateHISMHostBlueprint] CreateBlueprint failed for %s"),
               *AssetPath);
        return false;
    }

    USimpleConstructionScript *SCS = BP->SimpleConstructionScript;
    if (!SCS)
    {
        UE_LOG(LogHISMInstanceEditor, Error, TEXT("[CreateHISMHostBlueprint] no SCS"));
        return false;
    }

    USCS_Node *Node = SCS->CreateNode(UHierarchicalInstancedStaticMeshComponent::StaticClass(), TEXT("HISMRoot"));
    if (!Node)
    {
        UE_LOG(LogHISMInstanceEditor, Error, TEXT("[CreateHISMHostBlueprint] CreateNode failed"));
        return false;
    }
    if (UHierarchicalInstancedStaticMeshComponent *Tmpl =
            Cast<UHierarchicalInstancedStaticMeshComponent>(Node->ComponentTemplate))
    {
        Tmpl->SetCollisionProfileName(TEXT("BlockAll"));
        Tmpl->Mobility = EComponentMobility::Static;
        Tmpl->bUseDefaultCollision = false;
    }
    SCS->AddNode(Node);
    // AddNode to an SCS without a root makes the first node the root scene node
    // (UE5 assigns RootNodes[0] as the root when no default root exists).
    SCS->ValidateSceneRootNodes();

    // Remove the auto-created DefaultSceneRoot, if any, so HISMRoot is the single root.
    TArray<USCS_Node *> AllNodes = SCS->GetAllNodes();
    for (USCS_Node *Existing : AllNodes)
    {
        if (Existing && Existing != Node &&
            Existing->ComponentTemplate && Existing->ComponentTemplate->GetFName() == TEXT("DefaultSceneRoot"))
        {
            SCS->RemoveNode(Existing);
            break;
        }
    }

    FAssetRegistryModule::AssetCreated(BP);
    Pkg->MarkPackageDirty();
    const bool bSaved = UEditorAssetLibrary::SaveAsset(AssetPath, /*OnlyIfIsDirty*/ false);
    UE_LOG(LogHISMInstanceEditor, Log, TEXT("[CreateHISMHostBlueprint] created %s (saved=%d)"), *AssetPath, bSaved);
    return bSaved;
}

bool UHISMInstanceEditorLibrary::SetHISMHostTemplateDefaults(const FString &AssetPath, UStaticMesh *Mesh,
                                                             const FString &PhysMaterialPath)
{
    UBlueprint *BP = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if (!BP || !BP->SimpleConstructionScript)
    {
        UE_LOG(LogHISMInstanceEditor, Error, TEXT("[SetTemplateDefaults] BP or SCS missing: %s"), *AssetPath);
        return false;
    }

    USCS_Node *HISMNode = nullptr;
    for (USCS_Node *Node : BP->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->ComponentTemplate &&
            Node->ComponentTemplate->IsA<UHierarchicalInstancedStaticMeshComponent>())
        {
            HISMNode = Node;
            break;
        }
    }
    if (!HISMNode)
    {
        UE_LOG(LogHISMInstanceEditor, Error, TEXT("[SetTemplateDefaults] no HISM node in %s"), *AssetPath);
        return false;
    }

    auto *Tmpl = Cast<UHierarchicalInstancedStaticMeshComponent>(HISMNode->ComponentTemplate);
    Tmpl->Modify();
    if (Mesh)
        Tmpl->SetStaticMesh(Mesh);
    if (!PhysMaterialPath.IsEmpty())
    {
        if (UPhysicalMaterial *Phys = LoadObject<UPhysicalMaterial>(nullptr, *PhysMaterialPath))
        {
            Tmpl->SetPhysMaterialOverride(Phys);
            Tmpl->SetCollisionProfileName(TEXT("BlockAll"));
            Tmpl->Mobility = EComponentMobility::Static;
        }
        else
        {
            UE_LOG(LogHISMInstanceEditor, Error, TEXT("[SetTemplateDefaults] phys material not found: %s"),
                   *PhysMaterialPath);
            return false;
        }
    }

    const bool bSaved = UEditorAssetLibrary::SaveAsset(AssetPath, /*OnlyIfIsDirty*/ false);
    UE_LOG(LogHISMInstanceEditor, Log, TEXT("[SetTemplateDefaults] %s mesh=%s phys=%s saved=%d"), *AssetPath,
           Mesh ? *Mesh->GetName() : TEXT("keep"),
           *PhysMaterialPath, bSaved);
    return bSaved;
}

//----------------------------------------------------------------------
// PLA-like edit: Unpack (HISM -> individual actors) / Repack (back to HISM)
//----------------------------------------------------------------------

namespace HISMIE
{
    static FString SrcGuidTag(const AActor *Host)
    {
        return FString::Printf(TEXT("HISM_SRC_%s"), *Host->GetActorGuid().ToString());
    }

    static FString CompNameTag(const UActorComponent *Comp)
    {
        return FString::Printf(TEXT("HISM_COMP_%s"), *Comp->GetFName().ToString());
    }

    static bool ActorHasTag(const AActor *A, const FName &Tag)
    {
        return A && A->Tags.Contains(Tag);
    }

    static FString ActorSrcGuid(const AActor *A)
    {
        if (!A)
            return FString();
        for (const FName &T : A->Tags)
        {
            const FString S = T.ToString();
            if (S.StartsWith(TEXT("HISM_SRC_")))
                return S.RightChop(9);
        }
        return FString();
    }

    static FString ActorCompTag(const AActor *A)
    {
        if (!A)
            return FString();
        for (const FName &T : A->Tags)
        {
            const FString S = T.ToString();
            if (S.StartsWith(TEXT("HISM_COMP_")))
                return S.RightChop(10);
        }
        return FString();
    }

    static FString ActorHostLabelFromFolder(const AActor *A)
    {
        if (!A)
            return FString();
        const FString Folder = A->GetFolderPath().ToString();
        const FString Prefix = TEXT("HISM Edit/");
        if (Folder.StartsWith(Prefix))
            return Folder.RightChop(Prefix.Len());
        return FString();
    }

    static AActor *FindHostByGuid(UWorld *World, const FString &GuidStr)
    {
        if (!World || GuidStr.IsEmpty())
            return nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
            if (It->GetActorGuid().ToString() == GuidStr)
                return *It;
        return nullptr;
    }

    static AActor *FindHostByLabel(UWorld *World, const FString &Label)
    {
        if (!World || Label.IsEmpty())
            return nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetActorLabel() != Label)
                continue;
            TArray<UInstancedStaticMeshComponent *> Comps;
            It->GetComponents<UInstancedStaticMeshComponent>(Comps);
            if (Comps.Num() > 0)
                return *It;
        }
        return nullptr;
    }
} // namespace HISMIE

bool UHISMInstanceEditorLibrary::UnpackHISMHostsToEditActors()
{
    if (!GEditor)
        return false;
    UWorld *World = HISMIE::GetEditorWorld();
    if (!World)
        return false;

    USelection *Sel = GEditor->GetSelectedActors();
    TArray<AActor *> Hosts;
    const int32 Num = Sel ? Sel->Num() : 0;
    for (int32 i = 0; i < Num; ++i)
    {
        AActor *A = Cast<AActor>(Sel->GetSelectedObject(i));
        if (!A || HISMIE::ActorHasTag(A, FName("HISM_EDIT")))
            continue;
        TArray<UInstancedStaticMeshComponent *> Comps;
        A->GetComponents<UInstancedStaticMeshComponent>(Comps);
        if (Comps.Num() > 0)
            Hosts.Add(A);
    }
    if (Hosts.Num() == 0)
    {
        UE_LOG(LogHISMInstanceEditor, Warning,
               TEXT("[Unpack] select 1+ HISM host actors first (actors with ISM/HISM components)"));
        return false;
    }

    FScopedTransaction Transaction(LOCTEXT("HISMIE_Unpack", "HISM Instance Editor: Unpack to edit actors"));
    TArray<AActor *> NewActors;
    int32 TotalInstances = 0;

    for (AActor *Host : Hosts)
    {
        Host->Modify();
        const FString HostLabel = Host->GetActorLabel();
        const FString GuidTag = HISMIE::SrcGuidTag(Host);

        TArray<UInstancedStaticMeshComponent *> Comps;
        Host->GetComponents<UInstancedStaticMeshComponent>(Comps);
        for (UInstancedStaticMeshComponent *Comp : Comps)
        {
            if (!Comp || Comp->GetInstanceCount() <= 0)
                continue;
            UStaticMesh *Mesh = Comp->GetStaticMesh();
            if (!Mesh)
            {
                UE_LOG(LogHISMInstanceEditor, Warning, TEXT("[Unpack] host '%s' comp '%s' has no mesh, skipped"),
                       *HostLabel, *Comp->GetName());
                continue;
            }
            UPrimitiveComponent *SrcPrim = Cast<UPrimitiveComponent>(Comp);
            const FName DstFolder(*FString::Printf(TEXT("HISM Edit/%s"), *HostLabel));
            const FName CompTag(*HISMIE::CompNameTag(Comp));
            const int32 Count = Comp->GetInstanceCount();
            for (int32 Idx = 0; Idx < Count; ++Idx)
            {
                FTransform T;
                if (!Comp->GetInstanceTransform(Idx, T, /*bWorldSpace*/ true))
                    continue;
                FActorSpawnParameters Params;
                AStaticMeshActor *SM = World->SpawnActor<AStaticMeshActor>(
                    AStaticMeshActor::StaticClass(), T.GetTranslation(), T.Rotator(), Params);
                if (!SM)
                    continue;
                SM->SetActorScale3D(T.GetScale3D());
                if (UStaticMeshComponent *Dst = SM->GetStaticMeshComponent())
                {
                    Dst->SetStaticMesh(Mesh);
                    Dst->SetMobility(EComponentMobility::Static);
                    const int32 NumMats = Comp->GetNumMaterials();
                    for (int32 m = 0; m < NumMats; ++m)
                        if (UMaterialInterface *Mat = Comp->GetMaterial(m))
                            Dst->SetMaterial(m, Mat);
                    if (SrcPrim)
                    {
                        Dst->SetCollisionProfileName(SrcPrim->GetCollisionProfileName());
                        // No public getter for the override itself in 5.2; the effective
                        // material equals the override when one is set (our Snowdrift case).
                        if (UPhysicalMaterial *PM = SrcPrim->BodyInstance.GetSimplePhysicalMaterial())
                            Dst->SetPhysMaterialOverride(PM);
                    }
                }
                SM->SetActorLabel(FString::Printf(TEXT("EDIT_%s_%d"), *HostLabel, Idx), /*bMarkDirty*/ false);
                SM->SetFolderPath(DstFolder);
                SM->Tags.Add(FName("HISM_EDIT"));
                SM->Tags.Add(FName(*GuidTag));
                SM->Tags.Add(CompTag);
                SM->MarkPackageDirty();
                NewActors.Add(SM);
                ++TotalInstances;
            }
        }
        // Hide the packed host while its EDIT_ doubles are out (same trick as PLA edit mode:
        // the packed view disappears, each drift is a normal actor with a normal gizmo).
        Host->SetIsTemporarilyHiddenInEditor(true);
    }

    if (NewActors.Num() == 0)
    {
        UE_LOG(LogHISMInstanceEditor, Warning, TEXT("[Unpack] no instances found on selected hosts"));
        return false;
    }

    GEditor->SelectNone(/*bNoteSelectionChange*/ true, /*bDeselectBSPSurfs*/ false);
    for (AActor *E : NewActors)
        GEditor->SelectActor(E, /*bSelect*/ true, /*bNotify*/ false);
    GEditor->NoteSelectionChange();

    UE_LOG(LogHISMInstanceEditor, Log, TEXT("[Unpack] %d hosts -> %d EDIT_ actors (%d instances). Hosts hidden. Edit transforms, then Pack."),
           Hosts.Num(), NewActors.Num(), TotalInstances);
    return true;
}

bool UHISMInstanceEditorLibrary::RepackHISMHostsFromEditActors()
{
    if (!GEditor)
        return false;
    UWorld *World = HISMIE::GetEditorWorld();
    if (!World)
        return false;

    // 1. collect EDIT actors: prefer selection, else selected hosts' children, else all in level.
    TArray<AActor *> EditActors;
    TArray<AActor *> SelHosts;
    if (USelection *Sel = GEditor->GetSelectedActors())
    {
        for (int32 i = 0; i < Sel->Num(); ++i)
        {
            AActor *A = Cast<AActor>(Sel->GetSelectedObject(i));
            if (!A)
                continue;
            if (HISMIE::ActorHasTag(A, FName("HISM_EDIT")))
                EditActors.Add(A);
            else
            {
                TArray<UInstancedStaticMeshComponent *> Comps;
                A->GetComponents<UInstancedStaticMeshComponent>(Comps);
                if (Comps.Num() > 0)
                    SelHosts.Add(A);
            }
        }
    }
    if (EditActors.Num() == 0 && SelHosts.Num() > 0)
    {
        // User selected host(s): pick up their EDIT_ children by guid tag or folder.
        for (AActor *H : SelHosts)
        {
            const FString G = H->GetActorGuid().ToString();
            const FString FolderMatch = FString::Printf(TEXT("HISM Edit/%s"), *H->GetActorLabel());
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (!HISMIE::ActorHasTag(*It, FName("HISM_EDIT")))
                    continue;
                if (HISMIE::ActorSrcGuid(*It) == G || HISMIE::ActorHostLabelFromFolder(*It) == H->GetActorLabel() ||
                    It->GetFolderPath().ToString() == FolderMatch)
                    EditActors.Add(*It);
            }
        }
    }
    if (EditActors.Num() == 0)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
            if (HISMIE::ActorHasTag(*It, FName("HISM_EDIT")))
                EditActors.Add(*It);
    }
    if (EditActors.Num() == 0)
    {
        UE_LOG(LogHISMInstanceEditor, Warning,
               TEXT("[Pack] no EDIT_ actors found. Unpack a HISM host first, or select its EDIT_ actors."));
        return false;
    }

    // 2. group EDIT actors by source host.
    TMap<AActor *, TArray<AActor *>> Groups;
    TArray<AActor *> Orphans;
    for (AActor *E : EditActors)
    {
        AActor *Host = HISMIE::FindHostByGuid(World, HISMIE::ActorSrcGuid(E));
        if (!Host)
            Host = HISMIE::FindHostByLabel(World, HISMIE::ActorHostLabelFromFolder(E));
        if (!Host && SelHosts.Num() == 1)
            Host = SelHosts[0]; // fallback: exactly one host selected -> it owns them
        if (!Host)
        {
            Orphans.Add(E);
            continue;
        }
        Groups.FindOrAdd(Host).Add(E);
    }
    if (Orphans.Num() > 0)
        UE_LOG(LogHISMInstanceEditor, Warning, TEXT("[Pack] %d EDIT_ actors have no source host and were skipped"),
               Orphans.Num());
    if (Groups.Num() == 0)
        return false;

    // 3. write back in place (host actors preserved -> World Partition cells stay valid).
    FScopedTransaction Transaction(LOCTEXT("HISMIE_Repack", "HISM Instance Editor: Pack edit actors to HISM"));
    int32 PackedHosts = 0, PackedInstances = 0;
    TArray<AActor *> HostsToSelect;
    for (auto &Pair : Groups)
    {
        AActor *Host = Pair.Key;
        TArray<AActor *> &Edits = Pair.Value;
        Edits.Sort([](const AActor &A, const AActor &B) { return A.GetActorLabel() < B.GetActorLabel(); });

        TArray<UInstancedStaticMeshComponent *> Comps;
        Host->GetComponents<UInstancedStaticMeshComponent>(Comps);
        if (Comps.Num() == 0)
            continue;
        // If the host has several ISM comps (unusual), route each EDIT actor to the comp
        // named in its HISM_COMP_ tag; otherwise everything goes to the first comp.
        TMap<UInstancedStaticMeshComponent *, TArray<FTransform>> PerComp;
        for (AActor *E : Edits)
        {
            UInstancedStaticMeshComponent *Target = Comps[0];
            const FString WantComp = HISMIE::ActorCompTag(E);
            if (!WantComp.IsEmpty())
                for (UInstancedStaticMeshComponent *C : Comps)
                    if (C->GetFName().ToString() == WantComp)
                    {
                        Target = C;
                        break;
                    }
            PerComp.FindOrAdd(Target).Add(E->GetActorTransform());
        }

        Host->Modify();
        for (auto &CP : PerComp)
        {
            UInstancedStaticMeshComponent *Comp = CP.Key;
            Comp->Modify();
            Comp->ClearInstances();
            for (const FTransform &T : CP.Value)
                Comp->AddInstance(T, /*bWorldSpace*/ true);
            Comp->MarkRenderStateDirty();
            Comp->MarkPackageDirty();
            PackedInstances += CP.Value.Num();
        }
        Host->MarkPackageDirty();
        Host->SetIsTemporarilyHiddenInEditor(false);
        HostsToSelect.Add(Host);
        ++PackedHosts;

        for (AActor *E : Edits)
            World->DestroyActor(E);
    }

    GEditor->SelectNone(/*bNoteSelectionChange*/ true, /*bDeselectBSPSurfs*/ false);
    for (AActor *H : HostsToSelect)
        GEditor->SelectActor(H, true, false);
    GEditor->NoteSelectionChange();

    UE_LOG(LogHISMInstanceEditor, Log, TEXT("[Pack] %d hosts updated with %d instances. EDIT_ actors removed, hosts unhidden."),
           PackedHosts, PackedInstances);
    return PackedHosts > 0;
}


//----------------------------------------------------------------------
// Actor -> HISM converter (rocks, ice, etc.)
//----------------------------------------------------------------------

namespace HISMIE
{
    static constexpr float CellSize = 15600.f; // World Partition MainGrid cell size (uu)

    // FBodyInstance::PhysMaterialOverride is protected in C++; read via reflection
    // (same path python's get_editor_property uses).
    static UPhysicalMaterial *ReadPhysOverride(UStaticMeshComponent *SMC)
    {
        const FBodyInstance *BI = SMC ? SMC->GetBodyInstance() : nullptr;
        if (!BI)
            return nullptr;
        if (const FObjectPropertyBase *Prop =
                CastField<FObjectPropertyBase>(FBodyInstance::StaticStruct()->FindPropertyByName(TEXT("PhysMaterialOverride"))))
        {
            return Cast<UPhysicalMaterial>(Prop->GetObjectPropertyValue_InContainer(BI));
        }
        return nullptr;
    }

    struct FConvertCandidate
    {
        AActor *Actor = nullptr;
        UStaticMesh *Mesh = nullptr;
        UPhysicalMaterial *Phys = nullptr;
        FTransform World;
        TArray<UMaterialInterface *> Mats;
        int32 CellX = 0, CellY = 0;
    };

    static bool CollectCandidate(AActor *Actor, FConvertCandidate &Out)
    {
        if (!Actor || Actor->IsA<AInstanceMarker>())
            return false;
        if (Actor->GetClass()->GetFName() == TEXT("BP_HISMHost_C"))
            return false;

        UStaticMeshComponent *SMC = nullptr;
        TArray<UStaticMeshComponent *> Comps;
        Actor->GetComponents<UStaticMeshComponent, FDefaultAllocator>(Comps);
        for (UStaticMeshComponent *C : Comps)
            if (C && C->GetStaticMesh())
            {
                SMC = C;
                break;
            }
        if (!SMC)
            return false;

        Out.Actor = Actor;
        Out.Mesh = SMC->GetStaticMesh();
        Out.World = Actor->GetActorTransform();
        Out.Phys = ReadPhysOverride(SMC);
        const int32 NumSlots = SMC->GetNumMaterials();
        for (int32 Slot = 0; Slot < NumSlots; ++Slot)
            if (UMaterialInterface *M = SMC->GetMaterial(Slot))
                Out.Mats.Add(M);

        const FVector Center = Actor->GetActorLocation();
        Out.CellX = FMath::FloorToInt(Center.X / CellSize);
        Out.CellY = FMath::FloorToInt(Center.Y / CellSize);
        return true;
    }
} // namespace HISMIE

int32 UHISMInstanceEditorLibrary::ConvertActorsToHISM(const TArray<AActor *> &Actors)
{
    using namespace HISMIE;

    UWorld *World = GetEditorWorld();
    if (!World)
        return 0;

    // 1. collect + group by (cell, mesh, phys)
    struct FGroup
    {
        UStaticMesh *Mesh = nullptr;
        UPhysicalMaterial *Phys = nullptr;
        int32 CX = 0, CY = 0;
        TArray<FConvertCandidate> Items;
        FVector Sum = FVector::ZeroVector;
    };
    TMap<FString, FGroup> Groups;

    int32 Considered = 0;
    for (AActor *A : Actors)
    {
        FConvertCandidate C;
        if (!CollectCandidate(A, C))
            continue;
        ++Considered;

        const FString Key = FString::Printf(TEXT("%d|%d|%s|%s"), C.CellX, C.CellY,
                                            *C.Mesh->GetPathName(),
                                            C.Phys ? *C.Phys->GetPathName() : TEXT("none"));
        FGroup &G = Groups.FindOrAdd(Key);
        G.Mesh = C.Mesh;
        G.Phys = C.Phys;
        G.CX = C.CellX;
        G.CY = C.CellY;
        G.Sum += C.World.GetTranslation();
        G.Items.Add(C);
    }

    UE_LOG(LogHISMInstanceEditor, Log, TEXT("[Convert] considered=%d groups=%d"), Considered, Groups.Num());
    if (Groups.Num() == 0)
        return 0;

    // 2. host BP per (mesh, phys)
    const FString HostDir = TEXT("/Game/WinterHunt/Blueprints/Envirement/HISM");
    UEditorAssetLibrary::MakeDirectory(HostDir);

    // 3. create hosts, fill, delete originals
    int32 HostsCreated = 0;
    for (auto &Pair : Groups)
    {
        FGroup &G = Pair.Value;
        const FString MeshName = G.Mesh->GetName();
        FString BPPath = FString::Printf(TEXT("%s/BP_HISMHost_%s"), *HostDir, *MeshName);
        if (G.Phys)
            BPPath += FString::Printf(TEXT("_%s"), *G.Phys->GetName());

        if (!CreateHISMHostBlueprint(BPPath))
        {
            UE_LOG(LogHISMInstanceEditor, Error, TEXT("[Convert] host BP failed: %s"), *BPPath);
            continue;
        }
        if (!SetHISMHostTemplateDefaults(BPPath, G.Mesh,
                                         G.Phys ? G.Phys->GetPathName() : FString()))
        {
            UE_LOG(LogHISMInstanceEditor, Error, TEXT("[Convert] template defaults failed: %s"), *BPPath);
            continue;
        }
        UBlueprint *HostBP = LoadObject<UBlueprint>(nullptr, *BPPath);
        UClass *HostClass = HostBP ? HostBP->GeneratedClass : nullptr;
        if (!HostClass)
        {
            UE_LOG(LogHISMInstanceEditor, Error, TEXT("[Convert] no generated class: %s"), *BPPath);
            continue;
        }

        const int32 N = G.Items.Num();
        const FVector Centroid = G.Sum / N;

        AActor *Host = World->SpawnActor<AActor>(HostClass, Centroid, FRotator::ZeroRotator);
        if (!Host)
        {
            UE_LOG(LogHISMInstanceEditor, Error, TEXT("[Convert] spawn failed for %s"), *BPPath);
            continue;
        }
        Host->SetActorLabel(FString::Printf(TEXT("HISM_%s_X%d_Y%d"), *MeshName, G.CX, G.CY));
        Host->SetFolderPath(FName(TEXT("HISM Converted")));

        UHierarchicalInstancedStaticMeshComponent *HISM = nullptr;
        TArray<UHierarchicalInstancedStaticMeshComponent *> Comps;
        Host->GetComponents<UHierarchicalInstancedStaticMeshComponent, FDefaultAllocator>(Comps);
        if (Comps.Num() > 0)
            HISM = Comps[0];
        if (!HISM)
        {
            UE_LOG(LogHISMInstanceEditor, Error, TEXT("[Convert] no HISM comp on host for %s"), *MeshName);
            World->DestroyActor(Host);
            continue;
        }

        // override materials (from the first candidate that has them)
        for (const FConvertCandidate &C : G.Items)
        {
            if (C.Mats.Num() == 0)
                continue;
            HISM->Modify();
            for (int32 Slot = 0; Slot < C.Mats.Num(); ++Slot)
                if (C.Mats[Slot])
                    HISM->SetMaterial(Slot, C.Mats[Slot]);
            break;
        }

        // instances (world space); mesh+phys already on the SCS template
        bool bAllAdded = true;
        for (const FConvertCandidate &C : G.Items)
            if (HISM->AddInstance(C.World, /*bWorldSpace*/ true) < 0)
                bAllAdded = false;
        HISM->MarkRenderStateDirty();

        const int32 Got = HISM->GetInstanceCount();
        if (!bAllAdded || Got != N)
        {
            UE_LOG(LogHISMInstanceEditor, Error, TEXT("[Convert] instance mismatch for %s (%d/%d)"),
                   *MeshName, Got, N);
            World->DestroyActor(Host);
            continue;
        }

        // originals out
        for (const FConvertCandidate &C : G.Items)
            World->DestroyActor(C.Actor);

        ++HostsCreated;
        UE_LOG(LogHISMInstanceEditor, Log, TEXT("[Convert] host '%s' <- %d instances (mesh %s, phys %s)"),
               *Host->GetActorLabel(), N, *MeshName, G.Phys ? *G.Phys->GetName() : TEXT("none"));
    }

    UE_LOG(LogHISMInstanceEditor, Log, TEXT("[Convert] DONE: %d hosts created from %d actors"),
           HostsCreated, Considered);
    return HostsCreated;
}

int32 UHISMInstanceEditorLibrary::ConvertSelectedActorsToHISM()
{
    TArray<AActor *> Selected;
    if (GEditor)
    {
        USelection *Sel = GEditor->GetSelectedActors();
        const int32 Num = Sel ? Sel->Num() : 0;
        for (int32 i = 0; i < Num; ++i)
            if (AActor *A = Cast<AActor>(Sel->GetSelectedObject(i)))
                Selected.Add(A);
    }
    if (Selected.Num() == 0)
    {
        UE_LOG(LogHISMInstanceEditor, Warning, TEXT("[Convert] selection is empty"));
        return 0;
    }
    return ConvertActorsToHISM(Selected);
}

#undef LOCTEXT_NAMESPACE
