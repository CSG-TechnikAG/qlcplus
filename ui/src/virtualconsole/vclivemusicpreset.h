/*
  Q Light Controller Plus
  vclivemusicpreset.h

  A reusable boundary preset for the Live Music virtual console widget.

  Licensed under the Apache License, Version 2.0
*/

#ifndef VCLIVEMUSICPRESET_H
#define VCLIVEMUSICPRESET_H

#include <QKeySequence>
#include <QList>
#include <QSharedPointer>
#include <QString>

#include "qlcinputsource.h"
#include "livemusicengine.h"

class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup ui_vc_widgets
 * @{
 */

#define KXMLQLCVCLiveMusicPreset          QStringLiteral("Preset")
#define KXMLQLCVCLiveMusicPresetID        QStringLiteral("ID")
#define KXMLQLCVCLiveMusicPresetName      QStringLiteral("Name")
#define KXMLQLCVCLiveMusicFixture         QStringLiteral("Fixture")
#define KXMLQLCVCLiveMusicFixtureID       QStringLiteral("ID")
#define KXMLQLCVCLiveMusicAllChannels     QStringLiteral("AllChannels")
#define KXMLQLCVCLiveMusicChannel         QStringLiteral("Channel")
#define KXMLQLCVCLiveMusicIntensityLimit  QStringLiteral("IntensityLimit")
#define KXMLQLCVCLiveMusicMovementRange   QStringLiteral("MovementRange")
#define KXMLQLCVCLiveMusicInvertPan       QStringLiteral("InvertPan")
#define KXMLQLCVCLiveMusicInvertTilt      QStringLiteral("InvertTilt")
#define KXMLQLCVCLiveMusicSettings        QStringLiteral("Settings")
#define KXMLQLCVCLiveMusicStyle           QStringLiteral("Style")
#define KXMLQLCVCLiveMusicColorMode       QStringLiteral("ColorMode")
#define KXMLQLCVCLiveMusicMovementStyle   QStringLiteral("MovementStyle")
#define KXMLQLCVCLiveMusicEnergy          QStringLiteral("Energy")
#define KXMLQLCVCLiveMusicGlobalIntensity QStringLiteral("GlobalIntensity")
#define KXMLQLCVCLiveMusicGlobalMovement  QStringLiteral("GlobalMovement")
#define KXMLQLCVCLiveMusicAllowColor      QStringLiteral("AllowColor")
#define KXMLQLCVCLiveMusicAllowMovement   QStringLiteral("AllowMovement")
#define KXMLQLCVCLiveMusicAllowStrobe     QStringLiteral("AllowStrobe")
#define KXMLQLCVCLiveMusicAllowGobo       QStringLiteral("AllowGobo")
#define KXMLQLCVCLiveMusicAllowPrism      QStringLiteral("AllowPrism")
#define KXMLQLCVCLiveMusicAllowBeam       QStringLiteral("AllowBeam")
#define KXMLQLCVCLiveMusicAllowSmoke      QStringLiteral("AllowSmoke")
#define KXMLQLCVCLiveMusicChangeBars      QStringLiteral("ChangeBars")
#define KXMLQLCVCLiveMusicPrimaryColor    QStringLiteral("PrimaryColor")
#define KXMLQLCVCLiveMusicSecondaryColor  QStringLiteral("SecondaryColor")

class VCLiveMusicPreset final
{
public:
    explicit VCLiveMusicPreset(quint8 id = 1);
    VCLiveMusicPreset(const VCLiveMusicPreset &preset);
    ~VCLiveMusicPreset();

    VCLiveMusicPreset &operator=(const VCLiveMusicPreset &preset);
    bool operator<(const VCLiveMusicPreset &right) const;
    static bool compare(const VCLiveMusicPreset *left, const VCLiveMusicPreset *right);

    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;

    LiveMusic::FixtureConfig *fixtureConfig(quint32 fixtureId);
    const LiveMusic::FixtureConfig *fixtureConfig(quint32 fixtureId) const;

public:
    quint8 m_id;
    QString m_name;
    QList<LiveMusic::FixtureConfig> m_fixtures;
    LiveMusic::ShowSettings m_settings;
    QSharedPointer<QLCInputSource> m_inputSource;
    QKeySequence m_keySequence;
};

/** @} */

#endif // VCLIVEMUSICPRESET_H
