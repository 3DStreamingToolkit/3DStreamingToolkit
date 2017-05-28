### Setup Azure NSeries Host VM Host/Server Setup

N-Series VM's are currently only avialable in South Central DataCenter with HDD VM Drives selector
N-Series VM is deployed with SSD

1. Use/Create "3DToolkitResourceGroup"
2. Create VM
  * Select Windows Server 2016 Datacenter
  * Select "Resource Manager" as the deployment model
3. VM Basic Settings
  * Name = N3DToolkitVM
  * VM Disk Type = HDD (Needed to enable N-Series Selection, servers are SSD equipped)
  * User name = operator
  * Password = *default team password*
  * Confirm Password =
  * Subsctription = *default team subscription*
  * Resource Group = Use Existing / 3DToolkitResourceGroup
  * Location = South Central US
4. VM Size
  * Select "View All" to show N-Series (GPU) options
  * Select the N Series Server that matches need (lowest tier is NV6)
5. VM Settings
  * Storage = Use/Create "3dtoolkitstorage"
  * Virtual Network = Use/Create "3dToolkitVNET"
  * Subnet = Use/Create  "3dToolkitSubnet" (10.10.10.0/24) - CIDR setup
  * Public IP = Create New
  * Network Security Group = Create or Use 3DToolkitVM-nsg (Configure RDP TCP/3389 and WebRTC ports)
  * Extensions = none
  * High Availabilty = none
  * Monitoring = enabled
  * Guest OS Diagnostics = disabled
  * Diagnostics storage account = Use/Create 3dtoolkitdata

### Configure Server with WebRTC Client

1. Prepare WebRTC Binaries
* ZIP distribution files
* Place ZIP Archive Local File

2. Remote Desktop to Server using VM Connection Settings or Direct using Public IP listed on VM
* Update/Edit Remote Desktop Local Resources Settings
* Click "More" button on "Local devices and resources" on lowest section of tab
* Expand the "Drives" List and checkbox the Drive that contains the ZIP distribution files

3. Connect to VM using Remote Desktop
* Login using the Administrator Credentials defined during VM creation
* Create "C:\CONTENT", convention is to use this folder for all app installations
* Create "C:\CONTENT\WebRTC" folder
* Using File Explorer, copy the Zip Archive File to "C:\CONTENT\WebRTC", the local computer drive is accessbile from the VM
* Extract the file contents

4. Setup WebRTC peerconnection_server.exe
* Locate the "peerconnection_server.exe" file
* Open a Command Window in Admin Mode
* Go to the directory of exe file
* Run "peerconnection_server.exe",  note the TCP port it is using

5. Open Windows Firewall for Server App
* Select Inboud Rules
* Create Entry for TCP Port Number used by "peerconnection_server.exe"

### Configure Azure VM Settings for WebRTC Port

1. Select VM
2. On the Settings Section, Click "Network Interfaces"
3. Click on the listed NSG (network security group) interface
4. On the Network Security Group Settings Entry, Click "Inbound Security Rules"
5. Add and Inbound Security Rule
* Name = WebRTC-peerconnection-server
* Priority = *use default generated value*
* Source = Any
* Service = Custom
* Protocol = TCP
* Port Range = *use peerconnection_exe port*
* Action = Allow




  
  
  
  
  
  
  
  
  
  
