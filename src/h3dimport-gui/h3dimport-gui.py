from PyQt4.QtGui import *
from PyQt4.QtCore import *

import ConfigParser, os, platform, subprocess, sys, json
from copy_reg import add_extension

# globals
prefs = {}
main_wnd = None

# register module path
mod_path = os.path.normpath(os.path.dirname(os.path.abspath(__file__)) + '/../pymodules')
sys.path.append(mod_path)
from dhdlg import about
from dhutil import util
import dheng, clipedit, engine, modelprev

def add_extension(filepath, ext):
    (filename, fileext) = os.path.splitext(filepath)
    if not ("." + ext) in fileext:
        filepath = filepath + "." + ext
    return filepath

###############################################################################################
class w_prefs(QDialog):
    def __init__(self, parent):
        super(w_prefs, self).__init__(parent)
        self.init_ui()

    def init_ui(self):
        self.setMinimumWidth(500)
        self.setWindowTitle('Preferences')
        self.setWindowFlags(self.windowFlags() & (~Qt.WindowContextHelpButtonHint))

        layout = QVBoxLayout(self)

        self.edit_bin = QLineEdit(self)
        self.check_verbose = QCheckBox("Verbose mode", self)

        btn_browse = QPushButton("Browse ...", self)
        btn_browse.setFixedWidth(80)
        btn_browse.clicked.connect(self.browse_clicked)

        layout2 = QHBoxLayout()
        layout2.addWidget(QLabel("Importer binary :"))
        layout2.addWidget(self.edit_bin)
        layout2.addWidget(btn_browse)
        layout.addLayout(layout2)

        layout4 = QHBoxLayout()
        ed_assetdir = QLineEdit(self)
        btn_browse = QPushButton('Browse', self)
        btn_browse.setFixedWidth(80)
        btn_browse.clicked.connect(self.btn_assetdir_clicked)
        layout4.addWidget(QLabel('Asset Directory :'))
        layout4.addWidget(ed_assetdir)
        layout4.addWidget(btn_browse)
        layout.addLayout(layout4)
        self.ed_assetdir = ed_assetdir

        layout.addWidget(self.check_verbose)

        btn_ok = QPushButton("Ok", self)
        btn_cancel = QPushButton("Cancel", self)
        btn_ok.clicked.connect(self.ok_clicked)
        btn_cancel.clicked.connect(self.cancel_clicked)
        layout3 = QHBoxLayout()
        layout3.addWidget(btn_ok)
        layout3.addWidget(btn_cancel)
        layout3.addStretch()
        layout.addLayout(layout3)

        layout.addStretch()
        self.setLayout(layout)

    def btn_assetdir_clicked(self, checked):
        dlg = QFileDialog(self, "Choose asset directory", self.ed_assetdir.text())
        dlg.setFileMode(QFileDialog.Directory)
        if dlg.exec_():
            dirs = dlg.selectedFiles()
            self.ed_assetdir.setText(os.path.abspath(str(dirs[0])))

    def browse_clicked(self, checked):
        if platform.system() == "Windows":
            filters = "Executables (*.exe)"
        else:
            filters = "Executables (*)"
        binfile = QFileDialog.getOpenFileName(self, "Open h3dimport binary", "", filters)
        binfile = os.path.abspath(str(binfile))
        if binfile != "":
            self.edit_bin.setText(binfile)

    def ok_clicked(self, checked):
        self.accept()

    def cancel_clicked(self, checked):
        self.reject()

    def load_config(self, cfg):
        global prefs
        self.edit_bin.setText(prefs['binpath'])
        self.check_verbose.setChecked(prefs['verbose'])
        self.ed_assetdir.setText(prefs['assetdir'])

    def save_config(self, cfg):
        global prefs
        if not "general" in cfg.sections():
            cfg.add_section("general")
        prefs['verbose'] = self.check_verbose.isChecked()
        prefs['binpath'] = str(self.edit_bin.text())
        prefs['assetdir'] = str(self.ed_assetdir.text())

