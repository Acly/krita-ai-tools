#include "DLImgEditLibrary.h"
#include "KoResourcePaths.h"

#include <QCoreApplication>
#include <QDebug>
#include <QLibrary>

#include <memory>
#include <string>

namespace
{

bool openLibrary(QLibrary &library)
{
    library.setFileName("dlimgedit");
    if (!library.load()) {
        qWarning() << "[krita-ai-tools] Failed to load library 'dlimgedit' from path"
                   << QCoreApplication::applicationDirPath() << ":" << library.errorString();
        return false;
    }
    using dlimgInitType = decltype(dlimg_init) *;
    dlimgInitType dlimgInit = reinterpret_cast<dlimgInitType>(library.resolve("dlimg_init"));
    dlimgedit::initialize(dlimgInit());
    return true;
}

std::unique_ptr<dlimgedit::Environment> createEnvironment()
{
    std::string modelPath = QString(KoResourcePaths::getApplicationRoot() + "/share/krita/ai_models").toStdString();
    dlimgedit::Options opts;
    opts.model_path = modelPath.c_str();
    opts.device = dlimgedit::Device::cpu;
    try {
        return std::unique_ptr<dlimgedit::Environment>(new dlimgedit::Environment(opts));
    } catch (const std::exception &e) {
        qWarning() << "[krita-ai-tools] Failed to initialize:" << e.what();
    }
    return nullptr;
}

struct Global {
    QLibrary library;
    std::unique_ptr<dlimgedit::Environment> environment;
} global;

} // namespace

dlimgedit::Environment *initDLImgEditLibrary()
{
    if (!global.library.isLoaded()) {
        if (openLibrary(global.library)) {
            global.environment = createEnvironment();
        }
    }
    return global.environment.get();
}
