// This file is part of BOINC.
// http://boinc.berkeley.edu
// Copyright (C) 2010 University of California
//
// BOINC is free software; you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// BOINC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with BOINC.  If not, see <http://www.gnu.org/licenses/>.

#ifdef _WIN32
#include "boinc_win.h"
#include "win_util.h"
#else
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <unistd.h>
#endif

using std::string;

#if defined(_MSC_VER) || defined(__MINGW32__)
#define popen       _popen
#define pclose      _pclose
#define getcwd      _getcwd
#define putenv      _putenv
#endif

#include "diagnostics.h"
#include "filesys.h"
#include "parse.h"
#include "str_util.h"
#include "str_replace.h"
#include "util.h"
#include "error_numbers.h"
#include "procinfo.h"
#include "boinc_api.h"
#include "vbox.h"

VBOX_VM::VBOX_VM() {
    os_name.clear();
    memory_size_mb.clear();
    image_filename.clear();
    suspended = false;
    enable_network = false;
    enable_shared_directory = false;
}

int VBOX_VM::run() {
    int retval;

    retval = initialize();
    if (retval) return retval;

    if (!is_registered()) {
        if (is_hdd_registered()) {
            // Handle the case where a previous instance of the same projects VM
            // was already initialized for the current slot directory but aborted
            // while the task was suspended and unloaded from memory.
            retval = deregister_stale_vm();
            if (retval) return retval;
        }
        retval = register_vm();
        if (retval) return retval;
    }

    retval = startvm();
    if (retval) return retval;

    return 0;
}

void VBOX_VM::poll() {
    return;
}

void VBOX_VM::cleanup() {
    stop();
    deregister_vm();

    // Give time enough for external processes to finish the cleanup process
    boinc_sleep(5.0);
}

