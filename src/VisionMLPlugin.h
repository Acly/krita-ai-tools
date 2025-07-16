#ifndef VISION_ML_PLUGIN_H_
#define VISION_ML_PLUGIN_H_

#include <QObject>
#include <QVariant>
#include <QList>
#include <QString>

class VisionMLPlugin : public QObject
{
    Q_OBJECT
public:
    VisionMLPlugin(QObject *parent, const QVariantList &);
    ~VisionMLPlugin() override;

    void injectTools();

    QList<QString> m_toolIds;
};

#endif // VISION_ML_PLUGIN_H_
