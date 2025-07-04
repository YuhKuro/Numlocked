# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'form.ui'
##
## Created by: Qt User Interface Compiler version 6.8.1
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide6.QtCore import (QCoreApplication, QDate, QDateTime, QLocale,
    QMetaObject, QObject, QPoint, QRect,
    QSize, QTime, QUrl, Qt)
from PySide6.QtGui import (QBrush, QColor, QConicalGradient, QCursor,
    QFont, QFontDatabase, QGradient, QIcon,
    QImage, QKeySequence, QLinearGradient, QPainter,
    QPalette, QPixmap, QRadialGradient, QTransform)
from PySide6.QtWidgets import (QApplication, QCheckBox, QComboBox, QLabel,
    QListWidget, QListWidgetItem, QPushButton, QSizePolicy,
    QWidget)

class Ui_widget(object):
    def setupUi(self, widget):
        if not widget.objectName():
            widget.setObjectName(u"widget")
        widget.resize(563, 608)
        icon = QIcon(QIcon.fromTheme(QIcon.ThemeIcon.InputKeyboard))
        widget.setWindowIcon(icon)
        self.timezoneBox = QComboBox(widget)
        self.timezoneBox.addItem("")
        self.timezoneBox.addItem("")
        self.timezoneBox.addItem("")
        self.timezoneBox.addItem("")
        self.timezoneBox.setObjectName(u"timezoneBox")
        self.timezoneBox.setGeometry(QRect(350, 110, 171, 31))
        self.applyChanges = QPushButton(widget)
        self.applyChanges.setObjectName(u"applyChanges")
        self.applyChanges.setGeometry(QRect(319, 500, 101, 24))
        self.cancelChanges = QPushButton(widget)
        self.cancelChanges.setObjectName(u"cancelChanges")
        self.cancelChanges.setGeometry(QRect(420, 500, 101, 24))
        self.detectApps = QPushButton(widget)
        self.detectApps.setObjectName(u"detectApps")
        self.detectApps.setGeometry(QRect(40, 110, 131, 24))
        self.removeSelectedApp = QPushButton(widget)
        self.removeSelectedApp.setObjectName(u"removeSelectedApp")
        self.removeSelectedApp.setGeometry(QRect(180, 370, 151, 24))
        self.runningAppsList = QListWidget(widget)
        self.runningAppsList.setObjectName(u"runningAppsList")
        self.runningAppsList.setGeometry(QRect(30, 180, 151, 191))
        self.displayWPM = QCheckBox(widget)
        self.displayWPM.setObjectName(u"displayWPM")
        self.displayWPM.setGeometry(QRect(350, 180, 101, 22))
        self.displayTime = QCheckBox(widget)
        self.displayTime.setObjectName(u"displayTime")
        self.displayTime.setGeometry(QRect(350, 150, 101, 22))
        self.gameModeSelectedApp = QPushButton(widget)
        self.gameModeSelectedApp.setObjectName(u"gameModeSelectedApp")
        self.gameModeSelectedApp.setGeometry(QRect(30, 370, 151, 24))
        self.manuallyToggleGameMode = QPushButton(widget)
        self.manuallyToggleGameMode.setObjectName(u"manuallyToggleGameMode")
        self.manuallyToggleGameMode.setGeometry(QRect(350, 270, 131, 21))
        self.gameModeAppsList = QListWidget(widget)
        self.gameModeAppsList.setObjectName(u"gameModeAppsList")
        self.gameModeAppsList.setGeometry(QRect(180, 180, 151, 191))
        self.gamemodelabel = QLabel(widget)
        self.gamemodelabel.setObjectName(u"gamemodelabel")
        self.gamemodelabel.setGeometry(QRect(180, 160, 151, 20))
        self.gamemodelabel.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.currentapplication = QLabel(widget)
        self.currentapplication.setObjectName(u"currentapplication")
        self.currentapplication.setGeometry(QRect(30, 160, 151, 20))
        self.currentapplication.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.logoText = QLabel(widget)
        self.logoText.setObjectName(u"logoText")
        self.logoText.setGeometry(QRect(30, 10, 191, 51))
        font = QFont()
        font.setPointSize(25)
        self.logoText.setFont(font)
        self.label_2 = QLabel(widget)
        self.label_2.setObjectName(u"label_2")
        self.label_2.setGeometry(QRect(140, 70, 161, 21))
        font1 = QFont()
        font1.setPointSize(12)
        self.label_2.setFont(font1)
        self.label_3 = QLabel(widget)
        self.label_3.setObjectName(u"label_3")
        self.label_3.setGeometry(QRect(30, 10, 231, 81))
        font2 = QFont()
        font2.setPointSize(20)
        self.label_3.setFont(font2)
        self.autoGameMode = QCheckBox(widget)
        self.autoGameMode.setObjectName(u"autoGameMode")
        self.autoGameMode.setGeometry(QRect(350, 210, 201, 22))
        self.Logo = QLabel(widget)
        self.Logo.setObjectName(u"Logo")
        self.Logo.setGeometry(QRect(200, 20, 81, 41))
        self.Logo.setPixmap(QPixmap(u"Orig.png"))
        self.Logo.setScaledContents(True)
        self.label_3.raise_()
        self.timezoneBox.raise_()
        self.applyChanges.raise_()
        self.cancelChanges.raise_()
        self.detectApps.raise_()
        self.removeSelectedApp.raise_()
        self.runningAppsList.raise_()
        self.displayWPM.raise_()
        self.displayTime.raise_()
        self.gameModeSelectedApp.raise_()
        self.manuallyToggleGameMode.raise_()
        self.gameModeAppsList.raise_()
        self.gamemodelabel.raise_()
        self.currentapplication.raise_()
        self.logoText.raise_()
        self.label_2.raise_()
        self.autoGameMode.raise_()
        self.Logo.raise_()

        self.retranslateUi(widget)

        QMetaObject.connectSlotsByName(widget)
    # setupUi

    def retranslateUi(self, widget):
        widget.setWindowTitle(QCoreApplication.translate("widget", u"Numlocked Hub", None))
        self.timezoneBox.setItemText(0, QCoreApplication.translate("widget", u"Device Time (Default)", None))
        self.timezoneBox.setItemText(1, QCoreApplication.translate("widget", u"EST (New York)", None))
        self.timezoneBox.setItemText(2, QCoreApplication.translate("widget", u"CST (Chicago)", None))
        self.timezoneBox.setItemText(3, QCoreApplication.translate("widget", u"JST (Tokyo)", None))

        self.applyChanges.setText(QCoreApplication.translate("widget", u"Save Changes", None))
        self.cancelChanges.setText(QCoreApplication.translate("widget", u"Cancel", None))
        self.detectApps.setText(QCoreApplication.translate("widget", u"Detect Applications", None))
        self.removeSelectedApp.setText(QCoreApplication.translate("widget", u"Remove from Game Mode", None))
        self.displayWPM.setText(QCoreApplication.translate("widget", u"Display WPM", None))
        self.displayTime.setText(QCoreApplication.translate("widget", u"Display Time", None))
        self.gameModeSelectedApp.setText(QCoreApplication.translate("widget", u"Move to Game Mode Apps", None))
        self.manuallyToggleGameMode.setText(QCoreApplication.translate("widget", u"Activate Game-Mode", None))
        self.gamemodelabel.setText(QCoreApplication.translate("widget", u"Game Mode Applications", None))
        self.currentapplication.setText(QCoreApplication.translate("widget", u"Running Applications", None))
        self.logoText.setText(QCoreApplication.translate("widget", u"Numlocked", None))
        self.label_2.setText(QCoreApplication.translate("widget", u"Keyboard Control", None))
        self.label_3.setText(QCoreApplication.translate("widget", u"___________________________", None))
        self.autoGameMode.setText(QCoreApplication.translate("widget", u"Automatically Enable Game Mode", None))
        self.Logo.setText("")
    # retranslateUi

