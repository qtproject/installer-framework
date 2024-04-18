/**************************************************************************
**
** Copyright (C) 2024 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "componentalias.h"

#include "constants.h"
#include "globals.h"
#include "packagemanagercore.h"
#include "updater.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace QInstaller {

static const QStringList scPossibleElements {
    scName,
    scDisplayName,
    scDescription,
    scVersion,
    scVirtual,
    scRequiredComponents,
    scRequiredAliases,
    scOptionalComponents,
    scOptionalAliases,
    scReleaseDate
};

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::AliasSource
    \brief Describes a source for alias declarations.
*/

/*!
    \enum QInstaller::AliasSource::SourceFileFormat

    This enum type holds the possible file formats for alias source:

    \value  Unknown
            Invalid or unknown file format.
    \value  Xml
            XML file format.
    \value  Json
            JSON file format.
*/

/*!
    Constructs an alias source with empty information.
*/
AliasSource::AliasSource()
    : priority(-1)
{}

/*!
    Constructs an alias source with source file format \a aFormat, filename \a aFilename, and priority \a aPriority.
*/
AliasSource::AliasSource(SourceFileFormat aFormat, const QString &aFilename, int aPriority)
    : format(aFormat)
    , filename(aFilename)
    , priority(aPriority)
{}

/*!
    Copy-constructs an alias source from \a other.
*/
AliasSource::AliasSource(const AliasSource &other)
{
    format = other.format;
    filename = other.filename;
    priority = other.priority;
}

/*!
    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/
hashValue qHash(const AliasSource &key, hashValue seed)
{
    return qHash(key.filename, seed) ^ key.priority;
}

/*!
    Returns \c true if \a lhs and \a rhs are equal; otherwise returns \c false.
*/
bool operator==(const AliasSource &lhs, const AliasSource &rhs)
{
    return lhs.filename == rhs.filename && lhs.priority == rhs.priority && lhs.format == rhs.format;
}

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::AliasFinder
    \brief Creates component alias objects from parsed alias source files, based
           on version and source priorities.
*/

/*!
    Constructs a new alias finder with \a core as the package manager instance.
*/
AliasFinder::AliasFinder(PackageManagerCore *core)
    : m_core(core)
{
}

/*!
    Destroys the finder and cleans unreleased results.
*/
AliasFinder::~AliasFinder()
{
    clear();
}

/*!
    Runs the finder. Parses the alias source files and creates component alias
    objects based on the parsed data. Same alias may be declared in multiple source
    files, thus source priority and version information is used to decide which
    source is used for creating the alias object.

    Any previous results are cleared when calling this.

    Returns \c true if at least one alias was found, \c false otherwise.
*/
bool AliasFinder::run()
{
    clear();

    if (m_sources.isEmpty())
        return false;

    // 1. Parse source files
    for (auto &source : qAsConst(m_sources)) {
        if (source.format == AliasSource::SourceFileFormat::Unknown) {
            qCWarning(QInstaller::lcInstallerInstallLog)
                << "Unknown alias source format for file:" << source.filename;
            continue;
        }
        if (source.format == AliasSource::SourceFileFormat::Xml)
            parseXml(source);
        else if (source.format == AliasSource::SourceFileFormat::Json)
            parseJson(source);
    }

    // 2. Create aliases based on priority & version
    for (auto &data : qAsConst(m_aliasData)) {
        const QString name = data.value(scName).toString();
        const Resolution resolution = checkPriorityAndVersion(data);
        if (resolution == Resolution::KeepExisting)
            continue;

        if (resolution == Resolution::RemoveExisting)
            delete m_aliases.take(name);

        ComponentAlias *alias = new ComponentAlias(m_core);
        AliasData::const_iterator it;
        for (it = data.cbegin(); it != data.cend(); ++it) {
            if (it.value().canConvert<QString>())
                alias->setValue(it.key(), it.value().toString());
        }
        m_aliases.insert(name, alias);
    }

    return !m_aliases.isEmpty();
}

/*!
    Returns a list of the found aliases.
*/
QList<ComponentAlias *> AliasFinder::aliases() const
{
    return m_aliases.values();
}

/*!
    Sets the alias sources to look alias information from to \a sources.
*/
void AliasFinder::setAliasSources(const QSet<AliasSource> &sources)
{
    clear();
    m_sources = sources;
}

