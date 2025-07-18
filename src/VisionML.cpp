#include "VisionML.h"

#include "KisOptionButtonStrip.h"
#include "KoColorSpace.h"
#include "KoJsonTrader.h"
#include "KoResourcePaths.h"
#include "kis_icon_utils.h"
#include "kis_paint_device.h"
#include <klocalizedstring.h>
#include <ksharedconfig.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMutexLocker>
#include <QString>
#include <QToolButton>
#include <QUrl>

#include <string>

#include <ggml-backend.h>

namespace
{

struct Paths {
    QString plugin;
    QString lib;
    QString models;
} paths;

void initPaths()
{
    QString user = KoResourcePaths::getAppDataLocation();
    paths.plugin = user + "/pykrita/ai_tools/";
    paths.lib = paths.plugin + "lib/";
    paths.models = paths.plugin + "models/";

    if (!QDir(paths.plugin).exists()) {
        throw std::runtime_error("Plugin directory not found (expected at " + paths.plugin.toStdString() + ")");
    }
}

void loadGGMLBackend(char const *name)
{
#if defined(WIN32)
    char const *ext = "dll";
#elif defined(__APPLE__)
    char const *ext = "dylib";
#else
    char const *ext = "so";
#endif
    QString path = QString("%1ggml-%2.%3").arg(paths.lib, name, ext);
    ggml_backend_load(path.toUtf8().constData());
}

QString findModelPath(VisionMLTask task)
{
    switch (task) {
    case VisionMLTask::segmentation:
        return paths.models + "sam";
    case VisionMLTask::background_removal:
        return paths.models + "birefnet";
    case VisionMLTask::inpainting:
        return paths.models + "migan";
    default:
        return paths.models;
    }
}

} // namespace

QSharedPointer<VisionModels> VisionModels::create()
{
    initPaths();

    QSharedPointer<VisionModels> result(new VisionModels());
    if (!result->m_backend) {
        return nullptr;
    }
    return result;
}

VisionModels::VisionModels()
{
    loadGGMLBackend("cpu");
    loadGGMLBackend("vulkan");

    m_config = KSharedConfig::openConfig()->group("VisionML");
    QString backendString = m_config.readEntry("backend", "cpu");
    visp::backend_type backendType = backendString == "gpu" ? visp::backend_type::gpu : visp::backend_type::cpu;

    m_modelName[(int)VisionMLTask::segmentation] = m_config.readEntry("model_0", "sam/MobileSAM.gguf");
    m_modelName[(int)VisionMLTask::inpainting] = m_config.readEntry("model_1", "migan/MIGAN_512_places2-F16.gguf");
    m_modelName[(int)VisionMLTask::background_removal] =
        m_config.readEntry("model_2", "birefnet/BirefNet_lite-F16.gguf");

    QString err = initialize(backendType);
    if (!err.isEmpty()) {
        QMessageBox::warning(nullptr,
                             i18nc("@title:window", "Krita - VisionML Plugin"),
                             i18n("Failed to initialize AI tools plugin.\n") + err);
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
        QByteArray path = modelPath(VisionMLTask::segmentation);
        m_sam = visp::sam_load_model(path.data(), m_backend);
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
        QByteArray path = modelPath(VisionMLTask::background_removal);
        m_birefnet = visp::birefnet_load_model(path.data(), m_backend);
    }
    return visp::birefnet_compute(m_birefnet, image, m_backend);
}

visp::image_data VisionModels::inpaint(visp::image_view const &image, visp::image_view const &mask)
{
    QMutexLocker lock(&m_mutex);
    if (!m_migan.weights) {
        unloadModels();
        QByteArray path = modelPath(VisionMLTask::inpainting);
        m_migan = visp::migan_load_model(path.data(), m_backend);
    }
    return visp::migan_compute(m_migan, image, mask, m_backend);
}

QByteArray VisionModels::modelPath(VisionMLTask task) const
{
    QString path = paths.models + modelName(task);
    if (!QFile::exists(path)) {
        throw std::runtime_error("Model file not found: " + path.toStdString());
    }
    return path.toUtf8();
}

