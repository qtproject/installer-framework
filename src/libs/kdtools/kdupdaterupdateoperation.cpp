/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#include "kdupdaterupdateoperation.h"

#include "kdupdaterapplication.h"

#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>

using namespace KDUpdater;

/*!
   \inmodule kdupdater
   \class KDUpdater::UpdateOperation
   \brief The UpdateOperation class is an abstract base class for update operations.

   The KDUpdater::UpdateOperation is an abstract class that specifies an interface for
   update operations. Concrete implementations of this class must perform a single update
   operation, such as copy, move, or delete.

   \note Two separate threads cannot be using a single instance of KDUpdater::UpdateOperation
   at the same time.
*/

/*!
    \enum UpdateOperation::Error
    This enum code specifies error codes related to operation arguments and
    operation runtime failures.

    \value  NoError
            No error occurred.
    \value  InvalidArguments
            Number of arguments does not match or an invalid argument was set.
    \value  UserDefinedError
            An error occurred during operation run. Use UpdateOperation::errorString()
            to get the human-readable description of the error that occurred.
*/

/*
    \internal
    Returns a filename for a temporary file based on \a templateName.
*/
static QString backupFileName(const QString &templateName)
{
    const QFileInfo templ(templateName);
    QTemporaryFile file( QDir::temp().absoluteFilePath(templ.fileName()));
    file.open();
    const QString name = file.fileName();
    file.close();
    file.remove();
    return name;
}

/*!
    \internal
*/
UpdateOperation::UpdateOperation()
    : m_error(0)
{}

/*!
    \internal
*/
UpdateOperation::~UpdateOperation()
{
    if (Application *app = Application::instance())
        app->addFilesForDelayedDeletion(filesForDelayedDeletion());
}

/*!
    Returns the update operation name.

    \sa setName()
*/
QString UpdateOperation::name() const
{
    return m_name;
}

/*!
    Returns a command line string that describes the update operation. The returned string will be
    of the form:

    \c{<name> <arg1> <arg2> <arg3> ....}
*/
QString UpdateOperation::operationCommand() const
{
    QString argsStr = m_arguments.join(QLatin1String( " " ));
    return QString::fromLatin1( "%1 %2" ).arg(m_name, argsStr);
}

/*!
    Returns \c true if a value called \a name exists, otherwise returns \c false.
*/
bool UpdateOperation::hasValue(const QString &name) const
{
    return m_values.contains(name);
}

/*!
    Clears the value of \a name and removes it.
*/
void UpdateOperation::clearValue(const QString &name)
{
    m_values.remove(name);
}

/*!
    Returns the value of \a name. If the value does not exist, returns an empty QVariant.
*/
QVariant UpdateOperation::value(const QString &name) const
{
    return m_values.value(name);
}

/*!
    Sets the value of \a name to \a value.
*/
void UpdateOperation::setValue(const QString &name, const QVariant &value)
{
    m_values[name] = value;
}

/*!
    Sets the name of the operation to \a name. Subclasses will have to provide a unique name to
    describe the operation.
*/
void UpdateOperation::setName(const QString &name)
{
    m_name = name;
}

/*!
   Sets the arguments for the update operation to \a args.
*/
void UpdateOperation::setArguments(const QStringList &args)
{
    m_arguments = args;
}

/*!
    Returns the arguments of the update operation.
*/
QStringList UpdateOperation::arguments() const
{
    return m_arguments;
}

struct StartsWith
{
    StartsWith(const QString &searchTerm)
        : m_searchTerm(searchTerm) {}

    bool operator()(const QString &searchString)
    {
        return searchString.startsWith(m_searchTerm);
    }

    QString m_searchTerm;
};

