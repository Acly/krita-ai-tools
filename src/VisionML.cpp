#include "VisionML.h"

#include "KisOptionButtonStrip.h"
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

QSharedPointer<VisionModels> VisionModels::create()
{
    QSharedPointer<VisionModels> result(new VisionModels());
    if (!result->m_backend) {
        return nullptr;
    }
    return result;
}

VisionModels::VisionModels()
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

QString VisionModels::initialize(visp::backend_type backendType)
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
    m_migan = {};
    m_config.writeEntry("backend", backendType == visp::backend_type::gpu ? "gpu" : "cpu");
    return QString();
}

void VisionModels::encodeSegmentationImage(visp::image_view const &image)
{
    QMutexLocker lock(&m_mutex);
    if (!m_sam.weights) {
        unloadModels();
        QByteArray modelPath = (findModelPath() + "/sam/MobileSAM.gguf").toUtf8();
        m_sam = visp::sam_load_model(modelPath.data(), m_backend);
    }
    visp::sam_encode(m_sam, image, m_backend);
}

bool VisionModels::hasSegmentationImage() const
{
    return m_sam.input_image != nullptr;
}

visp::image_data VisionModels::predictSegmentationMask(visp::i32x2 point)
{
    QMutexLocker lock(&m_mutex);
    return visp::sam_compute(m_sam, point, m_backend);
}

visp::image_data VisionModels::predictSegmentationMask(visp::image_rect box)
{
    QMutexLocker lock(&m_mutex);
    return visp::sam_compute(m_sam, box, m_backend);
}

visp::image_data VisionModels::removeBackground(visp::image_view const &image)
{
    QMutexLocker lock(&m_mutex);
    if (!m_birefnet.weights) {
        unloadModels();
        QByteArray modelPath = (findModelPath() + "/birefnet/BiRefNet_lite-F16.gguf").toUtf8();
        m_birefnet = visp::birefnet_load_model(modelPath.data(), m_backend);
    }
    return visp::birefnet_compute(m_birefnet, image, m_backend);
}

visp::image_data VisionModels::inpaint(visp::image_view const &image, visp::image_view const &mask)
{
    QMutexLocker lock(&m_mutex);
    if (!m_migan.weights) {
        unloadModels();
        QByteArray modelPath = (findModelPath() + "/migan/MIGAN_512_places2-F16.gguf").toUtf8();
        m_migan = visp::migan_load_model(modelPath.data(), m_backend);
    }
    return visp::migan_compute(m_migan, image, mask, m_backend);
}

bool VisionModels::setBackend(visp::backend_type backendType)
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

void VisionModels::unloadModels()
{
    m_sam = {};
    m_birefnet = {};
    m_migan = {};
}

void VisionModels::cleanUp()
{
    // This would run in the destructor anyway, but because the plugin manager which keeps this
    // object alive is static, it may happen too late and in arbitrary order. Dynamic libraries
    // which the plugin relies on may already be gone.
    unloadModels();
    m_backend = {};
}

//
// VisionMLBackendWidget

VisionMLBackendWidget::VisionMLBackendWidget(QSharedPointer<VisionModels> shared, QWidget *parent)
    : KisOptionCollectionWidgetWithHeader(i18n("Backend"), parent)
    , m_shared(std::move(shared))
{
    KisOptionButtonStrip *strip = new KisOptionButtonStrip;
    m_cpuButton = strip->addButton(i18n("CPU"));
    m_cpuButton->setChecked(m_shared->backend() == visp::backend_type::cpu);
    m_gpuButton = strip->addButton(i18n("GPU"));
    m_gpuButton->setChecked(m_shared->backend() == visp::backend_type::gpu);

    setPrimaryWidget(strip);

    connect(strip, SIGNAL(buttonToggled(KoGroupButton *, bool)), this, SLOT(switchBackend(KoGroupButton *, bool)));
    connect(m_shared.get(), SIGNAL(backendChanged(visp::backend_type)), this, SLOT(updateBackend(visp::backend_type)));
}

void VisionMLBackendWidget::updateBackend(visp::backend_type backend)
{
    m_cpuButton->setChecked(backend == visp::backend_type::cpu);
    m_gpuButton->setChecked(backend == visp::backend_type::gpu);
}

void VisionMLBackendWidget::switchBackend(KoGroupButton *button, bool checked)
{
    if (checked) {
        bool success = m_shared->setBackend(button == m_cpuButton ? visp::backend_type::cpu : visp::backend_type::gpu);
        if (!success) {
            button->setEnabled(false);
            KoGroupButton *prev = m_shared->backend() == visp::backend_type::cpu ? m_cpuButton : m_gpuButton;
            bool blocked = prev->blockSignals(true);
            prev->setChecked(true);
            prev->blockSignals(blocked);
        }
    }
}

//
// VisionMLErrorReporter

VisionMLErrorReporter::VisionMLErrorReporter(QObject *parent)
    : QObject(parent)
{
    connect(this, &VisionMLErrorReporter::errorOccurred, this, &VisionMLErrorReporter::showError, Qt::QueuedConnection);
}

void VisionMLErrorReporter::showError(QString const &message)
{
    QMessageBox::warning(nullptr,
                         i18nc("@title:window", "Krita - Vision ML Tools Plugin"),
                         i18n("Error during image processing: ") + message);
}