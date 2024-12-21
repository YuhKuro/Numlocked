import sys
import json
from PySide6.QtWidgets import QApplication, QWidget, QListWidgetItem
from ui.userInterface import Widget


gameModeOn = False

if __name__ == "__main__":
    app = QApplication(sys.argv)
    widget = Widget()
    widget.show()
    sys.exit(app.exec())
