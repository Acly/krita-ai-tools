#include "SegmentationToolShared.h"

#include "KoResourcePaths.h"
#include "KoJsonTrader.h"
#include <ksharedconfig.h>
#include <klocalizedstring.h>

#include <QCoreApplication>
#include <QMessageBox>
#include <QString>
#include <QDebug>
#include <QDir>

#include <string>
#ifdef Q_OS_LINUX
#include <dlfcn.h>
#endif

namespace
{

int dummy;
QString findLibPath()
{
#ifdef Q_OS_WIN32
    return QCoreApplication::applicationDirPath();
#else
    // Find path of this SO (should be in the plugin directory)
    Dl_info info{};
    dladdr(&dummy, &info);
    QDir dir(info.dli_fname);
    dir.cdUp();
    dir.cd("toolsegmentation");
    return dir.path();
#endif
}

QString findModelPath()
{
    return KoResourcePaths::getApplicationRoot() + "share/krita/ai_models";
}

bool openLibrary(QLibrary &library)
{
#ifdef Q_OS_WIN32
    QString locationHint = "\nLibrary not found: " + findLibPath() + "/dlimgedit.dll";
    library.setFileName("dlimgedit");
#else
    QString libPath = findLibPath();
    QString locationHint = "\nLibrary not found: " + libPath + "/libdlimgedit.so";
    library.setFileName(libPath + "/dlimgedit");
#endif
    if (library.load()) {
        using dlimgInitType = decltype(dlimg_init) *;
        dlimgInitType dlimgInit = reinterpret_cast<dlimgInitType>(library.resolve("dlimg_init"));
        dlimg::initialize(dlimgInit());
        return true;
    } else {
        QMessageBox::warning(nullptr,
                             i18nc("@title:window", "Krita - Segmentation Tools Plugin"),
                             library.errorString() + locationHint);
        return false;
    }
}

}

QSharedPointer<SegmentationToolShared> SegmentationToolShared::create()
{
    QSharedPointer<SegmentationToolShared> result(new SegmentationToolShared());
    if (!result->m_cpu && !result->m_gpu) {
        return nullptr;
    }
    return result;
}

SegmentationToolShared::SegmentationToolShared()
{
    if (!openLibrary(m_lib)) {
        return;
    }
    m_config = KSharedConfig::openConfig()->group("SegmentationToolPlugin");
    QString backendString = m_config.readEntry("backend", "cpu");
    dlimg::Backend backend = backendString == "gpu" ? dlimg::Backend::gpu : dlimg::Backend::cpu;
    if (backend == dlimg::Backend::gpu && !dlimg::Environment::is_supported(backend)) {
        backend = dlimg::Backend::cpu;
    }

    QString err = initialize(backend);
    if (!err.isEmpty()) {
        QMessageBox::warning(nullptr,
                             i18nc("@title:window", "Krita - Segmentation Tools Plugin"),
                             i18n("Failed to initialize segmentation tool plugin.\n") + err);
        return;
    }

    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), this, SLOT(cleanUp()));
}

QString SegmentationToolShared::initialize(dlimg::Backend backend)
{
    std::string modelDir = findModelPath().toStdString();
    dlimg::Environment &env = backend == dlimg::Backend::gpu ? m_gpu : m_cpu;
    dlimg::Options opts;
    opts.model_directory = modelDir.c_str();
    opts.backend = backend;
    try {
        env = dlimg::Environment(opts);
    } catch (const std::exception &e) {
        return QString(e.what());
    }
    m_backend = backend;
    m_config.writeEntry("backend", backend == dlimg::Backend::gpu ? "gpu" : "cpu");
    return QString();
}

dlimg::Environment const &SegmentationToolShared::environment() const
{
    return m_backend == dlimg::Backend::gpu ? m_gpu : m_cpu;
}

bool SegmentationToolShared::setBackend(dlimg::Backend backend)
{
    if (backend == m_backend || !m_lib.isLoaded()) {
        return true;
    }
    QString err = initialize(backend);
    if (!err.isEmpty()) {
        QMessageBox::warning(nullptr,
                             i18nc("@title:window", "Krita - Segmentation Tools Plugin"),
                             i18n("Error while trying to switch segmentation backend.\n") + err);
        return false;
    }
    Q_EMIT backendChanged(m_backend);
    return true;
}

void SegmentationToolShared::cleanUp()
{
    // This would run in the destructor anyway, but because the plugin manager which keeps this
    // object alive is static, it may happen too late and in arbitrary order. Dynamic libraries
    // which the plugin relies on may already be gone.
    m_gpu = nullptr;
    m_cpu = nullptr;
    m_lib.unload();
}