###############################################################################################
class w_phx(QWidget):
    def __init__(self, parent):
        super(w_phx, self).__init__(parent)
        self.infiledir = ""
        self.outfiledir = ""
        self.ctrls = []
        self.quiet_mode = False
        self.watcher = QFileSystemWatcher(self)
        self.watcher.fileChanged.connect(self.filemon_onfilechange)
        self.init_ui()

    def init_ui(self):
        layout = QFormLayout(self)
        self.setLayout(layout)

        # make controls
        self.edit_infilepath = QLineEdit(self)
        self.edit_infilepath.setReadOnly(True)
        btn_browse_infile = QPushButton("Browse", self)
        btn_browse_infile.setFixedWidth(60)
        self.combo_names = QComboBox(self)
        self.edit_outfilepath = QLineEdit(self)
        self.edit_outfilepath.setReadOnly(True)
        btn_browse_outfile = QPushButton("Browse", self)
        btn_browse_outfile.setFixedWidth(60)
        self.check_zup = QCheckBox(self)

        btn_auto = QCheckBox("Auto", self)
        btn_auto.setMaximumWidth(50)
        btn_import = QPushButton("Import", self)

        self.btn_auto = btn_auto
        self.btn_import = btn_import

        # layouts
        layout_infile = QHBoxLayout()
        layout_infile.addWidget(self.edit_infilepath)
        layout_infile.addWidget(btn_browse_infile)
        layout.addRow("Input file:", layout_infile)

        layout.addRow("Object name:", self.combo_names)

        layout_outfile = QHBoxLayout()
        layout_outfile.addWidget(self.edit_outfilepath)
        layout_outfile.addWidget(btn_browse_outfile)
        layout.addRow("Output file:", layout_outfile)

        layout.addRow("Up is Z (3dsmax):", self.check_zup)

        layout_import = QHBoxLayout()
        layout_import.addWidget(btn_import)
        layout_import.addWidget(btn_auto)
        layout.addRow(layout_import)

        #events
        btn_browse_infile.clicked.connect(self.btn_browseinfile_click)
        btn_browse_outfile.clicked.connect(self.btn_browseoutfile_click)
        btn_auto.stateChanged.connect(self.btn_auto_checkstate)
        btn_import.clicked.connect(self.btn_import_click)

        # group controls for enable/disable
        self.ctrls.append(btn_browse_infile)
        self.ctrls.append(self.edit_infilepath)
        self.ctrls.append(self.combo_names)
        self.ctrls.append(self.edit_outfilepath)
        self.ctrls.append(self.check_zup)
        self.ctrls.append(btn_browse_outfile)
        self.ctrls.append(btn_import)

    def filemon_onfilechange(self, qfilepath):
        filepath = str(qfilepath)
        if filepath == str(self.edit_infilepath.text()):
            self.btn_import_click(True)

    def enable_controls(self, enable):
        for c in self.ctrls:
            c.setEnabled(enable)

    def btn_auto_checkstate(self, state):
        if state == Qt.Checked:
            self.btn_import.setCheckable(True)
            self.btn_import_click(True)
            self.btn_import.setChecked(True)
            self.enable_controls(False)
            self.quiet_mode = True
            if len(self.edit_infilepath.text()) > 0:
                self.watcher.addPath(self.edit_infilepath.text())
        else:
            self.quiet_mode = False
            self.btn_import.setChecked(False)
            self.btn_import.setCheckable(False)
            self.enable_controls(True)
            if len(self.edit_infilepath.text()) > 0:
                self.watcher.removePath(self.edit_infilepath.text())

    def btn_browseinfile_click(self):
        dlg = QFileDialog(self, "Open physics", self.infiledir, \
            "Physx3 files (*.RepX)")
        dlg.setFileMode(QFileDialog.ExistingFile)
        if dlg.exec_():
            files = dlg.selectedFiles()
            self.edit_infilepath.setText(os.path.normpath(str(files[0])))
            self.infiledir = os.path.normpath(str(dlg.directory().path()))
            in_filepath = str(self.edit_infilepath.text())
            out_filepath = str(self.edit_outfilepath.text())
            self.edit_outfilepath.setText(util.make_samefname(out_filepath, in_filepath, "h3dp"))
            self.enum_phxobjects()

    def btn_browseoutfile_click(self):
        dlg = QFileDialog(self, "Save h3dp file", self.outfiledir, \
            "dark-hammer physics (*.h3dp)")
        dlg.setFileMode(QFileDialog.AnyFile)
        dlg.setAcceptMode(QFileDialog.AcceptSave)
        if dlg.exec_():
            relative_path = util.get_rel_path(str(dlg.selectedFiles()[0]), prefs['assetdir'])
            if not relative_path:
                QMessageBox.warning(self, 'h3dimport', \
                    'Path must be under asset directory tree')
                return
            self.edit_outfilepath.setText(add_extension(relative_path, "h3dp"))
            self.outfiledir = os.path.normcase(str(dlg.directory().path()))

    def btn_import_click(self, checked):
        global prefs

        name = str(self.combo_names.itemText(self.combo_names.currentIndex()))

        args = [str(prefs['binpath']),
                "-p", str(self.edit_infilepath.text()),
                "-o", os.path.normcase(prefs['assetdir'] + '/' + str(self.edit_outfilepath.text())),
                "-n", name]
        if self.check_zup.isChecked():
            args.extend(["-zup"])
        if prefs['verbose']:
            args.extend(["-v"])
        QApplication.setOverrideCursor(QCursor(Qt.WaitCursor))
        r = subprocess.call(args)
        QApplication.restoreOverrideCursor()
        if r == -1 and not self.quiet_mode:
            QMessageBox.critical(self, "h3dimport", """Failed to import file, see the"""\
                                       """ terminal for more info""")

    def save_config(self, cfg):
        if not "physics" in cfg.sections():
            cfg.add_section("physics")
        cfg.set("physics", "in_filepath", str(self.edit_infilepath.text()))
        cfg.set("physics", "out_filepath", str(self.edit_outfilepath.text()))
        cfg.set("physics", "in_filedir", str(self.infiledir))
        cfg.set("physics", "out_filedir", str(self.outfiledir))
        cfg.set("physics", "zup", str(self.check_zup.isChecked()))

    def load_config(self, cfg):
        if not "physics" in cfg.sections():
            return

        self.edit_infilepath.setText(cfg.get("physics", "in_filepath"))
        self.edit_outfilepath.setText(cfg.get("physics", "out_filepath"))
        self.infiledir = cfg.get("physics", "in_filedir")
        self.outfiledir = cfg.get("physics", "out_filedir")
        self.check_zup.setChecked(cfg.getboolean("physics", "zup"))

    def enum_phxobjects(self):
        global prefs
        self.combo_names.clear()
        if len(prefs['binpath']) == 0 or len(self.edit_infilepath.text()) == 0:
            return

        phx_filepath = str(self.edit_infilepath.text())
        args = [prefs['binpath'], "-l", "-p", phx_filepath]
        try:
            r = subprocess.check_output(args)
        except subprocess.CalledProcessError as ce:
            print ce.output
            QMessageBox.critical(self, "h3dimport", "h3dimport raised error!")
        else:
            objs = str(r).replace("\r", "").split("\n")
            for ln in objs:
                if len(ln) > 0:
                    self.combo_names.addItem(ln)
            if self.combo_names.count() > 1:
                self.combo_names.setCurrentIndex(1)

