from PyQt4.QtCore import *
from PyQt4.QtGui import *
from dhutil import util

class qAboutDlg(QDialog):
    def __init__(self, parent, title, desc):
        super(qAboutDlg, self).__init__(parent)
        self.setFixedSize(400, 250)
        self.setWindowTitle('About')
        self.setWindowFlags(self.windowFlags() & (~Qt.WindowContextHelpButtonHint)) 
        
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        # title (program name and version)
        frame_title = QWidget(self)
        frame_title.setStyleSheet('background-color:white;')
        frame_title.setMinimumHeight(55)
        frame_title.setFixedWidth(self.width())

        layout3 = QVBoxLayout()
        lbl_title = QLabel('darkHAMMER: ' + title, frame_title)
        lbl_title.setStyleSheet('font: bold 14px;')
        layout3.addWidget(lbl_title)
        lbl_ver = QLabel('v' + util.VERSION, frame_title)
        layout3.addWidget(lbl_ver)
        frame_title.setLayout(layout3)
        layout.addWidget(frame_title)
        
        # copyright and site link
        layout4 = QVBoxLayout()
        layout4.setContentsMargins(10, 10, 10, 10)
        lbl_credits = QLabel("Copyright (c) 2013 - darkHAMMER team")
        layout4.addWidget(lbl_credits)
                                   
        lbl_link = QLabel("<a href='http://www.hmrengine.com'>http://www.hmrengine.com</a>",\
                             self)
        lbl_link.setOpenExternalLinks(True)
        lbl_link.setTextInteractionFlags(Qt.LinksAccessibleByMouse)
        lbl_link.setStyleSheet('margin-bottom: 10px')
        layout4.addWidget(lbl_link)
        
        desc = '<b>' + desc + '</b>' +  \
        """<br><br>This software is open-source and is written with the help of the following """\
        """open-source softwares:"""\
        """<ul>"""\
        """<li> darkHAMMER engine and tools</li>"""\
        """<li> Python: <a href='http://python.org/'>http://python.org/</a></li>"""\
        """<li> PyQt4: <a href='http://www.riverbankcomputing.com/software/pyqt/intro'>http://www.riverbankcomputing.com/software/pyqt/intro</a></li>"""\
        """</ul>"""
        
        txt_desc = QTextEdit(desc, self)
        txt_desc.setFrameStyle(QFrame.Panel)
        txt_desc.setLineWidth(1)
        txt_desc.setMinimumHeight(50)
        txt_desc.setReadOnly(True)
        layout4.addWidget(txt_desc)
        
        layout4.addStretch()

        btn_ok = QPushButton("Close", self)
        btn_ok.setFixedWidth(100)
        btn_ok.clicked.connect(self.btn_ok_click)
        layout4.addWidget(btn_ok)

        layout.addLayout(layout4)
    
    def btn_ok_click(self, checked):
        self.accept()
