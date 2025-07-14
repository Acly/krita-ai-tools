#ifndef VISION_ML_H_
#define VISION_ML_H_

#include "KisOptionCollectionWidget.h"
#include "KoGroupButton.h"
#include <kconfiggroup.h>

#include <visp/vision.hpp>

#include <QComboBox>
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QWidget>

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


// Segmentation library, environment and config. One instance is shared between individual tools.
class VisionModels : public QObject
{
    Q_OBJECT
public:
    static QSharedPointer<VisionModels> create();


    void encodeSegmentationImage(const visp::image_view &view);
    bool hasSegmentationImage() const;
    visp::image_data predictSegmentationMask(visp::i32x2 point);
    visp::image_data predictSegmentationMask(visp::image_rect box);

    visp::image_data removeBackground(const visp::image_view &view);

    visp::image_data inpaint(visp::image_view const &image, visp::image_view const &mask);

    
    visp::backend_type backend() const;
    bool setBackend(visp::backend_type backend);

    QString const& modelName(VisionMLTask task) const;
    void setModelName(VisionMLTask task, QString const& name);
    
Q_SIGNALS:
    void backendChanged(visp::backend_type);
    void modelNameChanged(VisionMLTask, QString const&);

private Q_SLOTS:
    void cleanUp();

private:
    VisionModels();
    QString initialize(visp::backend_type);
    QByteArray modelPath(VisionMLTask) const;
    void unloadModels();

    KConfigGroup m_config;
    visp::backend_type m_backendType = visp::backend_type::cpu;
    visp::backend m_backend;
    visp::sam_model m_sam;
    visp::birefnet_model m_birefnet;
    visp::migan_model m_migan;
    std::array<QString, (int)VisionMLTask::_count> m_modelName;
    QMutex m_mutex;
};

class VisionMLBackendWidget : public KisOptionCollectionWidgetWithHeader
{
    Q_OBJECT
public:
    VisionMLBackendWidget(QSharedPointer<VisionModels> shared, QWidget *parent = nullptr);

public Q_SLOTS:
    void switchBackend(KoGroupButton *, bool);
    void updateBackend(visp::backend_type);

private:
    QSharedPointer<VisionModels> m_shared;
    KoGroupButton *m_cpuButton;
    KoGroupButton *m_gpuButton;
};

class VisionMLModelSelect : public KisOptionCollectionWidgetWithHeader
{
    Q_OBJECT
public:
    VisionMLModelSelect(QSharedPointer<VisionModels> shared,
                        VisionMLTask task,
                        QWidget *parent = nullptr);

public Q_SLOTS:
    void switchModel(int);
    void updateModel(VisionMLTask, QString const& name);

private:
    QSharedPointer<VisionModels> m_shared;
    VisionMLTask m_task;
    QComboBox *m_select;
};

class VisionMLErrorReporter : public QObject
{
    Q_OBJECT
public:
    VisionMLErrorReporter(QObject *parent = nullptr);

    Q_SIGNAL void errorOccurred(QString const &message);
private Q_SLOTS:
    void showError(QString const &message);
};

#endif // VISION_ML_H_