// Execute the vbox manage application and copy the output to the
// designated buffer.
//
int VBOX_VM::vbm_popen(string& arguments, string& output) {
    char buf[256];
    string command;

    // Initialize command line
    command = "VBoxManage -q " + arguments;

#ifdef _WIN32
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;
    HANDLE hReadPipe, hWritePipe;
    void* pBuf;
    DWORD dwCount;
    unsigned long ulExitCode = 0;
    int retval = ERR_FOPEN;


    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    memset(&sa, 0, sizeof(sa));
    memset(&sd, 0, sizeof(sd));


    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, true, NULL, false);

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = &sd;


    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        fprintf(
            stderr,
            "%s CreatePipe failed! (%d).\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            GetLastError()
        );
        goto CLEANUP;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);


    si.cb = sizeof(STARTUPINFO);
    si.dwFlags |= STARTF_FORCEOFFFEEDBACK | STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = NULL;


    // Execute command
    if (!CreateProcess(NULL, (LPTSTR)command.c_str(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        fprintf(
            stderr,
            "%s CreateProcess failed! (%d).\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            GetLastError()
        );
        goto CLEANUP;
    }


    // Wait until process has completed
    while(1) {
        GetExitCodeProcess(pi.hProcess, &ulExitCode);
        if (ulExitCode != STILL_ACTIVE) break;
    }


    // Copy stdout/stderr to output buffer
    if (!PeekNamedPipe(hReadPipe, NULL, NULL, NULL, &dwCount, NULL)) {
        fprintf(
            stderr,
            "%s PeekNamedPipe failed! (%d).\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            GetLastError()
        );
    }

    if (dwCount) {
        pBuf = malloc(dwCount+1);
        memset(pBuf, 0, dwCount+1);

        if (ReadFile(hReadPipe, pBuf, dwCount, &dwCount, NULL)) {
            output += (char*)pBuf;
        }

        free(pBuf);
    }


CLEANUP:
    if (pi.hThread) CloseHandle(pi.hThread);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (hReadPipe) CloseHandle(hReadPipe);
    if (hWritePipe) CloseHandle(hWritePipe);

    if ((ulExitCode == 0) && (pi.hProcess)) {
        retval = 0;
    }

    return retval;

#else
    FILE* fp;

    // Execute command
    fp = popen(command.c_str(), "r");
    if (fp == NULL){
        fprintf(
            stderr,
            "%s vbm_popen popen failed! errno = %d\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            errno
        );
        return ERR_FOPEN;
    }

    // Copy output to buffer
    while (fgets(buf, 256, fp)) {
        output += buf;
    }

    // Close stream
    pclose(fp);

#endif

    return 0;
}


// Returns the current directory in which the executable resides.
//
int VBOX_VM::generate_vm_root_dir( string& dir ) {
    char root_dir[256];

    getcwd(root_dir, (sizeof(root_dir)*sizeof(char)));
    dir = root_dir;

    if (!dir.empty()) {
        return 1;
    }
    return 0;
}

bool VBOX_VM::is_registered() {
    string command;
    string output;

    command = "showvminfo " + vm_name;

    if (vbm_popen(command, output) == 0) {
        if (output.find("VBOX_E_OBJECT_NOT_FOUND") != string::npos) {
            return true;
        }
    }
    return false;
}


bool VBOX_VM::is_hdd_registered() {
    string command;
    string output;
    string virtual_machine_root_dir;

    generate_vm_root_dir(virtual_machine_root_dir);

    command = "showhdinfo \"" + virtual_machine_root_dir + "/" + image_filename + "\" ";

    if (vbm_popen(command, output) == 0) {
        if (output.find("VBOX_E_FILE_ERROR") != string::npos) {
            return true;
        }
    }
    return false;
}


bool VBOX_VM::is_running() {
    string command;
    string output;

    command = "list runningvms";

    if (vbm_popen(command, output) == 0) {
        if (output.find(vm_name) != string::npos) {
            return true;
        }
    }

    return false;
}

int VBOX_VM::get_install_directory(string& virtualbox_install_directory ) {
#ifdef _WIN32
    LONG    lReturnValue;
    HKEY    hkSetupHive;
    LPTSTR  lpszRegistryValue = NULL;
    DWORD   dwSize = 0;

    // change the current directory to the boinc data directory if it exists
    lReturnValue = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE, 
        _T("SOFTWARE\\Oracle\\VirtualBox"),  
        0, 
        KEY_READ,
        &hkSetupHive
    );
    if (lReturnValue == ERROR_SUCCESS) {
        // How large does our buffer need to be?
        lReturnValue = RegQueryValueEx(
            hkSetupHive,
            _T("InstallDir"),
            NULL,
            NULL,
            NULL,
            &dwSize
        );
        if (lReturnValue != ERROR_FILE_NOT_FOUND) {
            // Allocate the buffer space.
            lpszRegistryValue = (LPTSTR) malloc(dwSize);
            (*lpszRegistryValue) = NULL;

            // Now get the data
            lReturnValue = RegQueryValueEx( 
                hkSetupHive,
                _T("InstallDir"),
                NULL,
                NULL,
                (LPBYTE)lpszRegistryValue,
                &dwSize
            );

            virtualbox_install_directory = lpszRegistryValue;
        }
    }

    if (hkSetupHive) RegCloseKey(hkSetupHive);
    if (lpszRegistryValue) free(lpszRegistryValue);
#else
    virtualbox_install_directory = "";
#endif
    return 0;
}


int VBOX_VM::initialize() {
    string virtualbox_install_directory;
    string old_path;
    string new_path;
    char buf[256];

    get_install_directory(virtualbox_install_directory);

    // Prep the environment so we can execute the vboxmanage application
    if (!virtualbox_install_directory.empty()) {
        old_path = getenv("path");

        new_path = "path=";

        new_path += virtualbox_install_directory;

        // Path environment variable seperator
#ifdef _WIN32
        new_path += ";";
#else
        new_path += ":";
#endif

        new_path += old_path;

        if (putenv(const_cast<char*>(new_path.c_str()))) {
            fprintf(
                stderr,
                "%s Failed to modify the search path.\n",
                boinc_msg_prefix(buf, sizeof(buf))
            );
        }
    }

    return 0;
}


