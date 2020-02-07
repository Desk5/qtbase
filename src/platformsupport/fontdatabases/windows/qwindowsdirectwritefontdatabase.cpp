/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwindowsdirectwritefontdatabase_p.h"
#include "qwindowsfontenginedirectwrite_p.h"

#include <QtCore/qendian.h>
#include <QtCore/qstringbuilder.h>
#include <QtCore/qvarlengtharray.h>

#include <dwrite_3.h>
#include <d2d1.h>

QT_BEGIN_NAMESPACE

#ifdef QT_USE_DIRECTWRITE3

QWindowsDirectWriteFontDatabase::QWindowsDirectWriteFontDatabase()
{
    qCDebug(lcQpaFonts) << "Creating DirectWrite database";
}

QWindowsDirectWriteFontDatabase::~QWindowsDirectWriteFontDatabase()
{
    for (auto it = m_populatedFonts.begin(); it != m_populatedFonts.end(); ++it)
        it.value()->Release();
}

QString QWindowsDirectWriteFontDatabase::localeString(IDWriteLocalizedStrings *names,
                                                      wchar_t localeName[])
{
    uint index;
    BOOL exists;
    if (SUCCEEDED(names->FindLocaleName(localeName, &index, &exists)) && exists) {
        uint length;
        if (SUCCEEDED(names->GetStringLength(index, &length)) && length > 0) {
            QVarLengthArray<wchar_t> buffer(int(length) + 1);
            if (SUCCEEDED(names->GetString(index, buffer.data(), length + 1)))
                return QString::fromWCharArray(buffer.data());
        }
    }

    return QString();
}

static QFont::Stretch fromDirectWriteStretch(DWRITE_FONT_STRETCH stretch)
{
    switch (stretch) {
    case DWRITE_FONT_STRETCH_ULTRA_CONDENSED: return QFont::UltraCondensed;
    case DWRITE_FONT_STRETCH_EXTRA_CONDENSED: return QFont::ExtraCondensed;
    case DWRITE_FONT_STRETCH_CONDENSED: return QFont::Condensed;
    case DWRITE_FONT_STRETCH_SEMI_CONDENSED: return QFont::SemiCondensed;
    case DWRITE_FONT_STRETCH_NORMAL: return QFont::Unstretched;
    case DWRITE_FONT_STRETCH_SEMI_EXPANDED: return QFont::SemiExpanded;
    case DWRITE_FONT_STRETCH_EXPANDED: return QFont::Expanded;
    case DWRITE_FONT_STRETCH_EXTRA_EXPANDED: return QFont::ExtraExpanded;
    case DWRITE_FONT_STRETCH_ULTRA_EXPANDED: return QFont::UltraExpanded;
    default: return QFont::AnyStretch;
    }
}

static QFont::Weight fromDirectWriteWeight(DWRITE_FONT_WEIGHT weight)
{
    return QPlatformFontDatabase::weightFromInteger(int(weight));
}

static DWRITE_FONT_STYLE toDirectWriteStyle(QFont::Style style)
{
    switch (style) {
    case QFont::StyleNormal: return DWRITE_FONT_STYLE_NORMAL;
    case QFont::StyleOblique: return DWRITE_FONT_STYLE_OBLIQUE;
    case QFont::StyleItalic: return DWRITE_FONT_STYLE_ITALIC;
    default: return DWRITE_FONT_STYLE_NORMAL;
    }
}

static QFont::Style fromDirectWriteStyle(DWRITE_FONT_STYLE style)
{
    switch (style) {
    case DWRITE_FONT_STYLE_NORMAL: return QFont::StyleNormal;
    case DWRITE_FONT_STYLE_OBLIQUE: return QFont::StyleOblique;
    case DWRITE_FONT_STYLE_ITALIC: return QFont::StyleItalic;
    default: return QFont::StyleNormal;
    }
}

