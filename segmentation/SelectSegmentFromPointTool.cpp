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

#include "DLImgEditLibrary.h"

namespace
{

void selectSegment(KisPaintDevice const &paintDevice,
                   QPoint const &pos,
                   KisPixelSelection &selection,
                   dlimgedit::Environment const &env)
{
    QRect bounds = paintDevice.defaultBounds()->bounds();
    QImage image = paintDevice.convertToQImage(nullptr, bounds);
    dlimgedit::Channels channels = dlimgedit::Channels::rgba;
    switch (image.format()) {
    case QImage::Format_RGBA8888:
        // case QImage::Format_ARGB32:
        channels = dlimgedit::Channels::rgba;
        break;
    case QImage::Format_RGB888:
        channels = dlimgedit::Channels::rgb;
        break;
    default:
        qDebug() << "[krita-ai-tools] Converting from" << image.format() << " to RGBA8888";
        image = image.convertToFormat(QImage::Format_RGBA8888);
        break;
    }
    try {
        auto view = dlimgedit::ImageView(image.bits(), {image.width(), image.height()}, channels);
        auto seg = dlimgedit::Segmentation::process(view, env);
        auto mask = seg.get_mask(dlimgedit::Point{pos.x(), pos.y()});

        selection.writeBytes(mask.pixels(), QRect(0, 0, image.width(), image.height()));
        selection.invalidateOutlineCache();
    } catch (const std::exception &e) {
        qWarning() << "[krita-ai-tools] Error during segmentation:" << e.what();
    }
}

} // namespace

SelectSegmentFromPointTool::SelectSegmentFromPointTool(KoCanvasBase *canvas)
    : KisToolSelect(canvas,
                    KisCursor::load("tool_segmentation_point_cursor.png", 6, 6),
                    i18n("Segment Selection from Point"))
{
    setObjectName("tool_select_segment_from_point");
    m_dlimgEnv = initDLImgEditLibrary();
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

    KisPaintDeviceSP dev;

    if (!currentNode() || !(dev = currentNode()->projection()) || !selectionEditable()) {
        event->ignore();
        return;
    }

    beginSelectInteraction();

    QApplication::setOverrideCursor(KisCursor::waitCursor());

    // -------------------------------

    KisProcessingApplicator applicator(currentImage(),
                                       currentNode(),
                                       KisProcessingApplicator::NONE,
                                       KisImageSignalVector(),
                                       kundo2_i18n("Select Segment from Point"));

    QPoint pos = convertToImagePixelCoordFloored(event);
    // QRect rc = currentImage()->bounds();

    KisImageSP image = currentImage();
    KisPaintDeviceSP sourceDevice;
    if (sampleLayersMode() == SampleAllLayers) {
        sourceDevice = image->projection();
    } else if (sampleLayersMode() == SampleColorLabeledLayers) {
        KisImageSP refImage =
            KisMergeLabeledLayersCommand::createRefImage(image, "Segmentation Selection Tool Reference Image");
        refImage->animationInterface()->switchCurrentTimeAsync(image->animationInterface()->currentTime());
        refImage->waitForDone();

        sourceDevice = KisMergeLabeledLayersCommand::createRefPaintDevice(
            image,
            "Segmentation Selection Tool Reference Result Paint Device");

        KisMergeLabeledLayersCommand *command =
            new KisMergeLabeledLayersCommand(refImage,
                                             sourceDevice,
                                             image->root(),
                                             colorLabelsSelected(),
                                             KisMergeLabeledLayersCommand::GroupSelectionPolicy_SelectIfColorLabeled);
        applicator.applyCommand(command, KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::EXCLUSIVE);

    } else { // Sample Current Layer
        sourceDevice = dev;
    }

    KisPixelSelectionSP selection = new KisPixelSelection(new KisSelectionDefaultBounds(dev));

    // bool antiAlias = antiAliasSelection();
    // int grow = growSelection();
    // int feather = featherSelection();

    KisCanvas2 *kisCanvas = dynamic_cast<KisCanvas2 *>(canvas());
    KIS_SAFE_ASSERT_RECOVER(kisCanvas)
    {
        applicator.cancel();
        QApplication::restoreOverrideCursor();
        return;
    };

    // KisPixelSelectionSP existingSelection;
    // if (kisCanvas->imageView() && kisCanvas->imageView()->selection()) {
    //     existingSelection = kisCanvas->imageView()->selection()->pixelSelection();
    // }

    KUndo2Command *cmd = new KisCommandUtils::LambdaCommand(
        [selection, pos, sourceDevice, env = m_dlimgEnv]() mutable -> KUndo2Command * {
            selectSegment(*sourceDevice, pos, *selection, *env);
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
