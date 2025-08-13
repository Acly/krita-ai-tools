#include "InpaintTool.h"
#include "VisionML.h"

#include "QApplication"
#include "QPainterPath"
#include "QVBoxLayout"

#include "kis_canvas2.h"
#include "kis_cursor.h"
#include "kis_painter.h"
#include "kis_paintop_preset.h"
#include <KisViewManager.h>
#include <KoColor.h>
#include <klocalizedstring.h>

#include "commands_new/kis_transaction_based_command.h"
#include "kis_processing_applicator.h"
#include "kis_transaction.h"
#include "kundo2magicstring.h"
#include "kundo2stack.h"

#include "KoColorSpaceRegistry.h"
#include <KisCursorOverrideLock.h>

#include "libs/image/kis_paint_device_debug_utils.h"

#include "kis_algebra_2d.h"
#include "kis_paint_layer.h"
#include "kis_resources_snapshot.h"

namespace
{

QRect padBounds(const QRect &bounds, int pad, int targetSize, const QRect &imageBounds)
{
    QRect padded = bounds.adjusted(pad, pad, pad, pad);

    if (padded.width() < targetSize) {
        int diff = targetSize - padded.width();
        int leftPadding = diff / 2;
        int rightPadding = diff - leftPadding;

        if (padded.left() - leftPadding < imageBounds.left()) {
            leftPadding = padded.left() - imageBounds.left();
            rightPadding = diff - leftPadding;
        } else if (padded.right() + rightPadding > imageBounds.right()) {
            rightPadding = imageBounds.right() - padded.right();
            leftPadding = diff - rightPadding;
        }
        padded.adjust(-leftPadding, 0, rightPadding, 0);
    }

    if (padded.height() < targetSize) {
        int diff = targetSize - padded.height();
        int topPadding = diff / 2;
        int bottomPadding = diff - topPadding;

        if (padded.top() - topPadding < imageBounds.top()) {
            topPadding = padded.top() - imageBounds.top();
            bottomPadding = diff - topPadding;
        } else if (padded.bottom() + bottomPadding > imageBounds.bottom()) {
            bottomPadding = imageBounds.bottom() - padded.bottom();
            topPadding = diff - bottomPadding;
        }
        padded.adjust(0, -topPadding, 0, bottomPadding);
    }
    return padded.intersected(imageBounds);
}

}

class VisionMLInpaintCommand : public KisTransactionBasedCommand
{
public:
    VisionMLInpaintCommand(KisPaintDeviceSP maskDev,
                           KisPaintDeviceSP imageDev,
                           KisSelectionSP selection,
                           QSharedPointer<VisionModels> vision,
                           VisionMLErrorReporter &errorReporter)
        : m_maskDev(maskDev)
        , m_imageDev(imageDev)
        , m_selection(selection)
        , m_vision(std::move(vision))
        , m_report(errorReporter)
    {
    }

    static const int pad = 64;
    static const int minSize = 512;

    KUndo2Command *paint() override
    {
        KisTransaction transaction(m_imageDev);

        try {
            QRect fullBounds = m_maskDev->nonDefaultPixelArea();
            QRect bounds = padBounds(fullBounds, pad, minSize, m_imageDev->exactBounds());
            if (bounds.isEmpty()) {
                qWarning() << "Inpaint bounds are empty, nothing to do.";
                return transaction.endAndTake();
            }

            VisionMLImage image = VisionMLImage::prepare(*m_imageDev, bounds);

            KoColorSpace const *maskCS = m_maskDev->colorSpace();
            if (maskCS->pixelSize() != 1 || maskCS->id() != "ALPHA") {
                throw std::runtime_error("Unsupported mask color space: " + maskCS->id().toStdString());
            }

            QImage maskData = QImage(bounds.width(), bounds.height(), QImage::Format_Alpha8);
            m_maskDev->readBytes(maskData.bits(), bounds.x(), bounds.y(), bounds.width(), bounds.height());
            visp::image_span maskView({bounds.width(), bounds.height()}, visp::image_format::alpha_u8, maskData.bits());
            maskView.stride = maskData.bytesPerLine();

            visp::image_data result = m_vision->inpaint(image.view, maskView);
            visp::image_data maskF32 = visp::image_u8_to_f32(maskView, visp::image_format::alpha_f32);
            visp::image_data maskTmp = visp::image_alloc(maskF32.extent, visp::image_format::alpha_f32);
            visp::image_erosion(maskF32, maskTmp, 1);
            visp::image_blur(maskTmp, maskF32, 1);
            visp::image_f32_to_u8(maskF32, maskView);
            visp::image_set_alpha(result, maskView);

            QImage resultImage(result.extent[0], result.extent[1], QImage::Format_RGBA8888);
            // copy scanlines, row stride might be different
            size_t rowSize = result.extent[0] * n_bytes(result.format);
            for (int i = 0; i < result.extent[1]; ++i) {
                memcpy(resultImage.scanLine(i), result.data.get() + i * rowSize, rowSize);
            }

            KisPaintDeviceSP comp = m_imageDev->createCompositionSourceDevice();
            comp->convertFromQImage(resultImage, nullptr, bounds.x(), bounds.y());

            KisPainter p(m_imageDev);
            p.setCompositeOpId(COMPOSITE_OVER);
            p.setSelection(m_selection);
            p.bitBlt(bounds.topLeft(), comp, bounds);
        } catch (const std::exception &e) {
            Q_EMIT m_report.errorOccurred(QString(e.what()));
        }

        return transaction.endAndTake();
    }

private:
    KisPaintDeviceSP m_maskDev, m_imageDev;
    KisSelectionSP m_selection;
    QSharedPointer<VisionModels> m_vision;
    VisionMLErrorReporter &m_report;
};

