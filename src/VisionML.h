#ifndef VISION_ML_H_
#define VISION_ML_H_

#include "KisOptionCollectionWidget.h"
#include "KoGroupButton.h"
#include <kconfiggroup.h>

#include <visp/vision.h>

#include <QComboBox>
#include <QFileSystemWatcher>
#include <QImage>
#include <QLabel>
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QWidget>


class KisPaintDevice;

enum class SegmentationMode {
    fast,
    precise
};

enum class VisionMLTask {
    segmentation = 0,
    inpainting,
    background_removal,
    _count
};

// Vision ML library, environment and config. One instance is shared between individual tools.
class VisionModels : public QObject
{
    Q_OBJECT
public:
    static QSharedPointer<VisionModels> create();

    void encodeSegmentationImage(const visp::image_view &view);
    bool hasSegmentationImage() const;
    visp::image_data predictSegmentationMask(visp::i32x2 point);
    visp::image_data predictSegmentationMask(visp::box_2d box);

    visp::image_data removeBackground(const visp::image_view &view);

    visp::image_data inpaint(visp::image_view const &image, visp::image_view const &mask);

    void unload(VisionMLTask);

    visp::backend_type backend() const;
    bool setBackend(visp::backend_type backend);
    QString backendDeviceDescription() const;

    QString const &modelName(VisionMLTask task) const;
    void setModelName(VisionMLTask task, QString const &name);

Q_SIGNALS:
    void backendChanged(visp::backend_type);
    void modelNameChanged(VisionMLTask, QString const &);

private Q_SLOTS:
    void cleanUp();

private:
    VisionModels();
    QString initialize(visp::backend_type);
    void configureModel(VisionMLTask task, QString const& defaultName);
    QByteArray modelPath(VisionMLTask) const;
    void unloadModels();

    KConfigGroup m_config;
    visp::backend_type m_backendType = visp::backend_type::cpu;
    visp::backend_device m_backend;
    visp::sam_model m_sam;
    visp::birefnet_model m_birefnet;
    visp::migan_model m_migan;
    std::array<QString, (int)VisionMLTask::_count> m_modelName;
    QMutex m_mutex;
};

// Helper for reading images from paint device to a format compatible with vision models.
struct VisionMLImage {
    QImage data;
    visp::image_span view;

    explicit operator bool() const
    {
        return !data.isNull();
    }

    static VisionMLImage prepare(KisPaintDevice const &device, QRect bounds = {});

    static QImage convertToQImage(visp::image_view const &view, QRect bounds = {});
};

// Shows a widget to switch between CPU and GPU backends. Shared across all tools.
class VisionMLBackendWidget : public KisOptionCollectionWidgetWithHeader
{
    Q_OBJECT
public:
    VisionMLBackendWidget(QSharedPointer<VisionModels> shared, bool showDevice = false, QWidget *parent = nullptr);

public Q_SLOTS:
    void switchBackend(KoGroupButton *, bool);
    void updateBackend(visp::backend_type);

private:
    QSharedPointer<VisionModels> m_shared;
    KoGroupButton *m_cpuButton;
    KoGroupButton *m_gpuButton;
    QLabel *m_deviceLabel = nullptr;
};

// Shows a drop-down list with available models. Shared across specific tasks.
class VisionMLModelSelect : public KisOptionCollectionWidgetWithHeader
{
    Q_OBJECT
public:
    VisionMLModelSelect(QSharedPointer<VisionModels> shared,
                        VisionMLTask task,
                        bool showFolderButton = false,
                        QWidget *parent = nullptr);

public Q_SLOTS:
    void switchModel(int);
    void updateModel(VisionMLTask, QString const &name);
    void openModelsFolder();
    void updateModels();

private:
    QSharedPointer<VisionModels> m_shared;
    VisionMLTask m_task;
    QComboBox *m_select;
    QFileSystemWatcher *m_fileWatcher = nullptr;
};

// Helper to report errors from different threads ("stroke applicators")
class VisionMLErrorReporter : public QObject
{
    Q_OBJECT
public:
    VisionMLErrorReporter(QObject *parent = nullptr);

    Q_SIGNAL void errorOccurred(QString const &message) const;
private Q_SLOTS:
    void showError(QString const &message) const;
};

#endif // VISION_ML_H_