int VBOX_VM::register_vm() {
    string command;
    string output;
    string virtual_machine_root_dir;
    char buf[256];
    int retval;

    generate_vm_root_dir(virtual_machine_root_dir);

    fprintf(
        stderr,
        "%s Registering virtual machine. (%s) \n",
        boinc_msg_prefix(buf, sizeof(buf)),
        vm_name.c_str()
    );


    // Create and register the VM
    //
    command  = "createvm ";
    command += "--name \"" + vm_name + "\" ";
    command += "--basefolder \"" + virtual_machine_root_dir + "\" ";
    command += "--ostype \"" + os_name + "\" ";
    command += "--register";
    
    retval = vbm_popen(command, output);
    if (retval) {
        fprintf(
            stderr,
            "%s Error registering virtual machine! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            retval,
            command.c_str(),
            output.c_str()
        );
        return retval;
    }


    // Tweak the VM from it's default configuration
    //
    command  = "modifyvm \"" + vm_name + "\" ";
    command += "--memory " + memory_size_mb + " ";
    command += "--acpi on ";
    command += "--ioapic on ";
    command += "--boot1 disk ";
    command += "--boot2 none ";
    command += "--boot3 none ";
    command += "--boot4 none ";
    command += "--nic1 nat ";
    command += "--natdnsproxy1 on ";
    command += "--cableconnected1 off ";

    retval = vbm_popen(command, output);
    if (retval) {
        fprintf(
            stderr,
            "%s Error modifing virtual machine! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            retval,
            command.c_str(),
            output.c_str()
        );
        return retval;
    }


    // Add storage controller to VM
    //
    command  = "storagectl \"" + vm_name + "\" ";
    command += "--name \"IDE Controller\" ";
    command += "--add ide ";
    command += "--controller PIIX4 ";

    retval = vbm_popen(command, output);
    if (retval) {
        fprintf(
            stderr,
            "%s Error adding storage controller to virtual machine! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            retval,
            command.c_str(),
            output.c_str()
        );
        return retval;
    }


    // Adding virtual hard drive to VM
    //
    command  = "storageattach \"" + vm_name + "\" ";
    command += "--storagectl \"IDE Controller\" ";
    command += "--port 0 ";
    command += "--device 0 ";
    command += "--type hdd ";
    command += "--medium \"" + virtual_machine_root_dir + "/" + image_filename + "\" ";

    retval = vbm_popen(command, output);
    if (retval) {
        fprintf(
            stderr,
            "%s Error adding virtual disk drive to virtual machine! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            retval,
            command.c_str(),
            output.c_str()
        );
        return retval;
    }


    // Enable the network adapter if a network connection is required.
    // NOTE: Network access should never be allowed if the code running in a 
    //   shared directory or the VM itself is NOT signed.  Doing so opens up 
    //   the network behind the firewall to attack.
    //
    if (enable_network) {
        command  = "modifyvm \"" + vm_name + "\" ";
        command += "--cableconnected1 on ";

        retval = vbm_popen(command, output);
        if (retval) {
            fprintf(
                stderr,
                "%s Error enabling network access for virtual machine! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
                boinc_msg_prefix(buf, sizeof(buf)),
                retval,
                command.c_str(),
                output.c_str()
            );
            return retval;
        }
    }


    // Enable the shared folder if a shared folder is specified.
    //
    if (enable_shared_directory) {
        command  = "sharedfolder add \"" + vm_name + "\" ";
        command += "--name \"shared\" ";
        command += "--hostpath \"" + virtual_machine_root_dir + "/shared\"";

        retval = vbm_popen(command, output);
        if (retval) {
            fprintf(
                stderr,
                "%s Error enabling shared directory for virtual machine! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
                boinc_msg_prefix(buf, sizeof(buf)),
                retval,
                command.c_str(),
                output.c_str()
            );
            return retval;
        }
    }

    return 0;
}


int VBOX_VM::deregister_vm() {
    string command;
    string output;
    string virtual_machine_root_dir;
    char buf[256];
    int retval;

    generate_vm_root_dir(virtual_machine_root_dir);

    fprintf(
        stderr,
        "%s Deregistering virtual machine. (%s)\n",
        boinc_msg_prefix(buf, sizeof(buf)),
        vm_name.c_str()
    );


    // First step in deregistering a VM is to delete its storage controller
    //
    command  = "storagectl \"" + vm_name + "\" ";
    command += "--name \"IDE Controller\" ";
    command += "--remove ";

    retval = vbm_popen(command, output);
    if (retval) {
        fprintf(
            stderr,
            "%s Error removing storage controller from virtual machine! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            retval,
            command.c_str(),
            output.c_str()
        );
        return retval;
    }


    // Next delete VM
    //
    command  = "unregistervm \"" + vm_name + "\" ";
    command += "--delete ";

    retval = vbm_popen(command, output);
    if (retval) {
        fprintf(
            stderr,
            "%s Error removing virtual machine from virtual box! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            retval,
            command.c_str(),
            output.c_str()
        );
        return retval;
    }


    // Lastly delete medium from Virtual Box Media Registry
    //
    command  = "closemedium disk \"" + virtual_machine_root_dir + "/" + image_filename + "\" ";

    retval = vbm_popen(command, output);
    if (retval) {
        fprintf(
            stderr,
            "%s Error removing virtual hdd from virtual box! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            retval,
            command.c_str(),
            output.c_str()
        );
        return retval;
    }

    return 0;
}


