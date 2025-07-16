#include "VisionMLPlugin.h"
#include "VisionML.h"
#include "filters/BackgroundRemovalFilter.h"
#include "inpaint/InpaintTool.h"
#include "segmentation/SelectSegmentFromPointTool.h"
#include "segmentation/SelectSegmentFromRectTool.h"

#include <KoToolManager_p.h>
#include <KoToolRegistry.h>
#include <QDebug>
#include <filter/kis_filter_registry.h>
#include <kis_debug.h>
#include <kis_global.h>
#include <kis_types.h>
#include <kpluginfactory.h>

K_PLUGIN_FACTORY_WITH_JSON(VisionMLPluginFactory, "kritavisionml.json", registerPlugin<VisionMLPlugin>();)

VisionMLPlugin::VisionMLPlugin(QObject *parent, const QVariantList &)
    : QObject(parent)
{
    if (QSharedPointer<VisionModels> shared = VisionModels::create()) {
        auto addTool = [this](KoToolFactoryBase *toolFactory) {
            qDebug() << "[VisionMLPlugin] Registering tool factory:" << toolFactory->id();
            KoToolRegistry::instance()->add(toolFactory);
            m_toolIds.append(toolFactory->id());
        };

        addTool(new SelectSegmentFromPointToolFactory(shared));
        addTool(new SelectSegmentFromRectToolFactory(shared));
        addTool(new InpaintToolFactory(shared));

        KisFilterRegistry::instance()->add(new BackgroundRemovalFilter(shared));
    } else {
        qWarning() << "[VisionMLPlugin] Failed to create VisionModels instance. Tools will not be available.";
    }
}

VisionMLPlugin::~VisionMLPlugin()
{
}

// Injects tool actions into KoToolManager directly. Usually it polls them from KoToolRegistry when it's created. But
// when loading the plugin via Python, this has already happened. Tools won't show up unless they're added to the list
// manually.
void VisionMLPlugin::injectTools()
{
    KoToolRegistry *registry = KoToolRegistry::instance();
    KoToolManager::Private *p = KoToolManager::instance()->priv();

    for (const QString &id : registry->keys()) {
        if (m_toolIds.contains(id)) {
            qDebug() << "[VisionMLPlugin] Injecting tool action for" << id;
            p->toolActionList.append(new KoToolAction(registry->value(id)));
        }
    }
}

extern "C" {

// Entry point for loading the plugin dynamically from Python, rather than letting Krita discover it from the
// installation plugins folder.
Q_DECL_EXPORT void load_vision_ml_plugin()
{
    qDebug() << "[VisionMLPlugin] Loading VisionML plugin via external loader.";
    VisionMLPlugin plugin(nullptr, QVariantList());
    plugin.injectTools();
}

} // extern "C"

#include "VisionMLPlugin.moc"