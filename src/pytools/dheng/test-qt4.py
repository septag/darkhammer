import sys
from dhcore import *
from dhapp import *
from dheng import *
import dhlog

from PyQt4.QtOpenGL import *
from PyQt4.QtGui import *
from PyQt4.QtCore import *

class View(QGLWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.show()

    def paintGL(self):
        Engine.update()

    def resizeGL(self, width, height):
        Engine.resize_view(width, height)

app = QApplication(sys.argv)
main_wnd = View()

Core.init()
Log().console_output = True
conf = Config()
conf.engine_flags |= EngFlags.DEV | EngFlags.DISABLEBGLOAD | EngFlags.CONSOLE
conf.add_console_command('showgraph ft')

try:
    Engine.init(conf)
except Exception as e:
    dhlog.Log.fatal(str(e))

Engine.resize_view(main_wnd.width(), main_wnd.height())
s = Scene('main')
s.activate()
obj = s.create_object('main_cam', GameObject.Type.CAMERA)

obj.add_behavior(OrbitCam(), 'cam')
obj.camera.active = True
obj.transform.position = Vec3(0, 5, 0)
obj.get_behavior('cam').target = Vec3(0, 1, 0)

del conf
r = app.exec_()

s.destroy()
Engine.release()
Core.release()
del main_wnd
sys.exit(r)