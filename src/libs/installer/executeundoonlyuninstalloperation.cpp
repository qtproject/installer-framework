// executeundoonlyuninstalloperation.cpp
#include "executeundoonlyuninstalloperation.h"

using namespace QInstaller;

ExecuteUndoOnlyUninstallOperation::ExecuteUndoOnlyUninstallOperation(PackageManagerCore *core)
    : ElevatedExecuteOperation(core)
{
    setName(QLatin1String("ExecuteUndoOnlyUninstall"));
    setValue(QLatin1String("keep-when-update"), true);
}