void QWindowsDirectWriteFontDatabase::populateFamily(const QString &familyName)
{
    auto it = m_populatedFonts.find(familyName);
    IDWriteFontFamily *fontFamily = it != m_populatedFonts.end() ? it.value() : nullptr;
    if (fontFamily == nullptr) {
        qCWarning(lcQpaFonts) << "Cannot find" << familyName << "in list of fonts";
        return;
    }

    qCDebug(lcQpaFonts) << "Populate family:" << familyName;

    wchar_t defaultLocale[LOCALE_NAME_MAX_LENGTH];
    bool hasDefaultLocale = GetUserDefaultLocaleName(defaultLocale, LOCALE_NAME_MAX_LENGTH) != 0;
    wchar_t englishLocale[] = L"en-us";

    static const int SMOOTH_SCALABLE = 0xffff;
    const QString foundryName; // No such concept.
    const bool scalable = true;
    const bool antialias = false;
    const int size = SMOOTH_SCALABLE;

    IDWriteFontList *matchingFonts;
    if (SUCCEEDED(fontFamily->GetMatchingFonts(DWRITE_FONT_WEIGHT_REGULAR,
                                               DWRITE_FONT_STRETCH_NORMAL,
                                               DWRITE_FONT_STYLE_NORMAL,
                                               &matchingFonts))) {
        for (uint j = 0; j < matchingFonts->GetFontCount(); ++j) {
            IDWriteFont *font;
            if (SUCCEEDED(matchingFonts->GetFont(j, &font))) {
                IDWriteFont1 *font1 = nullptr;
                if (!SUCCEEDED(font->QueryInterface(__uuidof(IDWriteFont1),
                                                   reinterpret_cast<void **>(&font1)))) {
                    qCWarning(lcQpaFonts) << "COM object does not support IDWriteFont1";
                    continue;
                }

                QString defaultLocaleFamilyName;
                QString englishLocaleFamilyName;

                IDWriteFontFamily *fontFamily2;
                if (SUCCEEDED(font1->GetFontFamily(&fontFamily2))) {
                    IDWriteLocalizedStrings *names;
                    if (SUCCEEDED(fontFamily2->GetFamilyNames(&names))) {
                        defaultLocaleFamilyName = hasDefaultLocale ? localeString(names, defaultLocale) : QString();
                        englishLocaleFamilyName = localeString(names, englishLocale);

                        names->Release();
                    }

                    fontFamily2->Release();
                }

                if (defaultLocaleFamilyName.isEmpty() && englishLocaleFamilyName.isEmpty())
                    englishLocaleFamilyName = familyName;

                {
                    IDWriteLocalizedStrings *names;
                    if (SUCCEEDED(font1->GetFaceNames(&names))) {
                        QString defaultLocaleStyleName = hasDefaultLocale ? localeString(names, defaultLocale) : QString();
                        QString englishLocaleStyleName = localeString(names, englishLocale);

                        QFont::Stretch stretch = fromDirectWriteStretch(font1->GetStretch());
                        QFont::Style style = fromDirectWriteStyle(font1->GetStyle());
                        QFont::Weight weight = fromDirectWriteWeight(font1->GetWeight());
                        bool fixed = font1->IsMonospacedFont();

                        qCDebug(lcQpaFonts) << "Family" << familyName << "has english variant" << englishLocaleStyleName << ", in default locale:" << defaultLocaleStyleName << stretch << style << weight << fixed;

                        IDWriteFontFace *face = nullptr;
                        if (SUCCEEDED(font->CreateFontFace(&face))) {
                            QSupportedWritingSystems writingSystems;

                            const void *tableData = nullptr;
                            UINT32 tableSize;
                            void *tableContext = nullptr;
                            BOOL exists;
                            HRESULT hr = face->TryGetFontTable(qbswap<quint32>(MAKE_TAG('O','S','/','2')),
                                                               &tableData,
                                                               &tableSize,
                                                               &tableContext,
                                                               &exists);
                            if (SUCCEEDED(hr) && exists) {
                                writingSystems = QPlatformFontDatabase::writingSystemsFromOS2Table(reinterpret_cast<const char *>(tableData), tableSize);
                            } else { // Fall back to checking first character of each Unicode range in font (may include too many writing systems)
                                quint32 rangeCount;
                                hr = font1->GetUnicodeRanges(0, nullptr, &rangeCount);

                                if (rangeCount > 0) {
                                    QVarLengthArray<DWRITE_UNICODE_RANGE, QChar::ScriptCount> ranges(rangeCount);

                                    hr = font1->GetUnicodeRanges(rangeCount, ranges.data(), &rangeCount);
                                    if (SUCCEEDED(hr)) {
                                        for (uint i = 0; i < rangeCount; ++i) {
                                            QChar::Script script = QChar::script(ranges.at(i).first);

                                            Q_GUI_EXPORT QFontDatabase::WritingSystem qt_writing_system_for_script(int script);
                                            QFontDatabase::WritingSystem writingSystem = qt_writing_system_for_script(script);

                                            if (writingSystem > QFontDatabase::Any && writingSystem < QFontDatabase::WritingSystemsCount)
                                                writingSystems.setSupported(writingSystem);
                                        }
                                    } else {
                                        const QString errorString = qt_error_string(int(hr));
                                        qCWarning(lcQpaFonts) << "Failed to get unicode ranges for font" << englishLocaleFamilyName << englishLocaleStyleName << ":" << errorString;
                                    }
                                }
                            }

                            if (!englishLocaleStyleName.isEmpty() || defaultLocaleStyleName.isEmpty()) {
                                qCDebug(lcQpaFonts) << "Font" << englishLocaleFamilyName << englishLocaleStyleName << "supports writing systems:" << writingSystems;

                                QPlatformFontDatabase::registerFont(englishLocaleFamilyName, englishLocaleStyleName, QString(), weight, style, stretch, antialias, scalable, size, fixed, writingSystems, face);
                                face->AddRef();
                            }

                            if (!defaultLocaleFamilyName.isEmpty() && defaultLocaleFamilyName != englishLocaleFamilyName) {
                                QPlatformFontDatabase::registerFont(defaultLocaleFamilyName, defaultLocaleStyleName, QString(), weight, style, stretch, antialias, scalable, size, fixed, writingSystems, face);
                                face->AddRef();
                            }

                            face->Release();
                        }

                        names->Release();
                    }
                }

                font1->Release();
                font->Release();
            }
        }

        matchingFonts->Release();
    }
}

