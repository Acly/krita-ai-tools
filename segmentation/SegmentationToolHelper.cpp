#include "SegmentationToolHelper.h"

#include "KisCursorOverrideLock.h"
#include "KisOptionButtonStrip.h"
#include "KoGroupButton.h"
#include "KoResourcePaths.h"
#include "kis_command_utils.h"
#include "kis_default_bounds.h"
#include "kis_image_animation_interface.h"
#include "kis_paint_device.h"
#include "kis_selection.h"
#include "kis_selection_filters.h"
#include "kis_selection_tool_helper.h"

#include <QApplication>
#include <QDebug>
#include <QImage>
#include <QLibrary>
#include <QMessageBox>
#include <QRect>

namespace
{

dlimg::Point convert(QPoint const &point)
{
    return dlimg::Point{point.x(), point.y()};
}

dlimg::Region convert(QRect const &rect)
{
    return dlimg::Region(convert(rect.topLeft()), convert(rect.bottomRight()));
}

QRect imageBounds(QPoint offset, dlimg::Extent extent)
{
    return QRect(offset.x(), offset.y(), extent.width, extent.height);
}

struct Image {
    QImage data;
    dlimg::ImageView view;

    explicit operator bool() const
    {
        return !data.isNull();
    }
};

Image prepareImage(KisPaintDevice const &device, QRect bounds = {})
{
    Image result;
    if (bounds.isEmpty()) {
        bounds = device.exactBounds();
    }
    if (bounds.isEmpty()) {
        return result; // Can happen eg. when using color label mode without matching layers.
    }
    KoColorSpace const *cs = device.colorSpace();
    if (cs->pixelSize() == 4 && cs->id() == "RGBA") {
        // Stored as BGRA, 8 bits per channel in Krita. No conversions for now, the segmentation network expects
        // gamma-compressed sRGB, but works fine with other color spaces (probably).
        result.view.channels = dlimg::Channels::bgra;
        result.data = QImage(bounds.width(), bounds.height(), QImage::Format_ARGB32);
        device.readBytes(result.data.bits(), bounds.x(), bounds.y(), bounds.width(), bounds.height());
    } else {
        // Convert everything else to QImage::Format_ARGB32 in default color space (sRGB).
        result.view.channels = dlimg::Channels::argb;
        result.data = device.convertToQImage(nullptr, bounds);
    }
    result.view.extent = {result.data.width(), result.data.height()};
    result.view.stride = result.data.bytesPerLine();
    result.view.pixels = result.data.bits();
    return result;
}

void adjustSelection(KisPixelSelectionSP const &selection, SegmentationToolHelper::SelectionOptions const &o)
{
    if (o.grow > 0) {
        KisGrowSelectionFilter biggy(o.grow, o.grow);
        biggy.process(selection, selection->selectedRect().adjusted(-o.grow, -o.grow, o.grow, o.grow));
    } else if (o.grow < 0) {
        KisShrinkSelectionFilter tiny(-o.grow, -o.grow, false);
        tiny.process(selection, selection->selectedRect());
    }
    if (o.feather > 0) {
        KisFeatherSelectionFilter feathery(o.feather);
        feathery.process(selection, selection->selectedRect().adjusted(-o.feather, -o.feather, o.feather, o.feather));
    } else if (o.antiAlias) {
        KisAntiAliasSelectionFilter antiAliasFilter;
        antiAliasFilter.process(selection, selection->selectedRect());
    }
}

} // namespace

bool operator==(SegmentationToolHelper::ImageInput const &a, SegmentationToolHelper::ImageInput const &b)
{
    return a.canvas == b.canvas && a.image == b.image && a.node == b.node && a.sampleLayersMode == b.sampleLayersMode
        && a.colorLabelsSelected == b.colorLabelsSelected;
}

bool operator!=(SegmentationToolHelper::ImageInput const &a, SegmentationToolHelper::ImageInput const &b)
{
    return !(a == b);
}

