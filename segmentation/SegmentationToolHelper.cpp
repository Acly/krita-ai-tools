#include "SegmentationToolHelper.h"

#include "KisOptionButtonStrip.h"
#include "KoGroupButton.h"
#include "KoResourcePaths.h"
#include "commands_new/KisMergeLabeledLayersCommand.h"
#include "kis_command_utils.h"
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

struct Image {
    QImage data;
    dlimg::ImageView view;

    QRect rect() const
    {
        return QRect(0, 0, data.width(), data.height());
    }
};

Image prepareImage(KisPaintDevice const &device)
{
    Image result;
    QRect bounds = device.exactBounds();
    KoColorSpace const *cs = device.colorSpace();
    if (cs->pixelSize() == 4 && cs->id() == "RGBA") {
        // Stored as BGRA, 8 bits per channel in Krita. No conversions for now, the segmentation network expects
        // gamma-compressed sRGB, but works fine with other color spaces (probably).
        result.view.channels = dlimg::Channels::bgra;
        result.data = QImage(bounds.width(), bounds.height(), QImage::Format_ARGB32);
        device.readBytes(result.data.bits(), 0, 0, bounds.width(), bounds.height());
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

KisPaintDeviceSP
mergeColorLayers(KisImageSP const &image, QList<int> const &selectedLayers, KisProcessingApplicator &applicator)
{
    KisImageSP refImage = KisMergeLabeledLayersCommand::createRefImage(image, "Segmentation Tool Ref Image");
    refImage->animationInterface()->switchCurrentTimeAsync(image->animationInterface()->currentTime());
    refImage->waitForDone();

    KisPaintDeviceSP merged =
        KisMergeLabeledLayersCommand::createRefPaintDevice(image, "Segmentation Tool Ref Paint Device");

    KisMergeLabeledLayersCommand *command =
        new KisMergeLabeledLayersCommand(refImage,
                                         merged,
                                         image->root(),
                                         selectedLayers,
                                         KisMergeLabeledLayersCommand::GroupSelectionPolicy_SelectIfColorLabeled);
    applicator.applyCommand(command, KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::EXCLUSIVE);
    return merged;
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
    KisPaintDeviceSP layerImage;
    if (!input.node || !(layerImage = input.node->projection())) {
        return;
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

    KUndo2Command *cmd = new KisCommandUtils::LambdaCommand(
        [segmentation = &m_segmentation, inputImage, env = &m_shared->environment()]() mutable -> KUndo2Command * {
            try {
                auto image = prepareImage(*inputImage);
                segmentation->m = dlimg::Segmentation::process(image.view, *env);
            } catch (const std::exception &e) {
                Q_EMIT segmentation->errorOccurred(QString(e.what()));
            }
            return nullptr;
        });

    applicator.applyCommand(cmd, KisStrokeJobData::BARRIER);

    m_lastInput = input;
    m_requiresUpdate = false;
}

void SegmentationToolHelper::processImage(ImageInput const &input)
{
    KisProcessingApplicator applicator(input.image,
                                       input.node,
                                       KisProcessingApplicator::NO_IMAGE_UPDATES,
                                       KisImageSignalVector(),
                                       kundo2_i18n("Image segmentation"));
    processImage(input, applicator);
    applicator.end();
}

void SegmentationToolHelper::applySelectionMask(ImageInput const &input,
                                                QVariant const &prompt,
                                                SelectionOptions const &options)
{
    KisCanvas2 *kisCanvas = dynamic_cast<KisCanvas2 *>(input.canvas);
    if (!kisCanvas) {
        return;
    }
    KisSelectionToolHelper helper(kisCanvas, kundo2_i18n("Segment Selection"));

    if (prompt.canConvert<QRect>()) {
        QRect region = prompt.toRect();

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

        if (!input.image->bounds().contains(point, true)) {
            return;
        }
    }

    KisPaintDeviceSP layerImage;
    if (!input.node || !(layerImage = input.node->projection())) {
        return;
    }

    QApplication::setOverrideCursor(KisCursor::waitCursor());

    KisProcessingApplicator applicator(input.image,
                                       input.node,
                                       KisProcessingApplicator::NO_IMAGE_UPDATES, // XXX
                                       KisImageSignalVector(),
                                       kundo2_i18n("Select Segment"));

    if (m_requiresUpdate || input != m_lastInput) {
        processImage(input, applicator);
    }

    KisPixelSelectionSP selection = new KisPixelSelection(new KisSelectionDefaultBounds(layerImage));

    KUndo2Command *cmd = new KisCommandUtils::LambdaCommand(
        [segmentation = &m_segmentation, prompt, selection, options]() mutable -> KUndo2Command * {
            if (!segmentation->m) {
                qWarning() << "[krita-ai-tools] Segmentation not ready";
                return nullptr;
            }
            try {
                auto mask = prompt.canConvert<QPoint>() ? segmentation->m.compute_mask(convert(prompt.toPoint()))
                                                        : segmentation->m.compute_mask(convert(prompt.toRect()));
                selection->writeBytes(mask.pixels(), QRect(0, 0, mask.extent().width, mask.extent().height));
                adjustSelection(selection, options);
                selection->invalidateOutlineCache();
            } catch (const std::exception &e) {
                Q_EMIT segmentation->errorOccurred(QString(e.what()));
            }
            return nullptr;
        });
    applicator.applyCommand(cmd, KisStrokeJobData::BARRIER);

    helper.selectPixelSelection(applicator, selection, options.action);
    applicator.end();

    QApplication::restoreOverrideCursor();
}

void SegmentationToolHelper::reportError(const QString &message)
{
    QMessageBox::warning(nullptr,
                         i18nc("@title:window", "Krita - Segmentation Tools Plugin"),
                         i18n("Error during image segmentation: ") + message);
}

void SegmentationToolHelper::addOptions(KisSelectionOptions *selectionWidget)
{
    KisOptionButtonStrip *backendSelect = new KisOptionButtonStrip;
    m_backendCPUButton = backendSelect->addButton(i18n("CPU"));
    m_backendCPUButton->setChecked(m_shared->backend() == dlimg::Backend::cpu);
    m_backendGPUButton = backendSelect->addButton(i18n("GPU"));
    m_backendGPUButton->setEnabled(dlimg::Environment::is_supported(dlimg::Backend::gpu));
    m_backendGPUButton->setChecked(m_shared->backend() == dlimg::Backend::gpu);

    KisOptionCollectionWidgetWithHeader *segmentationBackendSection =
        new KisOptionCollectionWidgetWithHeader(i18n("Segmentation Backend"));
    segmentationBackendSection->setPrimaryWidget(backendSelect);
    selectionWidget->insertWidget(2, "segmentationBackendSection", segmentationBackendSection);

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