QFontEngine *QWindowsDirectWriteFontDatabase::fontEngine(const QFontDef &fontDef, void *handle)
{
    IDWriteFontFace *face = reinterpret_cast<IDWriteFontFace *>(handle);
    Q_ASSERT(face != nullptr);

    QWindowsFontEngineDirectWrite *fontEngine = new QWindowsFontEngineDirectWrite(face, fontDef.pixelSize, data());
    fontEngine->initFontInfo(fontDef, defaultVerticalDPI());

    return fontEngine;
}

QStringList QWindowsDirectWriteFontDatabase::fallbacksForFamily(const QString &family, QFont::Style style, QFont::StyleHint styleHint, QChar::Script script) const
{
    Q_UNUSED(styleHint);
    Q_UNUSED(script);

    wchar_t defaultLocale[LOCALE_NAME_MAX_LENGTH];
    bool hasDefaultLocale = GetUserDefaultLocaleName(defaultLocale, LOCALE_NAME_MAX_LENGTH) != 0;
    wchar_t englishLocale[] = L"en-us";

    QStringList ret;

    auto it = m_populatedFonts.find(family);
    IDWriteFontFamily *fontFamily = it != m_populatedFonts.end() ? it.value() : nullptr;
    if (fontFamily != nullptr) {
        IDWriteFontList *matchingFonts = nullptr;
        if (SUCCEEDED(fontFamily->GetMatchingFonts(DWRITE_FONT_WEIGHT_REGULAR,
                                                   DWRITE_FONT_STRETCH_NORMAL,
                                                   toDirectWriteStyle(style),
                                                   &matchingFonts))) {
            for (uint j = 0; j < matchingFonts->GetFontCount(); ++j) {
                IDWriteFont *font = nullptr;
                if (SUCCEEDED(matchingFonts->GetFont(j, &font))) {
                    IDWriteFontFamily *fontFamily2;
                    if (SUCCEEDED(font->GetFontFamily(&fontFamily2))) {
                        IDWriteLocalizedStrings *names;
                        if (SUCCEEDED(fontFamily2->GetFamilyNames(&names))) {
                            QString name = localeString(names, englishLocale);
                            if (name.isEmpty() && hasDefaultLocale)
                                name = localeString(names, defaultLocale);

                            if (!name.isEmpty() && m_populatedFonts.contains(name))
                                ret.append(name);

                            names->Release();
                        }

                        fontFamily2->Release();
                    }

                    font->Release();
                }
            }

            matchingFonts->Release();
        }
    }

    qDebug(lcQpaFonts) << "fallbacks for" << family << "is" << ret;

    return ret;
}

