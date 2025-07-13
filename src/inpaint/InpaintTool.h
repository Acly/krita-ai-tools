#ifndef INPAINT_TOOL_H_
#define INPAINT_TOOL_H_

#include <QKeySequence>
#include <QPainterPath>
#include <QScopedPointer>
#include <QSharedPointer>


#include "KisToolPaintFactoryBase.h"
#include "kis_tool_paint.h"

#include <KoIcon.h>
#include <flake/kis_node_shape.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kis_icon.h>

#include "VisionML.h"

class KisKActionCollection;
class KoCanvasBase;
class KisPaintInformation;
class KisSpacingInformation;

// Copied from KisToolSmartPatch (kis_tool_smart_patch.h)
// to provide it as optional plugin without modifying the original tool.

class InpaintTool : public KisToolPaint
{
    Q_OBJECT
public:
    InpaintTool(KoCanvasBase *canvas, QSharedPointer<VisionModels>);
    ~InpaintTool() override;

    QWidget *createOptionWidget() override;

    void activatePrimaryAction() override;
    void deactivatePrimaryAction() override;

    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;
    void paint(QPainter &painter, const KoViewConverter &converter) override;
    int flags() const override
    {
        return KisTool::FLAG_USES_CUSTOM_SIZE | KisTool::FLAG_USES_CUSTOM_PRESET;
    }

protected Q_SLOTS:
    void resetCursorStyle() override;

public Q_SLOTS:
    void activate(const QSet<KoShape *> &shapes) override;
    void deactivate() override;

private:
    QPainterPath getBrushOutlinePath(const QPointF &documentPos, const KoPointerEvent *event);
    QPainterPath brushOutline();
    void requestUpdateOutline(const QPointF &outlineDocPoint, const KoPointerEvent *event) override;

private:
    struct Private;
    const QScopedPointer<Private> m_d;

    void addMaskPath(KoPointerEvent *event);
};

class InpaintToolFactory : public KisToolPaintFactoryBase
{
public:
    InpaintToolFactory(QSharedPointer<VisionModels> shared)
        : KisToolPaintFactoryBase("InpaintTool")
        , m_shared(std::move(shared))
    {
        setToolTip(i18n("Smart Fill Tool"));

        setSection(ToolBoxSection::Fill);
        setIconName(koIconNameCStr("tool_inpaint"));
        setPriority(9);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    }

    ~InpaintToolFactory() override
    {
    }

    KoToolBase *createTool(KoCanvasBase *canvas) override
    {
        return new InpaintTool(canvas, m_shared);
    }

private:
    QSharedPointer<VisionModels> m_shared;
};

#endif // INPAINT_TOOL_H_
