import sys
import json
from PySide6.QtWidgets import QApplication, QWidget, QListWidgetItem
from ui.userInterface import Widget
from sockets.socketControl import SocketClient


gameModeOn = False

if __name__ == "__main__":
    app = QApplication(sys.argv)
    server = SocketClient()
    server.connect()
    widget = Widget(server)
    widget.show()


    sys.exit(app.exec())
    server.closeConnection()
