// executeundoonlyuninstalloperation.h
#ifndef EXECUTEUNDOONLYUNINSTALLOPERATION_H
#define EXECUTEUNDOONLYUNINSTALLOPERATION_H

#include "elevatedexecuteoperation.h"

namespace QInstaller {

class INSTALLER_EXPORT ExecuteUndoOnlyUninstallOperation : public ElevatedExecuteOperation
{
    Q_OBJECT

public:
    explicit ExecuteUndoOnlyUninstallOperation(PackageManagerCore *core);
};

}

#endif