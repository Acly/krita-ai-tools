#include "BackgroundRemovalFilter.h"

#include "KisGlobalResourcesInterface.h"
#include "KoUpdater.h"
#include "kis_config_widget.h"
#include "kis_filter_category_ids.h"
#include "kis_paint_device.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QCheckBox>

//
// Configuration widget

class BackgroundRemovalWidget : public KisConfigWidget
{
    Q_OBJECT
public:
    BackgroundRemovalWidget(QSharedPointer<VisionModels> vision, QWidget *parent)
        : KisConfigWidget(parent)
        , m_vision(vision)
    {
        QVBoxLayout *layout = new QVBoxLayout(this);

        QLabel *label = new QLabel(i18n("Uses a neural network to separate foreground objects from background."), this);
        layout->addWidget(label);

        m_modelSelectWidget = new VisionMLModelSelect(m_vision, VisionMLTask::background_removal, true);
        layout->addWidget(m_modelSelectWidget);

        VisionMLBackendWidget *backendSelect = new VisionMLBackendWidget(m_vision, true);
        layout->addWidget(backendSelect);

        m_foregroundEstimationCheckBox = new QCheckBox(i18n("Estimate pixel foreground contribution"), this);
        layout->addWidget(m_foregroundEstimationCheckBox);

        layout->addStretch();

        connect(m_vision.get(), &VisionModels::modelNameChanged, this, &BackgroundRemovalWidget::handleModelChange);
        connect(m_vision.get(), &VisionModels::backendChanged, this, &BackgroundRemovalWidget::handleBackendChange);
        connect(m_foregroundEstimationCheckBox,
                &QCheckBox::stateChanged,
                this,
                &KisConfigWidget::sigConfigurationItemChanged);
    }

    void setConfiguration(const KisPropertiesConfigurationSP config) override
    {
        QVariant value;
        if (config->getProperty("model", value)) {
            m_vision->setModelName(VisionMLTask::background_removal, value.toString());
        }
        if (config->getProperty("backend", value)) {
            auto backend = value.toString() == "gpu" ? visp::backend_type::gpu : visp::backend_type::cpu;
            m_vision->setBackend(backend);
        }
        if (config->getProperty("foreground_estimation", value)) {
            m_foregroundEstimationCheckBox->setChecked(value.toBool());
        }
    }

    KisPropertiesConfigurationSP configuration() const override
    {
        KisFilterConfigurationSP config =
            new KisFilterConfiguration("background_removal", 1, KisGlobalResourcesInterface::instance());
        config->setProperty("model", m_vision->modelName(VisionMLTask::background_removal));
        config->setProperty("backend", m_vision->backend() == visp::backend_type::gpu ? "gpu" : "cpu");
        config->setProperty("foreground_estimation", m_foregroundEstimationCheckBox->isChecked());
        return config;
    }

    void handleModelChange(VisionMLTask task, QString const &)
    {
        if (task == VisionMLTask::background_removal) {
            emit sigConfigurationItemChanged();
        }
    }

    void handleBackendChange(visp::backend_type)
    {
        emit sigConfigurationItemChanged();
    }

private:
    QSharedPointer<VisionModels> m_vision;
    VisionMLModelSelect *m_modelSelectWidget = nullptr;
    QCheckBox *m_foregroundEstimationCheckBox = nullptr;
};

//
// BackgroundRemovalFilter implementation

BackgroundRemovalFilter::BackgroundRemovalFilter(QSharedPointer<VisionModels> vision)
    : KisFilter(id(), FiltersCategoryOtherId, i18n("Background Removal..."))
    , m_vision(vision)
{
    setSupportsPainting(false);
    setSupportsAdjustmentLayers(false);
    setSupportsThreading(false);
    setSupportsLevelOfDetail(false);
    setColorSpaceIndependence(TO_RGBA8);
}

KisConfigWidget *BackgroundRemovalFilter::createConfigurationWidget(QWidget *parent, const KisPaintDeviceSP, bool) const
{
    return new BackgroundRemovalWidget(m_vision, parent);
}

KisFilterConfigurationSP BackgroundRemovalFilter::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);
    config->setProperty("model", m_vision->modelName(VisionMLTask::background_removal));
    config->setProperty("backend", m_vision->backend() == visp::backend_type::gpu ? "gpu" : "cpu");
    config->setProperty("foreground_estimation", true);
    return config;
}

void BackgroundRemovalFilter::processImpl(KisPaintDeviceSP device,
                                          const QRect &applyRect,
                                          const KisFilterConfigurationSP config,
                                          KoUpdater *progressUpdater) const
{
    if (progressUpdater) {
        progressUpdater->setAutoNestedName(i18n("Background Removal"));
    }

    VisionMLImage image = VisionMLImage::prepare(*device, device->extent());
    if (!image) {
        qWarning() << "Background Removal: No image data available in the specified rectangle.";
        return;
    }
    if (image.view.extent[0] < 64 || image.view.extent[1] < 64) {
        qWarning() << "Background Removal: Image is too small, minimum size is 64x64 pixels.";
        return;
    }

    if (progressUpdater)
        progressUpdater->setProgress(9);

    bool estimateForeground = true;
    if (QVariant configValue; config->getProperty("foreground_estimation", configValue)) {
        estimateForeground = configValue.toBool();
    }

    try {
        visp::image_data mask = m_vision->removeBackground(image.view);

        if (progressUpdater)
            progressUpdater->setProgress(90);

        QImage resultImage;
        if (estimateForeground) {
            visp::image_data maskF32 = visp::image_u8_to_f32(mask, visp::image_format::alpha_f32);
            visp::image_data imageF32 = visp::image_u8_to_f32(image.view, visp::image_format::rgba_f32);
            visp::image_data fgF32 = visp::image_estimate_foreground(imageF32, maskF32);
            visp::image_data fg = visp::image_f32_to_u8(fgF32, visp::image_format::rgba_u8);
            resultImage = VisionMLImage::convertToQImage(fg, applyRect);
        } else {
            visp::image_set_alpha(image.view, mask);
            resultImage = image.data;
        }
        if (progressUpdater)
            progressUpdater->setProgress(99);

        device->convertFromQImage(resultImage, nullptr, applyRect.x(), applyRect.y());

    } catch (const std::exception &e) {
        Q_EMIT m_report.errorOccurred(QString(e.what()));
    }
}

QRect BackgroundRemovalFilter::neededRect(const QRect &rect, const KisFilterConfigurationSP, int) const
{
    return rect;
}

QRect BackgroundRemovalFilter::changedRect(const QRect &rect, const KisFilterConfigurationSP, int) const
{
    return rect;
}

#include "BackgroundRemovalFilter.moc"
