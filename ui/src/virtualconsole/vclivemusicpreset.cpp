/*
  Q Light Controller Plus
  vclivemusicpreset.cpp

  Licensed under the Apache License, Version 2.0
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

#include <algorithm>

#include "vclivemusicpreset.h"
#include "qlcfile.h"
#include "vcwidget.h"

VCLiveMusicPreset::VCLiveMusicPreset(quint8 id)
    : m_id(id)
{
}

VCLiveMusicPreset::VCLiveMusicPreset(const VCLiveMusicPreset &preset)
{
    *this = preset;
}

VCLiveMusicPreset::~VCLiveMusicPreset()
{
}

VCLiveMusicPreset &VCLiveMusicPreset::operator=(const VCLiveMusicPreset &preset)
{
    if (this != &preset)
    {
        m_id = preset.m_id;
        m_name = preset.m_name;
        m_fixtures = preset.m_fixtures;
        m_settings = preset.m_settings;
        m_keySequence = preset.m_keySequence;

        if (preset.m_inputSource.isNull() == false)
        {
            m_inputSource = QSharedPointer<QLCInputSource>(
                        new QLCInputSource(preset.m_inputSource->universe(),
                                           preset.m_inputSource->channel()));
            m_inputSource->setFeedbackValue(QLCInputFeedback::LowerValue,
                    preset.m_inputSource->feedbackValue(QLCInputFeedback::LowerValue));
            m_inputSource->setFeedbackValue(QLCInputFeedback::UpperValue,
                    preset.m_inputSource->feedbackValue(QLCInputFeedback::UpperValue));
        }
    }
    return *this;
}

bool VCLiveMusicPreset::operator<(const VCLiveMusicPreset &right) const
{
    return m_id < right.m_id;
}

bool VCLiveMusicPreset::compare(const VCLiveMusicPreset *left,
                                const VCLiveMusicPreset *right)
{
    return *left < *right;
}

LiveMusic::FixtureConfig *VCLiveMusicPreset::fixtureConfig(quint32 fixtureId)
{
    for (int i = 0; i < m_fixtures.size(); i++)
    {
        if (m_fixtures[i].fixtureId == fixtureId)
            return &m_fixtures[i];
    }
    return NULL;
}

const LiveMusic::FixtureConfig *VCLiveMusicPreset::fixtureConfig(quint32 fixtureId) const
{
    for (int i = 0; i < m_fixtures.size(); i++)
    {
        if (m_fixtures[i].fixtureId == fixtureId)
            return &m_fixtures[i];
    }
    return NULL;
}

bool VCLiveMusicPreset::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCVCLiveMusicPreset)
    {
        qWarning() << Q_FUNC_INFO << "Live Music preset node not found";
        return false;
    }

    const QXmlStreamAttributes attrs = root.attributes();
    if (attrs.hasAttribute(KXMLQLCVCLiveMusicPresetID))
        m_id = attrs.value(KXMLQLCVCLiveMusicPresetID).toString().toUInt();
    if (attrs.hasAttribute(KXMLQLCVCLiveMusicPresetName))
        m_name = attrs.value(KXMLQLCVCLiveMusicPresetName).toString();

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCVCLiveMusicFixture)
        {
            const QXmlStreamAttributes fAttrs = root.attributes();
            LiveMusic::FixtureConfig config;
            config.fixtureId = fAttrs.value(KXMLQLCVCLiveMusicFixtureID).toString().toUInt();
            config.allChannels = fAttrs.value(KXMLQLCVCLiveMusicAllChannels).toString() != KXMLQLCFalse;
            if (fAttrs.hasAttribute(KXMLQLCVCLiveMusicIntensityLimit))
                config.intensityLimit = fAttrs.value(KXMLQLCVCLiveMusicIntensityLimit).toString().toInt();
            if (fAttrs.hasAttribute(KXMLQLCVCLiveMusicMovementRange))
                config.movementRange = fAttrs.value(KXMLQLCVCLiveMusicMovementRange).toString().toInt();
            config.invertPan = fAttrs.value(KXMLQLCVCLiveMusicInvertPan).toString() == KXMLQLCTrue;
            config.invertTilt = fAttrs.value(KXMLQLCVCLiveMusicInvertTilt).toString() == KXMLQLCTrue;

            while (root.readNextStartElement())
            {
                if (root.name() == KXMLQLCVCLiveMusicChannel)
                    config.allowedChannels.insert(root.readElementText().toUInt());
                else
                    root.skipCurrentElement();
            }
            m_fixtures.append(config);
        }
        else if (root.name() == KXMLQLCVCLiveMusicSettings)
        {
            const QXmlStreamAttributes sAttrs = root.attributes();
            m_settings.style = sAttrs.value(KXMLQLCVCLiveMusicStyle).toString().toInt();
            m_settings.colorMode = sAttrs.value(KXMLQLCVCLiveMusicColorMode).toString().toInt();
            m_settings.movementStyle = sAttrs.value(KXMLQLCVCLiveMusicMovementStyle).toString().toInt();
            m_settings.energy = sAttrs.value(KXMLQLCVCLiveMusicEnergy).toString().toInt();
            m_settings.intensityLimit = sAttrs.value(KXMLQLCVCLiveMusicGlobalIntensity).toString().toInt();
            m_settings.movementRange = sAttrs.value(KXMLQLCVCLiveMusicGlobalMovement).toString().toInt();
            m_settings.allowColor = sAttrs.value(KXMLQLCVCLiveMusicAllowColor).toString() != KXMLQLCFalse;
            m_settings.allowMovement = sAttrs.value(KXMLQLCVCLiveMusicAllowMovement).toString() != KXMLQLCFalse;
            m_settings.allowStrobe = sAttrs.value(KXMLQLCVCLiveMusicAllowStrobe).toString() != KXMLQLCFalse;
            m_settings.allowGobo = sAttrs.value(KXMLQLCVCLiveMusicAllowGobo).toString() != KXMLQLCFalse;
            m_settings.allowPrism = sAttrs.value(KXMLQLCVCLiveMusicAllowPrism).toString() != KXMLQLCFalse;
            m_settings.allowBeam = sAttrs.value(KXMLQLCVCLiveMusicAllowBeam).toString() != KXMLQLCFalse;
            m_settings.allowSmoke = sAttrs.value(KXMLQLCVCLiveMusicAllowSmoke).toString() != KXMLQLCFalse;
            m_settings.changeBars = sAttrs.value(KXMLQLCVCLiveMusicChangeBars).toString().toInt();
            const QString primary = sAttrs.value(KXMLQLCVCLiveMusicPrimaryColor).toString();
            const QString secondary = sAttrs.value(KXMLQLCVCLiveMusicSecondaryColor).toString();
            if (primary.isEmpty() == false)
                m_settings.primaryColor = QColor(primary);
            if (secondary.isEmpty() == false)
                m_settings.secondaryColor = QColor(secondary);
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCVCWidgetInput)
        {
            m_inputSource = VCWidget::getXMLInput(root);
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCVCWidgetKey)
        {
            m_keySequence = VCWidget::stripKeySequence(QKeySequence(root.readElementText()));
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown Live Music preset tag:" << root.name().toString();
            root.skipCurrentElement();
        }
    }

    if (m_settings.changeBars < 1)
        m_settings.changeBars = 4;
    if (m_settings.changeBars > 16)
        m_settings.changeBars = 16;

    return true;
}

bool VCLiveMusicPreset::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    doc->writeStartElement(KXMLQLCVCLiveMusicPreset);
    doc->writeAttribute(KXMLQLCVCLiveMusicPresetID, QString::number(m_id));
    doc->writeAttribute(KXMLQLCVCLiveMusicPresetName, m_name);

    foreach (const LiveMusic::FixtureConfig &config, m_fixtures)
    {
        doc->writeStartElement(KXMLQLCVCLiveMusicFixture);
        doc->writeAttribute(KXMLQLCVCLiveMusicFixtureID, QString::number(config.fixtureId));
        doc->writeAttribute(KXMLQLCVCLiveMusicAllChannels,
                            config.allChannels ? KXMLQLCTrue : KXMLQLCFalse);
        doc->writeAttribute(KXMLQLCVCLiveMusicIntensityLimit,
                            QString::number(config.intensityLimit));
        doc->writeAttribute(KXMLQLCVCLiveMusicMovementRange,
                            QString::number(config.movementRange));
        doc->writeAttribute(KXMLQLCVCLiveMusicInvertPan,
                            config.invertPan ? KXMLQLCTrue : KXMLQLCFalse);
        doc->writeAttribute(KXMLQLCVCLiveMusicInvertTilt,
                            config.invertTilt ? KXMLQLCTrue : KXMLQLCFalse);
        if (config.allChannels == false)
        {
            QList<quint32> channels = config.allowedChannels.values();
            std::sort(channels.begin(), channels.end());
            foreach (quint32 channel, channels)
                doc->writeTextElement(KXMLQLCVCLiveMusicChannel, QString::number(channel));
        }
        doc->writeEndElement();
    }

    doc->writeStartElement(KXMLQLCVCLiveMusicSettings);
    doc->writeAttribute(KXMLQLCVCLiveMusicStyle, QString::number(m_settings.style));
    doc->writeAttribute(KXMLQLCVCLiveMusicColorMode, QString::number(m_settings.colorMode));
    doc->writeAttribute(KXMLQLCVCLiveMusicMovementStyle, QString::number(m_settings.movementStyle));
    doc->writeAttribute(KXMLQLCVCLiveMusicEnergy, QString::number(m_settings.energy));
    doc->writeAttribute(KXMLQLCVCLiveMusicGlobalIntensity, QString::number(m_settings.intensityLimit));
    doc->writeAttribute(KXMLQLCVCLiveMusicGlobalMovement, QString::number(m_settings.movementRange));
    doc->writeAttribute(KXMLQLCVCLiveMusicAllowColor, m_settings.allowColor ? KXMLQLCTrue : KXMLQLCFalse);
    doc->writeAttribute(KXMLQLCVCLiveMusicAllowMovement, m_settings.allowMovement ? KXMLQLCTrue : KXMLQLCFalse);
    doc->writeAttribute(KXMLQLCVCLiveMusicAllowStrobe, m_settings.allowStrobe ? KXMLQLCTrue : KXMLQLCFalse);
    doc->writeAttribute(KXMLQLCVCLiveMusicAllowGobo, m_settings.allowGobo ? KXMLQLCTrue : KXMLQLCFalse);
    doc->writeAttribute(KXMLQLCVCLiveMusicAllowPrism, m_settings.allowPrism ? KXMLQLCTrue : KXMLQLCFalse);
    doc->writeAttribute(KXMLQLCVCLiveMusicAllowBeam, m_settings.allowBeam ? KXMLQLCTrue : KXMLQLCFalse);
    doc->writeAttribute(KXMLQLCVCLiveMusicAllowSmoke, m_settings.allowSmoke ? KXMLQLCTrue : KXMLQLCFalse);
    doc->writeAttribute(KXMLQLCVCLiveMusicChangeBars, QString::number(m_settings.changeBars));
    if (m_settings.primaryColor.isValid())
        doc->writeAttribute(KXMLQLCVCLiveMusicPrimaryColor, m_settings.primaryColor.name());
    if (m_settings.secondaryColor.isValid())
        doc->writeAttribute(KXMLQLCVCLiveMusicSecondaryColor, m_settings.secondaryColor.name());
    doc->writeEndElement();

    if (m_inputSource.isNull() == false && m_inputSource->isValid())
        VCWidget::saveXMLInput(doc, m_inputSource);

    if (m_keySequence.isEmpty() == false)
        doc->writeTextElement(KXMLQLCVCWidgetKey, m_keySequence.toString());

    doc->writeEndElement();
    return true;
}
