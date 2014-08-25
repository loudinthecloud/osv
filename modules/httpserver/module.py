import os
from osv.modules.api import *
from osv.modules.filemap import FileMap
from osv.modules import api

_module = '${OSV_BASE}/modules/httpserver'

_exe = '/libhttpserver.so'

usr_files = FileMap()
usr_files.add(os.path.join(_module, 'libhttpserver.so')).to(_exe)
usr_files.add(os.path.join(_module, 'api-doc')).to('/usr/mgmt/api')
usr_files.add(os.path.join(_module, 'swagger-ui', 'dist')).to('/usr/mgmt/swagger-ui/dist')
usr_files.add(os.path.join(_module, 'osv-gui/public')).to('/usr/mgmt/gui')
usr_files.add('${OSV_BASE}/java/jolokia-agent/target/jolokia-agent.jar').to('/usr/mgmt/jolokia-agent.jar')

api.require('openssl')
api.require('libtools')
api.require('libyaml')

# httpserver will run regardless of an explicit command line
# passed with "run.py -e".
daemon = api.run_on_init(_exe + ' &!')

fg = api.run(_exe)

fg_ssl = api.run(_exe + ' --ssl')

default = daemon