/*!
    Clears the results of the finder.
*/
void AliasFinder::clear()
{
    qDeleteAll(m_aliases);

    m_aliases.clear();
    m_aliasData.clear();
}

/*!
    Reads an XML file specified by \a filename, and constructs a variant map of
    the data for each alias.

    Returns \c true on success, \c false otherwise.
*/
bool AliasFinder::parseXml(AliasSource source)
{
    QFile file(source.filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(QInstaller::lcInstallerInstallLog)
            << "Cannot open alias definition for reading:" << file.errorString();
        return false;
    }

    QString error;
    int errorLine;
    int errorColumn;

    QDomDocument doc;
    if (!doc.setContent(&file, &error, &errorLine, &errorColumn)) {
        qCWarning(QInstaller::lcInstallerInstallLog)
            << "Cannot read alias definition document:" << error
            << "line:" << errorLine << "column:" << errorColumn;
        return false;
    }
    file.close();

    const QDomElement root = doc.documentElement();
    const QDomNodeList children = root.childNodes();

    for (int i = 0; i < children.count(); ++i) {
        const QDomElement el = children.at(i).toElement();
        const QString tag = el.tagName();
        if (el.isNull() || tag != scAlias) {
            qCWarning(lcInstallerInstallLog) << "Unexpected element name:" << tag;
            continue;
        }

        AliasData data;
        data.insert(QLatin1String("source"), QVariant::fromValue(source));

        const QDomNodeList c2 = el.childNodes();
        for (int j = 0; j < c2.count(); ++j) {
            const QDomElement el2 = c2.at(j).toElement();
            const QString tag2 = el2.tagName();
            if (!scPossibleElements.contains(tag2)) {
                qCWarning(lcInstallerInstallLog) << "Unexpected element name:" << tag2;
                continue;
            }
            data.insert(tag2, el2.text());
        }

        m_aliasData.insert(data.value(scName).toString(), data);
    }

    return true;
}

/*!
    Reads a JSON file specified by \a source, and constructs a variant map of
    the data for each alias.

    Returns \c true on success, \c false otherwise.
*/
bool AliasFinder::parseJson(AliasSource source)
{
    QFile file(source.filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(QInstaller::lcInstallerInstallLog)
            << "Cannot open alias definition for reading:" << file.errorString();
        return false;
    }

    const QByteArray jsonData = file.readAll();
    const QJsonDocument doc(QJsonDocument::fromJson(jsonData));
    const QJsonObject docJsonObject = doc.object();

    const QJsonArray aliases = docJsonObject.value(QLatin1String("alias-packages")).toArray();
    for (auto &it : aliases) {
        AliasData data;
        data.insert(QLatin1String("source"), QVariant::fromValue(source));

        QJsonObject aliasObj = it.toObject();
        for (const auto &key : aliasObj.keys()) {
            if (!scPossibleElements.contains(key)) {
                qCWarning(lcInstallerInstallLog) << "Unexpected element name:" << key;
                continue;
            }

            const QJsonValue jsonValue = aliasObj.value(key);
            if (key == scRequiredComponents || key == scRequiredAliases
                    || key == scOptionalComponents || key == scOptionalAliases) {
                const QJsonArray requirements = jsonValue.toArray();
                QString requiresString;

                for (const auto &it2 : requirements) {
                    requiresString.append(it2.toString());
                    if (it2 != requirements.last())
                        requiresString.append(QLatin1Char(','));
                }

                data.insert(key, requiresString);
            } else if (key == scVirtual) {
                data.insert(key, QVariant(jsonValue.toBool()))->toString();
            } else {
                data.insert(key, jsonValue.toString());
            }
        }

        m_aliasData.insert(data.value(scName).toString(), data);
    }

    return true;
}

