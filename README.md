# HISM Instance Editor

[Русский](#русский) | [English](#english)

## Русский

Editor-плагин для Unreal Engine 5.2: редактирование отдельных инстансов `UInstancedStaticMeshComponent` / `UHierarchicalInstancedStaticMeshComponent` прямо во вьюпорте.

Главная фишка — режим **как у Packed Level Actor**: распаковал хост в обычные актёры → подвигал каждый инстанс обычным гизмо → запаковал обратно.

### Возможности

- **Unpack / Pack (как PLA)** — правый клик по хосту:
  - `HISM: Unpack instances for editing (like PLA)` — каждый инстанс становится актёром `EDIT_<хост>_<i>` в папке `HISM Edit/<хост>` (меш, материалы, физмат и коллизия копируются). Хосты прячутся, как packed-вид у PLA.
  - `HISM: Pack edited instances back (like PLA)` — трансформы `EDIT_`-актёров записываются обратно **в те же хосты** (акторы хостов сохраняются → World Partition не ломается), `EDIT_` удаляются, хосты показываются.
  - Работает от выделения `EDIT_`-актёров, хостов или вообще без выделения (берутся все `EDIT_` на уровне). Операции в транзакциях (Ctrl+Z).
- **Конвертер статики в HISM** — `ConvertSelectedActorsToHISM` / `ConvertActorsToHISM`: выделенные `StaticMeshActor` группируются по (WP-ячейка, меш, физмат), под каждую группу создаётся хост-блюпринт с HISM-компонентом, оригиналы удаляются.
- **Маркерный инструмент**:
  - `HISM: Spawn instance marker here` — жёлтый маркер `INSTANCE MARKER`.
  - По выбранному маркеру: `Move nearest instance here`, `Duplicate nearest instance here`, `Rotate nearest instance 15°/90°`, `Delete nearest instance`. Ищется ближайший инстанс по всем ISM/HISM уровня.
- **Библиотека** `UHISMInstanceEditorLibrary` (BlueprintCallable): `FindNearestInstance`, `Move/Rotate/Delete/Duplicate` (по компоненту и глобальные), `GetInstanceWorldTransform`, плюс `UnpackHISMHostsToEditActors` / `RepackHISMHostsFromEditActors`.
- **Фабрика хостов**: `CreateHISMHostBlueprint` + `SetHISMHostTemplateDefaults` — блюпринт-хост с HISM-компонентом в руте, чтобы параметры и материалы правились в Details.

### Установка

1. Скопировать папку плагина в `<Project>/Plugins/HISMInstanceEditor`.
2. Убедиться, что в `.uproject` плагин включён (`"Name": "HISMInstanceEditor", "Enabled": true`).
3. Пересобрать `Development Editor` (Visual Studio или `Build.bat`). Отдельных зависимостей, кроме `EditorScriptingUtilities`, нет.

Требование: **Unreal Engine 5.2** (протестировано). На более новых версиях (5.3+) должен собираться — поменяйте `EngineVersion` в `.uplugin`; если Epic поменял API ISM/HISM, возможны мелкие правки.

### Типовой сценарий

1. Выделить хосты `HISM_*` → Unpack.
2. Двигать `EDIT_`-актёры гизмо (move / rotate / scale — каждый по отдельности).
3. Pack → проверить визуально / PIE → сохранить уровень вручную.
4. Не уносить инстансы за границу WP-ячейки хоста; не двигать сам хост (сдвиг хоста сдвинет все его инстансы сразу).

### Python-скрипты (Scripts/)

Примеры скриптов из прод-проекта (константы путей и фильтров — вверху файлов, под свой проект поправить). Запуск: в Output Log редактора, поле **Cmd**: `py "<путь>\Scripts\<имя>.py"`. Ничего не сохраняют — проверка + ручной сейв.

- `snowdrift_to_hism.py` — статика → нативные HISM-хосты, группировка по (WP-ячейка, меш).
- `hism_hosts_to_bp.py` — нативные хосты → блюпринт-хост.
- `hism_unpack_edit.py` / `hism_repack_edit.py` — Unpack/Pack без пересборки плагина.

### Состав

```
HISMInstanceEditor.uplugin
Source/HISMInstanceEditor/
  HISMInstanceEditor.Build.cs
  Public/HISMInstanceEditor.h/.cpp            — пункты контекстного меню
  Public/HISMInstanceEditorLibrary.h/.cpp     — вся логика
  Public/InstanceMarker.h/.cpp                — актёр-маркер
Scripts/                                      — примеры Python-скриптов
```

Binaries/Intermediate в репозиторий не коммитятся — собираются локально.

## English

Editor plugin for Unreal Engine 5.2: edit individual instances of `UInstancedStaticMeshComponent` / `UHierarchicalInstancedStaticMeshComponent` right in the viewport.

The headline feature is a **Packed Level Actor-like workflow**: unpack a host into regular actors → move each instance with the normal gizmo → pack back.

### Features

- **Unpack / Pack (like PLA)** — right-click a host:
  - `HISM: Unpack instances for editing (like PLA)` — every instance becomes an `EDIT_<host>_<i>` actor in the `HISM Edit/<host>` folder (mesh, materials, physical material and collision are copied). Hosts are hidden, like the packed view of a PLA.
  - `HISM: Pack edited instances back (like PLA)` — `EDIT_` actor transforms are written back **into the same hosts** (host actors are preserved, so World Partition stays valid), `EDIT_` actors are deleted, hosts are unhidden.
  - Works from selected `EDIT_` actors, selected hosts, or with no selection (all `EDIT_` actors in the level). Operations are transacted (Ctrl+Z).
- **Static-to-HISM converter** — `ConvertSelectedActorsToHISM` / `ConvertActorsToHISM`: selected `StaticMeshActor`s are grouped by (WP cell, mesh, physical material); a host Blueprint with a HISM component is created per group and the originals are deleted.
- **Marker tool**:
  - `HISM: Spawn instance marker here` — a yellow `INSTANCE MARKER` actor.
  - With the marker selected: `Move nearest instance here`, `Duplicate nearest instance here`, `Rotate nearest instance 15°/90°`, `Delete nearest instance`. The nearest instance across all ISM/HISM in the level is used.
- **Library** `UHISMInstanceEditorLibrary` (BlueprintCallable): `FindNearestInstance`, `Move/Rotate/Delete/Duplicate` (per-component and global), `GetInstanceWorldTransform`, plus `UnpackHISMHostsToEditActors` / `RepackHISMHostsFromEditActors`.
- **Host factory**: `CreateHISMHostBlueprint` + `SetHISMHostTemplateDefaults` — a host Blueprint with a HISM component as root, so parameters and materials stay editable in Details.

### Installation

1. Copy the plugin folder to `<Project>/Plugins/HISMInstanceEditor`.
2. Make sure the plugin is enabled in `.uproject` (`"Name": "HISMInstanceEditor", "Enabled": true`).
3. Rebuild `Development Editor` (Visual Studio or `Build.bat`). No extra dependencies besides `EditorScriptingUtilities`.

Requirement: **Unreal Engine 5.2** (tested). Should build on newer versions (5.3+) — update `EngineVersion` in `.uplugin`; minor fixes may be needed if Epic changed the ISM/HISM APIs.

### Typical workflow

1. Select `HISM_*` hosts → Unpack.
2. Move `EDIT_` actors with the gizmo (move / rotate / scale — each individually).
3. Pack → visual check / PIE → save the level manually.
4. Don't drag instances across the host's WP cell border; don't move the host itself (moves all its instances at once).

### Python scripts (Scripts/)

Example scripts from a production project (path and filter constants at the top of each file — adjust for your project). Run from the editor's Output Log, **Cmd** box: `py "<path>\Scripts\<name>.py"`. They never save — review, then save manually.

- `snowdrift_to_hism.py` — static meshes → native HISM hosts, grouped by (WP cell, mesh).
- `hism_hosts_to_bp.py` — native hosts → host Blueprint.
- `hism_unpack_edit.py` / `hism_repack_edit.py` — Unpack/Pack without rebuilding the plugin.

### Layout

```
HISMInstanceEditor.uplugin
Source/HISMInstanceEditor/
  HISMInstanceEditor.Build.cs
  Public/HISMInstanceEditor.h/.cpp            — context menu entries
  Public/HISMInstanceEditorLibrary.h/.cpp     — all logic
  Public/InstanceMarker.h/.cpp                — marker actor
Scripts/                                      — example Python scripts
```

Binaries/Intermediate are not committed — build locally.

© Olkon Games.
