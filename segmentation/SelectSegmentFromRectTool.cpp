#include "SelectSegmentFromRectTool.h"

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

SelectSegmentFromRectTool::SelectSegmentFromRectTool(KoCanvasBase *canvas,
                                                     QSharedPointer<SegmentationToolShared> shared)
    : KisToolSelectBase<RectangleForSegmentationTool>(canvas, i18n("Segment Selection (Box)"))
    , m_segmentation(std::move(shared))
{
}

void SelectSegmentFromRectTool::beginShape()
{
    beginSelectInteraction();
    m_segmentation.processImage({canvas(), currentNode(), currentImage(), sampleLayersMode(), colorLabelsSelected()});
}

void SelectSegmentFromRectTool::endShape()
{
    endSelectInteraction();
}

void SelectSegmentFromRectTool::finishRect(const QRectF &rect, qreal /*roundCornersX*/, qreal /*roundCornersY*/)
{
    if (!selectionEditable()) {
        return;
    }

    QRect region = rect.normalized().toRect();
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

    m_segmentation.applySelectionMask(input, region, options);
}

QWidget *SelectSegmentFromRectTool::createOptionWidget()
{
    KisToolSelectBase<RectangleForSegmentationTool>::createOptionWidget();
    KisSelectionOptions *selectionWidget = selectionOptionWidget();
    m_segmentation.addOptions(selectionWidget);
    return selectionWidget;
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
