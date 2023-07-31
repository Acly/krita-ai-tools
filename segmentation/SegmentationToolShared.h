#ifndef SEGMENTATION_TOOL_SHARED_H_
#define SEGMENTATION_TOOL_SHARED_H_

#include <KConfigGroup.h>

#define DLIMGEDIT_LOAD_DYNAMIC
#define DLIMGEDIT_NO_FILESYSTEM
#include <dlimgedit/dlimgedit.hpp>

#include <QLibrary>
#include <QObject>
#include <QSharedPointer>

// Segmentation library, environment and config. One instance is shared between individual tools.
class SegmentationToolShared : public QObject
{
    Q_OBJECT
public:
    static QSharedPointer<SegmentationToolShared> create();

    dlimg::Environment const &environment() const;

    dlimg::Backend backend() const
    {
        return m_backend;
    }

    bool setBackend(dlimg::Backend backend);

Q_SIGNALS:
    void backendChanged(dlimg::Backend);

private:
    SegmentationToolShared();
    QString initialize(dlimg::Backend);

    KConfigGroup m_config;
    QLibrary m_lib;
    dlimg::Environment m_cpu{nullptr};
    dlimg::Environment m_gpu{nullptr};
    dlimg::Backend m_backend = dlimg::Backend::cpu;
};

#endif // SEGMENTATION_TOOL_SHARED_H_
