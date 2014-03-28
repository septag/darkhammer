import sys
from dhcore import *
from dheng import *
import dhlog

Core.init()
Log().console_output = True
conf = Config()
try:
    Engine.init('test', conf)
except:
    dhlog.Log.fatal(Errors.last_error())
else:
    Engine.run()

Engine.release()
del conf
Core.release()
