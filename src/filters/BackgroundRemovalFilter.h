#ifndef BACKGROUND_REMOVAL_FILTER_H
#define BACKGROUND_REMOVAL_FILTER_H

#include "VisionML.h"
#include "filter/kis_filter.h"

class BackgroundRemovalFilter : public KisFilter
{
public:
    BackgroundRemovalFilter(QSharedPointer<VisionModels> vision);

    void processImpl(KisPaintDeviceSP device,
                     const QRect &applyRect,
                     const KisFilterConfigurationSP,
                     KoUpdater *) const override;

    static inline KoID id()
    {
        return KoID("background_removal", i18n("Background Removal"));
    }

    KisConfigWidget *
    createConfigurationWidget(QWidget *parent, const KisPaintDeviceSP, bool useForMasks) const override;
    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP) const override;

    QRect changedRect(const QRect &, const KisFilterConfigurationSP, int lod) const override;
    QRect neededRect(const QRect &, const KisFilterConfigurationSP, int lod) const override;

private:
    QSharedPointer<VisionModels> m_vision;
    VisionMLErrorReporter m_report;
};

#endif // BACKGROUND_REMOVAL_FILTER_H