/*!
    Searches the arguments for the key specified by \a key. If it can find the
    key, it returns the value set for it. Otherwise, it returns \a defaultValue.
    Arguments are specified in the following form: \c{key=value}.
*/
QString UpdateOperation::argumentKeyValue(const QString &key, const QString &defaultValue) const
{
    const QString keySeparater(key + QLatin1String("="));
    const QStringList tArguments(arguments());
    QStringList::const_iterator it = std::find_if(tArguments.begin(), tArguments.end(),
        StartsWith(keySeparater));
    if (it == tArguments.end())
        return defaultValue;

    const QString value = it->mid(keySeparater.size());

    it = std::find_if(++it, tArguments.end(), StartsWith(keySeparater));
    if (it != tArguments.end()) {
        qWarning() << QString::fromLatin1("There are multiple keys in the arguments calling"
            " '%1'. Only the first found '%2' is used: '%3'").arg(name(), key, arguments().join(
            QLatin1String("; ")));
    }
    return value;
}

/*!
    Returns a human-readable description of the last error that occurred.
*/
QString UpdateOperation::errorString() const
{
    return m_errorString;
}

/*!
    Returns the error that was found during the processing of the operation. If no
    error was found, returns NoError. Subclasses can set more detailed error codes (optional).

    \note To check if an operation was successful, use the return value of performOperation(),
    undoOperation(), or testOperation().
*/
int UpdateOperation::error() const
{
    return m_error;
}

/*!
    Sets the human-readable description of the last error that occurred to \a str.
*/
void UpdateOperation::setErrorString(const QString &str)
{
    m_errorString = str;
}

/*!
    Sets the error condition to be \a error. The human-readable message is set to \a errorString.

    \sa UpdateOperation::error()
    \sa UpdateOperation::errorString()
*/
void UpdateOperation::setError(int error, const QString &errorString)
{
    m_error = error;
    if (!errorString.isNull())
        m_errorString = errorString;
}

/*!
    Clears the previously set arguments.
*/
void UpdateOperation::clear()
{
    m_arguments.clear();
}

/*!
    Returns the list of files that are scheduled for later deletion.
*/
QStringList UpdateOperation::filesForDelayedDeletion() const
{
    return m_delayedDeletionFiles;
}

/*!
     Registers a list of \a files to be deleted later once the application was restarted and the
     file or files are not used anymore.
*/
void UpdateOperation::registerForDelayedDeletion(const QStringList &files)
{
    m_delayedDeletionFiles << files;
}

/*!
    Tries to delete \a file. If \a file cannot be deleted, it is registered for delayed deletion.

    If a backup copy of the file cannot be created, returns \c false and displays the error
    message specified by \a errorString.
*/
bool UpdateOperation::deleteFileNowOrLater(const QString &file, QString *errorString)
{
    if (file.isEmpty() || QFile::remove(file))
        return true;

    if (!QFile::exists(file))
        return true;

    const QString backup = backupFileName(file);
    QFile f(file);
    if (!f.rename(backup)) {
        if (errorString)
            *errorString = tr("Renaming %1 into %2 failed with %3.").arg(file, backup, f.errorString());
        return false;
    }
    registerForDelayedDeletion(QStringList(backup));
    return true;
}

/*!
    \fn virtual void KDUpdater::UpdateOperation::backup() = 0;

    Subclasses must implement this function to back up any data before performing the action.
*/

/*!
    \fn virtual bool KDUpdater::UpdateOperation::performOperation() = 0;

    Subclasses must implement this function to perform the update operation.

    Returns \c true if the operation is successful.
*/

/*!
    \fn virtual bool KDUpdater::UpdateOperation::undoOperation() = 0;

    Subclasses must implement this function to perform the undo of the update operation.

    Returns \c true if the operation is successful.
*/

/*!
    \fn virtual bool KDUpdater::UpdateOperation::testOperation() = 0;

    Subclasses must implement this function to perform the test operation.

    Returns \c true if the operation is successful.
*/

/*!
    \fn virtual bool KDUpdater::UpdateOperation::clone() const = 0;

    Subclasses must implement this function to clone the current operation.
*/

