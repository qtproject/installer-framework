#!/usr/bin/env python
#############################################################################
##
## Copyright (C) 2017 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the Qt Installer Framework.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

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
