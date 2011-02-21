function Component()
{
    if( installer.isUpdater() )
    {
        if( installer.value( "os" ) == "win" )
            component.addDownloadableArchive( "testapp.exe" );
        else 
            component.addDownloadableArchive( "testapp.app.7z" );
    }
}

Component.prototype.createOperationsForArchive = function( archive )
{
    component.createOperationsForArchive( archive );
    
    if( installer.isUpdater() )
        component.addOperation( "SelfRestart" );
}