/*!
    Saves operation arguments and values as an XML document and returns the
    document. You can override this method to store your
    own extra-data. Extra-data can be any data that you need to store to perform or undo the
    operation. The default implementation is taking care of arguments and values set via
    UpdateOperation::setValue().
*/
QDomDocument UpdateOperation::toXml() const
{
    QDomDocument doc;
    QDomElement root = doc.createElement(QLatin1String("operation"));
    doc.appendChild(root);
    QDomElement args = doc.createElement(QLatin1String("arguments"));
    Q_FOREACH (const QString &s, arguments()) {
        QDomElement arg = doc.createElement(QLatin1String("argument"));
        arg.appendChild(doc.createTextNode(s));
        args.appendChild(arg);
    }
    root.appendChild(args);
    if (m_values.isEmpty())
        return doc;

    // append all values set with setValue
    QDomElement values = doc.createElement(QLatin1String("values"));
    for (QVariantMap::const_iterator it = m_values.begin(); it != m_values.end(); ++it) {
        QDomElement value = doc.createElement(QLatin1String("value"));
        const QVariant& variant = it.value();
        value.setAttribute(QLatin1String("name"), it.key());
        value.setAttribute(QLatin1String("type"), QLatin1String( QVariant::typeToName( variant.type())));

        if (variant.type() != QVariant::List && variant.type() != QVariant::StringList
            && variant.canConvert(QVariant::String)) {
            // it can convert to string? great!
            value.appendChild( doc.createTextNode(variant.toString()));
        } else {
            // no? then we have to go the hard way...
            QByteArray data;
            QDataStream stream(&data, QIODevice::WriteOnly);
            stream << variant;
            value.appendChild(doc.createTextNode(QLatin1String( data.toBase64().data())));
        }
        values.appendChild(value);
    }
    root.appendChild(values);
    return doc;
}

/*!
    Restores operation arguments and values from the XML document \a doc. Returns \c true on
    success, otherwise \c false.
*/
bool UpdateOperation::fromXml(const QDomDocument &doc)
{
    QStringList args;
    const QDomElement root = doc.documentElement();
    const QDomElement argsElem = root.firstChildElement(QLatin1String("arguments"));
    Q_ASSERT(! argsElem.isNull());
    for (QDomNode n = argsElem.firstChild(); ! n.isNull(); n = n.nextSibling()) {
        const QDomElement e = n.toElement();
        if (!e.isNull() && e.tagName() == QLatin1String("argument"))
            args << e.text();
    }
    setArguments(args);

    m_values.clear();
    const QDomElement values = root.firstChildElement(QLatin1String("values"));
    for (QDomNode n = values.firstChild(); !n.isNull(); n = n.nextSibling()) {
        const QDomElement v = n.toElement();
        if (v.isNull() || v.tagName() != QLatin1String("value"))
            continue;

        const QString name = v.attribute(QLatin1String("name"));
        const QString type = v.attribute(QLatin1String("type"));
        const QString value = v.text();

        const QVariant::Type t = QVariant::nameToType(type.toLatin1().data());
        QVariant var = qVariantFromValue(value);
        if (t == QVariant::List || t == QVariant::StringList || !var.convert(t)) {
            QDataStream stream(QByteArray::fromBase64( value.toLatin1()));
            stream >> var;
        }

        m_values[name] = var;
    }

    return true;
}

/*!
    \overload

    Restores operation arguments and values from the XML file at path \a xml. Returns \c true on
    success, otherwise \c false.
*/
bool UpdateOperation::fromXml(const QString &xml)
{
    QDomDocument doc;
    QString errorMsg;
    int errorLine;
    int errorColumn;
    if (!doc.setContent( xml, &errorMsg, &errorLine, &errorColumn)) {
        qWarning() << "Error parsing xml error=" << errorMsg << "line=" << errorLine << "column=" << errorColumn;
        return false;
    }
    return fromXml(doc);
}
