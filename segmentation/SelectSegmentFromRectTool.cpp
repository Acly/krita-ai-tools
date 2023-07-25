#include "SelectSegmentFromRectTool.h"
#include "SegmentationToolCommon.h"

#include "canvas/kis_canvas2.h"
#include "kis_command_utils.h"
#include "kis_paint_device.h"
#include "kis_pixel_selection.h"
#include "kis_selection_tool_helper.h"

RectangleForSegmentationTool::RectangleForSegmentationTool(KoCanvasBase *canvas)
    : KisToolRectangleBase(canvas,
                           KisToolRectangleBase::SELECT,
                           KisCursor::load("tool_segmentation_rect_cursor.png", 6, 6))
{
    setObjectName("tool_select_segment_from_rect");
}

SelectSegmentFromRectTool::SelectSegmentFromRectTool(KoCanvasBase *canvas)
    : KisToolSelectBase<RectangleForSegmentationTool>(canvas, i18n("Segment Selection (Box)"))
{
    m_dlimgEnv = SegmentationToolCommon::initLibrary();
}

void SelectSegmentFromRectTool::beginShape()
{
    beginSelectInteraction();

    KisPaintDeviceSP layerImage;
    if (!currentNode() || !(layerImage = currentNode()->projection()) || !selectionEditable()) {
        return;
    }

    KisProcessingApplicator applicator(currentImage(),
                                       currentNode(),
                                       KisProcessingApplicator::NONE,
                                       KisImageSignalVector(),
                                       kundo2_i18n("Image segmentation"));
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

    KUndo2Command *cmd = new KisCommandUtils::LambdaCommand(
        [segmentation = &m_segmentation, inputImage, env = m_dlimgEnv]() mutable -> KUndo2Command * {
            try {
                auto image = SegmentationToolCommon::prepareImage(*inputImage);
                segmentation->reset(new dlimgedit::Segmentation(dlimgedit::Segmentation::process(image.view, *env)));
            } catch (const std::exception &e) {
                qWarning() << "[krita-ai-tools] Error during segmentation:" << e.what();
            }
            return nullptr;
        });

    applicator.applyCommand(cmd, KisStrokeJobData::BARRIER);
    applicator.end();
}

void SelectSegmentFromRectTool::endShape()
{
    endSelectInteraction();
}

void SelectSegmentFromRectTool::finishRect(const QRectF &rect, qreal /*roundCornersX*/, qreal /*roundCornersY*/)
{
    KisCanvas2 *kisCanvas = dynamic_cast<KisCanvas2 *>(canvas());
    if (!kisCanvas) {
        return;
    }
    KisSelectionToolHelper helper(kisCanvas, kundo2_i18n("Segment Selection (Box)"));

    QRect rc(rect.normalized().toRect());
    if (helper.tryDeselectCurrentSelection(pixelToView(rc), selectionAction())) {
        return;
    }
    if (helper.canShortcutToNoop(rc, selectionAction())) {
        return;
    }
    if (!rc.isValid()) {
        return;
    }

    KisPaintDeviceSP layerImage;
    if (!currentNode() || !(layerImage = currentNode()->projection()) || !selectionEditable()) {
        return;
    }

    QApplication::setOverrideCursor(KisCursor::waitCursor());

    KisProcessingApplicator applicator(currentImage(),
                                       currentNode(),
                                       KisProcessingApplicator::NONE,
                                       KisImageSignalVector(),
                                       kundo2_i18n("Segment Selection (Box)"));

    KisPixelSelectionSP selection = new KisPixelSelection(new KisSelectionDefaultBounds(layerImage));
    const bool antiAlias = antiAliasSelection();
    const int grow = growSelection();
    const int feather = featherSelection();

    KUndo2Command *cmd = new KisCommandUtils::LambdaCommand(
        [selection, rc, antiAlias, grow, feather, segmentation = m_segmentation]() mutable -> KUndo2Command * {
            try {
                auto mask = segmentation->get_mask(SegmentationToolCommon::toRegion(rc));
                selection->writeBytes(mask.pixels(), QRect(0, 0, mask.extent().width, mask.extent().height));
                selection->invalidateOutlineCache();
            } catch (const std::exception &e) {
                qWarning() << "[krita-ai-tools] Error during segmentation:" << e.what();
            }
            SegmentationToolCommon::adjustSelection(selection, grow, feather, antiAlias);
            return nullptr;
        });

    applicator.applyCommand(cmd, KisStrokeJobData::BARRIER);
    helper.selectPixelSelection(applicator, selection, selectionAction());
    applicator.end();

    QApplication::restoreOverrideCursor();
}

void SelectSegmentFromRectTool::resetCursorStyle()
{
    if (selectionAction() == SELECTION_ADD) {
        useCursor(KisCursor::load("tool_segmentation_rect_cursor_add.png", 6, 6));
    } else if (selectionAction() == SELECTION_SUBTRACT) {
        useCursor(KisCursor::load("tool_segmentation_rect_cursor_sub.png", 6, 6));
    } else if (selectionAction() == SELECTION_INTERSECT) {
        useCursor(KisCursor::load("tool_segmentation_rect_cursor_inter.png", 6, 6));
    } else if (selectionAction() == SELECTION_SYMMETRICDIFFERENCE) {
        useCursor(KisCursor::load("tool_segmentation_rect_cursor_symdiff.png", 6, 6));
    } else {
        KisToolSelectBase<RectangleForSegmentationTool>::resetCursorStyle();
    }
}
