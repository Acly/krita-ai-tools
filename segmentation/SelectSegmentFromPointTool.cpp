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

#include "SegmentationToolCommon.h"

SelectSegmentFromPointTool::SelectSegmentFromPointTool(KoCanvasBase *canvas)
    : KisToolSelect(canvas,
                    KisCursor::load("tool_segmentation_point_cursor.png", 6, 6),
                    i18n("Segment Selection from Point"))
{
    setObjectName("tool_select_segment_from_point");
    m_dlimgEnv = SegmentationToolCommon::initLibrary();
}

SelectSegmentFromPointTool::~SelectSegmentFromPointTool()
{
}

void SelectSegmentFromPointTool::activate(const QSet<KoShape *> &shapes)
{
    KisToolSelect::activate(shapes);
    // m_configGroup = KSharedConfig::openConfig()->group(toolId());
}

void SelectSegmentFromPointTool::beginPrimaryAction(KoPointerEvent *event)
{
    KisToolSelectBase::beginPrimaryAction(event);
    if (isMovingSelection()) {
        return;
    }
    if (!m_dlimgEnv) {
        return;
    }
    KisCanvas2 *kisCanvas = dynamic_cast<KisCanvas2 *>(canvas());
    if (!kisCanvas) {
        return;
    }
    KisPaintDeviceSP layerImage;
    if (!currentNode() || !(layerImage = currentNode()->projection()) || !selectionEditable()) {
        return;
    }

    beginSelectInteraction();
    QApplication::setOverrideCursor(KisCursor::waitCursor());

    QPoint pos = convertToImagePixelCoordFloored(event);

    KisProcessingApplicator applicator(currentImage(),
                                       currentNode(),
                                       KisProcessingApplicator::NONE,
                                       KisImageSignalVector(),
                                       kundo2_i18n("Select Segment from Point"));
    KisPaintDeviceSP inputImage;
    switch (sampleLayersMode()) {
    case SampleAllLayers:
        inputImage = currentImage()->projection();
        break;
    case SampleCurrentLayer:
        inputImage = layerImage;
        break;
    case SampleColorLabeledLayers:
        inputImage = SegmentationToolCommon::mergeColorLayers(currentImage(), colorLabelsSelected(), applicator);
        break;
    }

    KisPixelSelectionSP selection = new KisPixelSelection(new KisSelectionDefaultBounds(layerImage));

    const int grow = growSelection();
    const int feather = featherSelection();
    const bool antiAlias = antiAliasSelection();

    KUndo2Command *cmd = new KisCommandUtils::LambdaCommand(
        [selection, pos, inputImage, grow, feather, antiAlias, env = m_dlimgEnv]() mutable -> KUndo2Command * {
            try {
                auto image = SegmentationToolCommon::prepareImage(*inputImage);
                auto seg = dlimgedit::Segmentation::process(image.view, *env);
                auto mask = seg.get_mask(SegmentationToolCommon::toPoint(pos));

                selection->writeBytes(mask.pixels(), image.rect());
                SegmentationToolCommon::adjustSelection(selection, grow, feather, antiAlias);
                selection->invalidateOutlineCache();
            } catch (const std::exception &e) {
                qWarning() << "[krita-ai-tools] Error during segmentation:" << e.what();
            }
            return nullptr;
        });
    applicator.applyCommand(cmd, KisStrokeJobData::BARRIER);

    KisSelectionToolHelper helper(kisCanvas, kundo2_i18n("Select Segment from Point"));
    helper.selectPixelSelection(applicator, selection, selectionAction());

    applicator.end();
    QApplication::restoreOverrideCursor();
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