/*!
    Checks whether \a data should be used for creating a new alias object,
    based on version and source priority.

    If an alias of the same name exists, always use the one with the higher
    version. If the new alias has the same version but a higher
    priority, use the new new alias. Otherwise keep the already existing alias.

    Returns the resolution of the check.
*/
AliasFinder::Resolution AliasFinder::checkPriorityAndVersion(const AliasData &data) const
{
    for (const auto &existingData : m_aliasData.values(data.value(scName).toString())) {
        if (existingData == data)
            continue;

        const int versionMatch = KDUpdater::compareVersion(data.value(scVersion).toString(),
            existingData.value(scVersion).toString());

        const AliasSource newSource = data.value(QLatin1String("source")).value<AliasSource>();
        const AliasSource oldSource = existingData.value(QLatin1String("source")).value<AliasSource>();

        if (versionMatch > 0) {
            // new alias has higher version, use
            qCDebug(QInstaller::lcDeveloperBuild).nospace() << "Remove Alias 'Name: "
                << data.value(scName).toString() << ", Version: " << existingData.value(scVersion).toString()
                << ", Source: " << oldSource.filename
                << "' found an alias with higher version 'Name: "
                << data.value(scName).toString() << ", Version: " << data.value(scVersion).toString()
                << ", Source: " << newSource.filename << "'";

            return Resolution::RemoveExisting;
        }

        if ((versionMatch == 0) && (newSource.priority > oldSource.priority)) {
            // new alias version equals but priority is higher, use
            qCDebug(QInstaller::lcDeveloperBuild).nospace() << "Remove Alias 'Name: "
                << data.value(scName).toString() << ", Priority: " << oldSource.priority
                << ", Source: " << oldSource.filename
                << "' found an alias with higher priority 'Name: "
                << data.value(scName).toString() << ", Priority: " << newSource.priority
                << ", Source: " << newSource.filename << "'";

            return Resolution::RemoveExisting;
        }

        return Resolution::KeepExisting; // otherwise keep existing
    }

    return Resolution::AddNew;
}

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::ComponentAlias
    \brief The ComponentAlias class represents an alias for single or multiple components.
*/

/*!
    \enum QInstaller::ComponentAlias::UnstableError

    This enum type holds the possible reasons for marking an alias unstable:

    \value  ReferenceToUnstable
            Alias requires another alias that is marked unstable.
    \value  MissingComponent
            Alias requires a component that is missing.
    \value  UnselectableComponent
            Alias requires a component that cannot be selected.
    \value  MissingAlias
            Alias requires another alias that is missing.
    \value  ComponentNameConfict
            Alias has a name that conflicts with a name of a component
*/

/*!
    Constructs a new component alias with \a core as the package manager instance.
*/
ComponentAlias::ComponentAlias(PackageManagerCore *core)
    : m_core(core)
    , m_selected(false)
    , m_unstable(false)
    , m_missingOptionalComponents (false)
{
}

/*!
    Destructs the alias.
*/
ComponentAlias::~ComponentAlias()
{
}

/*!
    Returns the name of the alias.
*/
QString ComponentAlias::name() const
{
    return m_variables.value(scName);
}

/*!
    Returns the display name of the alias.
*/
QString ComponentAlias::displayName() const
{
    return m_variables.value(scDisplayName);
}

/*!
    Returns the description text of the alias.
*/
QString ComponentAlias::description() const
{
    return m_variables.value(scDescription);
}

/*!
    Returns the version of the alias.
*/
QString ComponentAlias::version() const
{
    return m_variables.value(scVersion);
}

/*!
    Returns \c true if the alias is virtual, \c false otherwise.

    Virtual aliases are aliases that cannot be selected by the
    user, and are invisible. They can be required by other aliases however.
*/
bool ComponentAlias::isVirtual() const
{
    return m_variables.value(scVirtual, scFalse).toLower() == scTrue;
}

/*!
    Returns \c true if the alias is selected for installation, \c false otherwise.
*/
bool ComponentAlias::isSelected() const
{
    return m_selected;
}

/*!
    Sets the selection state of the alias to \a selected. The selection
    does not have an effect if the alias is unselectable.
*/
void ComponentAlias::setSelected(bool selected)
{
    if (selected && (isUnstable() || isVirtual()))
        return;

    m_selected = selected;
}

/*!
    Returns the list of components required by this alias, or an
    empty list if this alias does not require any components.
*/
QList<Component *> ComponentAlias::components()
{
    if (m_components.isEmpty()) {
        m_componentErrorMessages.clear();
        m_missingOptionalComponents = false;
        const QStringList componentList = QInstaller::splitStringWithComma(
            m_variables.value(scRequiredComponents));

        const QStringList optionalComponentList = QInstaller::splitStringWithComma(
            m_variables.value(scOptionalComponents));

        addRequiredComponents(componentList, false);
        addRequiredComponents(optionalComponentList, true);
    }

    return m_components;
}

