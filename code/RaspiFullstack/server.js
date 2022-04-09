var Db = require('tingodb')().Db,
	assert = require('assert');
const fs = require('fs')
var app = require('express')();
const path = require("path");
var dgram = require("dgram");

//***************************************************/
//The variables used:
var buttonToggle;
var msgResponse
data = [0,0,0];
candidates = ["R","G","Y"];
count = 1;

// Setting up server
var PORT = 3000;
var HOST = '192.168.1.112';
// var HOST = '155.41.50.158';
var server = dgram.createSocket('udp4');

//Create a database for the votes
var db = new Db('.', {});
// Fetch a collection to insert document into
var collection = db.collection("voteDB");


app.get('/', (req,res) => {
    res.sendFile(__dirname + '/frontend.html')
})

app.get('/api', async (req, res) => {
    console.log(req.query.q);
    //function that clears the entire database
    collection.remove({},function(err){
        if (err) throw err;
    })
    //resolve front end
    res.send("ok");
});


//UDP stuff
server.on('listening', function () {
    var address = server.address();
    console.log('UDP Server listening on ' + address.address + ":" + address.port);
});

server.on('message', function (message, remote) {
    console.log(remote.address + ':' + remote.port +' - ' + message);

    today = new Date();
    curtime = today.getHours() + ":" + today.getMinutes() + ":" + today.getSeconds();
    
    newMessage = message.toString('utf-8');
    senderID = hexToDec(newMessage.substring(2,3));
    senderVote = hex2a(newMessage.substring(3,5));

    collection.insert({_id:count,time:curtime,voterID:senderID,vote:senderVote});

    count += 1;
});

server.bind(PORT, HOST);

setInterval(function(){
    highestToLowest = [];
    //Who is winning
    collection.find({vote:"R"}).toArray(function(err, item) {
        assert.equal(null, err);
        data[0] = item.length
    })
    collection.find({vote:"G"}).toArray(function(err, item) {
        assert.equal(null, err);
        data[1] = item.length
    })
    collection.find({vote:"Y"}).toArray(function(err, item) {
        assert.equal(null, err);
        data[2] = item.length
    })

    collection.find({voterID:1}).toArray(function(err, item) {
        assert.equal(null, err);
        data[3] = JSON.stringify(item)
    })
    collection.find({voterID:2}).toArray(function(err, item) {
        assert.equal(null, err);
        data[4] = JSON.stringify(item)
    })
    collection.find({voterID:3}).toArray(function(err, item) {
        assert.equal(null, err);
        data[5] = JSON.stringify(item)
    })
    collection.find({voterID:4}).toArray(function(err, item) {
        assert.equal(null, err);
        data[6] = JSON.stringify(item)
    })
    collection.find({voterID:5}).toArray(function(err, item) {
        assert.equal(null, err);
        data[7] = JSON.stringify(item)
    })
    collection.find({voterID:6}).toArray(function(err, item) {
        assert.equal(null, err);
        data[8] = JSON.stringify(item)
    })

    app.get('/data', (req,res) => {
        app.emit('dataEvent');
        res.send(data)
    });

}, 1000);

app.listen(8800, () => {
    console.log("go to http://localhost:8800/");
});


function hex2a(hexx) {
    var hex = hexx.toString();//force conversion
    var str = '';
    for (var i = 0; i < hex.length; i += 2)
        str += String.fromCharCode(parseInt(hex.substr(i, 2), 16));
    return str;
}

function hexToDec(hexString){
    return parseInt(hexString, 16);
  }