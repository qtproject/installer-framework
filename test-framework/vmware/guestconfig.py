import os, ConfigParser, tempfile, platform
config = ConfigParser.SafeConfigParser()
config.add_section('Guest')
config.set('Guest', 'temp_dir', str(tempfile.gettempdir()))
config.set('Guest', 'path_sep', os.sep)
config.set('Guest', 'system', platform.system())
config.set('Guest', 'release', platform.release())
config.set('Guest', 'version', platform.version())
config.set('Guest', 'machine', platform.machine())
with open('example.cfg', 'wb') as configFile:
    config.write(configFile)
