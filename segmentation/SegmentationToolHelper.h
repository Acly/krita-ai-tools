#ifndef SEGMENTATION_TOOL_COMMON_H_
#define SEGMENTATION_TOOL_COMMON_H_

#include "SegmentationToolShared.h"

#include "kis_image.h"
#include "kis_paint_device.h"
#include "kis_pixel_selection.h"
#include "kis_tool_select_base.h"

#include <QImage>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QSharedPointer>

class KisProcessingApplicator;
class KoGroupButton;

// Class which implements the shared functionality for segmentation tools. Each tool has its own instance.
class SegmentationToolHelper : QObject
{
    Q_OBJECT
public:
    explicit SegmentationToolHelper(QSharedPointer<SegmentationToolShared>);

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

    void addOptions(KisSelectionOptions *);

    void processImage(ImageInput const &, KisProcessingApplicator &);
    void processImage(ImageInput const &);

    void applySelectionMask(ImageInput const &, QVariant const &pointOrRect, SelectionOptions const &);

    void notifyImageChanged()
    {
        m_requiresUpdate = true;
    }

public Q_SLOTS:
    void switchBackend(KoGroupButton *, bool);
    void updateBackend(dlimg::Backend);

private:
    QSharedPointer<SegmentationToolShared> m_shared;
    dlimg::Segmentation m_segmentation = nullptr;
    ImageInput m_lastInput;
    bool m_requiresUpdate = true;

    KoGroupButton *m_backendCPUButton = nullptr;
    KoGroupButton *m_backendGPUButton = nullptr;
};

#endif // SEGMENTATION_TOOL_COMMON_H_