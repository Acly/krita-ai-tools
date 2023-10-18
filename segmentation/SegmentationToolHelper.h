#ifndef SEGMENTATION_TOOL_COMMON_H_
#define SEGMENTATION_TOOL_COMMON_H_

#include "SegmentationToolShared.h"

#include "commands_new/KisMergeLabeledLayersCommand.h"
#include "kis_image.h"
#include "kis_paint_device.h"
#include "kis_pixel_selection.h"
#include "kis_tool_select_base.h"

#include <QImage>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QScopedPointer>
#include <QSharedPointer>

class KisProcessingApplicator;
class KoGroupButton;

class SegmentationState : public QObject
{
    Q_OBJECT
public:
    dlimg::Segmentation m = nullptr;

    Q_SIGNAL void errorOccurred(QString const &message);
};

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

    void processImage(ImageInput const &);

    void applySelectionMask(ImageInput const &, QVariant pointOrRect, SelectionOptions const &);

    void notifyImageChanged()
    {
        m_requiresUpdate = true;
    }

    void deactivate();

public Q_SLOTS:
    void switchBackend(KoGroupButton *, bool);
    void updateBackend(dlimg::Backend);
    void reportError(QString const &);

private:
    KisPaintDeviceSP mergeColorLayers(KisImageSP const &, QList<int> const &selectedLayers, KisProcessingApplicator &);
    void processImage(ImageInput const &, KisProcessingApplicator &);

    // UI thread
    QSharedPointer<SegmentationToolShared> m_shared;
    ImageInput m_lastInput;
    QRect m_bounds;
    bool m_requiresUpdate = true;
    KisPaintDeviceSP m_referencePaintDevice;
    KisMergeLabeledLayersCommand::ReferenceNodeInfoListSP m_referenceNodeList;
    int m_previousTime = 0;
    KoGroupButton *m_backendCPUButton = nullptr;
    KoGroupButton *m_backendGPUButton = nullptr;

    // Stroke thread
    SegmentationState m_segmentation;
};

#endif // SEGMENTATION_TOOL_COMMON_H_