#ifndef SELECT_SEGMENT_FROM_POINT_TOOL_H_
#define SELECT_SEGMENT_FROM_POINT_TOOL_H_

#include "KisSelectionToolFactoryBase.h"
#include "kis_tool_select_base.h"
#include <kis_icon.h>

namespace dlimgedit
{
class Environment;
}

class SelectSegmentFromPointTool : public KisToolSelect
{
    Q_OBJECT

public:
    SelectSegmentFromPointTool(KoCanvasBase *canvas);
    ~SelectSegmentFromPointTool() override;

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

protected:
    using KisToolSelectBase::m_widgetHelper;

private:
    dlimgedit::Environment *m_dlimgEnv = nullptr;
};

class SelectSegmentFromPointToolFactory : public KisSelectionToolFactoryBase
{
public:
    SelectSegmentFromPointToolFactory()
        : KisSelectionToolFactoryBase("SelectSegmentFromPointTool")
    {
        setToolTip(i18n("Segment Selection Tool"));
        setSection(ToolBoxSection::Select);
        setIconName(koIconNameCStr("tool_contiguous_selection"));
        setPriority(9);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    }

    ~SelectSegmentFromPointToolFactory() override
    {
    }

    KoToolBase *createTool(KoCanvasBase *canvas) override
    {
        return new SelectSegmentFromPointTool(canvas);
    }
};

#endif // SELECT_SEGMENT_FROM_POINT_TOOL_H_