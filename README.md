# ProcessMenu

<img width="626" height="503" alt="ProcessMenu_UCKRcbnlmN" src="https://github.com/user-attachments/assets/b2d50b83-1dd0-468c-9336-de0e66b2304b" />


## Description

`ProcessMenu` is a robust C++ utility for Windows, designed to provide advanced process management capabilities through an intuitive graphical user interface. It allows users to launch new processes with custom parentage, offering enhanced control over process hierarchies. Additionally, it integrates a powerful debugging feature to log critical process events and a comprehensive memory viewer for in-depth analysis of running applications.

## Key Features

*   **Custom Process Parentage:**
    *   **Launch with an existing parent process:** This feature enables you to select any currently running process on your system to act as the parent for the new process you wish to launch. This is particularly useful for simulating specific application environments or for bypassing certain security restrictions that rely on process lineage.
    *   **Launch with a new, temporary parent process:** For scenarios where you need a clean or isolated parent, `ProcessMenu` can create a temporary "dummy parent" process. This dummy parent then launches your target executable, and is subsequently terminated, leaving your target process with a custom, short-lived parentage. This can be beneficial for testing or for obscuring the true origin of a process.

*   **Comprehensive Process Event Logging:**
    *   When launching a process with a new, temporary parent, you have the option to enable detailed logging of debugging events. This includes, but is not limited to:
        *   **DLL Load/Unload Events:** Monitor which dynamic-link libraries are loaded and unloaded by the target process, providing insights into its dependencies and runtime behavior.
        *   **Thread Creation/Termination:** Track the lifecycle of threads within the target process, helping to understand its concurrency model and identify potential threading issues.
        *   **Exception Handling:** Log all exceptions (first-chance and second-chance) that occur within the target process, including their codes, descriptions, and memory addresses. This is invaluable for diagnosing crashes and unexpected behavior.

*   **Advanced Memory Viewer:**
    *   `ProcessMenu` includes an integrated memory viewer that allows for real-time inspection of the debugged process's memory space.
    *   **Memory Dump:** View raw hexadecimal dumps of memory at specified addresses, accompanied by their ASCII representations, facilitating the analysis of data structures and code segments.
    *   **Memory Regions Listing:** Obtain a detailed list of all allocated memory regions within the target process. This includes crucial information such as base address, size, current state (committed, reserved, free), type (image, mapped, private), and memory protection attributes (read, write, execute, copy-on-write, guard pages, no-cache, write-combine).

*   **Intuitive Process Picker Dialog:**
    *   A user-friendly dialog simplifies the selection of parent processes. It displays a comprehensive list of all currently running processes, showing their executable names and Process IDs (PIDs), making it easy to identify and choose the desired parent.

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

This window provides two main tabs for memory analysis:

1.  **Address Input:** Enter a hexadecimal memory address (e.g., `0x401000`) in the "Address" field.
2.  **View Memory Button:** Clicking this button will fetch and display a memory dump starting from the specified address.
3.  **Tabs:**
    *   **Memory Dump:** This tab displays a formatted view of the memory content. It shows the hexadecimal values of bytes, grouped for readability, alongside their ASCII character representations. This is crucial for understanding data structures, strings, and executable code.
    *   **Memory Regions:** This tab provides a tabular listing of all virtual memory regions allocated by the debugged process. Each entry includes:
        *   **Base Address:** The starting address of the memory region.
        *   **Size:** The total size of the region.
        *   **State:** Indicates whether the memory is `Committed`, `Reserved`, or `Free`.
        *   **Type:** Specifies the type of memory, such as `Image` (executable code/data), `Mapped` (file-backed), or `Private` (application-specific data).
        *   **Protection:** Describes the access rights for the memory region (e.g., `R` for Read, `RW` for Read/Write, `RX` for Read/Execute, `RWX` for Read/Write/Execute, `NA` for No Access, `G` for Guard Page, `NC` for No Cache, `WC` for Write Combine).
        Right-clicking on an item in this list allows you to copy its base address or view its content directly in the "Memory Dump" tab.

## License

This project is licensed under the **GNU General Public License v3.0**.

## Contributing

Suggestions and improvements are welcome. Please feel free to create an issue or submit a pull request on GitHub.