visp::backend_type VisionModels::backend() const
{
    return m_backendType;
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

QString const &VisionModels::modelName(VisionMLTask task) const
{
    return m_modelName[(int)task];
}

void VisionModels::setModelName(VisionMLTask task, QString const &name)
{
    if (modelName(task) == name) {
        return; // no change
    }
    QMutexLocker lock(&m_mutex);
    m_modelName[(int)task] = name;
    m_config.writeEntry(QString("model_%1").arg((int)task), name);
    unloadModels();
    Q_EMIT modelNameChanged(task, name);
}

QString VisionModels::backendDeviceDescription() const
{
    ggml_backend_dev_t dev = ggml_backend_get_device(m_backend);
    char const *name = ggml_backend_dev_name(dev);
    char const *desc = ggml_backend_dev_description(dev);
    return QString("%1 [%2]").arg(QString(desc).trimmed(), name);
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
// VisionMLImage

VisionMLImage VisionMLImage::prepare(KisPaintDevice const &device, QRect bounds)
{
    VisionMLImage result;
    if (bounds.isEmpty()) {
        bounds = device.exactBounds();
    }
    if (bounds.isEmpty()) {
        return result; // Can happen eg. when using color label mode without matching layers.
    }
    KoColorSpace const *cs = device.colorSpace();
    if (cs->pixelSize() == 4 && cs->id() == "RGBA") {
        // Stored as BGRA, 8 bits per channel in Krita. No conversions for now, the segmentation network expects
        // gamma-compressed sRGB, but works fine with other color spaces (probably).
        result.view.format = visp::image_format::bgra_u8;
        result.data = QImage(bounds.width(), bounds.height(), QImage::Format_ARGB32);
        device.readBytes(result.data.bits(), bounds.x(), bounds.y(), bounds.width(), bounds.height());
    } else {
        // Convert everything else to QImage::Format_ARGB32 in default color space (sRGB).
        result.view.format = visp::image_format::argb_u8;
        result.data = device.convertToQImage(nullptr, bounds);
    }
    result.view.extent = {result.data.width(), result.data.height()};
    result.view.stride = result.data.bytesPerLine();
    result.view.data = result.data.bits();
    return result;
}

// Convert outputs to QImage - this is mainly because they're RGBA, but Krita paint device uses BGRA internally (but may
// also use some other color space).
QImage VisionMLImage::convertToQImage(visp::image_view const &img, QRect b)
{
    if (img.format != visp::image_format::rgba_u8) {
        throw std::runtime_error("Unsupported image format for conversion to QImage");
    }

    QImage result(b.width(), b.height(), QImage::Format_RGBA8888);
    // copy scanlines, row stride might be different
    size_t rowSize = b.width() * n_bytes(img.format);
    size_t rowStride = img.extent[0] * n_bytes(img.format);
    for (int y = 0; y < b.height(); ++y) {
        memcpy(result.scanLine(y), ((uint8_t const *)img.data) + (y + b.y()) * rowStride, rowSize);
    }
    return result;
}

//
// VisionMLBackendWidget

VisionMLBackendWidget::VisionMLBackendWidget(QSharedPointer<VisionModels> shared, bool showDevice, QWidget *parent)
    : KisOptionCollectionWidgetWithHeader(i18n("Backend"), parent)
    , m_shared(std::move(shared))
{
    QWidget *widget = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    KisOptionButtonStrip *strip = new KisOptionButtonStrip;
    m_cpuButton = strip->addButton(i18n("CPU"));
    m_gpuButton = strip->addButton(i18n("GPU"));
    layout->addWidget(strip);

    if (showDevice) {
        m_deviceLabel = new QLabel;
        layout->addWidget(m_deviceLabel);
    }

    setPrimaryWidget(widget);

    connect(strip, SIGNAL(buttonToggled(KoGroupButton *, bool)), this, SLOT(switchBackend(KoGroupButton *, bool)));
    connect(m_shared.get(), SIGNAL(backendChanged(visp::backend_type)), this, SLOT(updateBackend(visp::backend_type)));

    updateBackend(m_shared->backend());
}

void VisionMLBackendWidget::updateBackend(visp::backend_type backend)
{
    m_cpuButton->setChecked(backend == visp::backend_type::cpu);
    m_gpuButton->setChecked(backend == visp::backend_type::gpu);

    if (m_deviceLabel) {
        m_deviceLabel->setText(QString(m_shared->backendDeviceDescription()).trimmed());
    }
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
// VisionMLModelSelect

VisionMLModelSelect::VisionMLModelSelect(QSharedPointer<VisionModels> models,
                                         VisionMLTask task,
                                         bool showFolderButton,
                                         QWidget *parent)
    : KisOptionCollectionWidgetWithHeader(i18n("Model"), parent)
    , m_shared(std::move(models))
    , m_task(task)
{
    QWidget *widget = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    m_select = new QComboBox;
    updateModels();
    updateModel(m_task, m_shared->modelName(m_task));
    connect(m_select, SIGNAL(currentIndexChanged(int)), this, SLOT(switchModel(int)));
    connect(m_shared.get(), &VisionModels::modelNameChanged, this, &VisionMLModelSelect::updateModel);
    layout->addWidget(m_select);

    if (showFolderButton) {
        QToolButton *folderButton = new QToolButton;
        folderButton->setIcon(KisIconUtils::loadIcon("document-open"));
        folderButton->setFixedSize(24, 24);
        folderButton->setToolTip(i18n("Open models folder"));
        connect(folderButton, &QToolButton::clicked, this, &VisionMLModelSelect::openModelsFolder);
        layout->addWidget(folderButton);
    }

    m_fileWatcher = new QFileSystemWatcher(this);
    m_fileWatcher->addPath(findModelPath(m_task));
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged, this, &VisionMLModelSelect::updateModels);

    setPrimaryWidget(widget);
}

void VisionMLModelSelect::updateModels()
{
    m_select->blockSignals(true);

    QVariant current = m_select->currentData();
    m_select->clear();

    auto addModels = [this](const QString &arch) {
        QDir modelDir(paths.models + arch);
        QStringList modelFiles = modelDir.entryList(QStringList() << "*.gguf", QDir::Files);
        for (QString &file : modelFiles) {
            QString fullName = arch + "/" + file;
            m_select->addItem(file.replace(".gguf", ""), fullName);
        }
    };

    switch (m_task) {
    case VisionMLTask::segmentation:
        addModels("sam");
        break;
    case VisionMLTask::background_removal:
        addModels("birefnet");
        break;
    case VisionMLTask::inpainting:
        addModels("migan");
        break;
    default:
        qWarning() << "Unknown VisionMLTask" << (int)m_task;
        return;
    }

    m_select->blockSignals(false);
    if (current.isValid()) {
        if (int index = m_select->findData(current); index != -1) {
            m_select->setCurrentIndex(index);
        }
    }
}

void VisionMLModelSelect::switchModel(int index)
{
    if (index < 0 || index >= m_select->count()) {
        return;
    }
    QString modelName = m_select->itemData(index).toString();
    m_shared->setModelName(m_task, modelName);
}

void VisionMLModelSelect::updateModel(VisionMLTask task, QString const &name)
{
    if (m_task != task) {
        return; // not the model we are interested in
    }
    int index = m_select->findData(name);
    if (index != -1) {
        m_select->setCurrentIndex(index);
    } else {
        m_select->setCurrentIndex(0); // Fallback to the first model if the current one is not found
    }
}

void VisionMLModelSelect::openModelsFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(findModelPath(m_task)));
}

//
// VisionMLErrorReporter

VisionMLErrorReporter::VisionMLErrorReporter(QObject *parent)
    : QObject(parent)
{
    connect(this, &VisionMLErrorReporter::errorOccurred, this, &VisionMLErrorReporter::showError, Qt::QueuedConnection);
}

void VisionMLErrorReporter::showError(QString const &message) const
{
    QMessageBox::warning(nullptr,
                         i18nc("@title:window", "Krita - Vision ML Tools Plugin"),
                         i18n("Error during image processing: ") + message);
}