//
// SegmentationToolHelper

SegmentationToolHelper::SegmentationToolHelper(QSharedPointer<SegmentationToolShared> shared)
    : m_shared(std::move(shared))
{
    connect(&m_segmentation,
            SIGNAL(errorOccurred(QString const &)),
            this,
            SLOT(reportError(QString const &)),
            Qt::QueuedConnection);
}

void SegmentationToolHelper::processImage(ImageInput const &input, KisProcessingApplicator &applicator)
{
    KisPaintDeviceSP inputImage = selectPaintDevice(input, applicator);
    if (!inputImage) {
        return;
    }

    m_bounds = inputImage->exactBounds();

    if (m_mode == SegmentationMode::precise) {
        return; // No separate image processing step, everything happens in applySelectionMask.
    }

    KUndo2Command *cmd = new KisCommandUtils::LambdaCommand(
        [segmentation = &m_segmentation, inputImage, env = &m_shared->environment()]() mutable -> KUndo2Command * {
            try {
                if (Image image = prepareImage(*inputImage)) {
                    segmentation->m = dlimg::Segmentation::process(image.view, *env);
                } else {
                    segmentation->m = nullptr;
                }
            } catch (const std::exception &e) {
                Q_EMIT segmentation->errorOccurred(QString(e.what()));
            }
            return nullptr;
        });

    applicator.applyCommand(cmd, KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::EXCLUSIVE);

    m_lastInput = input;
    m_requiresUpdate = false;
}

void SegmentationToolHelper::processImage(ImageInput const &input)
{
    KisProcessingApplicator applicator(input.image,
                                       input.node,
                                       KisProcessingApplicator::NO_IMAGE_UPDATES, // XXX
                                       KisImageSignalVector(),
                                       kundo2_i18n("Select Segment"));
    processImage(input, applicator);
    applicator.end();
}

void SegmentationToolHelper::applySelectionMask(ImageInput const &input,
                                                QVariant prompt,
                                                SelectionOptions const &options)
{
    KisCanvas2 *kisCanvas = dynamic_cast<KisCanvas2 *>(input.canvas);
    if (!kisCanvas) {
        return;
    }
    KisSelectionToolHelper helper(kisCanvas, kundo2_i18n("Segment Selection"));

    if (prompt.canConvert<QRect>()) {
        QRect region = prompt.toRect().intersected(m_bounds);
        region.translate(-m_bounds.topLeft());
        prompt = region;

        if (helper.tryDeselectCurrentSelection(QRectF(region), options.action)) {
            return;
        }
        if (helper.canShortcutToNoop(region, options.action)) {
            return;
        }
        if (!region.isValid()) {
            return;
        }
    } else if (prompt.canConvert<QPoint>()) {
        QPoint point = prompt.toPoint();

        if (!m_bounds.contains(point, true)) {
            return;
        }
        point -= m_bounds.topLeft();
        prompt = point;
    }

    KisPaintDeviceSP inputImage;
    if (!input.node || !(inputImage = input.node->projection())) {
        return;
    }

    KisCursorOverrideLock cursorLock(KisCursor::waitCursor());

    KisProcessingApplicator applicator(input.image,
                                       input.node,
                                       KisProcessingApplicator::NO_IMAGE_UPDATES, // XXX
                                       KisImageSignalVector(),
                                       kundo2_i18n("Select Segment"));

    if (m_mode == SegmentationMode::fast) {
        if (m_requiresUpdate || input != m_lastInput) {
            processImage(input, applicator);
        }
    } else { // SegmentationMode::precise
        inputImage = selectPaintDevice(input, applicator);
        m_bounds = inputImage->exactBounds();
    }

    KisPixelSelectionSP selection = new KisPixelSelection(new KisSelectionDefaultBounds(inputImage));

    KUndo2Command *cmd = new KisCommandUtils::LambdaCommand([mode = m_mode,
                                                             segmentation = &m_segmentation,
                                                             env = &m_shared->environment(),
                                                             inputImage,
                                                             bounds = m_bounds,
                                                             prompt,
                                                             selection,
                                                             options]() mutable -> KUndo2Command * {
        try {
            if (mode == SegmentationMode::fast) {
                if (!segmentation->m) {
                    return nullptr; // Early out when there was no input image to process.
                }
                auto mask = prompt.canConvert<QPoint>() ? segmentation->m.compute_mask(convert(prompt.toPoint()))
                                                        : segmentation->m.compute_mask(convert(prompt.toRect()));
                selection->writeBytes(mask.pixels(), imageBounds(bounds.topLeft(), mask.extent()));
            } else {
                QRect region = prompt.canConvert<QRect>() ? prompt.toRect() : QRect();
                Image image = prepareImage(*inputImage, prompt.toRect());
                if (!image) {
                    return nullptr;
                }
                auto mask = dlimg::segment_objects(image.view, *env);
                bounds.translate(prompt.toRect().topLeft());
                selection->writeBytes(mask.pixels(), imageBounds(bounds.topLeft(), mask.extent()));
            }
            adjustSelection(selection, options);
            selection->invalidateOutlineCache();
        } catch (const std::exception &e) {
            Q_EMIT segmentation->errorOccurred(QString(e.what()));
        }
        return nullptr;
    });
    applicator.applyCommand(cmd, KisStrokeJobData::SEQUENTIAL);

    helper.selectPixelSelection(applicator, selection, options.action);
    applicator.end();
}

