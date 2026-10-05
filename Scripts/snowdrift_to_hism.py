# -*- coding: utf-8 -*-
# Convert snowdrift StaticMeshActors (PhysMaterialOverride == Snowdrift ONLY) into
# HISM actors, grouped by (World Partition cell, mesh type) so instances never span
# WP cells and stream correctly.
#
# Run inside the UE 5.2 editor:  py "F:/z.ai/GameDev/tmp/snowdrift_to_hism.py"
#
# NO undo transaction (transactions over hundreds of World Partition external actors
# freeze the editor). Safety instead: the script does NOT save - review the result
# and save manually, or just close without saving.
#
# NOTE: CELL_SIZE must match the World Partition grid (read from Main_LVL.umap: MainGrid, 15600).
import math
import time
import unreal

TAG = "[SNOWDRIFT->HISM]"
PHYS_PATH = "/Game/WinterHunt/Materials/Phys/Snowdrift"
CELL_SIZE = 15600.0          # World Partition MainGrid cell size (uu) - update if WP settings change
HOST_LABEL_PREFIX = "HISM_Snowdrift_"
OUTLINER_FOLDER = "HISM Snowdrift"

T0 = time.time()

def log(msg):
    unreal.log_warning("%s [+%.1fs] %s" % (TAG, time.time() - T0, msg))

def read_override(comp):
    try:
        bi = comp.get_editor_property("body_instance")
        return bi.get_editor_property("phys_material_override")
    except Exception:
        try:
            return comp.get_editor_property("phys_material_override")
        except Exception:
            return None

def write_override(comp, mat):
    try:
        bi = comp.get_editor_property("body_instance")
        bi.set_editor_property("phys_material_override", mat)
        comp.set_editor_property("body_instance", bi)
        return True
    except Exception:
        try:
            comp.set_editor_property("phys_material_override", mat)
            return True
        except Exception:
            return False

def instance_count(ism):
    try:
        return len(ism.get_editor_property("per_instance_sm_data"))
    except Exception:
        return -1

def add_instance_safe(ism, transform):
    try:
        return ism.add_instance(transform, False)
    except Exception:
        return ism.add_instance(transform)

def bounds_center(a):
    try:
        origin, extent = a.get_actor_bounds(False)
        return origin
    except Exception:
        return a.get_actor_location()

phys = unreal.load_asset(PHYS_PATH, unreal.PhysicalMaterial)
if phys is None:
    log("FATAL: phys material not found: " + PHYS_PATH)
    raise SystemExit(1)
phys_path = phys.get_path_name()

actors = unreal.EditorLevelLibrary.get_all_level_actors()
log("total actors in level: %d" % len(actors))

# ---------- 0. clean up stray hosts from previous failed runs ----------
strays = []
for a in actors:
    try:
        if a.get_actor_label().startswith(HOST_LABEL_PREFIX):
            strays.append(a)
    except Exception:
        pass
if strays:
    log("removing %d stray host actors from previous runs" % len(strays))
    for a in strays:
        unreal.EditorLevelLibrary.destroy_actor(a)
    log("strays removed")
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    log("total actors after cleanup: %d" % len(actors))

# ---------- 1. collect candidates: Snowdrift override ONLY ----------
candidates = []
other_overrides = 0
skipped_nomesh = 0
for a in actors:
    try:
        comps = a.get_components_by_class(unreal.StaticMeshComponent)
    except Exception:
        comps = []
    matched = None
    for c in comps:
        ov = read_override(c)
        if ov is not None and ov.get_path_name() == phys_path:
            matched = c
            break
    if matched is None:
        for c in comps:
            ov = read_override(c)
            if ov is not None:
                other_overrides += 1
                break
        continue
    try:
        mesh = matched.get_editor_property("static_mesh")
    except Exception:
        mesh = None
    if mesh is None:
        skipped_nomesh += 1
        log("SKIP (no static mesh): '%s' class=%s" % (a.get_actor_label(), a.get_class().get_name()))
        continue
    try:
        mats = [m for m in matched.get_editor_property("override_materials") if m]
    except Exception:
        mats = []
    center = bounds_center(a)
    candidates.append((a, mesh, a.get_actor_transform(), mats, center))

