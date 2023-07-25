#ifndef SEGMENTATION_TOOL_COMMON_H_
#define SEGMENTATION_TOOL_COMMON_H_

#define DLIMGEDIT_LOAD_DYNAMIC
#define DLIMGEDIT_NO_FILESYSTEM
#include <dlimgedit/dlimgedit.hpp>

#include "kis_image.h"
#include "kis_paint_device.h"
#include "kis_pixel_selection.h"
#include "kis_tool_select_base.h"

#include <QImage>
#include <QList>
#include <QPoint>
#include <QRect>

class KisProcessingApplicator;

namespace SegmentationToolCommon
{

dlimgedit::Environment *initLibrary();

KisPaintDeviceSP
mergeColorLayers(KisImageSP const &image, QList<int> const &selectedLayers, KisProcessingApplicator &applicator);

struct Image {
    QImage data;
    dlimgedit::ImageView view;

    QRect rect() const
    {
        return QRect(0, 0, data.width(), data.height());
    }
};

Image prepareImage(KisPaintDevice const &);

void adjustSelection(KisPixelSelectionSP const &selection, int grow, int feather, bool antiAlias);

dlimgedit::Point toPoint(QPoint const &);
dlimgedit::Region toRegion(QRect const &);

} // namespace SegmentationToolCommon

#endif // SEGMENTATION_TOOL_COMMON_H_