###############################################################################################
class w_anim(QWidget):
    def __init__(self, parent):
        super(w_anim, self).__init__(parent)
        global prefs
        self.infiledir = ""
        self.outfiledir = ""
        self.init_ui()
        self.dlg_clipedit = clipedit.qClipEditDlg(self)
        self.watcher = QFileSystemWatcher(self)
        self.watcher.fileChanged.connect(self.monitor_onfilechange)
        self.quiet_mode = False

    def init_ui(self):
        layout = QFormLayout(self)
        self.setLayout(layout)

        # make controls
        self.edit_infilepath = QLineEdit(self)
        self.edit_infilepath.setReadOnly(True)
        btn_browse_infile = QPushButton("Browse", self)
        btn_browse_infile.setFixedWidth(60)
        self.edit_outfilepath = QLineEdit(self)
        self.edit_outfilepath.setReadOnly(True)
        btn_browse_outfile = QPushButton("Browse", self)
        btn_browse_outfile.setFixedWidth(60)
        btn_import = QPushButton("Import", self)
        self.edit_fps = QLineEdit("30", self)
        self.edit_fps.setMaximumWidth(40)
        btn_clipedit = QPushButton('Edit clips', self)

        # add to layout
        layout_infile = QHBoxLayout()
        layout_infile.addWidget(self.edit_infilepath)
        layout_infile.addWidget(btn_browse_infile)
        layout.addRow("Input file:", layout_infile)

        layout_outfile = QHBoxLayout()
        layout_outfile.addWidget(self.edit_outfilepath)
        layout_outfile.addWidget(btn_browse_outfile)
        layout.addRow("Output file:", layout_outfile)

        layout.addRow("Fps:", self.edit_fps)

        layout2 = QHBoxLayout()
        btn_auto = QCheckBox("Auto", self)
        btn_auto.setMaximumWidth(50)

        layout2.addWidget(btn_clipedit)
        layout2.addWidget(btn_import)
        layout2.addWidget(btn_auto)
        layout.addRow(layout2)

        # vars
        self.btn_auto = btn_auto
        self.btn_import = btn_import
        self.ctrls = [btn_browse_infile, btn_browse_outfile, btn_import, self.edit_infilepath,
                      self.edit_outfilepath, self.edit_fps, btn_clipedit]

        # events
        btn_browse_infile.clicked.connect(self.browse_infile_clicked)
        btn_browse_outfile.clicked.connect(self.browse_outfile_clicked)
        btn_import.clicked.connect(self.btn_import_clicked)
        btn_clipedit.clicked.connect(self.btn_clipedit_clicked)
        btn_auto.stateChanged.connect(self.btn_auto_checkstate)

    def monitor_onfilechange(self, qfilepath):
        filepath = str(qfilepath)
        if filepath == str(self.edit_infilepath.text()):
            self.btn_import_clicked(True)

    def enable_controls(self, enable):
        for c in self.ctrls:
            c.setEnabled(enable)

    def btn_auto_checkstate(self, state):
        if state == Qt.Checked:
            self.btn_import.setCheckable(True)
            self.btn_import_clicked(True)
            self.btn_import.setChecked(True)
            self.enable_controls(False)
            self.quiet_mode = True
            if len(self.edit_infilepath.text()) > 0:
                self.watcher.addPath(self.edit_infilepath.text())
        else:
            self.quiet_mode = False
            self.btn_import.setChecked(False)
            self.btn_import.setCheckable(False)
            self.enable_controls(True)
            if len(self.edit_infilepath.text()) > 0:
                self.watcher.removePath(self.edit_infilepath.text())

    def btn_clipedit_clicked(self, checked):
        global prefs
        if not engine.initialize(prefs['assetdir'], self.dlg_clipedit.eng_view):
            print 'could not initialize dark-hammer engine'
        else:
            # get model file from the current imported model
            global main_wnd

            # before anything, do the animation import process
            self.btn_import_clicked(True)

            model_file = str(main_wnd.wnds['model'].edit_outfilepath.text())
            anim_file = str(self.edit_outfilepath.text())
            self.dlg_clipedit.load_props(model_file, anim_file, self.in_jsonfile)
            self.dlg_clipedit.exec_()

    def init_clips_jsonfile(self):
        global prefs
        in_filepath = str(self.edit_infilepath.text())
        in_jsonfile = util.make_samefname(in_filepath, in_filepath, 'json')
        if not os.path.isfile(in_jsonfile):
            # create an empty json clips file
            open(in_jsonfile, 'w').write('[{"name":"all"}]')
        self.in_jsonfile = in_jsonfile

    def browse_infile_clicked(self):
        dlg = QFileDialog(self, "Open animation", self.infiledir, \
            "Animation files (*.dae *.obj *.x *.ase *.ms3d)")
        dlg.setFileMode(QFileDialog.ExistingFile)
        if dlg.exec_():
            files = dlg.selectedFiles()
            self.edit_infilepath.setText(os.path.normpath(str(files[0])))
            self.infiledir = os.path.normpath(str(dlg.directory().path()))
            in_filepath = str(self.edit_infilepath.text())
            out_filepath = str(self.edit_outfilepath.text())
            self.edit_outfilepath.setText(util.make_samefname(out_filepath, in_filepath, "h3da"))

            # try to locate clip (json) file in the same directory and same name as input file,
            # if not found, create an empty
            self.init_clips_jsonfile()

    def browse_outfile_clicked(self):
        dlg = QFileDialog(self, "Save h3da file", self.outfiledir, \
            "dark-hammer anims (*.h3da)")
        dlg.setFileMode(QFileDialog.AnyFile)
        dlg.setAcceptMode(QFileDialog.AcceptSave)
        if dlg.exec_():
            relative_path = util.get_rel_path(str(dlg.selectedFiles()[0]), prefs['assetdir'])
            if not relative_path:
                QMessageBox.warning(self, 'h3dimport', 'Path must be under asset directory tree')
                return
            self.edit_outfilepath.setText(add_extension(relative_path, "h3da"))
            self.outfiledir = os.path.normpath(str(dlg.directory().path()))

    def btn_import_clicked(self, checked):
        global prefs
        args = [prefs['binpath'],
                "-a", str(self.edit_infilepath.text()),
                "-o", os.path.normcase(prefs['assetdir'] + '/' + str(self.edit_outfilepath.text())),
                "-fps", str(self.edit_fps.text()),
                '-clips', self.in_jsonfile]
        if prefs['verbose']:
            args.extend(["-v"])
        QApplication.setOverrideCursor(QCursor(Qt.WaitCursor))
        r = subprocess.call(args)
        QApplication.restoreOverrideCursor()
        if r == -1 and not self.quiet_mode:
            QMessageBox.critical(self, "h3dimport", """Failed to import file, see the"""\
                                       """ terminal for more info""")

    def save_config(self, cfg):
        if not "anim" in cfg.sections():
            cfg.add_section("anim")
        cfg.set("anim", "in_filepath", str(self.edit_infilepath.text()))
        cfg.set("anim", "out_filepath", str(self.edit_outfilepath.text()))
        cfg.set("anim", "in_filedir", str(self.infiledir))
        cfg.set("anim", "out_filedir", str(self.outfiledir))
        cfg.set("anim", "fps", str(self.edit_fps.text()))

    def load_config(self, cfg):
        if not "anim" in cfg.sections():
            return

        self.edit_infilepath.setText(cfg.get("anim", "in_filepath"))
        self.edit_outfilepath.setText(cfg.get("anim", "out_filepath"))
        self.infiledir = cfg.get("anim", "in_filedir")
        self.outfiledir = cfg.get("anim", "out_filedir")
        self.edit_fps.setText(cfg.get("anim", "fps"))
        self.init_clips_jsonfile()

