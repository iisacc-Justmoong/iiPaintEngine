#include <cstddef>

#include "History/HistorySnapshot.h"
#include "History/HistoryStack.h"
#include "History/UndoRedoController.h"

namespace {

PaintUuid uuidWithFirstByte(std::uint8_t value)
{
    PaintUuid uuid{};
    uuid.bytes[0] = value;
    return uuid;
}

bool byteVectorEquals(const std::vector<std::byte> &bytes, std::initializer_list<unsigned int> expected)
{
    if (bytes.size() != expected.size()) {
        return false;
    }

    std::size_t index = 0;
    for (const unsigned int expectedByte : expected) {
        if (std::to_integer<unsigned int>(bytes[index]) != expectedByte) {
            return false;
        }
        ++index;
    }
    return true;
}

Command makeLayerPixelCommand(PaintUuid targetId, std::string label)
{
    Command command;
    command.kind = CommandKind::PaintStroke;
    command.scope = CommandScope::Layer;
    command.label = std::move(label);
    command.targetId = targetId;
    command.transactionId = uuidWithFirstByte(99);
    command.timestamp = 12.5;
    command.coalescingKey = "layer:ink:pixel";
    command.dirtyBounds = {{1.0, 2.0}, 3.0, 4.0};

    CommandPatch patch;
    patch.scope = CommandScope::Layer;
    patch.targetId = targetId;
    patch.dirtyBounds = command.dirtyBounds;
    patch.beforeState.storage = CommandPayloadStorage::InlineBytes;
    patch.beforeState.mimeType = "application/x-iipaint-rgba-tile";
    patch.beforeState.bytes = {std::byte{0x00}, std::byte{0x01}};
    patch.afterState.storage = CommandPayloadStorage::InlineBytes;
    patch.afterState.mimeType = "application/x-iipaint-rgba-tile";
    patch.afterState.bytes = {std::byte{0xFE}, std::byte{0xFF}};
    command.patches.push_back(patch);
    return command;
}

} // namespace

int main()
{
    static_assert(CommandKind::PaintStroke != CommandKind::LayerCreate);
    static_assert(CommandScope::Layer != CommandScope::Selection);
    static_assert(CommandPayloadStorage::InlineBytes != CommandPayloadStorage::AssetReference);
    static_assert(HistoryStepDirection::Undo != HistoryStepDirection::Redo);

    UndoRedoController controller;
    controller.history.maxUndoCommands = 2;

    if (canUndo(controller.history) || canRedo(controller.history)) {
        return 1;
    }

    const PaintUuid layerId = uuidWithFirstByte(7);
    Command first = makeLayerPixelCommand(layerId, "Paint stroke");
    if (!recordHistoryCommand(controller, first)
            || controller.history.cursor != 1
            || controller.history.nextSequence != 2
            || controller.history.undoCommands.size() != 1
            || controller.history.undoCommands.front().sequence != 1
            || controller.history.undoCommands.front().patches.size() != 1
            || !byteVectorEquals(controller.history.undoCommands.front().patches.front().beforeState.bytes,
                                 {0x00, 0x01})) {
        return 1;
    }

    const HistorySnapshot recordedSnapshot = makeHistorySnapshot(controller.history);
    if (!recordedSnapshot.canUndo
            || recordedSnapshot.canRedo
            || recordedSnapshot.undoDepth != 1
            || recordedSnapshot.redoDepth != 0
            || recordedSnapshot.cursor != 1) {
        return 1;
    }

    const HistoryStepResult undo = undoHistoryCommand(controller);
    if (!undo.applied
            || undo.direction != HistoryStepDirection::Undo
            || undo.command.sequence != 1
            || !controller.history.undoCommands.empty()
            || controller.history.redoCommands.size() != 1
            || controller.history.cursor != 0
            || !canRedo(controller.history)) {
        return 1;
    }

    const HistoryStepResult redo = redoHistoryCommand(controller);
    if (!redo.applied
            || redo.direction != HistoryStepDirection::Redo
            || redo.command.sequence != 1
            || controller.history.undoCommands.size() != 1
            || !controller.history.redoCommands.empty()
            || controller.history.cursor != 1) {
        return 1;
    }

    undoHistoryCommand(controller);
    Command branch = makeLayerPixelCommand(layerId, "Branch paint");
    if (!recordHistoryCommand(controller, branch)
            || !controller.history.redoCommands.empty()
            || controller.history.undoCommands.back().sequence != 2
            || controller.history.cursor != 1) {
        return 1;
    }

    recordHistoryCommand(controller, makeLayerPixelCommand(layerId, "Second"));
    recordHistoryCommand(controller, makeLayerPixelCommand(layerId, "Third"));
    if (controller.history.undoCommands.size() != 2
            || controller.history.undoCommands.front().sequence != 3
            || controller.history.undoCommands.back().sequence != 4
            || controller.history.cursor != 2) {
        return 1;
    }

    Command notUndoable = makeLayerPixelCommand(layerId, "View only");
    notUndoable.reversible = false;
    if (recordHistoryCommand(controller, notUndoable)) {
        return 1;
    }

    return 0;
}
