# -*- coding: utf-8 -*-
# PLA-like edit STEP 1 (Unpack): selected HISM hosts -> individual EDIT_ actors.
# Run inside the UE 5.2 editor:  py "F:/z.ai/GameDev/tmp/hism_unpack_edit.py"
# Select 1+ HISM_Snowdrift_* hosts first (outliner), then run.
# Result: folder "HISM Edit/<host>" with EDIT_<host>_<i> actors you move with the
# normal gizmo (like Packed Level Actor edit mode). Hosts are hidden while editing.
# Does NOT save - review, then save manually. Close WITHOUT saving to revert.
import unreal

TAG = "[HISM-UNPACK]"
EDIT_FOLDER_ROOT = "HISM Edit"

def log(msg):
    unreal.log_warning("%s %s" % (TAG, msg))

def real_count(ism):
    best = -1
    try:
        best = max(best, len(ism.get_editor_property("per_instance_sm_data")))
    except Exception:
        pass
    try:
        best = max(best, ism.call_method("GetInstanceCount"))
    except Exception:
        pass
    return best

def get_phys(comp):
    try:
        return comp.get_editor_property("body_instance").get_editor_property("phys_material_override")
    except Exception:
        return None

def set_phys(comp, mat):
    try:
        bi = comp.get_editor_property("body_instance")
        bi.set_editor_property("phys_material_override", mat)
        comp.set_editor_property("body_instance", bi)
        return True
    except Exception:
        return False

sel = unreal.EditorLevelLibrary.get_selected_level_actors()
hosts = []
for a in sel:
    try:
        comps = a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
    except Exception:
        comps = []
    if comps:
        hosts.append(a)
log("selected hosts: %d" % len(hosts))
if not hosts:
    log("select 1+ HISM host actors first (e.g. HISM_Snowdrift_*), then re-run")
    raise SystemExit(0)

created = []
for host in hosts:
    label = host.get_actor_label()
    try:
        comps = host.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
    except Exception:
        comps = []
    for comp in comps:
        n = real_count(comp)
        if n <= 0:
            continue
        try:
            mesh = comp.get_editor_property("static_mesh")
        except Exception:
            mesh = None
        if mesh is None:
            log("SKIP (no mesh): %s" % label)
            continue
        try:
            mats = [m for m in comp.get_editor_property("override_materials")]
        except Exception:
            mats = []
        try:
            prof = comp.get_collision_profile_name()
        except Exception:
            prof = "BlockAll"
        phys = get_phys(comp)
        folder = "%s/%s" % (EDIT_FOLDER_ROOT, label)
        for i in range(n):
            t = comp.get_instance_transform(i, True)
            loc = t.translation
            rot = t.rotation.rotator() if hasattr(t.rotation, "rotator") else unreal.Rotator(0, 0, 0)
            try:
                rot = t.rotation.rotator()
            except Exception:
                pass
            new_a = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)
            if new_a is None:
                log("FATAL: spawn failed for %s #%d" % (label, i))
                raise SystemExit(1)
            try:
                new_a.set_actor_scale3d(t.scale3d)
            except Exception as e:
                log("WARN scale %s #%d: %r" % (label, i, e))
            try:
                sm = new_a.get_components_by_class(unreal.StaticMeshComponent)[0]
                sm.set_editor_property("static_mesh", mesh)
                try:
                    sm.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
                except Exception:
                    pass
                if mats:
                    try:
                        sm.set_editor_property("override_materials", mats)
                    except Exception:
                        pass
                try:
                    sm.set_collision_profile_name(prof)
                except Exception:
                    pass
                if phys is not None:
                    set_phys(sm, phys)
            except Exception as e:
                log("WARN setup %s #%d: %r" % (label, i, e))
            new_a.set_actor_label("EDIT_%s_%d" % (label, i))
            try:
                new_a.set_editor_property("folder_path", folder)
            except Exception:
                pass
            created.append(new_a)
    try:
        host.call_method("SetIsTemporarilyHiddenInEditor", args=(True,))
    except Exception:
        log("WARN: could not hide host '%s' - hide it with the eye icon manually" % label)

try:
    unreal.EditorLevelLibrary.set_selected_level_actors(created)
except Exception:
    pass
log("UNPACKED %d hosts -> %d EDIT_ actors. Move them with the gizmo, then run hism_repack_edit.py" % (len(hosts), len(created)))
log("NOT SAVED - save manually when happy. Close WITHOUT saving to revert.")
log("DONE")
