#ifndef SELECT_SEGMENT_FROM_RECT_TOOL_H_
#define SELECT_SEGMENT_FROM_RECT_TOOL_H_

#include "KisSelectionToolFactoryBase.h"
#include "SegmentationToolCommon.h"
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
    explicit SelectSegmentFromRectTool(KoCanvasBase *canvas);
    void resetCursorStyle() override;

private:
    void finishRect(const QRectF &rect, qreal roundCornersX, qreal roundCornersY) override;
    void beginShape() override;
    void endShape() override;

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
    SelectSegmentFromRectToolFactory()
        : KisSelectionToolFactoryBase("SelectSegmentFromRectTool")
    {
        setToolTip(i18n("Segment Selection Tool (Box)"));
        setSection(ToolBoxSection::Select);
        setIconName(koIconNameCStr("tool_segmentation_rect"));
        setPriority(10);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    }

    KoToolBase *createTool(KoCanvasBase *canvas) override
    {
        return new SelectSegmentFromRectTool(canvas);
    }
};

#endif // SELECT_SEGMENT_FROM_RECT_TOOL_H_