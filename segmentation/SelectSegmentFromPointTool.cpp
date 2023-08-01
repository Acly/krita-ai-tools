#include "SelectSegmentFromPointTool.h"

#include <QApplication>
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QLibrary>

#include "canvas/kis_canvas2.h"
#include "commands_new/KisMergeLabeledLayersCommand.h"
#include "kis_command_utils.h"
#include "kis_paint_device.h"
#include "kis_pixel_selection.h"
#include "kis_selection_tool_helper.h"
#include <kis_image_animation_interface.h>

SelectSegmentFromPointTool::SelectSegmentFromPointTool(KoCanvasBase *canvas,
                                                       QSharedPointer<SegmentationToolShared> shared)
    : KisToolSelect(canvas,
                    KisCursor::load("tool_segmentation_point_cursor.png", 6, 6),
                    i18n("Segment Selection from Point"))
    , m_segmentation(std::move(shared))
{
    setObjectName("tool_select_segment_from_point");
}

void SelectSegmentFromPointTool::activate(const QSet<KoShape *> &shapes)
{
    KisToolSelect::activate(shapes);

    KisImage *image = currentImage().data();
    connect(image, SIGNAL(sigImageUpdated(QRect)), this, SLOT(updateImage(QRect)));

    m_segmentation.processImage({canvas(), currentNode(), currentImage(), sampleLayersMode(), colorLabelsSelected()});
}

void SelectSegmentFromPointTool::deactivate()
{
    KisToolSelect::deactivate();

    KisImage *image = currentImage().data();
    disconnect(image, SIGNAL(sigImageUpdated(QRect)), this, SLOT(updateImage(QRect)));
    m_segmentation.deactivate();
}

void SelectSegmentFromPointTool::updateImage(QRect const &)
{
    m_segmentation.notifyImageChanged();
}

void SelectSegmentFromPointTool::beginPrimaryAction(KoPointerEvent *event)
{
    KisToolSelectBase::beginPrimaryAction(event);
    if (isMovingSelection()) {
        return;
    }
    if (!selectionEditable()) {
        return;
    }

    beginSelectInteraction();

    QPoint position = convertToImagePixelCoordFloored(event);
    SegmentationToolHelper::ImageInput input;
    input.canvas = canvas();
    input.image = currentImage();
    input.node = currentNode();
    input.sampleLayersMode = sampleLayersMode();
    input.colorLabelsSelected = colorLabelsSelected();
    SegmentationToolHelper::SelectionOptions options;
    options.action = selectionAction();
    options.grow = growSelection();
    options.feather = featherSelection();
    options.antiAlias = antiAliasSelection();

    m_segmentation.applySelectionMask(input, position, options);
}

void SelectSegmentFromPointTool::endPrimaryAction(KoPointerEvent *event)
{
    if (isMovingSelection()) {
        KisToolSelectBase::endPrimaryAction(event);
        return;
    }

    endSelectInteraction();
}

void SelectSegmentFromPointTool::paint(QPainter &painter, const KoViewConverter &converter)
{
    Q_UNUSED(painter);
    Q_UNUSED(converter);
}

QWidget *SelectSegmentFromPointTool::createOptionWidget()
{
    KisToolSelectBase::createOptionWidget();
    KisSelectionOptions *selectionWidget = selectionOptionWidget();
    m_segmentation.addOptions(selectionWidget);
    return selectionWidget;
}

void SelectSegmentFromPointTool::resetCursorStyle()
{
    if (selectionAction() == SELECTION_ADD) {
        useCursor(KisCursor::load("tool_segmentation_point_cursor_add.png", 6, 6));
    } else if (selectionAction() == SELECTION_SUBTRACT) {
        useCursor(KisCursor::load("tool_segmentation_point_cursor_sub.png", 6, 6));
    } else if (selectionAction() == SELECTION_INTERSECT) {
        useCursor(KisCursor::load("tool_segmentation_point_cursor_inter.png", 6, 6));
    } else if (selectionAction() == SELECTION_SYMMETRICDIFFERENCE) {
        useCursor(KisCursor::load("tool_segmentation_point_cursor_symdiff.png", 6, 6));
    } else {
        KisToolSelect::resetCursorStyle();
    }
}
