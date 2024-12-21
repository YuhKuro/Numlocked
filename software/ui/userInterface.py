
import sys
import os
import json
from .ui_form import Ui_widget  # Import the generated UI class
from PySide6.QtWidgets import QApplication, QWidget, QListWidgetItem
from core.process_monitor import get_running_processes
from core.usb_communication import sendUSBCommand
from core.process_monitor import generate_background_process_list

# USB Messaging RULES
# 1. enable/disable WPM
# 2. enable/disable Time (enable and send c)
# 3. Set timezone (send current time)
# 4. enable/disable automatic game mode
# 5. enable/disable game mode manually

scriptDir = os.path.dirname(os.path.abspath(__file__))
backGroundPath = os.path.join(scriptDir, "../settings/backgroundApps.json")
configPath = os.path.join(scriptDir, "../settings/userConfig.json")
gameModeAppsPath = os.path.join(scriptDir, "../settings/gameModeAppsList.json")

class Widget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.ui = Ui_widget()
        self.ui.setupUi(self)

        self.gameModeOn = False

        # Create a background process list; This will not be used unless needed.
        # generate_background_process_list()
        
        # Load game mode apps from JSON
        self.loadUserSettings()

        # Populate the gameModeAppsList with initial values
        self.updateRunningApps()
        self.update_game_mode_apps_list()

        # Connect the detectApps button to the update method
        self.ui.detectApps.clicked.connect(self.updateRunningApps)
        self.ui.gameModeSelectedApp.clicked.connect(self.addSelectedtoGame)
        self.ui.removeSelectedApp.clicked.connect(self.removeSelectedAppFromGameMode)
        self.ui.applyChanges.clicked.connect(self.applyChanges)
        self.ui.cancelChanges.clicked.connect(self.cancelChanges)
        self.ui.manuallyToggleGameMode.clicked.connect(self.toggleGameMode)

        self.ui.displayWPM.stateChanged.connect(self.displayWPM)
        self.ui.displayTime.stateChanged.connect(self.displayTime)
        self.ui.timezoneBox.currentIndexChanged.connect(self.updateTimezone)
        self.ui.autoGameMode.stateChanged.connect(self.autoGameMode)

    #These updates are not saved unless the apply button is clicked. A preview is shown to the user.
    def updateTimezone(self):
        #update to the selected timezone on the keyboard.
        sendUSBCommand("3", self.ui.timezoneBox.currentText())
        
    def displayWPM(self):
        # Display the WPM on the keyboard
        sendUSBCommand("1", "")
    
    def displayTime(self):
        # check timezone, then send the current time to the keyboard
        sendUSBCommand("2", "")
    
    def autoGameMode(self):
        # Automatically enable game mode when a game is detected
        # Send signal to background C program.
        sendUSBCommand("4", "")

    def loadUserSettings(self):
        """Load user settings and game mode apps from JSON files."""
        try:
            # Load game mode apps list
            with open(gameModeAppsPath, "r") as file:
                data = json.load(file)
                self.gameModeApps = data.get("gameModeApps", [])
        except (FileNotFoundError, json.JSONDecodeError):
            self.gameModeApps = []

        try:
            # Load user configuration settings
            with open(configPath, "r") as file:
                settings = json.load(file)
                self.ui.displayTime.setChecked(settings["OLEDsettings"]["displayTime"])
                self.ui.displayWPM.setChecked(settings["OLEDsettings"]["displayWPM"])
                self.ui.autoGameMode.setChecked(settings["UserSettings"]["autoGameMode"])
                self.ui.timezoneBox.setCurrentText(settings["OLEDsettings"]["timezone"])
        except (FileNotFoundError, json.JSONDecodeError):
            # Initialize empty data if file not found or invalid
            self.gameModeApps = []

                    
    def saveUserSettings(self):
        """Save user settings and game mode apps to JSON files."""
        # Ensure all game mode apps are saved with `.exe` extensions
        gameModeAppsWithExe = [
            app if app.endswith(".exe") else f"{app}.exe" for app in self.gameModeApps
        ]
        with open(gameModeAppsPath, "w") as file:
            json.dump({"gameModeApps": gameModeAppsWithExe}, file, indent=4)

        # Save other user settings
        settings = {
            "OLEDsettings": {
                "displayTime": self.ui.displayTime.isChecked(),
                "displayWPM": self.ui.displayWPM.isChecked(),
                "timezone": self.ui.timezoneBox.currentText(),
            },
            "UserSettings": {"autoGameMode": self.ui.autoGameMode.isChecked()},
        }
        with open(configPath, "w") as file:
            json.dump(settings, file, indent=4)
        print("Settings saved!")

    def updateRunningApps(self):
        processes = get_running_processes()

        # Clear the list widget
        self.ui.runningAppsList.clear()
        seenProcesses = set()

        try:
            # Load background processes from JSON
            with open(backGroundPath, "r") as file:
                data = json.load(file)
                backgroundProcesses = data.get("backgroundApps", [])
        except (FileNotFoundError, json.JSONDecodeError):
            backgroundProcesses = []

        # Add processes to the list widget, excluding unwanted entries
        for process in processes:
            processName = process.replace(".exe", "")  # Strip .exe for display
            if (
                process not in self.gameModeApps
                and process not in seenProcesses
                and process not in backgroundProcesses
            ):
                item = QListWidgetItem(processName)
                seenProcesses.add(process)
                self.ui.runningAppsList.addItem(item)

    def addSelectedtoGame(self):
        """Add selected app(s) to the game mode list."""
        selected_items = self.ui.runningAppsList.selectedItems()
        for item in selected_items:
            app_name = item.text() + ".exe"  # Add .exe back for internal storage
            if app_name not in self.gameModeApps:
                self.gameModeApps.append(app_name)

            # Remove the item from the runningAppsList
            row = self.ui.runningAppsList.row(item)
            self.ui.runningAppsList.takeItem(row)

        # Update widgets
        self.update_game_mode_apps_list()
        self.updateRunningApps()

    def update_game_mode_apps_list(self):
        self.ui.gameModeAppsList.clear()
        for app in self.gameModeApps:
            appName = app.replace(".exe", "")  # Strip .exe for display
            QListWidgetItem(appName, self.ui.gameModeAppsList)

    def removeSelectedAppFromGameMode(self):
        """Remove selected app(s) from the game mode list."""
        selected_items = self.ui.gameModeAppsList.selectedItems()
        if not selected_items:
            print("No items selected for removal.")  # Debugging message
            return

        for item in selected_items:
            app_name = item.text() + ".exe"  # Add .exe back for internal storage
            if app_name in self.gameModeApps:
                print(f"Removing {app_name} from gameModeApps")  # Debugging message
                self.gameModeApps.remove(app_name)

        # Update the UI to reflect changes
        self.update_game_mode_apps_list()
        self.updateRunningApps()


    def applyChanges(self):
        #Save the game mode apps list to a JSON file.
        self.saveUserSettings()
        # Send the updated game mode apps list to the keyboard
        #sendUSBCommand(5, self.gameModeApps)
    
    def cancelChanges(self):
        #Load the game mode apps list from a JSON file.
        self.loadUserSettings()
        # Update the gameModeAppsList widget
        self.update_game_mode_apps_list()
        # Send the current settings to the keyboard, undoing any changes
        sendUSBCommand("")

    def toggleGameMode(self):
        """Toggle the game mode on/off."""
        # Toggle the game mode state
        self.gameModeOn = not self.gameModeOn

        # Update button text based on the new state
        if self.gameModeOn:
            self.ui.manuallyToggleGameMode.setText("Deactivate Game Mode")
        else:
            self.ui.manuallyToggleGameMode.setText("Activate Game Mode")

        # Send the USB command to toggle game mode on the keyboard
        sendUSBCommand("5", "")

