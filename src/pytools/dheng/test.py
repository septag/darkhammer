import sys
from dhcore import *
from dheng import *
import dhlog

Core.init()
Log().console_output = True
conf = Config()
conf.engine_flags |= EngFlags.DEV | EngFlags.CONSOLE

try:
    Engine.init('test', conf)
except:
    dhlog.Log.fatal(Errors.last_error())
else:
    FileIO.add_virtual_path('~/dev/projects/darkhammer/test-data')

    s = Scene('main')
    s.activate()
    obj = s.create_object('main_cam', GameObjectType.CAMERA)

    obj.add_behavior(OrbitCam(), 'cam')
    obj.camera.active = True
    obj.transform.position = Vec3(0, 5, 0)
    obj.get_behavior('cam').target = Vec3(0, 1, 0)

    obj = s.create_object('ground', GameObjectType.MODEL)
    obj.model.filepath = 'plane.h3dm'

    obj = s.create_object('barrel', GameObjectType.MODEL)
    obj.model.filepath = 'soldier.h3dm'

    Engine.run()
    s.destroy()

Engine.release()
del conf
Core.release()
