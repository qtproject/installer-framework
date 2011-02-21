#ifndef LIBINSTALLER_ENVIRONMENT_H
#define LIBINSTALLER_ENVIRONMENT_H

#include "kdupdaterupdateoperation.h"

#include <QString>

class QProcess;
class QProcessEnvironment;

namespace KDUpdater {

class KDTOOLS_UPDATER_EXPORT Environment {
    public:
        static Environment* instance();

        ~Environment();

        QString value( const QString& key, const QString& defaultValue=QString() ) const;
        void setTemporaryValue( const QString& key, const QString& value );

        QProcessEnvironment applyTo( const QProcessEnvironment& qpe ) const;
        void applyTo( QProcess* process );

    private:
        Environment();
        
    private:
        Q_DISABLE_COPY(Environment)
        class Private;
        Private* const d;
};

}

#endif
