# -*- coding: utf-8 -*-
# PLA-like edit STEP 2 (Pack): EDIT_ actors -> back into their HISM hosts (in place).
# Run inside the UE 5.2 editor:  py "F:/z.ai/GameDev/tmp/hism_repack_edit.py"
# Works from: selected EDIT_ actors, selected hosts, or all EDIT_ actors in level.
# Mapping is by outliner folder "HISM Edit/<host label>" (same as hism_unpack_edit.py).
# Hosts are preserved (World Partition cells stay valid). EDIT_ actors are deleted.
# Does NOT save - review, then save manually. Close WITHOUT saving to revert.
import unreal

TAG = "[HISM-PACK]"
EDIT_FOLDER_ROOT = "HISM Edit"

def log(msg):
    unreal.log_warning("%s %s" % (TAG, msg))

def folder_of(a):
    try:
        return str(a.get_editor_property("folder_path"))
    except Exception:
        return ""

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

def clear_comp(comp):
    for meth in ("clear_instances",):
        try:
            getattr(comp, meth)()
            return True
        except Exception:
            pass
    try:
        comp.call_method("ClearInstances")
        return True
    except Exception as e:
        log("clear fallback remove-loop: %r" % e)
    n = real_count(comp)
    ok = True
    for i in range(n - 1, -1, -1):
        try:
            comp.remove_instance(i)
        except Exception:
            try:
                comp.call_method("RemoveInstance", args=(i,))
            except Exception as e2:
                log("WARN remove %d: %r" % (i, e2))
                ok = False
    return ok

actors = unreal.EditorLevelLibrary.get_all_level_actors()
sel = unreal.EditorLevelLibrary.get_selected_level_actors()
sel_names = set()
for a in sel:
    try:
        sel_names.add(a.get_actor_label())
    except Exception:
        pass

# collect EDIT actors: selection first, else whole level
edits = []
if sel:
    for a in sel:
        try:
            if a.get_actor_label().startswith("EDIT_") or folder_of(a).startswith(EDIT_FOLDER_ROOT + "/"):
                edits.append(a)
        except Exception:
            pass
if not edits:
    for a in actors:
        try:
            if folder_of(a).startswith(EDIT_FOLDER_ROOT + "/"):
                edits.append(a)
        except Exception:
            pass
log("EDIT_ actors to pack: %d" % len(edits))
if not edits:
    log("nothing to pack - run hism_unpack_edit.py first")
    raise SystemExit(0)

# group by host label (folder suffix)
groups = {}
orphans = []
for e in edits:
    f = folder_of(e)
    host_label = f[len(EDIT_FOLDER_ROOT) + 1:] if f.startswith(EDIT_FOLDER_ROOT + "/") else ""
    if not host_label and e.get_actor_label().startswith("EDIT_"):
        # fallback: single selected host owns them
        hosts_sel = [a for a in sel if not folder_of(a).startswith(EDIT_FOLDER_ROOT)]
        if len(hosts_sel) == 1:
            try:
                host_label = hosts_sel[0].get_actor_label()
            except Exception:
                pass
    if not host_label:
        orphans.append(e)
        continue
    groups.setdefault(host_label, []).append(e)
if orphans:
    log("WARN: %d EDIT_ actors without host folder, skipped" % len(orphans))

packed_hosts = 0
packed_inst = 0
packed_host_objs = []
for host_label, items in sorted(groups.items()):
    host = None
    for a in actors:
        try:
            if a.get_actor_label() == host_label and a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
                host = a
                break
        except Exception:
            pass
    if host is None:
        log("WARN: host not found for '%s' (%d edits skipped)" % (host_label, len(items)))
        continue
    comp = host.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)[0]
    items_sorted = sorted(items, key=lambda x: x.get_actor_label())
    ts = [e.get_actor_transform() for e in items_sorted]
    clear_comp(comp)
    for t in ts:
        try:
            comp.add_instance(t, True)
        except Exception:
            comp.add_instance(t)
    got = real_count(comp)
    if got != len(ts):
        log("WARN '%s': count %s != %d" % (host_label, got, len(ts)))
    try:
        host.call_method("SetIsTemporarilyHiddenInEditor", args=(False,))
    except Exception:
        pass
    for e in items_sorted:
        unreal.EditorLevelLibrary.destroy_actor(e)
    packed_hosts += 1
    packed_inst += len(ts)
    packed_host_objs.append(host)
    log("packed: %s (%d inst)" % (host_label, len(ts)))

try:
    unreal.EditorLevelLibrary.set_selected_level_actors(packed_host_objs)
except Exception:
    pass
log("PACKED %d hosts, %d instances. EDIT_ actors removed, hosts unhidden." % (packed_hosts, packed_inst))
log("NOT SAVED - review (PIE + visual check), then save manually. Close WITHOUT saving to revert.")
log("DONE")
