from PyQt4.QtGui import *
from PyQt4.QtCore import *
import os, platform, sys, json, math
import dheng
from dhwidgets import eng_view
from dhutil import util
import engine

class qClipList(QWidget):
    def __init__(self, parent):
        super(qClipList, self).__init__(parent)
        self.clips = []
        self.frame_cnt = 60

        self.setFixedWidth(200)

        # layout and controls
        layout = QVBoxLayout()
        self.setLayout(layout)

        lbl_framecnt = QLabel('frame-cnt: ', self)
        layout.addWidget(lbl_framecnt)

        imgpath = os.path.normcase(util.get_exec_dir(__file__) + '/img/')
        layout2 = QHBoxLayout()
        layout2.setSpacing(1)
        ed_name = QLineEdit(self)
        ed_name.setMaxLength(30)
        layout2.addWidget(ed_name)
        btn_add = QPushButton(self)
        btn_remove = QPushButton(self)
        btn_add.setIcon(QIcon(imgpath + 'glyphicons_432_plus.png'))
        btn_remove.setIcon(QIcon(imgpath + 'glyphicons_207_remove_2.png'))
        btn_add.setFixedSize(24, 24)
        btn_remove.setFixedSize(24, 24)
        layout2.addWidget(btn_add)
        layout2.addWidget(btn_remove)
        layout.addLayout(layout2)

        self.lbl_framecnt = lbl_framecnt
        self.ed_name = ed_name
        self.btn_remove = btn_remove
        self.btn_add = btn_add

        lbl_clips = QLabel('Clips', self)
        self.lst_clips = QListWidget(self)
        self.lst_clips.setMaximumHeight(150)
        lst_palette = self.lst_clips.palette()
        lst_palette.setBrush(QPalette.Inactive, QPalette.Highlight,
            lst_palette.brush(QPalette.Active, QPalette.Highlight))
        lst_palette.setBrush(QPalette.Inactive, QPalette.HighlightedText,
            lst_palette.brush(QPalette.Active, QPalette.HighlightedText))
        self.lst_clips.setPalette(lst_palette)

        layout.addWidget(lbl_clips)
        layout.addWidget(self.lst_clips)

        layout3 = QFormLayout()
        self.ed_start = QLineEdit(self)
        self.ed_start.setValidator(QIntValidator())
        self.ed_end = QLineEdit(self)
        self.ed_end.setValidator(QIntValidator())
        self.chk_looped = QCheckBox(self)
        layout3.addRow('Start: ', self.ed_start)
        layout3.addRow('End: ', self.ed_end)
        layout3.addRow('Looped: ', self.chk_looped)
        self.edit_widgets = [self.ed_start, self.ed_end, self.chk_looped]
        layout.addLayout(layout3)
        layout.addStretch()

        ## events
        btn_add.clicked.connect(self.btn_add_clicked)
        self.ed_name.textChanged.connect(self.ed_name_changed)
        self.lst_clips.itemSelectionChanged.connect(self.lst_clips_selected)
        self.lst_clips.itemClicked.connect(self.lst_clips_clicked)
        btn_remove.clicked.connect(self.btn_remove_clicked)

        self.ed_start.textChanged.connect(self.ed_start_changed)
        self.ed_end.textChanged.connect(self.ed_end_changed)
        self.chk_looped.stateChanged.connect(self.chk_looped_changed)

        for widget in self.edit_widgets: widget.setEnabled(False)
        self.btn_remove.setEnabled(False)
        self.btn_add.setEnabled(False)

    def set_item(self):
        pass

    def load_clips(self, clips_jsonfile):
        self.clips = []
        self.lst_clips.clear()

        jclips = json.loads(open(clips_jsonfile, 'r').read())

        # fill the data
        if not jclips:  return

        for i in range(0, len(jclips)):
            # set default values
            if 'start' not in jclips[i]:     jclips[i]['start'] = 0
            if 'end' not in jclips[i]:       jclips[i]['end'] = self.frame_cnt
            if 'looped' not in jclips[i]:    jclips[i]['looped'] = False
            self.clips.append(jclips[i])

        # fill the list
        for clip in self.clips:
            self.lst_clips.addItem(clip['name'])

    def save_clips(self, clips_jsonfile):
        open(clips_jsonfile, 'w').write(json.dumps(self.clips, indent=4))

    def ed_start_changed(self, text):
        row = self.lst_clips.currentRow()
        if row != -1 and text != '':
            self.clips[row]['start'] = max(0, int(str(text)))
    def ed_end_changed(self, text):
        row = self.lst_clips.currentRow()
        if row != -1 and text != '':
            self.clips[row]['end'] = min(int(str(text)), self.frame_cnt)
    def chk_looped_changed(self, state):
        row = self.lst_clips.currentRow()
        if row != -1:
            self.clips[row]['looped'] = (state == Qt.Checked)

    def set_framecnt(self, framecnt):
        self.frame_cnt = framecnt
        self.lbl_framecnt.setText('frame-cnt: {0}'.format(framecnt))

    def ed_name_changed(self, text):
        if text.count() > 0:
            self.btn_add.setEnabled(True)
            # search in the list for the specified name and select it
            for i in range(0, self.lst_clips.count()):
                item = self.lst_clips.item(i)
                item_text = item.text()
                if str(item_text).find(text) == 0:
                    self.lst_clips.setCurrentRow(i)
                    return
        else:
            self.btn_add.setEnabled(False)
            self.btn_remove.setEnabled(False)
            self.lst_clips.setCurrentRow(-1)
            self.reset_controls()

    def preview_clip(self, clip):
        parent = self.parent()
        if parent.btn_play_state:
            parent.play_stop()

        parent.btn_play.setIcon(parent.icn_pause)

        # initialize playing (manual)
        parent.preview_clip = clip
        parent.preview_frame = clip['start']
        parent.tm_preview.start()

    def lst_clips_selected(self):
        self.btn_remove.setEnabled(True)
        row = self.lst_clips.currentRow()
        if row != -1:
            for widget in self.edit_widgets: widget.setEnabled(True)
            self.update_controls(self.clips[row])
            self.preview_clip(self.clips[row])

    def lst_clips_clicked(self, item):
        self.lst_clips_selected()

    def btn_add_clicked(self, checked):
        # check with the list (don't accept duplicate names)
        for i in range(0, self.lst_clips.count()):
            item = self.lst_clips.item(i)
            if item.text() == self.ed_name.text():
                QMessageBox.information(self, 'clip-editor', 'Name already exist in clips')
                return

        for widget in self.edit_widgets: widget.setEnabled(True)
        self.lst_clips.addItem(self.ed_name.text())
        self.edit_widgets[0].setFocus()

        # add to clips
        self.clips.append({ \
            'name': str(self.ed_name.text()), \
            'start': min(self.clips[-1]['end'] + 1, self.frame_cnt) if len(self.clips)>0 else 0, \
            'end': self.frame_cnt, \
            'looped': False
            })

        # update controls for subclip
        self.update_controls(self.clips[-1])

    def update_controls(self, clip):
        self.ed_start.setText(str(clip['start']))
        self.ed_end.setText(str(clip['end']))
        self.chk_looped.setChecked(clip['looped'])

    def reset_controls(self):
        self.ed_start.setText('')
        self.ed_end.setText('')
        self.chk_looped.setChecked(False)
        for widget in self.edit_widgets: widget.setEnabled(False)

    def btn_remove_clicked(self, checked):
        selected_id = self.lst_clips.currentRow()
        if selected_id != -1:
            self.lst_clips.removeItemWidget(self.lst_clips.takeItem(selected_id))
            self.reset_controls()
            self.parent().play_stop()
            del self.clips[selected_id]

