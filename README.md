# Electronic Voting
Authors: Keven DeOliveira, Ayrton Reulet, Brian Jung

Date: 2022-04-05
-----

## Summary

In this Electronic Voting quest, our goal was to create an electronic voting network where all votes would be securely accounted for and then sent to the main voting database to be tallied. All of the votes and who they were from were to be saved and displayed from the database to an HTML server where all the results of the past or current election could be cleared from the database with the press of a button (on the HTML frontend). 

To do this we used a combination of the knowledge we learned from IR RX/TX and Leader Election skills. We had to take the secure IR connection from the IR RX/TX skill and incorporate that as individual votes in an election. We made sure that any esp32 could vote to any one of the other five connected esp32s in the LAN including the current leader. Specifics of how this was done can be found in the README file. 

When an esp32 would cast a vote to a receiving esp32 the onboard LED would flash to indicate it had casted a vote. The receiving esp32 would also blink the onboard LED to indicate that it had received a vote. Once any esp32 had received a vote their job was to send that vote over UDP to the current leader. When the current leader received a vote its job was to forward that vote also through UDP to the raspberry pi server. It should be noted that the UDP message sent to the leader also carried the voter ID so that the database could store who was casting the vote. The leader also had to peridoically send out a heartbeat message so that all the esp32s were aware of who the leader is and that the current leader is still online. That way if the leader was disconnected either through power or off the network the first esp32 that noticed the lack of a heartbeat message would become the new leader and immediately send out a heartbeat pulse. Once the raspberry pi server had received the UDP message from the leader the information was stored in the database and updated to the HTML interface to display a vote had been casted along with who casted the vote color that was voted for.

To understand the code in more detail, please go to [CODE README](https://github.com/BU-EC444/Team13-DeOliveira-Jung-Reulet/blob/master/quest-4/code/README.md) in the code folder for more information. 


### Investigative Question
List 5 different ways that you can hack the system (including influencing the vote outcome or prevenging votes via denial of service). For each above, explain how you would mitigate these issues in your system.

There are some security flaws involed with any sort of networking and connected systems, electronic voting is no exception. Here are a list of some potential ways that our voting system could be hacked:

1.) VOTING FRAUD - someone could hack into an esp and change the ID number to pretent to be voting as someone else. This would be like if someone voted under someone else's name. Someone could also change the way the system work in which, they change the voterID that is recieved from the IR reciever and send to the server as if someone else voted. For our system, this will be hard because this would require reflashing the ESP with new code, which we will see if someone does. In terms of more complicated systems where we might not have easy access to the hardware, we can change to code so that the voterID is not a varaible we set but instead does an "OS.getID" sort of code to get the exact unique identifier of the ESP. On top of that another layer of security can be added by checking the IP of the ESP and cross referencing them to make sure that there is no fraud going on.

2.) ADDITIONAL VOTING - Another esp could be logged on to the LAN of our network and cast a vote from a new voting ID. Maybe a vote from an unknown voter would be counted in an electronic voting election. This could also be done by changing a esp's ID that is already on the LAN to a new ID not accounted for. This would be hard to identify once the data has already entered the database, especially since the totally tally of the candidates are done by just looking at all the entries with "R","G","B" vote. One way to address this in our system is to do a check before adding to the database to parse out anything that doesn't come from a ID we know and discard that vote.

3.) SECURITY BREACH - the database itself could be hacked into and artifically add votes for a specific color of the hackers choice; or for the same result votes already casted could be deleted from the database of certain colors that the hacker doesn't like. This would be a problem because this would require figuring out that the database itself has been breached. This can be mitigated by decoupling and having a log file that is secure and in intervals compare the results of the database and log file to make sure nothing is tampered with. We can also deploy a Web application firewall.

4.) NETWORK OFFLINE - one easy way to stop the entire election process would be to take the router offline. Taking the router offline would stop all communication between both server and the leader, and also the voters and the leader. This would be very hard to fix because the WIFI configuration for the pi and the ESP are made to that specific wifi. One way we could mitigate this problem is to have a backup way to connect to the wifi, as in configuring the pi and ESP to try to connect to another wifi. One other thing we wanted to list under NETWORK OFFLINE is DOS (Denial of Service) and one method is DDOS. We can limit the number of requests a server will accept over a certain time window, or have web application firewall.

5.) TAMPERING WITH EQUIPMENT - someone could tamper with the circuit of the fobs and maybe replace the IR with a white LED, broken IR, or even disconnect the transmission button. There are a whole lot of ways someone could change the circuit to not allow one or multiple of the esps to cast votes. There would also be no way for the leader or the server to know that the voting isn't working for certain esps. This physical "hacking" can be mitigated by ensuring that there is someone watching over the physical device and doing a routine check of the physical equipment.


## Solution Design

Our design does everything the quest asks for and more! The devices is only able to vote by sending an IR message to a receiving fob. The receiving fob has a UDP connection to the "poll leader", and sends the sender's voterID (unique for each voter) and vote (R,G,B) to the "poll leader". When the "poll leader" receives a UDP message (or recieves IR message while being a poll leader), they will package this data and send it to the server running (UDP connection). We used knowledge from prior clustor for the UDP, for the IR we used skill 25, and for the leader selection we used skill 28.

The server here is the backend javascript file. We use node.js packages to create a udp connection to the "poll leader" as well as create a database that it has constant connection to. We used tingoDB because it's light and we have experience with mongoDB. The backend opens this UDP connection, and also the database. On top of that it uses express to host a frontend (html) in a specified port. This express also allows the connection between the frontend and backend that powers the button as well as the vote information passed to the front end. By using express get functions, we were able to received a signal when the reset button was pressed. We also set up a function that runs continously every 1 second that does a GET request from the database to get the most up to date information from the database it self. Then this information was passed to the frontend through express as well. We wanted a nice intuitive UI/UX so we created a good looking front end website. (please take a look at the picture belwo and the video!) 
The server and web page was deployed on the Pi, but can be deployed on any device that can be connected to and host a web page on a specific port.

### How our solution went beyond in terms of quality.
First of all we put a lot more effort to making the voting seemless. Not only do we have (GPIO 13) built in LED flash whenever the vote is sent, we also have it flash when it is recieved. On top of that, to awknowledge the specfic color was voted on, the recieving fob also quickly flashes the led of the color of the vote! This way there are multiple ways of knowing that the vote was received, and the right vote was received!
We also wanted to show who the leader is, so added another functionality where the leader has the built in LED flash continously flash if they are the leader. On top of that we also had the fobs communicate to the server who the leader is, that was shown in the front end as well.
Finally, we added a lot of css features to make the website look really nice. The information printed for each fob doesn't include just their vote but also the time that vote was casted as well. Take a look at our frontend below in the photos!


## Sketches and Photos
<center> <img src="https://user-images.githubusercontent.com/65934595/162093197-9bc3da2d-c222-4bf6-b11c-ab3969a2653a.png"></img>
</center>  
<center> </center>


## Supporting Artifacts
- [Press here to watch the video demo!](https://drive.google.com/file/d/1_ilZExYNMJ1wD292D40Bi5A-SnDH0FJf/view)
- [Press here to go to the code README!](https://github.com/BU-EC444/Team13-DeOliveira-Jung-Reulet/blob/master/quest-4/code/README.md)

