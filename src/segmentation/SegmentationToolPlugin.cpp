#include "SegmentationToolPlugin.h"
#include "SegmentationToolShared.h"
#include "SelectSegmentFromPointTool.h"
#include "SelectSegmentFromRectTool.h"

#include <KoToolRegistry.h>
#include <kis_debug.h>
#include <kis_global.h>
#include <kis_types.h>
#include <kpluginfactory.h>

K_PLUGIN_FACTORY_WITH_JSON(SegmentationToolPluginFactory,
                           "kritatoolsegmentation.json",
                           registerPlugin<SegmentationToolPlugin>();)

SegmentationToolPlugin::SegmentationToolPlugin(QObject *parent, const QVariantList &)
    : QObject(parent)
{
    if (QSharedPointer<SegmentationToolShared> shared = SegmentationToolShared::create()) {
        KoToolRegistry::instance()->add(new SelectSegmentFromPointToolFactory(shared));
        KoToolRegistry::instance()->add(new SelectSegmentFromRectToolFactory(shared));
    }
}

SegmentationToolPlugin::~SegmentationToolPlugin()
{
}

#include "SegmentationToolPlugin.moc"