class qClipController(QWidget):
    def __init__(self, parent):
        super(qClipController, self).__init__(parent)
        self.page_sz = 10
        self.tick_sz = 1
        self.frame_cnt = 60
        self.frame_cursor = 0 # current frame number
        self.frame_cursor_start = 0
        self.frame_cursor_end = 0
        self.frame_cursor_prev = 0
        self.setMinimumHeight(24)
        self.setMouseTracking(True)
        self.anim_cmp = None
        self.mouse_dwn = False

    def deselect(self):
        self.frame_cursor_start = self.frame_cursor_end = self.frame_cursor

    def calc_frame_x(self, n, w):
        return int(math.floor(n * w / self.frame_cnt))

    def calc_frame_n(self, x, w):
        f = int(math.floor(x * self.frame_cnt/w))
        return min(f, self.frame_cnt - 1)

    def paintEvent(self, e):
        w = self.width()
        h = self.height()

        qp = QPainter()
        qp.begin(self)
        qp.fillRect(-1, -1, w + 1, h + 1, QColor(150, 150, 150))

        w -= 4
        start_x = 2

        ## selection
        if self.frame_cursor_start < self.frame_cursor_end:
            x = start_x + self.calc_frame_x(self.frame_cursor_start, w)
            qp.fillRect(x, 3, \
                self.calc_frame_x(self.frame_cursor_end, w) - x + 1, h - 3, \
                QBrush(QColor(70, 150, 70), Qt.Dense4Pattern))

        ## ticks
        qp.setPen(QPen(QColor(0, 0, 0), 1))
        ticks = []
        for i in range(0, self.frame_cnt, self.tick_sz):
            x = self.calc_frame_x(i, w)
            ticks.append(QLine(start_x + x, max(0, h-3), start_x + x, max(h-8, 5)))
        qp.drawLines(ticks)

        ## pages
        qp.setPen(QPen(QColor(0, 0, 0), 2))
        pages = []
        for i in range(0, self.frame_cnt, self.page_sz):
            x = self.calc_frame_x(i, w)
            pages.append(QLine(start_x + x, max(0, h-3), start_x + x, max(h-15, 12)))
        qp.drawLines(pages)

        ## indicator (snap to frames)
        cursor_x = self.calc_frame_x(self.frame_cursor, w)
        qp.setPen(QPen(QColor(200, 0, 0), 2))
        qp.drawLine(QLine(start_x + cursor_x, max(0, h-3), start_x + cursor_x, 3))

        ## frame index
        if self.frame_cursor_start == self.frame_cursor_end:
            ftext = str(self.frame_cursor)
            text_offset = 0
        else:
            ftext = str(self.frame_cursor_start) + '-' + str(self.frame_cursor_end)
            text_offset = 16
        qp.setBackground(QColor(200, 0, 0))
        qp.setBackgroundMode(Qt.OpaqueMode)
        qp.setPen(QPen(QColor(255, 255, 255), 1))
        qp.drawText(w - 30 - text_offset, 3, w - 3, 24, Qt.TextDontClip, ftext)

        qp.end()

    def mousePressEvent(self, e):
        if not self.mouse_dwn:
            w = self.width() - 4
            cursor_x = max(e.pos().x() - 2, 0)
            self.parent().play_stop()

            if e.modifiers() & Qt.ShiftModifier:
                self.frame_cursor_end = self.calc_frame_n(cursor_x, w)
                dheng.cmp_value_setui(self.anim_cmp, 'frame_idx', self.frame_cursor_end)
                if self.frame_cursor_start > self.frame_cursor_end:
                    tmp = self.frame_cursor_end
                    self.frame_cursor_end = self.frame_cursor_start
                    self.frame_cursor_start = tmp
            else:
                self.frame_cursor = self.calc_frame_n(cursor_x, w)
                self.frame_cursor_start = self.frame_cursor
                self.frame_cursor_end = self.frame_cursor
                dheng.cmp_value_setui(self.anim_cmp, 'frame_idx', self.frame_cursor)

        self.update()
        self.mouse_dwn = True

    def mouseReleaseEvent(self, e):
        self.mouse_dwn = False

    def mouseMoveEvent(self, e):
        if self.mouse_dwn:
            w = self.width() - 4
            cursor_x = max(e.pos().x() - 2, 0)
            if e.modifiers() & Qt.ShiftModifier:
                self.frame_cursor_end = self.calc_frame_n(cursor_x, w)
                dheng.cmp_value_setui(self.anim_cmp, 'frame_idx', self.frame_cursor_end)
                if self.frame_cursor > self.frame_cursor_end:
                    tmp = self.frame_cursor_end
                    self.frame_cursor_end = self.frame_cursor
                    self.frame_cursor = tmp
            else:
                self.frame_cursor = self.calc_frame_n(cursor_x, w)
                self.frame_cursor_end = self.frame_cursor
                self.frame_cursor_start = self.frame_cursor
                dheng.cmp_value_setui(self.anim_cmp, 'frame_idx', self.frame_cursor)

            self.update()

    def set_pagesize(self, page_sz):
        self.page_sz = page_sz
        self.update()

    def set_framecnt(self, frame_cnt):
        self.frame_cnt = max(frame_cnt, 1)
        self.update()

    def set_ticksize(self, tick_sz):
        self.tick_sz = tick_sz
        self.update()

    def set_frame(self, f):
        if f != self.frame_cursor:
            if self.frame_cursor_start == self.frame_cursor_end:
                self.frame_cursor_start = self.frame_cursor_end = f
            self.frame_cursor = f
            self.update()

