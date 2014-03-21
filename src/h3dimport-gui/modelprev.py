from PyQt4.QtGui import *
from PyQt4.QtCore import *

import dheng
from dhwidgets import eng_view
from dhutil import util
import engine

class qModelPrev(QDialog):
    def __init__(self, parent):
        super(qModelPrev, self).__init__(parent)

        self.setMinimumSize(800, 600)
        self.setWindowTitle('Model preview')
        self.setSizeGripEnabled(True)
        self.setWindowFlags(self.windowFlags() & (~Qt.WindowContextHelpButtonHint))

        layout = QVBoxLayout()
        self.eng_view = eng_view.qEngineView(self)
        layout.addWidget(self.eng_view)
        self.setLayout(layout)

        self.camera = None

    def load_props(self, model_file):
        # initialize camera
        if self.camera == None:
            self.camera = dheng.camera()
            pos = dheng.vec4f()
            lookat = dheng.vec4f()
            pos.y = 2
            pos.z = -5
            dheng.cam_init(self.camera, pos, lookat, 0.2, 300, dheng.math_torad(50))

        dheng.cam_update(self.camera)
        self.eng_view.set_cam(self.camera)

        # ground
        ground = dheng.scn_create_obj(dheng.scn_getactive(), 'ground', dheng.CMP_OBJTYPE_MODEL)
        dheng.cmp_value_sets(dheng.cmp_findinstance_inobj(ground, 'model'), 'filepath',
            'plane.h3dm')
        ##
        obj = dheng.scn_create_obj(dheng.scn_getactive(), 'test', dheng.CMP_OBJTYPE_MODEL)
        model_cmp = dheng.cmp_findinstance_inobj(obj, 'model')
        dheng.cmp_value_sets(model_cmp, 'filepath', model_file)

        self.obj = obj
        self.ground = ground

    def unload_props(self):
        if self.obj:        dheng.scn_destroy_obj(self.obj)
        if self.ground:     dheng.scn_destroy_obj(self.ground)
        self.ground = None
        self.obj = None

    def closeEvent(self, e):
        self.unload_props()
        e.accept()



