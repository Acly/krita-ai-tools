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

class SegmentationToolHelper
{
public:
    SegmentationToolHelper();

    struct ImageInput {
        KoCanvasBase *canvas = nullptr;
        KisNodeSP node;
        KisImageSP image;
        int sampleLayersMode = 0;
        QList<int> colorLabelsSelected;
    };

    struct SelectionOptions {
        SelectionAction action;
        int grow;
        int feather;
        bool antiAlias;
    };

    void processImage(ImageInput const &, KisProcessingApplicator &);
    void processImage(ImageInput const &);

    void applySelectionMask(ImageInput const &, QVariant const &pointOrRect, SelectionOptions const &);

    void notifyImageChanged()
    {
        m_requiresUpdate = true;
    }

private:
    dlimg::Environment const &m_env = nullptr;
    dlimg::Segmentation m_segmentation = nullptr;
    ImageInput m_lastInput;
    bool m_requiresUpdate = true;
};

#endif // SEGMENTATION_TOOL_COMMON_H_