#ifndef SELECT_SEGMENT_FROM_RECT_TOOL_H_
#define SELECT_SEGMENT_FROM_RECT_TOOL_H_

#include "KisSelectionToolFactoryBase.h"
#include "SegmentationToolHelper.h"
#include "kis_tool_rectangle_base.h"
#include "kis_tool_select_base.h"
#include <kis_icon.h>

class RectangleForSegmentationTool : public KisToolRectangleBase
{
    Q_OBJECT
public:
    RectangleForSegmentationTool(KoCanvasBase *canvas);
};

class SelectSegmentFromRectTool : public KisToolSelectBase<RectangleForSegmentationTool>
{
    Q_OBJECT
public:
    using Base = KisToolSelectBase<RectangleForSegmentationTool>;

    explicit SelectSegmentFromRectTool(KoCanvasBase *canvas, QSharedPointer<VisionModels>);
    QWidget *createOptionWidget() override;
    void resetCursorStyle() override;

    void beginPrimaryAction(KoPointerEvent *) override;

private:
    void finishRect(const QRectF &rect, qreal roundCornersX, qreal roundCornersY) override;
    void beginShape() override;
    void endShape() override;

public Q_SLOTS:
    void deactivate() override;

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

private:
    SegmentationToolHelper m_segmentation;
};

class SelectSegmentFromRectToolFactory : public KisSelectionToolFactoryBase
{
public:
    SelectSegmentFromRectToolFactory(QSharedPointer<VisionModels> shared)
        : KisSelectionToolFactoryBase("SelectSegmentFromRectTool")
        , m_shared(std::move(shared))
    {
        setToolTip(i18n("Segment Selection Tool (Box)"));
        setSection(ToolBoxSection::Select);
        setIconName(koIconNameCStr("tool_segmentation_rect"));
        setPriority(10);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    }

    KoToolBase *createTool(KoCanvasBase *canvas) override
    {
        return new SelectSegmentFromRectTool(canvas, m_shared);
    }

private:
    QSharedPointer<VisionModels> m_shared;
};

#endif // SELECT_SEGMENT_FROM_RECT_TOOL_H_