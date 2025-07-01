#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

int main() {
    HANDLE hSnapshot;
    PROCESSENTRY32 pe32;
    
    // Take a snapshot of all processes in the system
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        printf("Error: Unable to create process snapshot\n");
        return 1;
    }
    
    // Set the size of the structure before using it
    pe32.dwSize = sizeof(PROCESSENTRY32);
    
    // Retrieve information about the first process
    if (!Process32First(hSnapshot, &pe32)) {
        printf("Error: Failed to get first process\n");
        CloseHandle(hSnapshot);
        return 1;
    }
    
    printf("Running processes:\n");
    printf("==================\n");
    
    // Walk through the process list
    do {
        printf("%s\n", pe32.szExeFile);
    } while (Process32Next(hSnapshot, &pe32));
    
    // Clean up
    CloseHandle(hSnapshot);
    
    return 0;
}
