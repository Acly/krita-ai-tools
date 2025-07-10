#ifndef SEGMENTATION_TOOL_SHARED_H_
#define SEGMENTATION_TOOL_SHARED_H_

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
class SegmentationToolShared : public QObject
{
    Q_OBJECT
public:
    static QSharedPointer<SegmentationToolShared> create();

    visp::backend_type backend() const
    {
        return m_backendType;
    }

    bool setBackend(visp::backend_type backend);

    void encodeImage(const visp::image_view &view);
    bool hasEncodedImage() const;
    visp::image_data predictMask(visp::i32x2 point);
    visp::image_data predictMask(visp::image_rect box);

    visp::image_data removeBackground(const visp::image_view &view);

Q_SIGNALS:
    void backendChanged(visp::backend_type);

private Q_SLOTS:
    void cleanUp();

private:
    SegmentationToolShared();
    QString initialize(visp::backend_type);

    KConfigGroup m_config;
    visp::backend_type m_backendType = visp::backend_type::cpu;
    visp::backend m_backend;
    visp::sam_model m_sam;
    visp::birefnet_model m_birefnet;
    QMutex m_mutex;
};

#endif // SEGMENTATION_TOOL_SHARED_H_
