#include "SegmentationToolCommon.h"

#include "KoResourcePaths.h"
#include "commands_new/KisMergeLabeledLayersCommand.h"
#include "kis_image_animation_interface.h"
#include "kis_paint_device.h"
#include "kis_selection.h"
#include "kis_selection_filters.h"

#include <QCoreApplication>
#include <QDebug>
#include <QImage>
#include <QLibrary>
#include <QRect>

#include <memory>
#include <string>

namespace SegmentationToolCommon
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
    dlimgedit::initialize(dlimgInit());
    return true;
}

std::unique_ptr<dlimgedit::Environment> createEnvironment()
{
    std::string modelPath = QString(KoResourcePaths::getApplicationRoot() + "/share/krita/ai_models").toStdString();
    dlimgedit::Options opts;
    opts.model_path = modelPath.c_str();
    opts.device = dlimgedit::Device::cpu;
    try {
        return std::unique_ptr<dlimgedit::Environment>(new dlimgedit::Environment(opts));
    } catch (const std::exception &e) {
        qWarning() << "[krita-ai-tools] Failed to initialize:" << e.what();
    }
    return nullptr;
}

struct Global {
    QLibrary library;
    std::unique_ptr<dlimgedit::Environment> environment;
} global;

dlimgedit::Environment *initLibrary()
{
    if (!global.library.isLoaded()) {
        if (openLibrary(global.library)) {
            global.environment = createEnvironment();
        }
    }
    return global.environment.get();
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

Image prepareImage(KisPaintDevice const &device)
{
    Image result;
    QRect bounds = device.defaultBounds()->bounds();
    result.data = device.convertToQImage(nullptr, bounds);

    dlimgedit::Channels channels = dlimgedit::Channels::rgba;
    switch (result.data.format()) {
    case QImage::Format_RGBA8888:
        // case QImage::Format_ARGB32:
        channels = dlimgedit::Channels::rgba;
        break;
    case QImage::Format_RGB888:
        channels = dlimgedit::Channels::rgb;
        break;
    default:
        qDebug() << "[krita-ai-tools] Converting from" << result.data.format() << " to RGBA8888";
        result.data = result.data.convertToFormat(QImage::Format_RGBA8888);
        break;
    }
    result.view = dlimgedit::ImageView(result.data.bits(), {result.data.width(), result.data.height()}, channels);
    return result;
}

void adjustSelection(KisPixelSelectionSP const &selection, int grow, int feather, bool antiAlias)
{
    if (grow > 0) {
        KisGrowSelectionFilter biggy(grow, grow);
        biggy.process(selection, selection->selectedRect().adjusted(-grow, -grow, grow, grow));
    } else if (grow < 0) {
        KisShrinkSelectionFilter tiny(-grow, -grow, false);
        tiny.process(selection, selection->selectedRect());
    }
    if (feather > 0) {
        KisFeatherSelectionFilter feathery(feather);
        feathery.process(selection, selection->selectedRect().adjusted(-feather, -feather, feather, feather));
    } else if (antiAlias) {
        KisAntiAliasSelectionFilter antiAliasFilter;
        antiAliasFilter.process(selection, selection->selectedRect());
    }
}

dlimgedit::Point toPoint(QPoint const &point)
{
    return dlimgedit::Point{point.x(), point.y()};
}

dlimgedit::Region toRegion(QRect const &rect)
{
    return dlimgedit::Region(toPoint(rect.topLeft()), toPoint(rect.bottomRight()));
}

} // namespace SegmentationToolCommon
