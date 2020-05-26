# kernel process manager
This driver code is from a security application I wrote that profiles process launches.
    
## Overview
This code:<br> 
    - Monitors for process launches from kernel.<br>
    - Queues new process information. <br><br>
        - Notifies the user mode client of launch by a shared kernel / user event.<br>
        - Waits on a processing complete event or timeout.<br>
        - Processes client's response via IOCTL.<br>
    - Only the Kernel Process Manager code is provided.

