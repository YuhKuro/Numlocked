import sys
from PySide6.QtWidgets import QApplication
from ui.userInterface import Widget
from sockets.socketControl import SocketServer
import threading

def main():
    app = QApplication(sys.argv)

    # Start the server in a separate thread
    server = SocketServer()
    server_thread = threading.Thread(target=server.start, daemon=True)
    server_thread.start()

    # Create and show the main widget
    widget = Widget(server)
    widget.show()

    # Run the application event loop
    exit_code = app.exec()

    # Shut down the server gracefully
    server.closeConnection()
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
