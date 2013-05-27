/****************************************************************************
** Copyright (C) 2001-2010 Klaralvdalens Datakonsult AB.  All rights reserved.
**
** This file is part of the KD Tools library.
**
** Licensees holding valid commercial KD Tools licenses may use this file in
** accordance with the KD Tools Commercial License Agreement provided with
** the Software.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
**********************************************************************/

#include "kdupdaterupdateoperation.h"

#include "kdupdaterapplication.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>

/*!
   \ingroup kdupdater
   \class KDUpdater::UpdateOperation kdupdaterupdateoperation.h KDUpdaterUpdateOperation
   \brief Abstract base class for update operations.

   The \ref KDUpdater::UpdateOperation is an abstract class that specifies an interface for
   update operations. Concrete implementations of this class must perform a single update
   operation like copy, move, delete etc.

   \note Two separate threads cannot be using a single instance of KDUpdater::UpdateOperation
   at the same time.
*/

/*
 * \internal
 * Returns a filename for a temporary file based on \a templateName
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

using namespace KDUpdater;

/*!
   Constructor
*/
UpdateOperation::UpdateOperation()
    : m_error(0)
{}

/*!
   Destructor
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
   Returns a command line string that describes the update operation. The returned
   string would be of the form

   <name> <arg1> <arg2> <arg3> ....
*/
QString UpdateOperation::operationCommand() const
{
    QString argsStr = m_arguments.join(QLatin1String( " " ));
    return QString::fromLatin1( "%1 %2" ).arg(m_name, argsStr);
}

/*!
 Returns true if there exists a setting called \a name. Otherwise returns false.
*/
bool UpdateOperation::hasValue(const QString &name) const
{
    return m_values.contains(name);
}

/*!
 Clears the value of setting \a name and removes it.
 \post hasValue( \a name ) returns false.
*/
void UpdateOperation::clearValue(const QString &name)
{
    m_values.remove(name);
}

/*!
 Returns the value of setting \a name. If the setting does not exists,
 this returns an empty QVariant.
*/
QVariant UpdateOperation::value(const QString &name) const
{
    return hasValue(name) ? m_values[name] : QVariant();
}

/*!
  Sets the value of setting \a name to \a value.
*/
void UpdateOperation::setValue(const QString &name, const QVariant &value)
{
    m_values[name] = value;
}

/*!
   Sets a update operation name. Subclasses will have to provide a unique
   name to describe this operation.
*/
void UpdateOperation::setName(const QString &name)
{
    m_name = name;
}

/*!
   Through this function, arguments to the update operation can be specified
   to the update operation.
*/
void UpdateOperation::setArguments(const QStringList &args)
{
    m_arguments = args;
}

/*!
   Returns the last set function arguments.
*/
QStringList UpdateOperation::arguments() const
{
    return m_arguments;
}

/*!
   Returns error details in case performOperation() failed.
*/
QString UpdateOperation::errorString() const
{
    return m_errorString;
}

/*!
 * Can be used by subclasses to report more detailed error codes (optional).
 * To check if an operation was successful, use the return value of performOperation().
 */
int UpdateOperation::error() const
{
    return m_error;
}

/*!
 * Used by subclasses to set the error string.
 */
void UpdateOperation::setErrorString(const QString &str)
{
    m_errorString = str;
}

/*!
 * Used by subclasses to set the error code.
 */
void UpdateOperation::setError(int error, const QString &errorString)
{
    m_error = error;
    if (!errorString.isNull())
        m_errorString = errorString;
}

/*!
   Clears the previously set argument list and application
*/
void UpdateOperation::clear()
{
    m_arguments.clear();
}

QStringList UpdateOperation::filesForDelayedDeletion() const
{
    return m_delayedDeletionFiles;
}

/*!
   Registers a file to be deleted later, once the application was restarted
   (and the file isn't used anymore for sure).
   @param files the files to be registered
*/
void UpdateOperation::registerForDelayedDeletion(const QStringList &files)
{
    m_delayedDeletionFiles << files;
}
 
/*!
 Tries to delete \a file. If \a file can't be deleted, it gets registered for delayed deletion.
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
            *errorString = f.errorString();
        return false;
    }
    registerForDelayedDeletion(QStringList(backup));
    return true;
}

/*!
   \fn virtual void KDUpdater::UpdateOperation::backup() = 0;

   Subclasses must implement this function to backup any data before performing the action.
*/

/*!
   \fn virtual bool KDUpdater::UpdateOperation::performOperation() = 0;

   Subclasses must implement this function to perform the update operation
*/

/*!
   \fn virtual bool KDUpdater::UpdateOperation::undoOperation() = 0;

   Subclasses must implement this function to perform the reverse of the operation.
*/

/*!
   \fn virtual bool KDUpdater::UpdateOperation::testOperation() = 0;

   Subclasses must implement this function to perform the test operation.
*/

/*!
   \fn virtual bool KDUpdater::UpdateOperation::clone() = 0;

   Subclasses must implement this function to clone the current operation.
*/

/*!
  Saves this UpdateOperation in XML. You can override this method to store your own extra-data.
  The default implementation is taking care of arguments and values set via setValue.
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
  Restores UpdateOperation's arguments and values from the XML document \a doc.
  Returns true on success, otherwise false.
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
  Restores UpdateOperation's arguments and values from the XML document at path \a xml.
  Returns true on success, otherwise false.
  \overload
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