struct InpaintTool::Private {
    KisPaintDeviceSP maskDev = nullptr;
    KisPainter maskDevPainter;
    float brushRadius = 50.; // initial default. actually read from ui.
    QWidget *optionsWidget = nullptr;
    VisionMLModelSelect *modelSelectWidget = nullptr;
    QRectF oldOutlineRect;
    QPainterPath brushOutline;
    QSharedPointer<VisionModels> vision;
    VisionMLErrorReporter errorReporter;
};

InpaintTool::InpaintTool(KoCanvasBase *canvas, QSharedPointer<VisionModels> vision)
    : KisToolPaint(canvas, KisCursor::blankCursor())
    , m_d(new Private)
{
    setSupportOutline(true);
    setObjectName("tool_Inpaint");
    m_d->maskDev = new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8());
    m_d->maskDevPainter.begin(m_d->maskDev);

    m_d->maskDevPainter.setPaintColor(KoColor(Qt::cyan, m_d->maskDev->colorSpace()));
    m_d->maskDevPainter.setBackgroundColor(KoColor(Qt::white, m_d->maskDev->colorSpace()));
    m_d->maskDevPainter.setFillStyle(KisPainter::FillStyleForegroundColor);

    m_d->vision = std::move(vision);
}

InpaintTool::~InpaintTool()
{
    m_d->optionsWidget = nullptr;
    m_d->maskDevPainter.end();
}

void InpaintTool::activate(const QSet<KoShape *> &shapes)
{
    KisToolPaint::activate(shapes);
}

void InpaintTool::deactivate()
{
    m_d->vision->unload(VisionMLTask::inpainting);
    KisToolPaint::deactivate();
}

void InpaintTool::resetCursorStyle()
{
    KisToolPaint::resetCursorStyle();
}

void InpaintTool::activatePrimaryAction()
{
    setOutlineVisible(true);
    KisToolPaint::activatePrimaryAction();
}

void InpaintTool::deactivatePrimaryAction()
{
    setOutlineVisible(false);
    KisToolPaint::deactivatePrimaryAction();
}

void InpaintTool::addMaskPath(KoPointerEvent *event)
{
    KisCanvas2 *canvas2 = dynamic_cast<KisCanvas2 *>(canvas());
    KIS_ASSERT(canvas2);
    const KisCoordinatesConverter *converter = canvas2->coordinatesConverter();

    QPointF imagePos = currentImage()->documentToPixel(event->point);
    QPainterPath currentBrushOutline =
        brushOutline().translated(KisAlgebra2D::alignForZoom(imagePos, converter->effectivePhysicalZoom()));
    m_d->maskDevPainter.fillPainterPath(currentBrushOutline);

    canvas()->updateCanvas(currentImage()->pixelToDocument(m_d->maskDev->exactBounds()));
}

void InpaintTool::beginPrimaryAction(KoPointerEvent *event)
{
    // we can only apply inpaint operation to paint layer
    if (currentNode().isNull() || !currentNode()->inherits("KisPaintLayer")
        || nodePaintAbility() != NodePaintAbility::PAINT) {
        KisCanvas2 *kiscanvas = static_cast<KisCanvas2 *>(canvas());
        kiscanvas->viewManager()->showFloatingMessage(i18n("Select a paint layer to use this tool"),
                                                      QIcon(),
                                                      2000,
                                                      KisFloatingMessage::Medium,
                                                      Qt::AlignCenter);
        event->ignore();
        return;
    }

    addMaskPath(event);
    setMode(KisTool::PAINT_MODE);
    KisToolPaint::beginPrimaryAction(event);
}

void InpaintTool::continuePrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);
    addMaskPath(event);
    KisToolPaint::continuePrimaryAction(event);
}

