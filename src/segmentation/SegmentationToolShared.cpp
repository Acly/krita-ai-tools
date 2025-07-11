#include "SegmentationToolShared.h"

#include "KoJsonTrader.h"
#include "KoResourcePaths.h"
#include <klocalizedstring.h>
#include <ksharedconfig.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QMutexLocker>
#include <QString>

#include <string>

namespace
{

QString findModelPath()
{
    return KoResourcePaths::getApplicationRoot() + "share/krita/ai_models";
}

}

QSharedPointer<SegmentationToolShared> SegmentationToolShared::create()
{
    QSharedPointer<SegmentationToolShared> result(new SegmentationToolShared());
    if (!result->m_backend) {
        return nullptr;
    }
    return result;
}

SegmentationToolShared::SegmentationToolShared()
{
    m_config = KSharedConfig::openConfig()->group("SegmentationToolPlugin");
    QString backendString = m_config.readEntry("backend", "cpu");
    visp::backend_type backendType = backendString == "gpu" ? visp::backend_type::gpu : visp::backend_type::cpu;

    QString err = initialize(backendType);
    if (!err.isEmpty()) {
        QMessageBox::warning(nullptr,
                             i18nc("@title:window", "Krita - Segmentation Tools Plugin"),
                             i18n("Failed to initialize segmentation tool plugin.\n") + err);
        return;
    }

    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), this, SLOT(cleanUp()));
}

QString SegmentationToolShared::initialize(visp::backend_type backendType)
{
    QMutexLocker lock(&m_mutex);
    try {
        m_backend = visp::backend_init(backendType);
    } catch (const std::exception &e) {
        return QString(e.what());
    }
    m_backendType = backendType;
    m_sam = {};
    m_birefnet = {};
    m_config.writeEntry("backend", backendType == visp::backend_type::gpu ? "gpu" : "cpu");
    return QString();
}

void SegmentationToolShared::encodeImage(visp::image_view const &image)
{
    QMutexLocker lock(&m_mutex);
    if (!m_sam.weights) {
        QByteArray modelPath = (findModelPath() + "/sam/MobileSAM.gguf").toUtf8();
        m_sam = visp::sam_load_model(modelPath.data(), m_backend);
    }
    visp::sam_encode(m_sam, image, m_backend);
}

bool SegmentationToolShared::hasEncodedImage() const
{
    return m_sam.input_image != nullptr;
}

visp::image_data SegmentationToolShared::predictMask(visp::i32x2 point)
{
    QMutexLocker lock(&m_mutex);
    return visp::sam_compute(m_sam, point, m_backend);
}

visp::image_data SegmentationToolShared::predictMask(visp::image_rect box)
{
    QMutexLocker lock(&m_mutex);
    return visp::sam_compute(m_sam, box, m_backend);
}

visp::image_data SegmentationToolShared::removeBackground(visp::image_view const &image)
{
    QMutexLocker lock(&m_mutex);
    if (!m_birefnet.weights) {
        QByteArray modelPath = (findModelPath() + "/birefnet/BiRefNet_lite-F16.gguf").toUtf8();
        m_birefnet = visp::birefnet_load_model(modelPath.data(), m_backend);
    }
    return visp::birefnet_compute(m_birefnet, image, m_backend);
}

bool SegmentationToolShared::setBackend(visp::backend_type backendType)
{
    if (backendType == m_backendType) {
        return true;
    }
    QString err = initialize(backendType);
    if (!err.isEmpty()) {
        QMessageBox::warning(nullptr,
                             i18nc("@title:window", "Krita - Vision ML Tools Plugin"),
                             i18n("Error while trying to switch inference backend.\n") + err);
        return false;
    }
    Q_EMIT backendChanged(m_backendType);
    return true;
}

void SegmentationToolShared::cleanUp()
{
    // This would run in the destructor anyway, but because the plugin manager which keeps this
    // object alive is static, it may happen too late and in arbitrary order. Dynamic libraries
    // which the plugin relies on may already be gone.
    m_sam = {};
    m_birefnet = {};
    m_backend = {};
}
