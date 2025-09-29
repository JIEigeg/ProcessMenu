# ProcessMenu

<img width="626" height="503" alt="ProcessMenu_UCKRcbnlmN" src="https://github.com/user-attachments/assets/b2d50b83-1dd0-468c-9336-de0e66b2304b" />


## Description

`ProcessMenu` is a robust C++ utility for Windows, designed to provide advanced process management capabilities through an intuitive graphical user interface. It allows users to launch new processes with custom parentage, offering enhanced control over process hierarchies. Additionally, it integrates a powerful debugging feature to log critical process events and a comprehensive memory viewer for in-depth analysis of running applications.

## Key Features

*   **Custom Process Parentage:**
    *   Launch new processes with an existing running process as the parent.
    *   Launch new processes with a new, temporary, "dummy parent" process.
*   **Comprehensive Process Event Logging:**
    *   Log debugging events, including DLL loads/unloads, thread creation/termination, and exceptions.
    *   **Capture Debug Strings:** The debugger now captures and displays output from `OutputDebugStringA` and `OutputDebugStringW` from the target process in the main log window.
*   **Advanced Memory Viewer:**
    *   View raw hexadecimal dumps of memory at specified addresses.
    *   List all virtual memory regions with details on base address, size, state, type, and protection.
    *   **Module List:** A new tab lists all loaded modules (DLLs) in the debugged process, including their name, base address, size, and full path.
*   **Intuitive Process Picker Dialog:**
    *   A user-friendly dialog to select parent processes from a list of all running processes.

## Build and Run

### Requirements

*   Microsoft Visual Studio (Visual Studio 2019 or newer recommended)
*   Windows SDK

### Build

1.  Open the `ProcessMenu.sln` file in Visual Studio.
2.  Select the desired build configuration (e.g., `Debug` or `Release`) and platform (`x64`).
3.  Build the project by navigating to `Build -> Build Solution` in the Visual Studio menu.

The executable file `ProcessMenu.exe` will be generated in the `x64/Debug` or `x64/Release` directory within your project folder.

### Run

Simply launch `ProcessMenu.exe` from its build directory.

## Usage

### Main Window

1.  **Launch Mode:** This section allows you to define how the target process will be launched.
    *   **Use existing parent process:** Select this option to specify a currently running process as the parent for your new application. Enter the parent process's executable name in the "Parent Process Name" field or use the `...` button to browse and select from a list.
    *   **Create new parent process:** Choose this mode to launch your target application under a newly created, temporary parent process. This parent will be automatically terminated once your target application starts.
    *   **Create new parent and log events:** This option combines the creation of a new temporary parent with comprehensive debugging event logging, providing detailed insights into the target process's lifecycle and behavior.

2.  **Parent Process Name:** Input the executable name (e.g., `explorer.exe`) of the desired parent process. The `...` button opens the Process Picker Dialog for easy selection.

3.  **Memory Viewer:** Click this button to open a dedicated window for advanced memory inspection of the currently debugged process.

4.  **Target Executable Path:** Specify the full path to the executable file you wish to launch. The `Browse...` button provides a file dialog for convenient selection.

5.  **Launch Process:** Initiates the launch of the target application based on the configured "Launch Mode" and paths.

6.  **Log Output:** This read-only text area displays real-time logs of process events when debugging is enabled. It provides color-coded messages for different event types (e.g., DLL loads, thread events, exceptions).

7.  **Copy Log / Save Log As...:** These buttons allow you to copy the entire content of the "Log Output" to your clipboard or save it as a text file to a specified location.

### Process Picker Dialog

This dialog appears when you click the `...` button next to the "Parent Process Name" field. It presents a sortable list of all active processes on your system, showing their executable names and Process IDs (PIDs). Double-clicking a process or selecting it and clicking "OK" will populate the "Parent Process Name" field in the main window.

### Memory Viewer Window

This window provides three main tabs for memory analysis:

1.  **Address Input:** Enter a hexadecimal memory address (e.g., `0x401000`) in the "Address" field.
2.  **View Memory Button:** Clicking this button will fetch and display a memory dump starting from the specified address.
3.  **Tabs:**
    *   **Memory Dump:** This tab displays a formatted view of the memory content. It shows the hexadecimal values of bytes, grouped for readability, alongside their ASCII character representations. This is crucial for understanding data structures, strings, and executable code.
    *   **Memory Regions:** This tab provides a tabular listing of all virtual memory regions allocated by the debugged process. Each entry includes details like Base Address, Size, State, Type, and Protection. Right-clicking an item allows you to copy its base address or view its content directly in the "Memory Dump" tab.
    *   **Modules:** This new tab lists all modules (executables and DLLs) loaded by the debugged process. It displays the module name, base address, size, and the full path to the module file on disk. This is useful for understanding the process's dependencies and memory layout.

## License

This project is licensed under the **GNU General Public License v3.0**.

## Contributing

Suggestions and improvements are welcome. Please feel free to create an issue or submit a pull request on GitHub.
