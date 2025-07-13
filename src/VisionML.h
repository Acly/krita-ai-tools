#ifndef VISION_ML_H_
#define VISION_ML_H_

#include <kconfiggroup.h>

#include <visp/vision.hpp>

#include <QMutex>
#include <QObject>
#include <QSharedPointer>

enum class SegmentationMode {
    fast,
    precise
};

// Segmentation library, environment and config. One instance is shared between individual tools.
class VisionModels : public QObject
{
    Q_OBJECT
public:
    static QSharedPointer<VisionModels> create();

    visp::backend_type backend() const
    {
        return m_backendType;
    }

    bool setBackend(visp::backend_type backend);

    void encodeSegmentationImage(const visp::image_view &view);
    bool hasSegmentationImage() const;
    visp::image_data predictSegmentationMask(visp::i32x2 point);
    visp::image_data predictSegmentationMask(visp::image_rect box);

    visp::image_data removeBackground(const visp::image_view &view);

    visp::image_data inpaint(visp::image_view const &image, visp::image_view const &mask);

Q_SIGNALS:
    void backendChanged(visp::backend_type);

private Q_SLOTS:
    void cleanUp();

private:
    VisionModels();
    QString initialize(visp::backend_type);

    KConfigGroup m_config;
    visp::backend_type m_backendType = visp::backend_type::cpu;
    visp::backend m_backend;
    visp::sam_model m_sam;
    visp::birefnet_model m_birefnet;
    visp::migan_model m_migan;
    QMutex m_mutex;
};

#endif // VISION_ML_H_