void SegmentationToolHelper::reportError(const QString &message)
{
    QMessageBox::warning(nullptr,
                         i18nc("@title:window", "Krita - Segmentation Tools Plugin"),
                         i18n("Error during image segmentation: ") + message);
}

KisPaintDeviceSP SegmentationToolHelper::selectPaintDevice(ImageInput const &input, KisProcessingApplicator &applicator)
{
    KisPaintDeviceSP layerImage;
    if (!input.node || !(layerImage = input.node->projection())) {
        return nullptr;
    }

    KisPaintDeviceSP inputImage;
    switch (input.sampleLayersMode) {
    case KisToolSelect::SampleAllLayers:
        inputImage = input.image->projection();
        break;
    case KisToolSelect::SampleCurrentLayer:
        inputImage = layerImage;
        break;
    case KisToolSelect::SampleColorLabeledLayers:
        inputImage = mergeColorLayers(input.image, input.colorLabelsSelected, applicator);
        break;
    }
    return inputImage;
}

KisPaintDeviceSP SegmentationToolHelper::mergeColorLayers(KisImageSP const &image,
                                                          QList<int> const &selectedLayers,
                                                          KisProcessingApplicator &applicator)
{
    if (!m_referenceNodeList) {
        m_referencePaintDevice =
            KisMergeLabeledLayersCommand::createRefPaintDevice(image, "Segmentation Tool Reference");
        m_referenceNodeList.reset(new KisMergeLabeledLayersCommand::ReferenceNodeInfoList);
    }
    KisPaintDeviceSP newReferencePaintDevice =
        KisMergeLabeledLayersCommand::createRefPaintDevice(image, "Segmentation Tool Reference");
    KisMergeLabeledLayersCommand::ReferenceNodeInfoListSP newReferenceNodeList(
        new KisMergeLabeledLayersCommand::ReferenceNodeInfoList);
    const int currentTime = image->animationInterface()->currentTime();
    applicator.applyCommand(
        new KisMergeLabeledLayersCommand(image,
                                         m_referenceNodeList,
                                         newReferenceNodeList,
                                         m_referencePaintDevice,
                                         newReferencePaintDevice,
                                         selectedLayers,
                                         KisMergeLabeledLayersCommand::GroupSelectionPolicy_SelectIfColorLabeled,
                                         m_previousTime != currentTime),
        KisStrokeJobData::SEQUENTIAL,
        KisStrokeJobData::EXCLUSIVE);
    m_referencePaintDevice = newReferencePaintDevice;
    m_referenceNodeList = newReferenceNodeList;
    m_previousTime = currentTime;
    return m_referencePaintDevice;
}