###############################################################################################
class w_model(QWidget):
    def __init__(self, parent):
        super(w_model, self).__init__(parent)
        self.infile_dir = ""
        self.outfile_dir = ""
        self.texdir_dir = ""
        self.watcher = QFileSystemWatcher(self)
        self.watcher.fileChanged.connect(self.monitor_onfilechange)
        self.quiet_mode = False
        self.ctrls = []
        self.textures = {}
        self.init_ui()

    def init_ui(self):
        layout = QFormLayout(self)
        self.setLayout(layout)

        self.edit_infilepath = QLineEdit(self)
        self.edit_infilepath.setReadOnly(True)
        self.combo_names = QComboBox(self)
        self.combo_occ = QComboBox(self)
        self.edit_outfilepath = QLineEdit(self)
        self.edit_outfilepath.setReadOnly(True)
        self.edit_texdir = QLineEdit(self)
        self.edit_texdir.setReadOnly(True)
        self.check_calctng = QCheckBox(self)
        self.check_fastcompress = QCheckBox(self)
        self.check_dxt3 = QCheckBox(self)
        self.edit_scale = QLineEdit(self)
        self.edit_scale.setMaximumWidth(40)
        self.edit_scale.setValidator(QDoubleValidator())

        btn_browse_infile = QPushButton("Browse", self)
        btn_browse_outfile = QPushButton("Browse", self)
        btn_choose_texdir = QPushButton("Choose", self)
        btn_browse_infile.setFixedWidth(60)
        btn_browse_outfile.setFixedWidth(60)
        btn_choose_texdir.setFixedWidth(60)
        layout_infile = QHBoxLayout()
        layout_infile.addWidget(self.edit_infilepath)
        layout_infile.addWidget(btn_browse_infile)
        layout_outfile = QHBoxLayout()
        layout_outfile.addWidget(self.edit_outfilepath)
        layout_outfile.addWidget(btn_browse_outfile)
        layout_texdir = QHBoxLayout()
        layout_texdir.addWidget(self.edit_texdir)
        layout_texdir.addWidget(btn_choose_texdir)

        btn_browse_infile.clicked.connect(self.browse_infile_clicked)
        btn_browse_outfile.clicked.connect(self.browse_outfile_clicked)
        btn_choose_texdir.clicked.connect(self.choose_texdir_clicked)
        self.cmbo_names_update = True
        self.combo_names.currentIndexChanged.connect(self.cmbo_names_changed)

        btn_prev = QPushButton('Preview', self)
        btn_prev.clicked.connect(self.btn_prev_clicked)

        layout_import = QHBoxLayout()
        btn_auto = QCheckBox("Auto", self)
        btn_auto.stateChanged.connect(self.btn_auto_checkstate)
        btn_auto.setMaximumWidth(50)
        self.btn_auto = btn_auto
        btn_import = QPushButton("Import", self)
        btn_import.clicked.connect(self.import_clicked)
        layout_import.addWidget(btn_prev)
        layout_import.addWidget(btn_import)
        layout_import.addWidget(btn_auto)
        self.btn_import = btn_import

        layout.addRow("Input file:", layout_infile)
        layout.addRow("Model name:", self.combo_names)
        layout.addRow("Occluder:", self.combo_occ)
        layout.addRow("Output file:", layout_outfile)
        layout.addRow("Texture dir:", layout_texdir)
        layout.addRow("Scale:", self.edit_scale)
        layout.addRow("Calculate tangents:", self.check_calctng)
        layout.addRow("Fast texture compress:", self.check_fastcompress)
        layout.addRow("Force DXT3 for alpha:", self.check_dxt3)
        layout.addRow(layout_import)

        # add main controls to array for group enable/disable
        self.ctrls = [btn_browse_infile, self.edit_infilepath, self.combo_names, self.combo_occ, \
            self.edit_outfilepath, self.edit_texdir, self.check_calctng, self.check_dxt3, \
            self.check_fastcompress, btn_browse_outfile, btn_choose_texdir, btn_import]

        self.dlg_prev = modelprev.qModelPrev(self)

    def btn_prev_clicked(self, checked):
        global prefs
        if not engine.initialize(prefs['assetdir'], self.dlg_prev.eng_view):
            print 'could not initialize dark-hammer engine'
        else:
            # get model file from the current imported model
            global main_wnd

            # before anything, do the animation import process
            self.import_clicked(True)

            model_file = str(self.edit_outfilepath.text())
            self.dlg_prev.load_props(model_file)
            self.dlg_prev.exec_()

    def import_texture(self, tex_filepath, tex_type):
        global prefs
        texdir = os.path.normcase(prefs['assetdir'] + '/' + str(self.edit_texdir.text()))
        texdir_alias = util.valid_engine_path(str(self.edit_texdir.text()))
        calctng = self.check_calctng.isChecked()
        fastcompress = self.check_fastcompress.isChecked()
        forcedxt3 = self.check_dxt3.isChecked()
        args = [prefs['binpath']]
        if tex_filepath != "":  args.extend(["-t", tex_filepath])
        if texdir != "":        args.extend(["-tdir", texdir])
        if fastcompress:        args.extend(["-tfast"])
        if forcedxt3:           args.extend(["-tdxt3"])
        args.extend(["-ttype", str(tex_type)])

        # call h3dimport command
        QApplication.setOverrideCursor(QCursor(Qt.WaitCursor))
        r = subprocess.call(args)
        QApplication.restoreOverrideCursor()
        if r == -1 and not self.quiet_mode:
            QMessageBox.critical(self, "h3dimport", """Failed to import file, see the"""\
                                       """ terminal for more info""")
        self.watcher.addPaths(self.textures.keys())

    def cmbo_names_changed(self, idx):
        global prefs
        if len(prefs['binpath']) == 0 or len(self.edit_infilepath.text()) == 0:
            return
        if not self.cmbo_names_update:
            return

        model_filepath = str(self.edit_infilepath.text())
        name = str(self.combo_names.itemText(self.combo_names.currentIndex()))
        args = [prefs['binpath'], "-lm", "-m", model_filepath, "-n", name]
        try:
            r = subprocess.check_output(args)
        except subprocess.CalledProcessError as ce:
            print ce.output
            QMessageBox.critical(self, "h3dimport", "h3dimport raised error!")
        else:
            self.textures = {}
            objs = str(r).replace("\r", "").split("\n")
            for ln in objs:
                if len(ln) > 0 and ("Error:" not in ln) and ("material:" in ln):
                    self.read_material_textures(ln.lstrip("material: "))

    def monitor_reg_files(self):
        if len(self.edit_infilepath.text()) > 0:
            self.watcher.addPath(self.edit_infilepath.text())
        if len(self.textures) > 0:
            self.watcher.addPaths(self.textures.keys())

    def monitor_unreg_files(self):
        if len(self.edit_infilepath.text()) > 0:
            self.watcher.removePath(self.edit_infilepath.text())
        if len(self.textures) > 0:
            self.watcher.removePaths(self.textures.keys())

    def monitor_onfilechange(self, qfilepath):
        filepath = str(qfilepath)
        if filepath == str(self.edit_infilepath.text()):
            self.import_clicked(True)
        elif filepath in self.textures:
            self.import_texture(filepath, self.textures[filepath])

    def enable_controls(self, enable):
        for c in self.ctrls:
            c.setEnabled(enable)

    def btn_auto_checkstate(self, state):
        if state == Qt.Checked:
            self.btn_import.setCheckable(True)
            self.import_clicked(True)
            self.btn_import.setChecked(True)
            self.monitor_reg_files()
            self.enable_controls(False)
            self.quiet_mode = True
        else:
            self.quiet_mode = False
            self.btn_import.setChecked(False)
            self.btn_import.setCheckable(False)
            self.monitor_unreg_files()
            self.enable_controls(True)

    def read_material_textures(self, jstr):
        # parse textures from material data
        jdata = json.loads(jstr)
        for (k, v) in jdata.items():
            if "-tex" in k:
                v = os.path.normpath(v)
                if v not in self.textures:
                    self.textures[v] = k     # key=texture-file, value=type-string

    def enum_models(self):
        global prefs
        self.cmbo_names_update = False
        self.combo_names.clear()
        self.combo_occ.clear()
        self.combo_occ.addItem("[None]")
        if len(prefs['binpath']) == 0 or len(self.edit_infilepath.text()) == 0:
            return

        model_filepath = str(self.edit_infilepath.text())
        args = [prefs['binpath'], "-l", "-lm", "-m", model_filepath]
        try:
            r = subprocess.check_output(args)
        except subprocess.CalledProcessError as ce:
            print ce.output
            QMessageBox.critical(self, "h3dimport", "h3dimport raised error!")
            self.cmbo_names_update = True
        else:
            self.textures = {}
            objs = str(r).replace("\r", "").split("\n")
            for ln in objs:
                if len(ln) > 0 and ("Error:" not in ln):
                    if "model:" in ln:
                        name = ln.lstrip("model: ")
                        self.combo_names.addItem(name)
                        self.combo_occ.addItem(name)
                    elif "material:" in ln:
                        self.read_material_textures(ln.lstrip("material: "))
            self.cmbo_names_update = True

    def browse_infile_clicked(self, checked):
        global prefs
        dlg = QFileDialog(self, "Open model", self.infile_dir, \
            "Models (*.dae *.obj *.x *.ase *.ms3d *.fbx)")
        dlg.setFileMode(QFileDialog.ExistingFile)
        if dlg.exec_():
            filepath = os.path.normcase(str(dlg.selectedFiles()[0]))
            self.edit_infilepath.setText(filepath)
            self.infile_dir = os.path.abspath(str(dlg.directory().path()))
            self.enum_models()
            # automatically set the name of the output file to the name of the input file
            in_filepath = str(self.edit_infilepath.text())
            out_filepath = str(self.edit_outfilepath.text())
            self.edit_outfilepath.setText(util.make_samefname(out_filepath, in_filepath, "h3dm"))

    def browse_outfile_clicked(self, checked):
        dlg = QFileDialog(self, "Save h3dm file", self.outfile_dir, \
            "dark-hammer models (*.h3dm)")
        dlg.setFileMode(QFileDialog.AnyFile)
        dlg.setAcceptMode(QFileDialog.AcceptSave)
        if dlg.exec_():
            relative_path = util.get_rel_path(str(dlg.selectedFiles()[0]), prefs['assetdir'])
            if not relative_path:
                QMessageBox.warning(self, 'h3dimport', \
                    'Path must be under asset directory tree')
                return
            self.edit_outfilepath.setText(add_extension(relative_path, "h3dm"))
            self.outfile_dir = os.path.normpath(str(dlg.directory().path()))

    def choose_texdir_clicked(self, checked):
        dlg = QFileDialog(self, "Choose texture output directory", self.texdir_dir)
        dlg.setFileMode(QFileDialog.Directory)
        if dlg.exec_():
            relative_path = util.get_rel_path(str(dlg.selectedFiles()[0]), prefs['assetdir'])
            if not relative_path:
                QMessageBox.warning(self, 'h3dimport', \
                    'Path must be under asset directory tree')
                return

            self.edit_texdir.setText(relative_path)

    def load_config(self, cfg):
        if not "model" in cfg.sections():
            return
        self.edit_infilepath.setText(cfg.get("model", "in_filepath"))
        self.edit_outfilepath.setText(cfg.get("model", "out_filepath"))
        self.edit_texdir.setText(cfg.get("model", "texdir"))
        self.check_calctng.setChecked(cfg.getboolean("model", "calctng"))
        self.check_fastcompress.setChecked(cfg.getboolean("model", "fastcompress"))
        self.check_dxt3.setChecked(cfg.getboolean("model", "forcedxt3"))
        self.infile_dir = cfg.get("model", "infile_dir")
        self.outfile_dir = cfg.get("model", "outfile_dir")
        self.texdir_dir = cfg.get("model", "texdir")
        self.edit_scale.setText(cfg.get("model", "scale"))

    def save_config(self, cfg):
        if not "model" in cfg.sections():
            cfg.add_section("model")
        cfg.set("model", "in_filepath", str(self.edit_infilepath.text()))
        cfg.set("model", "out_filepath", str(self.edit_outfilepath.text()))
        cfg.set("model", "texdir", str(self.edit_texdir.text()))
        cfg.set("model", "calctng", str(self.check_calctng.isChecked()))
        cfg.set("model", "fastcompress", str(self.check_fastcompress.isChecked()))
        cfg.set("model", "forcedxt3", str(self.check_dxt3.isChecked()))
        cfg.set("model", "infile_dir", str(self.infile_dir))
        cfg.set("model", "outfile_dir", str(self.outfile_dir))
        cfg.set("model", "scale", str(self.edit_scale.text()))

    def import_clicked(self, checked):
        global prefs
        name = str(self.combo_names.itemText(self.combo_names.currentIndex()))
        occname = ""
        if self.combo_occ.currentIndex() != 0:
            occname = str(self.combo_occ.itemText(self.combo_occ.currentIndex()))
        infilepath = str(self.edit_infilepath.text())
        outfilepath = os.path.normcase(prefs['assetdir'] + '/' + str(self.edit_outfilepath.text()))
        texdir = os.path.normcase(prefs['assetdir'] + '/' + str(self.edit_texdir.text()))
        texdir_alias = util.valid_engine_path(str(self.edit_texdir.text()))
        calctng = self.check_calctng.isChecked()
        fastcompress = self.check_fastcompress.isChecked()
        forcedxt3 = self.check_dxt3.isChecked()
        scale = self.edit_scale.text().toFloat()[0]
        if scale <= 0:  scale = 1

        args = [prefs['binpath']]
        if prefs['verbose']:    args.extend(["-v"])
        if name != "":          args.extend(["-n", name])
        if infilepath != "":    args.extend(["-m", infilepath])
        if outfilepath != "":   args.extend(["-o", outfilepath])
        if texdir != "":        args.extend(["-tdir", texdir])
        if texdir_alias != "":  args.extend(["-talias", texdir_alias])
        if calctng:             args.extend(["-calctng"])
        if fastcompress:        args.extend(["-tfast"])
        if forcedxt3:           args.extend(["-tdxt3"])
        if len(occname) > 0:    args.extend(["-occ", occname])
        if self.quiet_mode:     args.extend(["-toff"])
        if scale != 1:          args.extend(["-scale", str(scale)])

        print args

        # call h3dimport command
        QApplication.setOverrideCursor(QCursor(Qt.WaitCursor))
        r = subprocess.call(args)
        QApplication.restoreOverrideCursor()
        if r == -1 and not self.quiet_mode:
            QMessageBox.critical(self, "h3dimport", """Failed to import file, see the""" \
                                       """ terminal for more info""")

