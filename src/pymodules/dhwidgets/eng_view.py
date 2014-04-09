import sys
from PyQt4 import QtCore, QtGui
import dheng

class qEngineView(QtGui.QWidget):
    def __init__(self, parent):
        super(qEngineView, self).__init__(parent)

        self.setAttribute(QtCore.Qt.WA_PaintOnScreen, True)
        self.setAttribute(QtCore.Qt.WA_NativeWindow, True)
        self.setFocusPolicy(QtCore.Qt.StrongFocus)
        self.setMouseTracking(True)
        self.setMinimumSize(64, 64)
        self.setSizePolicy(\
            QtGui.QSizePolicy(QtGui.QSizePolicy.Expanding, QtGui.QSizePolicy.Expanding))

        self.timer = QtCore.QTimer(self)
        self.sz_timer = QtCore.QTimer(self)
        self.sz_timer.setSingleShot(True)
        self.timer.start(20)

        self.prev_x = 0
        self.prev_y = 0
        self.mouse_dwn = False
        self.cam = None

        # events
        self.timer.timeout.connect(self.update)
        self.sz_timer.timeout.connect(self.resize)

    def set_cam(self, cam):
        name = str(self.winId().__hex__())
        dheng.app_set_rendertarget(name)
        dheng.app_window_resize(name, self.width(), self.height())
        if cam != None:
            dheng.wld_set_cam(cam)
            dheng.cam_set_viewsize(cam, self.width(), self.height())
        self.cam = cam

    def paintEvent(self, e):
        if not self.cam:
            return
        dheng.eng_update()

    def paintEngine(self):
        return None

    def resize(self):
        if not self.cam:
            return
        dheng.app_window_resize(str(self.winId().__hex__()), self.width(), self.height())
        dheng.cam_set_viewsize(self.cam, self.width(), self.height())

    def keyPressEvent(self, e):
        if not self.cam:
            return
        if self.mouse_dwn:
            cam = dheng.wld_get_cam()
            key = e.key()
            if key == QtCore.Qt.Key_W or key == QtCore.Qt.Key_Up:
                dheng.cam_fwd(self.cam, 0.5)
            elif e.key() == QtCore.Qt.Key_S or key == QtCore.Qt.Key_Down:
                dheng.cam_fwd(self.cam, -0.5)
            elif e.key() == QtCore.Qt.Key_D or key == QtCore.Qt.Key_Right:
                dheng.cam_strafe(self.cam, 0.5)
            elif e.key() == QtCore.Qt.Key_A or key == QtCore.Qt.Key_Left:
                dheng.cam_strafe(self.cam, -0.5)

    def mouseMoveEvent(self, e):
        if not self.cam:
            return
        pt = e.pos()

        if self.mouse_dwn:
            cam = dheng.wld_get_cam()
            dx = pt.x() - self.prev_x
            dy = pt.y() - self.prev_y
            dheng.cam_yaw(cam, dx*0.01)
            dheng.cam_pitch(cam, dy*0.01)

        self.prev_x = pt.x()
        self.prev_y = pt.y()

    def resizeEvent(self, e):
        if not self.cam:
            return
        if not self.sz_timer.isActive():
            self.sz_timer.start(500)

    def update(self):
        if not self.cam:
            return
        dheng.cam_update(self.cam)
        dheng.eng_update()

    def mousePressEvent(self, e):
        self.mouse_dwn = True

    def mouseReleaseEvent(self, e):
        self.mouse_dwn = False

    def wheelEvent(self, e):
        if not self.cam:
            return
        steps = e.delta()/15
        dheng.cam_fwd(self.cam, steps)