void SegmentationToolHelper::deactivate()
{
    m_referencePaintDevice = nullptr;
    m_referenceNodeList = nullptr;
}

void SegmentationToolHelper::addOptions(KisSelectionOptions *selectionWidget, bool showMode)
{
    if (showMode) {
        KisOptionButtonStrip *modeSelect = new KisOptionButtonStrip;
        m_modeFastButton = modeSelect->addButton(i18n("Fast"));
        m_modeFastButton->setChecked(m_mode == SegmentationMode::fast);
        m_modePreciseButton = modeSelect->addButton(i18n("Precise"));
        m_modePreciseButton->setChecked(m_mode == SegmentationMode::precise);

        KisOptionCollectionWidgetWithHeader *segmentationModeSection =
            new KisOptionCollectionWidgetWithHeader(i18n("Mode"));
        segmentationModeSection->setPrimaryWidget(modeSelect);
        selectionWidget->insertWidget(2, "segmentationModeSection", segmentationModeSection);

        connect(modeSelect,
                SIGNAL(buttonToggled(KoGroupButton *, bool)),
                this,
                SLOT(switchMode(KoGroupButton *, bool)));
    }

    KisOptionButtonStrip *backendSelect = new KisOptionButtonStrip;
    m_backendCPUButton = backendSelect->addButton(i18n("CPU"));
    m_backendCPUButton->setChecked(m_shared->backend() == dlimg::Backend::cpu);
    m_backendGPUButton = backendSelect->addButton(i18n("GPU"));
    m_backendGPUButton->setEnabled(dlimg::Environment::is_supported(dlimg::Backend::gpu));
    m_backendGPUButton->setChecked(m_shared->backend() == dlimg::Backend::gpu);

    KisOptionCollectionWidgetWithHeader *segmentationBackendSection =
        new KisOptionCollectionWidgetWithHeader(i18n("Backend"));
    segmentationBackendSection->setPrimaryWidget(backendSelect);
    selectionWidget->insertWidget(3, "segmentationBackendSection", segmentationBackendSection);

    connect(backendSelect,
            SIGNAL(buttonToggled(KoGroupButton *, bool)),
            this,
            SLOT(switchBackend(KoGroupButton *, bool)));
    connect(m_shared.get(), SIGNAL(backendChanged(dlimg::Backend)), this, SLOT(updateBackend(dlimg::Backend)));
}

void SegmentationToolHelper::updateBackend(dlimg::Backend backend)
{
    m_backendCPUButton->setChecked(backend == dlimg::Backend::cpu);
    m_backendGPUButton->setChecked(backend == dlimg::Backend::gpu);
    m_requiresUpdate = true;
}

void SegmentationToolHelper::switchBackend(KoGroupButton *button, bool checked)
{
    if (checked) {
        bool success = m_shared->setBackend(button == m_backendCPUButton ? dlimg::Backend::cpu : dlimg::Backend::gpu);
        if (!success) {
            button->setEnabled(false);
            KoGroupButton *prev = m_shared->backend() == dlimg::Backend::cpu ? m_backendCPUButton : m_backendGPUButton;
            bool blocked = prev->blockSignals(true);
            prev->setChecked(true);
            prev->blockSignals(blocked);
        }
    }
}

void SegmentationToolHelper::switchMode(KoGroupButton *button, bool checked)
{
    if (checked) {
        m_mode = button == m_modeFastButton ? SegmentationMode::fast : SegmentationMode::precise;
        m_requiresUpdate = true;
    }
}
