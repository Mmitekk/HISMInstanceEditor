# -*- coding: utf-8 -*-
# Migrate native HISM hosts onto BP_HISMHost (v3: per-host method cascade, no batch abort).
#
# Run inside the UE 5.2 editor:  py "F:/z.ai/GameDev/tmp/hism_hosts_to_bp.py"
# Does NOT save - review, then save manually. Close WITHOUT saving to revert.
import unreal

TAG = "[HISM->BP-HOST]"
HOST_BP = "/Game/WinterHunt/Blueprints/Envirement/BP_HISMHost"
PHYS_PATH = "/Game/WinterHunt/Materials/Phys/Snowdrift"
HOST_LABEL_PREFIX = "HISM_Snowdrift_"
OUTLINER_FOLDER = "HISM Snowdrift"

def log(msg):
    unreal.log_warning("%s %s" % (TAG, msg))

def cnt_data(ism):
    try:
        return len(ism.get_editor_property("per_instance_sm_data"))
    except Exception:
        return -1

def cnt_api(ism):
    try:
        return ism.call_method("GetInstanceCount")
    except Exception:
        return None

def real_count(ism):
    a = cnt_data(ism)
    b = cnt_api(ism)
    return max(x for x in (a, b) if x is not None) if (a is not None or b is not None) else -1

LIB = unreal.HISMInstanceEditorLibrary

# ---------- 1. BP host + template defaults ----------
if not unreal.EditorAssetLibrary.does_asset_exist(HOST_BP):
    if not LIB.create_hism_host_blueprint(HOST_BP):
        log("FATAL: cannot create host BP")
        raise SystemExit(1)
if not LIB.set_hism_host_template_defaults(HOST_BP, None, PHYS_PATH):
    log("FATAL: cannot set template defaults")
    raise SystemExit(1)
bp = unreal.load_asset(HOST_BP, unreal.Blueprint)
cls = bp.generated_class()
bp_class_name = cls.get_name()
log("host BP class: %s" % bp_class_name)

# ---------- 2. cleanup empty BP hosts ----------
actors = unreal.EditorLevelLibrary.get_all_level_actors()
cleaned = 0
for a in actors:
    try:
        if a.get_class().get_name() == bp_class_name and a.get_actor_label().startswith(HOST_LABEL_PREFIX):
            comps = a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
            if not comps or real_count(comps[0]) == 0:
                log("cleanup: removing empty BP host '%s'" % a.get_actor_label())
                unreal.EditorLevelLibrary.destroy_actor(a)
                cleaned += 1
    except Exception:
        pass
if cleaned:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()

# ---------- 3. collect native hosts ----------
hosts = []
for a in actors:
    try:
        label = a.get_actor_label()
    except Exception:
        continue
    if label.startswith(HOST_LABEL_PREFIX) and a.get_class().get_name() == "StaticMeshActor":
        hosts.append(a)
log("native HISM hosts to migrate: %d" % len(hosts))
if not hosts:
    log("nothing to migrate")
    raise SystemExit(0)

# ---------- 4. per-host migration with method cascade ----------
def try_method(letter, world_ts, mesh, mats, label):
    """Spawn a fresh BP host and try one fill method. Returns dst host or None."""
    host = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    if host is None:
        return None
    d = host.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)[0]
    try:
        if letter == "A":   # instances first, mesh after (run-1 order)
            for t in world_ts:
                d.add_instance(t, True)
            d.set_editor_property("static_mesh", mesh)
        elif letter == "B":  # mesh first, instances after
            d.set_editor_property("static_mesh", mesh)
            for t in world_ts:
                d.add_instance(t, True)
        elif letter == "C":  # mesh via python call, instances via call_method AddInstance
            d.call_method("SetStaticMesh", args=(mesh,))
            for t in world_ts:
                d.call_method("AddInstance", args=(t, True))
        elif letter == "D":  # mesh first, batch AddInstances
            d.set_editor_property("static_mesh", mesh)
            d.call_method("AddInstances", args=(world_ts, True))
    except Exception as e:
        log("  method %s exception on '%s': %r" % (letter, label, e))
        unreal.EditorLevelLibrary.destroy_actor(host)
        return None

    got = real_count(d)
    if got != len(world_ts):
        log("  method %s on '%s': count %s != %s" % (letter, label, got, len(world_ts)))
        unreal.EditorLevelLibrary.destroy_actor(host)
        return None
    # materials
    if mats:
        try:
            d.set_editor_property("override_materials", mats)
        except Exception:
            try:
                for mi, mm in enumerate(mats):
                    if mm:
                        d.set_material(mi, mm)
            except Exception as e:
                log("  WARN materials '%s': %r" % (label, e))
    return host

migrated = 0
skipped = []
method_stats = {}
for old in hosts:
    label = old.get_actor_label()
    old_isms = old.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
    if not old_isms:
        log("SKIP (no HISM comp): %s" % label)
        continue
    src = old_isms[0]
    mesh = src.get_editor_property("static_mesh")
    if mesh is None:
        log("SKIP (no mesh): %s" % label)
        continue
    try:
        mats = [m for m in src.get_editor_property("override_materials") if m]
    except Exception:
        mats = []
    n = real_count(src)
    if n <= 0:
        log("SKIP (empty): %s" % label)
        continue
    world_ts = [src.get_instance_transform(i, True) for i in range(n)]

    done = None
    for letter in ("B", "A", "C", "D"):
        host = try_method(letter, world_ts, mesh, mats, label)
        if host is not None:
            done = (host, letter)
            break
    if done is None:
        log("WARN: could not migrate '%s' (kept native) - mesh=%s" % (label, mesh.get_name()))
        skipped.append(label)
        continue

    host, letter = done
    host.set_actor_label(label)
    try:
        host.set_editor_property("folder_path", OUTLINER_FOLDER)
    except Exception:
        pass
    unreal.EditorLevelLibrary.destroy_actor(old)
    migrated += 1
    method_stats[letter] = method_stats.get(letter, 0) + 1
    log("migrated %d/%d via %s: %s (%d inst)" % (migrated, len(hosts), letter, label, n))

log("MIGRATED: %d/%d  methods used: %s" % (migrated, len(hosts), method_stats))
if skipped:
    log("LEFT NATIVE (%d): %s" % (len(skipped), ", ".join(skipped)))
try:
    new_actors = unreal.EditorLevelLibrary.get_all_level_actors()
    bp_hosts = [a for a in new_actors if a.get_class().get_name() == bp_class_name]
    unreal.EditorLevelLibrary.set_selected_level_actors(bp_hosts)
except Exception:
    pass
log("NOT SAVED - review, then save manually. Close WITHOUT saving to revert.")
log("DONE")
