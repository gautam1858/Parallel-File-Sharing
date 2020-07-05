# Parallel-File-Sharing

![parallel.png](parallel.png)
Parallel upload and download of file 

From the Server this platform allows parallel upload and download.
Epoll is used to allow Asynchronous IO and parallel uploads & downloads. 

Principles of Design :

1. Client regsiteds to start/ initiate the sharing.
2. Client establishes connectivity to speicif client after regsitration with other clients. 
3. The register gets updated everytime a new client registers or drops.
