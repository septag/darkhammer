import dheng
from dhutil import util
import os, sys
import ctypes
from PyQt4 import QtGui

is_init = False
scene = 0
rts = []

def initialize(assetdir, widget):
    global is_init, rts
    name = str(widget.winId().__hex__())
    hwnd = dheng.str_toptr(widget.winId().__hex__())

    if 'linux' in sys.platform:
        QtGui.QApplication.syncX()    # important, must be called after winId()

    if is_init:
        if widget not in rts:
            if not dheng.app_add_rendertarget(name, hwnd, widget.width(), widget.height()):
                return False
            else:
                rts.append(widget)
        return True

    if not dheng.core_init(ctypes.c_uint32(dheng.CORE_INIT_ALL).value):
        print 'Engine core init failed'
        return False

    dheng.log_outputconsole(True)

    params = dheng.init_params()
    params.flags = dheng.ENG_FLAG_DEBUG | dheng.ENG_FLAG_DISABLEPHX | dheng.ENG_FLAG_DEV | \
        dheng.ENG_FLAG_DISABLEBGLOAD
    params.gfx.flags = dheng.GFX_FLAG_DEBUG | dheng.GFX_FLAG_FXAA
    params.gfx.width = 128
    params.gfx.height = 128
    params.sct.mem_sz = 128
    params.dev.buffsize_data = 256
    params.data_dir = os.path.abspath(util.get_exec_dir(__file__) + '/../../data')

    if not dheng.app_init(name, params, hwnd):
        dheng.err_sendtolog(False)
        dheng.core_release(False)
        return False

    if not dheng.eng_init(params):
        dheng.err_sendtolog(False)
        dheng.app_release()
        dheng.core_release(False)
        dheng.err_sendtolog(True)
        return False

    dheng.gfx_set_debug_renderfunc(dheng.gfx_render_grid)

    ## set path for our stuff
    dheng.fio_addvdir(assetdir, True)

    ## single scene with camera
    global scene
    scene = dheng.scn_create_scene('main')
    dheng.scn_setactive(scene)

    is_init = True
    rts.append(widget)
    return True

def release():
    global is_init
    if is_init:
        dheng.eng_release()
        dheng.app_release()
        dheng.core_release(True)
    is_init = False
