import sys
from dhcore import *
from dheng import *
import dhlog

class SoldierCtrl(Behavior):
    def init(self, game_obj):
        self._obj = game_obj
        return True

    def update(self, dt):
        try:
            if Input.is_keydown(Key.UP):
                self._obj.animator.set_param('Walk', True)
                if Input.is_keydown(Key.LEFT):
                    self._obj.animator.set_param('MoveDir', 
                        self._obj.animator.get_param('MoveDir') - 0.01)
                elif Input.is_keydown(Key.RIGHT):
                    self._obj.animator.set_param('MoveDir',
                        self._obj.animator.get_param('MoveDir') + 0.01)
            else:
                self._obj.animator.set_param('Walk', False)
        except:
            pass

Core.init()
Log().console_output = True
conf = Config()
conf.engine_flags |= EngFlags.DEV | EngFlags.CONSOLE

try:
    Engine.init('test', conf)
except:
    dhlog.Log.fatal(Errors.last_error())
else:
    testdata_path = os.path.join(Engine.get_share_dir(), 'test-data')
    FileIO.add_virtual_path(testdata_path)

    s = Scene('main')
    s.activate()
    obj = s.create_object('main_cam', GameObjectType.CAMERA)

    obj.add_behavior(OrbitCam(), 'cam')
    obj.camera.active = True
    obj.transform.position = Vec3(0, 5, 0)
    obj.get_behavior('cam').target = Vec3(0, 1, 0)

    obj = s.create_object('ground', GameObjectType.MODEL)
    obj.model.filepath = 'plane.h3dm'

    obj = s.create_object('soldier', GameObjectType.MODEL)
    obj.model.filepath = 'soldier.h3dm'
    obj.add_component('animator')
    obj.animator.filepath = 'controller1.json'
    obj.add_behavior(SoldierCtrl(), 'soldier_ctrl')

    Engine.run()
    s.destroy()

Engine.release()
del conf
Core.release()