class qClipEditDlg(QDialog):
    def __init__(self, parent):
        super(qClipEditDlg, self).__init__(parent)

        self.setMinimumSize(800, 600)
        self.setWindowTitle('Clip editor')
        self.setSizeGripEnabled(True)
        self.setWindowFlags(self.windowFlags() & (~Qt.WindowContextHelpButtonHint))

        exedir = util.get_exec_dir(__file__)
        self.icn_play = QIcon(os.path.normcase(exedir + '/img/glyphicons_173_play.png'))
        self.icn_pause = QIcon(os.path.normcase(exedir + '/img/glyphicons_174_pause.png'))

        layout = QVBoxLayout()

        layout2 = QHBoxLayout()
        self.eng_view = eng_view.qEngineView(self)
        layout2.addWidget(self.eng_view)
        self.wnd_clips = qClipList(self)
        layout2.addWidget(self.wnd_clips)
        layout.addLayout(layout2)

        layout3 = QHBoxLayout()
        self.btn_play = QPushButton(self)
        self.btn_play.setIcon(self.icn_play)
        self.btn_play_state = False # keeps playing state of 'play/pause' button
        self.btn_play_state_prev = False

        self.clip_ctrl = qClipController(self)
        self.btn_play.setFixedSize(32, 32)
        layout3.addWidget(self.btn_play)
        layout3.addWidget(self.clip_ctrl)
        layout.addLayout(layout3)

        self.setLayout(layout)

        self.tm_play = QTimer(self)
        self.tm_play.setInterval(20)

        self.tm_preview = QTimer(self)
        self.tm_preview.setInterval(33)
        self.preview_clip = {}
        self.preview_frame = 0
        self.camera = None

        # events
        self.btn_play.clicked.connect(self.btn_play_clicked)
        self.tm_play.timeout.connect(self.tm_play_timeout)
        self.tm_preview.timeout.connect(self.tm_preview_timeout)

    def tm_preview_timeout(self):
         dheng.cmp_value_setui(self.anim_cmp, 'frame_idx', self.preview_frame)
         self.clip_ctrl.set_frame(self.preview_frame)
         self.preview_frame += 1
         if self.preview_frame > self.preview_clip['end'] - 1:
            if not self.preview_clip['looped']:
                self.play_stop()
            else:
                self.preview_frame = self.preview_clip['start']

    def tm_play_timeout(self):
        if self.anim_cmp:
            self.btn_play_state = dheng.cmp_anim_isplaying(self.anim_cmp)
            if self.btn_play_state != self.btn_play_state_prev:
                if not self.btn_play_state:
                    self.play_stop()
                else:
                    self.btn_play.setIcon(self.icn_pause)
            self.btn_play_state_prev = self.btn_play_state
            self.clip_ctrl.set_frame(dheng.cmp_anim_getcurframe(self.anim_cmp))

    def play_stop(self):
        self.btn_play.setIcon(self.icn_play)
        self.tm_play.stop()
        self.tm_preview.stop()
        dheng.cmp_anim_stop(self.anim_cmp)

    def btn_play_clicked(self, checked):
        # normal play, play from the start
        if not self.btn_play_state:
            # custom play (range is selected), we play the range in loop mode
            if self.clip_ctrl.frame_cursor_start != self.clip_ctrl.frame_cursor_end:
                self.preview_frame = self.clip_ctrl.frame_cursor_start
                self.preview_clip = {'start': self.clip_ctrl.frame_cursor_start,
                    'end': self.clip_ctrl.frame_cursor_end,
                    'looped': True}
                self.tm_preview.start()
            else:
                dheng.cmp_anim_play(self.anim_cmp)
                self.tm_play.start()
            self.btn_play.setIcon(self.icn_pause)
        else:
            if self.clip_ctrl.frame_cursor_start != self.clip_ctrl.frame_cursor_end:
                self.tm_preview.stop()
                self.preview_frame = self.clip_ctrl.frame_cursor_start
            else:
                self.tm_play.stop()
                dheng.cmp_anim_stop(self.anim_cmp)

            self.btn_play.setIcon(self.icn_play)

        self.btn_play_state = not self.btn_play_state
        self.btn_play_state_prev = self.btn_play_state

    def closeEvent(self, e):
        self.unload_props()
        self.tm_preview.stop()
        self.tm_play.stop()
        e.accept()

    def load_props(self, model_file, anim_file, clips_jsonfile):
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

        anim_cmp = dheng.cmp_create_instance_forobj('anim', obj)
        dheng.cmp_value_sets(anim_cmp, 'filepath', anim_file)
        dheng.cmp_anim_stop(anim_cmp)

        frame_cnt = dheng.cmp_anim_getframecnt(anim_cmp)
        self.clip_ctrl.set_framecnt(frame_cnt)
        self.clip_ctrl.anim_cmp = anim_cmp
        self.wnd_clips.set_framecnt(frame_cnt)

        self.obj = obj
        self.ground = ground
        self.anim_cmp = anim_cmp

        self.wnd_clips.load_clips(clips_jsonfile)
        self.tm_preview.setInterval(1000/dheng.cmp_anim_getfps(anim_cmp))
        self.clips_jsonfile = clips_jsonfile

    def unload_props(self):
        if self.obj:        dheng.scn_destroy_obj(self.obj)
        if self.ground:     dheng.scn_destroy_obj(self.ground)
        self.wnd_clips.save_clips(self.clips_jsonfile)
        self.obj = None
        self.ground = None
        self.anim_cmp = None