void InpaintTool::endPrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);
    addMaskPath(event);
    KisToolPaint::endPrimaryAction(event);
    setMode(KisTool::HOVER_MODE);

    KisCursorOverrideLock cursorLock(KisCursor::waitCursor());

    KisResourcesSnapshotSP resources =
        new KisResourcesSnapshot(image(), currentNode(), this->canvas()->resourceManager());

    KisProcessingApplicator applicator(image(),
                                       currentNode(),
                                       KisProcessingApplicator::NONE,
                                       KisImageSignalVector(),
                                       kundo2_i18n("Smart Patch"));

    // actual inpaint operation. filling in areas masked by user
    applicator.applyCommand(new VisionMLInpaintCommand(KisPainter::convertToAlphaAsPureAlpha(m_d->maskDev),
                                                       currentNode()->paintDevice(),
                                                       resources->activeSelection(),
                                                       m_d->vision,
                                                       m_d->errorReporter),
                            KisStrokeJobData::BARRIER,
                            KisStrokeJobData::EXCLUSIVE);

    applicator.end();
    image()->waitForDone();

    m_d->maskDev->clear();
}

QPainterPath InpaintTool::brushOutline(void)
{
    const qreal diameter = m_d->brushRadius;
    QPainterPath outline;
    outline.addEllipse(QPointF(0, 0), -0.5 * diameter, -0.5 * diameter);
    return outline;
}

QPainterPath InpaintTool::getBrushOutlinePath(const QPointF &documentPos, const KoPointerEvent *event)
{
    Q_UNUSED(event);

    QPointF imagePos = currentImage()->documentToPixel(documentPos);
    QPainterPath path = brushOutline();

    KisCanvas2 *canvas2 = dynamic_cast<KisCanvas2 *>(canvas());
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(canvas2, QPainterPath());
    const KisCoordinatesConverter *converter = canvas2->coordinatesConverter();

    return path.translated(KisAlgebra2D::alignForZoom(imagePos, converter->effectivePhysicalZoom()));
}

void InpaintTool::requestUpdateOutline(const QPointF &outlineDocPoint, const KoPointerEvent *event)
{
    static QPointF lastDocPoint = QPointF(0, 0);
    if (event)
        lastDocPoint = outlineDocPoint;

    m_d->brushRadius = currentPaintOpPreset()->settings()->paintOpSize();
    m_d->brushOutline = getBrushOutlinePath(lastDocPoint, event);

    QRectF outlinePixelRect = m_d->brushOutline.boundingRect();
    QRectF outlineDocRect = currentImage()->pixelToDocument(outlinePixelRect);

    // This adjusted call is needed as we paint with a 3 pixel wide brush and the pen is outside the bounds of the path
    // Pen uses view coordinates so we have to zoom the document value to match 2 pixel in view coordinates
    // See BUG 275829
    qreal zoomX;
    qreal zoomY;
    canvas()->viewConverter()->zoom(&zoomX, &zoomY);
    qreal xoffset = 2.0 / zoomX;
    qreal yoffset = 2.0 / zoomY;

    if (!outlineDocRect.isEmpty()) {
        outlineDocRect.adjust(-xoffset, -yoffset, xoffset, yoffset);
    }

    if (!m_d->oldOutlineRect.isEmpty()) {
        canvas()->updateCanvas(m_d->oldOutlineRect);
    }

    if (!outlineDocRect.isEmpty()) {
        canvas()->updateCanvas(outlineDocRect);
    }

    m_d->oldOutlineRect = outlineDocRect;
}

void InpaintTool::paint(QPainter &painter, const KoViewConverter &converter)
{
    Q_UNUSED(converter);

    painter.save();
    QPainterPath path = pixelToView(m_d->brushOutline);
    paintToolOutline(&painter, path);
    painter.restore();

    painter.save();
    painter.setBrush(Qt::magenta);
    QImage img = m_d->maskDev->convertToQImage(0);
    if (!img.size().isEmpty()) {
        painter.drawImage(pixelToView(m_d->maskDev->exactBounds()), img);
    }
    painter.restore();
}

QWidget *InpaintTool::createOptionWidget()
{
    m_d->optionsWidget = new QWidget();
    m_d->optionsWidget->setObjectName(toolId() + "option widget");

    QVBoxLayout *layout = new QVBoxLayout(m_d->optionsWidget);
    m_d->optionsWidget->setLayout(layout);

    m_d->modelSelectWidget = new VisionMLModelSelect(m_d->vision, VisionMLTask::inpainting);
    layout->addWidget(m_d->modelSelectWidget);

    VisionMLBackendWidget *backendSelect = new VisionMLBackendWidget(m_d->vision);
    layout->addWidget(backendSelect);

    return m_d->optionsWidget;
}
