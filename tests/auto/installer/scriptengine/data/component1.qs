function Component()
{
    print("Component constructor - OK");
}

Component.prototype.retranslateUi = function()
{
    // arguments.callee to get the current function name doesn't work in that case
    print("retranslateUi - OK");
    // no default implementation for this method
    // component.languageChanged();
}

Component.prototype.createOperationsForPath = function(path)
{
    print("createOperationsForPath - OK");
    component.createOperationsForPath(path);
}

Component.prototype.createOperationsForArchive = function(archive)
{
    print("createOperationsForArchive - OK");
    component.createOperationsForArchive(archive);
}

Component.prototype.beginInstallation = function()
{
    print("beginInstallation - OK");
    component.beginInstallation();
}

Component.prototype.createOperations = function()
{
    print("createOperations - OK");
    component.createOperations();
}

Component.prototype.isDefault = function()
{
    print("isDefault - OK");
    return false;
}
