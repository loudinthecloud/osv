import os
from osv.modules.api import *
from osv.modules.filemap import FileMap
from osv.modules import api

_module = '${OSV_BASE}/modules/httpserver'

usr_files = FileMap()
usr_files.add(os.path.join(_module, 'libhttpserver.so')).to('/libhttpserver.so')
usr_files.add(os.path.join(_module, 'api-doc')).to('/usr/mgmt/api')
usr_files.add(os.path.join(_module, 'swagger-ui', 'dist')).to('/usr/mgmt/swagger-ui/dist')

default = api.run('/libhttpserver.so')
