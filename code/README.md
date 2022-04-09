# Code Readme

## Folder Structure
<pre>
code/
├── README.md <-- You're here
└── RaspiFullstack/
    ├── frontend.html
    ├── server.js
    └── voteDB
└── esp-code/
    └── ...
    └── main/
        └── e-voting.c
    └── ...
</pre>

## RaspiFullStack
A large part of the solution requires a server running on raspberry pi that is in the local network and listening to a UDP port to any incoming message. The leader is communciating to this specific ip and port. The server.js written in javascript, and utilizing node.js libaries it is able to open up to the specific port and listen for messages. It also sets up another port with the frontend html file and updates the frontend element values with the data that is being directed to a database.

### server.js 
Written in javascript, and utilizing node.js libaries, it first creates a new database utilizing tingoDB. We used tingoDB because it has the same syntax as mongoDB(something we have experience with). TingoDb creates a database file in the same directory (although you can set it to be anywhere), then we create a collection that will be used to store the data we receive from the leader ESP. Then it uses expresses packages to open up a port with the frontend.html (I will talk about later). Finally, using dgram packages, we listen to the port that the leader ESP is communicating the data to.
When data is recieved in the port, this backend script parses the important data and stores it in to tingoDB. Every second, the backend is also doing a collection.find to find the numbers of votes for each R,G,B candidates as well as looking at all voterIDs to find all votes (including time stamp, vote, and voterID) that matches the voterID.
All this data is then sent to the frontend using app.get. We package all the data in to one package but we keep in mind how the index of the data array matches to the data information.

### frontend.html
Used by server.js as the frontend, this file stores all the frontend related code. It has a function to communicate to the backend when a button is pressed in order to clear the database as well as a constant jqeury get to get the most updated data from the backend every second. It then sets the text of each corresponding element to the data it received, showing the interface updating real-time as votes as casted.
<b>Extra</b> effort was placed to make it look nice by implementing css with stylesheet widely avaliable through w3 css.

### voteDB
A file created by tingoDB that stores the query of the data. This can be removed and the code will run fine, there is another version of this file on our raspberry pi.

### Resources used
- jquery (https://code.jquery.com/jquery-1.11.1.js)
- w3 css (for front end)

## esp-code
The driver code and build for the fobs can be found within this directory. Most of the files and subfolders contained are necessary to build the project and can be ignored during code review. The main file is named "e-voting.c" and can be found under code/esp-code/main
    
### e-voting.c
This code is the same code used for *every* fob. This is possible due to a couple of hardcoded values such as unique IDs. This quest challenged us to combine our IR/RX skill and Leader Election skill with our past knowledge of UDP connections to create a system where data is sent from one fob to another via IR communication, confirmed by the receiver, and then forwarded to the "Leader" via UDP. The leader then sends the data (vote, ID) to the server to be shown on the webpage. The onboard LED (#13) is used to confirm when messages are successfully sent and received via IR, and the current leader can be distinguished by their constantly on onboard LED. In case of leader failure (power loss, wifi loss, etc.), leader election has been implemented using the Bully Algorithm dicussed in the skill cluster. The current leader periodically sends a heartbeat out to all fobs connected on the UDP port, and concurrently those fobs all listen for a heartbeat with a timeout. If a heartbeat is not received within that window, an election begins to elect a new leader and continue the process.
    
### Resources Used:
- IR/RX skill
- Leader Election Skill
- Past UDP skills