log("SNOWDRIFT candidates: %d (untouched actors with other phys overrides: %d, no-mesh skips: %d)"
    % (len(candidates), other_overrides, skipped_nomesh))
if not candidates:
    log("nothing to convert")
    raise SystemExit(0)

# ---------- 2. group by (WP cell, mesh) ----------
groups = {}
mesh_counter = {}
for a, mesh, t, mats, center in candidates:
    mp = mesh.get_path_name()
    mesh_counter[mp] = mesh_counter.get(mp, 0) + 1
    cx = int(math.floor(center.x / CELL_SIZE))
    cy = int(math.floor(center.y / CELL_SIZE))
    groups.setdefault((cx, cy, mp), []).append((a, mesh, t, mats, center))

log("distinct meshes: %d, groups by (cell, mesh): %d" % (len(mesh_counter), len(groups)))
for mp, cnt in sorted(mesh_counter.items()):
    log("  %4d x %s" % (cnt, mp))

# ---------- 3. convert (NO transaction - review + manual save instead) ----------
created = []
for gi, ((cx, cy, mp), items) in enumerate(sorted(groups.items()), 1):
    mesh = items[0][1]
    mesh_name = mesh.get_name()

    host = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    if host is None:
        log("FATAL: could not spawn host actor")
        raise SystemExit(1)
    host.set_actor_label("%s%s_X%d_Y%d" % (HOST_LABEL_PREFIX, mesh_name, cx, cy))
    try:
        host.set_editor_property("folder_path", OUTLINER_FOLDER)
    except Exception:
        pass

    ism = unreal.HierarchicalInstancedStaticMeshComponent(outer=host, name="HISMComp_" + mesh_name)
    ism.set_editor_property("static_mesh", mesh)
    try:
        ism.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    except Exception:
        pass
    try:
        ism.set_collision_profile_name("BlockAll")
    except Exception:
        log("WARN: could not set collision profile for cell %d_%d" % (cx, cy))
    if not write_override(ism, phys):
        log("FATAL: could not write Snowdrift override onto HISM (cell %d_%d)" % (cx, cy))
        raise SystemExit(1)

    mats_for_hism = next((m[3] for m in items if m[3]), None)
    if mats_for_hism:
        ok = False
        try:
            ism.set_editor_property("override_materials", mats_for_hism)
            ok = True
        except Exception:
            pass
        if not ok:
            try:
                for mi, mm in enumerate(mats_for_hism):
                    if mm:
                        ism.set_material(mi, mm)
                ok = True
            except Exception as e:
                log("WARN: could not copy override materials for cell %d_%d: %r" % (cx, cy, e))

    # instances in world coordinates; host sits at origin so local == world (E2E-proven path)
    for a, m, t, mats, center in items:
        add_instance_safe(ism, t)

    # HISM becomes the actor ROOT - this is what makes it persist through save (E2E-verified)
    try:
        host.set_editor_property("root_component", ism)
    except Exception as e:
        log("FATAL: root_component swap failed: %r" % e)
        raise SystemExit(1)

    got = instance_count(ism)
    if got >= 0 and got != len(items):
        log("FATAL: instance count mismatch in cell %d_%d (expected %d, got %d). Aborting before deleting originals."
            % (cx, cy, len(items), got))
        raise SystemExit(1)
    created.append((host, len(items)))
    log("group %d/%d: cell X%d Y%d, %s: %d instances" % (gi, len(groups), cx, cy, mesh_name, len(items)))

log("all groups created: %d HISM actors, %d instances (%.1fs)" % (len(created), sum(c for _, c in created), time.time() - T0))

# ---------- 4. delete originals ----------
deleted = 0
for a, mesh, t, mats, center in candidates:
    unreal.EditorLevelLibrary.destroy_actor(a)
    deleted += 1
    if deleted % 25 == 0:
        log("deleted %d/%d originals" % (deleted, len(candidates)))
log("deleted original actors: %d (%.1fs)" % (deleted, time.time() - T0))

try:
    unreal.EditorLevelLibrary.set_selected_level_actors([h for h, _ in created])
except Exception:
    pass
log("CREATED %d HISM actors with %d instances total" % (len(created), sum(c for _, c in created)))
log("NOT SAVED - review, run PIE, then save manually. If anything is wrong: close WITHOUT saving.")
log("DONE")
