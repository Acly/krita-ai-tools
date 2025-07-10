#ifndef VISION_ML_PLUGIN_H_
#define VISION_ML_PLUGIN_H_

#include <QObject>
#include <QVariant>

class VisionMLPlugin : public QObject
{
    Q_OBJECT
public:
    VisionMLPlugin(QObject *parent, const QVariantList &);
    ~VisionMLPlugin() override;
};

#endif // VISION_ML_PLUGIN_H_
