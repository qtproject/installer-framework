#ifndef SENTRYHELPER_H
#define SENTRYHELPER_H

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)
#define CCP_DOMAIN L"CCP.AD.LOCAL"

#include <QString>
#include <QStringList>

void initializeSentry(const QStringList &arguments);
void setTag(const QString& tag, const QString& value);
void setCommandLineArguments(const QStringList &arguments);
void crashApplication();
void sendException(const QString &exceptionType, const QString &value, const QString &functionName, const QString &file, int line);
void closeSentry();

#endif // SENTRYHELPER_H
