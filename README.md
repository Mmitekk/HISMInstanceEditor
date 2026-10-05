# HISMInstanceEditor

Editor-плагин для Unreal Engine 5.2: редактирование отдельных инстансов `UInstancedStaticMeshComponent` / `UHierarchicalInstancedStaticMeshComponent` прямо во вьюпорте.

Главная фишка — режим **как у Packed Level Actor**: распаковал хост в обычные актёры → подёргал каждый сугроб обычным гизмо → запаковал обратно.

## Возможности

- **Unpack / Pack (как PLA)** — правый клик по хосту:
  - `HISM: Unpack instances for editing (like PLA)` — каждый инстанс становится актёром `EDIT_<хост>_<i>` в папке `HISM Edit/<хост>` (меш, материалы, физмат и коллизия копируются). Хосты прячутся, как packed-вид у PLA.
  - `HISM: Pack edited instances back (like PLA)` — трансформы `EDIT_` актёров записываются обратно **в те же хосты** (акторы хостов сохраняются → World Partition не ломается), `EDIT_` удаляются, хосты показываются.
  - Работает от выделения `EDIT_` актёров, хостов или вообще без выделения (берутся все `EDIT_` на уровне). Операции в транзакциях (Ctrl+Z).
- **Маркерный инструмент** (альтернатива Instance Tool):
  - `HISM: Spawn instance marker here` — жёлтый маркер `INSTANCE MARKER`.
  - По выбранному маркеру: `Move nearest instance here`, `Duplicate nearest instance here`, `Rotate nearest instance 15°/90°`, `Delete nearest instance`. Ищется ближайший инстанс по всем ISM/HISM уровня.
- **Библиотека** `UHISMInstanceEditorLibrary` (BlueprintCallable): `FindNearestInstance`, `Move/Rotate/Delete/Duplicate` (по компоненту и глобальные), `GetInstanceWorldTransform`, плюс `UnpackHISMHostsToEditActors` / `RepackHISMHostsFromEditActors`.
- **Фабрика `BP_HISMHost`**: `CreateHISMHostBlueprint` + `SetHISMHostTemplateDefaults` — блюпринт-хост с HISM-компонентом в руте, чтобы параметры и материалы крутились в Details.

## Установка

1. Скопировать папку плагина в `<Project>/Plugins/HISMInstanceEditor`.
2. Убедиться, что в `.uproject` плагин включён (`"Name": "HISMInstanceEditor", "Enabled": true`).
3. Пересобрать `Development Editor` (Visual Studio или `Build.bat`). Отдельных зависимостей, кроме `EditorScriptingUtilities`, нет.

Требование: **Unreal Engine 5.2**.

## Типовой сценарий

1. Выделить хосты `HISM_*` → Unpack.
2. Двигать `EDIT_` актёры гизмо (move / rotate / scale по одному).
3. Pack → проверить визуально / PIE → сохранить уровень вручную.
4. Не держать сугробы через границу WP-ячейки хоста; не двигать сам хост (у него мировой ноль, сдвиг уедет всё сразу).

## Состав

```
HISMInstanceEditor.uplugin
Source/HISMInstanceEditor/
  HISMInstanceEditor.Build.cs
  Public/HISMInstanceEditor.h/.cpp            — пункты контекстного меню
  Public/HISMInstanceEditorLibrary.h/.cpp     — вся логика (Unpack/Pack, маркеры, фабрика BP)
  Public/InstanceMarker.h/.cpp                — актёр-маркер
```

Binaries/Intermediate в репозиторий не коммитятся — собираются локально.

© Olkon Games.
