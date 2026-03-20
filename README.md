# TCP_ChatApp
Chat Application for communicating with TCP programmed in C.
This program allows for hosting a chat server, and allowing up to 4 concurrent connections to the server. 
Each client connected can message each other by getting the current connected users, and sending them a personal message, which is recorded by the server.
Also the user has an option to broadcast a message to the whole server, which will send the message to all connected clients.
<h3>Instructions</h3>
<b>Before beginning, compile the program and install all library prerequisites if prompted</b>

1. To start the server, open a terminal and run the program as shown below:
<img src=images/2_startup.png></img>
2. To start the client, open another terminal and run the program as shown below:
<img src=images/1_startup.png></img>
3. To get the IP of the server, go to the terminal which you ran to start the server and type the following:
<img src=images/3_getIP.png></img>
4. To get the PORT of the server or the client, type the following command:                                   
<img src=images/4_getPort.png></img>
5. Now that you have the server hosted, and you know the IP and PORT of the server via steps 3 and 4, we can login.
To login, switch to the client terminal, and type in:
LOGIN <IP_OF_SERVER> <PORT_OF_SERVER>
6. If you type in an invalid IP address or PORT when attempting to log in, you will receive this message:
<img src=images/6_badLoginPort.png></img>
7. Once you are logged in, you can see the currently connected users via the LIST command:
<img src=images/5_getList.png></img>
8. To refresh the list of logged in users, use the REFRESH command:
<img src=images/7_refreshList.png></img>
9. To send a message to another user, first get their IP from the LIST command, then use the SEND command as shown below:
<img src=images/9_messageRecorded.png></img>
10. To broadcast a message to every user connected, use the BROADCAST command as shown below:
<img src=images/10_broadcast.png></img>
11. To logout as a client from the server, use the LOGOUT command as shown below:
<img src=images/11_logout.png></img>
12. You must be logged in to logout, as shown below:
<img src=images/12_logoutexception.png></img>
13. Finally to close down the client, use the EXIT command:
<img src=images/8_exit.png></img>



 

