#include "VisionMLPlugin.h"
#include "VisionML.h"
#include "inpaint/InpaintTool.h"
#include "segmentation/SelectSegmentFromPointTool.h"
#include "segmentation/SelectSegmentFromRectTool.h"

#include <KoToolRegistry.h>
#include <kis_debug.h>
#include <kis_global.h>
#include <kis_types.h>
#include <kpluginfactory.h>

K_PLUGIN_FACTORY_WITH_JSON(VisionMLPluginFactory, "kritavisionml.json", registerPlugin<VisionMLPlugin>();)

VisionMLPlugin::VisionMLPlugin(QObject *parent, const QVariantList &)
    : QObject(parent)
{
    if (QSharedPointer<VisionModels> shared = VisionModels::create()) {
        KoToolRegistry::instance()->add(new SelectSegmentFromPointToolFactory(shared));
        KoToolRegistry::instance()->add(new SelectSegmentFromRectToolFactory(shared));
        KoToolRegistry::instance()->add(new InpaintToolFactory(shared));
    }
}

VisionMLPlugin::~VisionMLPlugin()
{
}

#include "VisionMLPlugin.moc"