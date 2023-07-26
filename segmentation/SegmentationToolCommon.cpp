#include "SegmentationToolCommon.h"

#include "KoResourcePaths.h"
#include "commands_new/KisMergeLabeledLayersCommand.h"
#include "kis_command_utils.h"
#include "kis_image_animation_interface.h"
#include "kis_paint_device.h"
#include "kis_selection.h"
#include "kis_selection_filters.h"
#include "kis_selection_tool_helper.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QImage>
#include <QLibrary>
#include <QRect>

#include <string>

namespace
{

bool openLibrary(QLibrary &library)
{
    library.setFileName("dlimgedit");
    if (!library.load()) {
        qWarning() << "[krita-ai-tools] Failed to load library 'dlimgedit' from path"
                   << QCoreApplication::applicationDirPath() << ":" << library.errorString();
        return false;
    }
    using dlimgInitType = decltype(dlimg_init) *;
    dlimgInitType dlimgInit = reinterpret_cast<dlimgInitType>(library.resolve("dlimg_init"));
    dlimg::initialize(dlimgInit());
    return true;
}

dlimg::Environment createEnvironment()
{
    std::string modelPath = QString(KoResourcePaths::getApplicationRoot() + "/share/krita/ai_models").toStdString();
    dlimg::Options opts;
    opts.model_path = modelPath.c_str();
    opts.device = dlimg::Device::cpu;
    try {
        return dlimg::Environment(opts);
    } catch (const std::exception &e) {
        qWarning() << "[krita-ai-tools] Failed to initialize:" << e.what();
    }
    return nullptr;
}

struct Global {
    QLibrary library;
    dlimg::Environment environment{nullptr};
} global;

dlimg::Environment const &initLibrary()
{
    if (!global.library.isLoaded()) {
        if (openLibrary(global.library)) {
            global.environment = createEnvironment();
        }
    }
    return global.environment;
}

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
    QRect bounds = device.defaultBounds()->bounds();
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

SegmentationToolHelper::SegmentationToolHelper()
    : m_env(initLibrary())
{
}

void SegmentationToolHelper::processImage(ImageInput const &input, KisProcessingApplicator &applicator)
{
    if (!m_env) {
        return;
    }
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
        [segmentation = &m_segmentation, inputImage, env = &m_env]() mutable -> KUndo2Command * {
            try {
                qDebug() << "[krita-ai-tools] Segmentation started";
                auto image = prepareImage(*inputImage);
                *segmentation = dlimg::Segmentation::process(image.view, *env);
                qDebug() << "[krita-ai-tools] Segmentation finished";
            } catch (const std::exception &e) {
                qWarning() << "[krita-ai-tools] Error during segmentation:" << e.what();
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
    if (!m_env) {
        return;
    }
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
    }

    KisPaintDeviceSP layerImage;
    if (!input.node || !(layerImage = input.node->projection())) {
        return;
    }

    QApplication::setOverrideCursor(KisCursor::waitCursor());

    KisProcessingApplicator applicator(input.image,
                                       input.node,
                                       KisProcessingApplicator::NO_IMAGE_UPDATES, // IS THIS SMART?!?
                                       KisImageSignalVector(),
                                       kundo2_i18n("Select Segment"));

    if (!m_segmentation || m_requiresUpdate || input != m_lastInput) {
        processImage(input, applicator);
    }

    KisPixelSelectionSP selection = new KisPixelSelection(new KisSelectionDefaultBounds(layerImage));

    KUndo2Command *cmd = new KisCommandUtils::LambdaCommand(
        [segmentation = &m_segmentation, prompt, selection, options]() mutable -> KUndo2Command * {
            if (!(*segmentation)) {
                qWarning() << "[krita-ai-tools] Segmentation not ready";
                return nullptr;
            }
            try {
                auto mask = prompt.canConvert<QPoint>() ? segmentation->get_mask(convert(prompt.toPoint()))
                                                        : segmentation->get_mask(convert(prompt.toRect()));
                selection->writeBytes(mask.pixels(), QRect(0, 0, mask.extent().width, mask.extent().height));
                adjustSelection(selection, options);
                selection->invalidateOutlineCache();
            } catch (const std::exception &e) {
                qWarning() << "[krita-ai-tools] Error during segmentation:" << e.what();
            }
            return nullptr;
        });
    applicator.applyCommand(cmd, KisStrokeJobData::BARRIER);

    helper.selectPixelSelection(applicator, selection, options.action);
    applicator.end();

    QApplication::restoreOverrideCursor();
}
