import sys
from dhcore import *
from dhapp import *
from dheng import *
import dhlog

class SoldierCtrl(Behavior):
    def init(self, game_obj):
        self._obj = game_obj
        return True

    def update(self, dt):
        try:
            if Input.is_keydown(Key.UP) or Input.is_keydown(Key.W):
                self._obj.animator.set_param('Walk', True)
                if Input.is_keydown(Key.LEFT) or Input.is_keydown(Key.A):
                    self._obj.animator.set_param('Direction', 
                        self._obj.animator.get_param('Direction') - 0.01)
                elif Input.is_keydown(Key.RIGHT) or Input.is_keydown(Key.D):
                    self._obj.animator.set_param('Direction',
                        self._obj.animator.get_param('Direction') + 0.01)
            else:
                self._obj.animator.set_param('Walk', False)
                #obj.animator.set_param('Direction', 0.5)
        except:
            pass

class Events(AppEvents):
    def on_update(self):
        Engine.update()
        App.swapbuffers()

    def on_keypress(self, ch, vkey):
        Engine.send_keys(ch, vkey)

    def on_resize(self, width, height):
        super().on_resize(width, height)
        Engine.resize_view(width, height)

Core.init()
Log.set_console_output(True)
conf = Config()
conf.engine_flags |= EngFlags.DEV | EngFlags.CONSOLE

try:
    App.init('test', conf)
    Engine.init(conf)
    App.set_events(Events())
except Exception as e:
    dhlog.Log.fatal(str(e))
else:
    App.show_window()
    testdata_path = os.path.join(Engine.get_share_dir(), 'test-data')
    FileIO.add_virtual_path(testdata_path)

    cam = World.create_object('main_cam', GameObject.Type.CAMERA)
    cam.add_behavior(OrbitCam(), 'cam')
    cam.camera.active = True
    cam.transform.position = Vec3(0, 5, 0)
    cam.get_behavior('cam').target = Vec3(0, 1, 0)

    s = Scene('main')
    s.activate()

    obj = s.create_object('ground', GameObject.Type.MODEL)
    obj.model.filepath = 'plane.h3dm'

    obj = s.create_object('soldier', GameObject.Type.MODEL)
    obj.model.filepath = 'soldier.h3dm'
    obj.add_component('animator')
    obj.animator.filepath = 'controller1.json'
    obj.add_behavior(SoldierCtrl(), 'soldier_ctrl')
    
    light = s.create_object('light', GameObject.Type.LIGHT)
    light.transform.position = Vec3(1, 1, 1)
    light.light.color = Color(1, 1, 0)

    light = s.create_object('light2', GameObject.Type.LIGHT)
    light.transform.position = Vec3(-1, 1, 1)
    light.light.color = Color(1, 0, 0)

    App.run()
    s.destroy()

del conf
Engine.release()
App.release()
Core.release()