###############################################################################################
class w_main(QMainWindow):
    def __init__(self):
        super(w_main, self).__init__()
        self.init_ui()

    def __del__(self):
        engine.release()

    def init_ui(self):
        app_icon_path = os.path.normcase(util.get_exec_dir(__file__) + '/img/icon.ico')
        self.setMinimumWidth(600)
        self.setWindowTitle("darkHAMMER: h3dimport")
        self.setWindowFlags(self.windowFlags() & (~Qt.WindowMaximizeButtonHint))
        self.setWindowIcon(QIcon(app_icon_path))

        main_menu = QMenuBar(self)

        main_menu.addAction("Preferences", self.pref_clicked)
        mnu_help = QMenu("Help", self)
        mnu_help.addAction("About", self.mnu_help_click)
        main_menu.addMenu(mnu_help)

        self.setMenuBar(main_menu)

        self.main_tab = QTabWidget(self)
        self.setCentralWidget(self.main_tab)

        # child tabs
        self.wnds = dict()

        model_wnd = w_model(self)
        self.wnds["model"] = model_wnd
        self.main_tab.addTab(model_wnd, "Model")

        anim_wnd = w_anim(self)
        self.wnds["anim"] = anim_wnd
        self.main_tab.addTab(anim_wnd, "Anim")

        phx_wnd = w_phx(self)
        self.wnds["phx"] = phx_wnd
        self.main_tab.addTab(phx_wnd, "Physics")

        self.show()
        self.setSizePolicy(QSizePolicy.Minimum, QSizePolicy.Fixed)

        self.main_tab.currentChanged.connect(self.tab_change)

        # load ui state
        self.load_config()

    def tab_change(self, index):
        # turn off auto import on all panels
        for (wnd_name, wnd) in self.wnds.iteritems():
            if wnd_name == "model":
                wnd.btn_auto.setChecked(False)
            elif wnd_name == "phx":
                wnd.btn_auto.setChecked(False)

        if index == 0:
            self.wnds["model"].enum_models()
        elif index == 2:
            self.wnds["phx"].enum_phxobjects()


    def mnu_help_click(self):
        wnd_about = about.qAboutDlg(self, "h3dimport-gui", "GUI front-end for h3dimport tool")
        wnd_about.exec_()

    def pref_clicked(self):
        pref_dlg = w_prefs(self)
        pref_dlg.load_config(self.config)
        if pref_dlg.exec_():
            pref_dlg.save_config(self.config)

    def load_prefs(self, cfg):
        global prefs
        if 'general' in cfg.sections():
            prefs['binpath'] = cfg.get('general', 'binary_path')
            prefs['assetdir'] = cfg.get('general', 'asset_dir')
            prefs['verbose'] = cfg.getboolean('general', 'verbose')
        else:
            prefs['binpath'] = os.path.abspath(util.get_exec_dir(__file__) + '/../../bin/h3dimport')
            prefs['assetdir'] = os.path.abspath(util.get_exec_dir(__file__) + '/../..')
            prefs['verbose'] = False
            if platform.system() == "Windows":
                prefs['binpath'] += ".exe"

    def save_prefs(self, cfg):
        if not 'general' in cfg.sections():
            cfg.add_section('general')

        cfg.set('general', 'binary_path', prefs['binpath'])
        cfg.set('general', 'verbose', str(prefs['verbose']))
        cfg.set('general', 'asset_dir', prefs['assetdir'])

    def load_config(self):
        try:
            self.config = ConfigParser.SafeConfigParser()
            f = self.config.read(os.path.normpath(util.get_exec_dir(__file__) + \
                "/h3dimport-gui.ini"))
            if len(f) == 0:
                raise BaseException()
        except:
            print "Warning: could not load program config, reset to defaults"
            self.load_prefs(self.config)
        else:
            self.load_prefs(self.config)
            if "ui" in self.config.sections():
                tab_idx = int(self.config.get("ui", "tab_idx"))
            else:
                tab_idx = 0
            self.wnds["model"].load_config(self.config)
            self.wnds["anim"].load_config(self.config)
            self.wnds["phx"].load_config(self.config)

            self.main_tab.setCurrentIndex(tab_idx)
            self.tab_change(tab_idx)

    def save_config(self):
        # save config
        self.save_prefs(self.config)
        if not "ui" in self.config.sections():
            self.config.add_section("ui")
        self.config.set("ui", "tab_idx", str(self.main_tab.currentIndex()))
        self.wnds["model"].save_config(self.config)
        self.wnds["anim"].save_config(self.config)
        self.wnds["phx"].save_config(self.config)
        self.config.write(open(os.path.normpath(util.get_exec_dir(__file__) + '/h3dimport-gui.ini'),
             'w'))

    def closeEvent(self, e):
        self.save_config()

###############################################################################################
def main():
    global main_wnd
    app = QApplication(sys.argv)
    main_wnd = w_main()
    r = app.exec_()
    del main_wnd
    sys.exit(r)

if __name__ == "__main__":
    main()

