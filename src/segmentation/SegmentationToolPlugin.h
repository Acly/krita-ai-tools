#ifndef SEGMENTATION_TOOL_PLUGIN_H_
#define SEGMENTATION_TOOL_PLUGIN_H_

#include <QObject>
#include <QVariant>

class SegmentationToolPlugin : public QObject
{
    Q_OBJECT
public:
    SegmentationToolPlugin(QObject *parent, const QVariantList &);
    ~SegmentationToolPlugin() override;
};

#endif // SEGMENTATION_TOOL_PLUGIN_H_
