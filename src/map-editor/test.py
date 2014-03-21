from PyQt4 import QtCore, QtGui
import os, platform, sys

# register module path
mod_path = os.path.normpath(os.path.dirname(os.path.abspath(__file__)) + '/../pymodules')
sys.path.append(mod_path)

from dhutil import util
from dhdlg import about
import dheng

##################################################################################################
# globals
g_cam = None

##################################################################################################
class w_view(QtGui.QWidget):
    def __init__(self, parent):
        super(w_view, self).__init__(parent)
        self.setAttribute(QtCore.Qt.WA_PaintOnScreen, True)
        self.setAttribute(QtCore.Qt.WA_NativeWindow, True)
        self.setFocusPolicy(QtCore.Qt.StrongFocus)
        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self.update)
        self.timer.start(20)
        self.prev_x = 0
        self.prev_y = 0
        self.sz_timer = QtCore.QTimer(self)
        self.sz_timer.timeout.connect(self.resize)
        self.sz_timer.setSingleShot(True)
        self.sz_cur = self.size()
        self.mouse_dwn = False
        self.setMouseTracking(True)

    def paintEvent(self, pe):
        dheng.eng_update()

    def paintEngine(self):
        return None

    def resize(self):
        dheng.app_resize_window('main', self.sz_cur.width(), self.sz_cur.height())
        global g_cam
        dheng.cam_set_viewsize(g_cam, self.sz_cur.width(), self.sz_cur.height())
    
    def keyPressEvent(self, e):
        if self.mouse_dwn:
            cam = dheng.scn_getcam()
            key = e.key()
            if key == QtCore.Qt.Key_W or key == QtCore.Qt.Key_Up:
                dheng.cam_fwd(g_cam, 0.5)
            elif e.key() == QtCore.Qt.Key_S or key == QtCore.Qt.Key_Down:
                dheng.cam_fwd(g_cam, -0.5)
            elif e.key() == QtCore.Qt.Key_D or key == QtCore.Qt.Key_Right:
                dheng.cam_strafe(g_cam, 0.5)
            elif e.key() == QtCore.Qt.Key_A or key == QtCore.Qt.Key_Left:
                dheng.cam_strafe(g_cam, -0.5)
            
    def mouseMoveEvent(self, e):
        pt = e.pos()

        if self.mouse_dwn:
            cam = dheng.scn_getcam()
            dx = pt.x() - self.prev_x
            dy = pt.y() - self.prev_y
            dheng.cam_yaw(cam, dx*0.01)
            dheng.cam_pitch(cam, dy*0.01)
            
        self.prev_x = pt.x()
        self.prev_y = pt.y()
        
            
    def resizeEvent(self, re):
        cam = dheng.scn_getcam()
        if not self.sz_timer.isActive():
            self.sz_timer.start(500)
        self.sz_cur = self.size()

    def update(self):
        dheng.cam_update(dheng.scn_getcam())
        dheng.eng_update()

    def mousePressEvent(self, me):
        self.mouse_dwn = True

    def mouseReleaseEvent(self, me):
        self.mouse_dwn = False

class w_main(QtGui.QMainWindow):
    def __init__(self):
        super(w_main, self).__init__()

        menu = QtGui.QMenuBar(self)

        # file menu
        mnu_file = QtGui.QMenu("&File", self)
        menu.addMenu(mnu_file)
        act_exit = QtGui.QAction("Exit", self)
        act_exit.setShortcut("Ctrl+Q")
        act_exit.triggered.connect(self.mnu_file_exit)
        mnu_file.addAction(act_exit)

        # help menu
        mnu_help = QtGui.QMenu("&Help", self)
        mnu_help.addMenu(mnu_help)
        act_about = QtGui.QAction("About", self)
        act_about.setShortcut("Ctrl+F1")
        act_about.triggered.connect(self.mnu_help_about)
        mnu_help.addAction(act_about)
        menu.addMenu(mnu_help)

        self.setMenuBar(menu)
        self.setMinimumSize(800, 600)

        self.view_wnd = w_view(self)
        self.setCentralWidget(self.view_wnd)
        self.setWindowTitle("dark-hammer: python sample")

    def get_view3d(self):
        return self.view_wnd

    def mnu_file_exit(self, checked):
        self.close()

    def mnu_help_about(self, checked):
        about_wnd = about.qtAboutDlg(self, "map-editor", "Editor for level layout and gameplay")
        about_wnd.exec_()

def load_stuff():
    global g_cam
    g_cam = dheng.camera()
    pos = dheng.vec4f()
    lookat = dheng.vec4f()
    pos.z = -5
    pos.y = 3
    dheng.cam_init(g_cam, pos, lookat, 0.2, 300, dheng.math_torad(60))
    dheng.cam_update(g_cam)
    dheng.cam_set_viewsize(g_cam, 640, 480)

    scn = dheng.scn_create_scene("test-scene")
    dheng.scn_setactive(scn)
    dheng.scn_setcam(g_cam)
    
    dheng.io_addvdir(os.path.abspath(util.get_exec_dir(__file__) + '/../../'), False)
    dheng.sct_runfile('test-data/test6.lua')

def main():
    app = QtGui.QApplication(sys.argv)
    main_wnd = w_main()

    if not dheng.core_init(True):
        print "could not initialize engine core"
        sys.exit(-1)

    dheng.log_outputconsole(True)

    params = dheng.init_params()
    params.flags = dheng.ENG_FLAG_DEBUG | dheng.ENG_FLAG_DEV
    params.gfx.flags = dheng.GFX_FLAG_DEBUG
    params.gfx.width = 640
    params.gfx.height = 480
    params.gfx.refresh_rate = 60
    params.dev.webserver_port = 8888
    params.data_dir = os.path.abspath(util.get_exec_dir(__file__) + "/../../data")

    hwnd = dheng.str_toptr(main_wnd.get_view3d().winId().__hex__())
    print main_wnd.get_view3d().winId()
    if not dheng.app_init('main', params, hwnd):
        print "could not initialize engine core"
        dheng.core_release(True)
        sys.exit(-1)

    r = dheng.eng_init(params)
    if r != True:
        dheng.err_sendtolog(False)

    dheng.gfx_set_debug_renderfunc(dheng.gfx_render_grid)

    main_wnd.show()
    load_stuff()
    r = app.exec_()

    dheng.eng_release()
    dheng.app_release()
    dheng.core_release(True)
    exit(r)

if __name__ == "__main__":
    main()