/*!
    Returns the list of other aliases required by this alias, or an
    empty list if this alias does not require any other aliases.
*/
QList<ComponentAlias *> ComponentAlias::aliases()
{
    if (m_aliases.isEmpty()) {
        const QStringList aliasList = QInstaller::splitStringWithComma(
            m_variables.value(scRequiredAliases));

        const QStringList optionalAliasList = QInstaller::splitStringWithComma(
            m_variables.value(scOptionalAliases));

        addRequiredAliases(aliasList, false);
        addRequiredAliases(optionalAliasList, true);
    }

    return m_aliases;
}

/*!
    Returns the value specified by \a key, with an optional default value \a defaultValue.
*/
QString ComponentAlias::value(const QString &key, const QString &defaultValue) const
{
    return m_variables.value(key, defaultValue);
}

/*!
    Sets the value specified by \a key to \a value. If the value exists already,
    it is replaced with the new value.
*/
void ComponentAlias::setValue(const QString &key, const QString &value)
{
    const QString normalizedValue = m_core->replaceVariables(value);
    if (m_variables.value(key) == normalizedValue)
        return;

    m_variables[key] = normalizedValue;
}

/*!
    Returns all keys for the component alias values.
*/
QStringList ComponentAlias::keys() const
{
    return m_variables.keys();
}

/*!
    Returns \c true if the alias is marked unstable, \c false otherwise.
*/
bool ComponentAlias::isUnstable() const
{
    return m_unstable;
}

/*!
    Sets the alias unstable with \a error, and a \a message describing the error.
*/
void ComponentAlias::setUnstable(UnstableError error, const QString &message)
{
    setSelected(false);
    m_unstable = true;

    const QMetaEnum metaEnum = QMetaEnum::fromType<ComponentAlias::UnstableError>();
    emit m_core->unstableComponentFound(
        QLatin1String(metaEnum.valueToKey(error)), message, name());
}

QString ComponentAlias::componentErrorMessage() const
{
    return m_componentErrorMessages;
}

bool ComponentAlias::missingOptionalComponents() const
{
    return m_missingOptionalComponents;
}

/*!
    \internal

    Adds the \a aliases to the list of required aliases by this alias. If \a optional
    is \c true, missing alias references are ignored.
*/
void ComponentAlias::addRequiredAliases(const QStringList &aliases, const bool optional)
{
    for (const auto &aliasName : aliases) {
        ComponentAlias *alias = m_core->aliasByName(aliasName);
        if (!alias) {
            if (optional)
                continue;

            const QString error = name() + QLatin1String(" alias requires alias ") + aliasName
                + QLatin1String(", that is not found");
            qCWarning(lcInstallerInstallLog) << error;

            setUnstable(UnstableError::MissingAlias, error);
            continue;
        }

        if (alias->isUnstable()) {
            if (optional)
                continue;
            const QString error = name() + QLatin1String(" alias requires alias ")
                 + aliasName + QLatin1String(", that is marked unstable");
            qCWarning(lcInstallerInstallLog) << error;

            setUnstable(UnstableError::ReferenceToUnstable, error);
            continue;
        }

        m_aliases.append(alias);
    }
}

/*!
    \internal

    Adds the \a components to the list of required components by this alias. If \a optional
    is \c true, missing component references are ignored.
*/
void ComponentAlias::addRequiredComponents(const QStringList &components, const bool optional)
{
    for (const auto &componentName : components) {
        Component *component = m_core->componentByName(componentName);
        if (!component) {
            if (optional) {
                m_missingOptionalComponents = true;
                continue;
            }

            const QString error = name() + QLatin1String(" alias requires component ")
                + componentName + QLatin1String(", that is not found");
            if (!m_componentErrorMessages.isEmpty())
                m_componentErrorMessages.append(QLatin1String("\n"));
            m_componentErrorMessages.append(error);

            setUnstable(UnstableError::MissingComponent, error);
            continue;
        }

        if (component->isUnstable() || !component->isCheckable()) {
            if (optional)
                continue;
            const QString error = name() + QLatin1String(" alias requires component ")
                + componentName + QLatin1String(", that is uncheckable or unstable");
            if (!m_componentErrorMessages.isEmpty())
                m_componentErrorMessages.append(QLatin1String("\n"));
            m_componentErrorMessages.append(error);

            setUnstable(UnstableError::UnselectableComponent, error);
            continue;
        }

        m_components.append(component);
    }
}

} // namespace QInstaller
