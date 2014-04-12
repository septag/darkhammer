import sys
from dhcore import *
from dhapp import *
from dheng import *
import dhlog

from PyQt5.QtCore import *
from PyQt5.QtWidgets import *

class View(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(Qt.WA_PaintOnScreen, True)
        self.setAttribute(Qt.WA_NativeWindow, True)        
        self.show()

    def paintEngine(self):
        return None
    
    def paintEvent(self, e):
        Engine.update()
        App.swapbuffers()
        
    def resizeEvent(self, e):
        sz = e.size()
        App.resize_view(wnd.width(), wnd.height())
        Engine.resize_view(sz.width(), sz.height())

app = QApplication(sys.argv)
wnd = View()

Core.init()
Log().console_output = True
conf = Config()
conf.engine_flags |= EngFlags.DEV | EngFlags.DISABLEBGLOAD | EngFlags.CONSOLE
conf.add_console_command('showgraph ft')

try:
    App.init_d3d_device(wnd.winId(), 'test', conf)
    Engine.init(conf)
except Exception as e:
    dhlog.Log.fatal(str(e))

del conf

App.resize_view(wnd.width(), wnd.height())
Engine.resize_view(wnd.width(), wnd.height())

app.exec()
Engine.release()
App.release()
Core.release()