QStringList QWindowsDirectWriteFontDatabase::addApplicationFont(const QByteArray &fontData, const QString &fileName)
{
    qCDebug(lcQpaFonts) << "Adding application font" << fileName;

    QByteArray loadedData = fontData;
    if (loadedData.isEmpty()) {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            qCWarning(lcQpaFonts) << "Cannot open" << fileName << "for reading.";
            return QStringList();
        }
        loadedData = file.readAll();
    }

    IDWriteFontFace *face = createDirectWriteFace(loadedData);
    if (face == nullptr) {
        qCWarning(lcQpaFonts) << "Failed to create DirectWrite face from font data. Font may be unsupported.";
        return QStringList();
    }

    wchar_t defaultLocale[LOCALE_NAME_MAX_LENGTH];
    bool hasDefaultLocale = GetUserDefaultLocaleName(defaultLocale, LOCALE_NAME_MAX_LENGTH) != 0;
    wchar_t englishLocale[] = L"en-us";

    static const int SMOOTH_SCALABLE = 0xffff;
    const QString foundryName; // No such concept.
    const bool scalable = true;
    const bool antialias = false;
    const int size = SMOOTH_SCALABLE;

    QSupportedWritingSystems writingSystems;
    writingSystems.setSupported(QFontDatabase::Any);
    writingSystems.setSupported(QFontDatabase::Latin);

    QStringList ret;
    IDWriteFontFace3 *face3 = nullptr;
    if (SUCCEEDED(face->QueryInterface(__uuidof(IDWriteFontFace3),
                                      reinterpret_cast<void **>(&face3)))) {
        QString defaultLocaleFamilyName;
        QString englishLocaleFamilyName;

        IDWriteLocalizedStrings *names;
        if (SUCCEEDED(face3->GetFamilyNames(&names))) {
            defaultLocaleFamilyName = hasDefaultLocale ? localeString(names, defaultLocale) : QString();
            englishLocaleFamilyName = localeString(names, englishLocale);

            names->Release();
        }

        QString defaultLocaleStyleName;
        QString englishLocaleStyleName;
        if (SUCCEEDED(face3->GetFaceNames(&names))) {
            defaultLocaleStyleName = hasDefaultLocale ? localeString(names, defaultLocale) : QString();
            englishLocaleStyleName = localeString(names, englishLocale);

            names->Release();
        }

        QFont::Stretch stretch = fromDirectWriteStretch(face3->GetStretch());
        QFont::Style style = fromDirectWriteStyle(face3->GetStyle());
        QFont::Weight weight = fromDirectWriteWeight(face3->GetWeight());
        bool fixed = face3->IsMonospacedFont();

        qCDebug(lcQpaFonts) << "\tFont names:" << englishLocaleFamilyName << ", " << defaultLocaleFamilyName
                            << ", style names:" << englishLocaleStyleName << ", " << defaultLocaleStyleName
                            << ", stretch:" << stretch
                            << ", style:" << style
                            << ", weight:" << weight
                            << ", fixed:" << fixed;

        if (!englishLocaleFamilyName.isEmpty()) {
            ret.append(englishLocaleFamilyName);
            QPlatformFontDatabase::registerFont(englishLocaleFamilyName, englishLocaleStyleName, QString(), weight, style, stretch, antialias, scalable, size, fixed, writingSystems, face);
            face->AddRef();
        }

        if (!defaultLocaleFamilyName.isEmpty() && defaultLocaleFamilyName != englishLocaleFamilyName) {
            ret.append(defaultLocaleFamilyName);
            QPlatformFontDatabase::registerFont(defaultLocaleFamilyName, defaultLocaleStyleName, QString(), weight, style, stretch, antialias, scalable, size, fixed, writingSystems, face);
            face->AddRef();
        }

        face3->Release();
    } else {
        qCWarning(lcQpaFonts) << "Unable to query IDWriteFontFace3 interface from font face.";
    }

    face->Release();

    return ret;
}

