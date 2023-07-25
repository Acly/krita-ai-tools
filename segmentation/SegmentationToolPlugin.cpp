#include "SegmentationToolPlugin.h"
#include "SelectSegmentFromPointTool.h"
#include "SelectSegmentFromRectTool.h"

#include <klocalizedstring.h>

#include <kis_debug.h>
#include <kpluginfactory.h>

#include <KoToolRegistry.h>
#include <kis_global.h>
#include <kis_types.h>

K_PLUGIN_FACTORY_WITH_JSON(SegmentationToolPluginFactory,
                           "kritatoolsegmentation.json",
                           registerPlugin<SegmentationToolPlugin>();)

SegmentationToolPlugin::SegmentationToolPlugin(QObject *parent, const QVariantList &)
    : QObject(parent)
{
    KoToolRegistry::instance()->add(new SelectSegmentFromPointToolFactory());
    KoToolRegistry::instance()->add(new SelectSegmentFromRectToolFactory());
}

SegmentationToolPlugin::~SegmentationToolPlugin()
{
}

#include "SegmentationToolPlugin.moc"