int VBOX_VM::deregister_stale_vm() {
    string command;
    string output;
    string virtual_machine_root_dir;
    size_t uuid_location;
    size_t uuid_length;
    char buf[256];
    int retval;

    generate_vm_root_dir(virtual_machine_root_dir);

    // We need to determine what the name or uuid is of the previous VM which owns
    // this virtual disk
    //
    command  = "showhdinfo \"" + virtual_machine_root_dir + "/" + image_filename + "\" ";

    retval = vbm_popen(command, output);
    if (retval) {
        fprintf(
            stderr,
            "%s Error retrieving virtual hard disk information from virtual box! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            retval,
            command.c_str(),
            output.c_str()
        );
        return retval;
    }


    // Output should look a little like this:
    //   UUID:                 c119acaf-636c-41f6-86c9-38e639a31339
    //   Accessible:           yes
    //   Logical size:         10240 MBytes
    //   Current size on disk: 0 MBytes
    //   Type:                 normal (base)
    //   Storage format:       VDI
    //   Format variant:       dynamic default
    //   In use by VMs:        test2 (UUID: 000ab2be-1254-4c6a-9fdc-1536a478f601)
    //   Location:             C:\Users\romw\VirtualBox VMs\test2\test2.vdi
    //
    uuid_location = output.find("(UUID: ");
    if (uuid_location != string::npos) {
        // We can parse the virtual machine ID from the output

        uuid_location += 7;
        uuid_length = output.find(")", uuid_location);

        vm_name = output.substr(uuid_location, uuid_length);

        // Deregister stale VM by UUID
        return deregister_vm();
    } else {
        // Did the user delete the VM in VirtualBox and not the medium?  If so,
        // just remove the medium.
        command  = "closemedium \"" + virtual_machine_root_dir + "/" + image_filename + "\" ";

        retval = vbm_popen(command, output);
        if (retval) {
            fprintf(
                stderr,
                "%s Error removing virtual hdd from virtual box! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
                boinc_msg_prefix(buf, sizeof(buf)),
                retval,
                command.c_str(),
                output.c_str()
            );
            return retval;
        }
    }

    return 0;
}


int VBOX_VM::startvm() {
    string command;
    string output;
    char buf[256];
    int retval;

    command = "startvm \"" + vm_name + "\" --type headless";
    retval = vbm_popen(command, output);
    if (retval) {
        fprintf(
            stderr,
            "%s Error starting virtual machine! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            retval,
            command.c_str(),
            output.c_str()
        );
        return retval;
    }
    return 0;
}


int VBOX_VM::stop() {
    string command;
    string output;
    char buf[256];
    int retval;

    command = "controlvm \"" + vm_name + "\" savestate";
    retval = vbm_popen(command, output);
    if (retval) {
        fprintf(
            stderr,
            "%s Error stopping virtual machine! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            retval,
            command.c_str(),
            output.c_str()
        );
        return retval;
    }
    return 0;
}


int VBOX_VM::pause() {
    string command;
    string output;
    char buf[256];
    int retval;

    command = "controlvm \"" + vm_name + "\" pause";
    retval = vbm_popen(command, output);
    if (retval) {
        fprintf(
            stderr,
            "%s Error pausing virtual machine! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            retval,
            command.c_str(),
            output.c_str()
        );
        return retval;
    }
    suspended = true;
    return 0;
}


int VBOX_VM::resume() {
    string command;
    string output;
    char buf[256];
    int retval;

    command = "controlvm \"" + vm_name + "\" resume";
    retval = vbm_popen(command, output);
    if (retval) {
        fprintf(
            stderr,
            "%s Error resuming virtual machine! rc = 0x%x\nCommand:\n%s\nOutput:\n%s\n",
            boinc_msg_prefix(buf, sizeof(buf)),
            retval,
            command.c_str(),
            output.c_str()
        );
        return retval;
    }
    suspended = false;
    return 0;
}