void QWindowsDirectWriteFontDatabase::releaseHandle(void *handle)
{
    IDWriteFontFace *face = reinterpret_cast<IDWriteFontFace *>(handle);
    face->Release();
}

bool QWindowsDirectWriteFontDatabase::fontsAlwaysScalable() const
{
    return true;
}

bool QWindowsDirectWriteFontDatabase::isPrivateFontFamily(const QString &family) const
{
    Q_UNUSED(family);
    return false;
}

void QWindowsDirectWriteFontDatabase::populateFontDatabase()
{
    wchar_t defaultLocale[LOCALE_NAME_MAX_LENGTH];
    bool hasDefaultLocale = GetUserDefaultLocaleName(defaultLocale, LOCALE_NAME_MAX_LENGTH) != 0;
    wchar_t englishLocale[] = L"en-us";

    QString defaultFontName = defaultFont().family();
    QString systemDefaultFontName = systemDefaultFont().family();

    IDWriteFontCollection *fontCollection;
    if (SUCCEEDED(data()->directWriteFactory->GetSystemFontCollection(&fontCollection))) {
        for (uint i = 0; i < fontCollection->GetFontFamilyCount(); ++i) {
            IDWriteFontFamily *fontFamily;
            if (SUCCEEDED(fontCollection->GetFontFamily(i, &fontFamily))) {
                QString defaultLocaleName;
                QString englishLocaleName;

                IDWriteLocalizedStrings *names;
                if (SUCCEEDED(fontFamily->GetFamilyNames(&names))) {
                    if (hasDefaultLocale)
                        defaultLocaleName = localeString(names, defaultLocale);

                    englishLocaleName = localeString(names, englishLocale);
                }

                qCDebug(lcQpaFonts) << "Registering font, english name = " << englishLocaleName << ", name in current locale = " << defaultLocaleName;
                if (!defaultLocaleName.isEmpty()) {
                    registerFontFamily(defaultLocaleName);
                    m_populatedFonts.insert(defaultLocaleName, fontFamily);
                    fontFamily->AddRef();

                    if (defaultLocaleName == defaultFontName && defaultFontName != systemDefaultFontName) {
                        qDebug(lcQpaFonts) << "Adding default font" << systemDefaultFontName << "as alternative to" << defaultLocaleName;

                        m_populatedFonts.insert(systemDefaultFontName, fontFamily);
                        fontFamily->AddRef();
                    }
                }

                if (!englishLocaleName.isEmpty() && englishLocaleName != defaultLocaleName) {
                    registerFontFamily(englishLocaleName);
                    m_populatedFonts.insert(englishLocaleName, fontFamily);
                    fontFamily->AddRef();

                    if (englishLocaleName == defaultFontName && defaultFontName != systemDefaultFontName) {
                        qDebug(lcQpaFonts) << "Adding default font" << systemDefaultFontName << "as alternative to" << englishLocaleName;

                        m_populatedFonts.insert(systemDefaultFontName, fontFamily);
                        fontFamily->AddRef();
                    }
                }

                fontFamily->Release();
            }
        }
    }
}

QFont QWindowsDirectWriteFontDatabase::defaultFont() const
{
    return QFont(QStringLiteral("Segoe UI"));
}

#endif // QT_USE_DIRECTWRITE3

QT_END_NAMESPACE
