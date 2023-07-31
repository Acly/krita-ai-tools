#ifndef SELECT_SEGMENT_FROM_POINT_TOOL_H_
#define SELECT_SEGMENT_FROM_POINT_TOOL_H_

#include "KisSelectionToolFactoryBase.h"
#include "SegmentationToolCommon.h"
#include "kis_tool_select_base.h"
#include <kis_icon.h>

class SelectSegmentFromPointTool : public KisToolSelect
{
    Q_OBJECT

public:
    explicit SelectSegmentFromPointTool(KoCanvasBase *canvas, QSharedPointer<SegmentationToolShared>);

    QWidget *createOptionWidget() override;
    void paint(QPainter &painter, const KoViewConverter &converter) override;

    void beginPrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;

    void resetCursorStyle() override;

protected:
    bool wantsAutoScroll() const override
    {
        return false;
    }

    bool isPixelOnly() const override
    {
        return true;
    }

    bool usesColorLabels() const override
    {
        return true;
    }

public Q_SLOTS:
    void activate(const QSet<KoShape *> &shapes) override;
    void deactivate() override;
    void updateImage(QRect const &);

protected:
    using KisToolSelectBase::m_widgetHelper;

private:
    SegmentationToolHelper m_segmentation;
};

class SelectSegmentFromPointToolFactory : public KisSelectionToolFactoryBase
{
public:
    SelectSegmentFromPointToolFactory(QSharedPointer<SegmentationToolShared> shared)
        : KisSelectionToolFactoryBase("SelectSegmentFromPointTool")
        , m_shared(std::move(shared))
    {
        setToolTip(i18n("Segment Selection Tool"));
        setSection(ToolBoxSection::Select);
        setIconName(koIconNameCStr("tool_segmentation_point"));
        setPriority(9);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    }

    KoToolBase *createTool(KoCanvasBase *canvas) override
    {
        return new SelectSegmentFromPointTool(canvas, m_shared);
    }

private:
    QSharedPointer<SegmentationToolShared> m_shared;
};

#endif // SELECT_SEGMENT_FROM_POINT_TOOL_H_