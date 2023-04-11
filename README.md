# EE450 Course Projects

a. Name: Yuhua Wu
b. Student ID: 8513508483
c. I have completed all the operations mentioned in the document(including optional part):
   1. CHECK BALANCE: Check Wallet of a centain user
   2. TXCOINS： make transactions between users in the network
   3. TXLIST: get list of all transactions in order
   4. STATS: get statistical result with a summary of all the transactions and provides a list of all users the client has made transactions with a certain user
d. There are 6 source code files:
   1. serverM.c: the main server get the command from the clientA/clientB, then it sends request to the serverA, serverB and serverC, after getting response from the serverA, serverB and serverC, it deal with the response and then send the result to clientA/clientB.
   2. serverA.c: serverA has a log file "block1.txt" which stores part of the transactions, it send the information about the transactions to the main server when requested.
   3. serverB.c: serverB has a log file "block2.txt" which stores part of the transactions, it send the information about the transactions to the main server when requested.
   4. serverC.c: serverC has a log file "block3.txt" which stores part of the transactions, it send the information about the transactions to the main server when requested.
   5. clientA.c: clientA sent command(CHECK BALANCE, TXCOINS, TXLIST and STATS) to the main server and output the results.
   6. clientB.c: clientB sent command(CHECK BALANCE, TXCOINS, TXLIST and STATS) to the main server and output the results.
   Type `make` to compile all the files
   NOTICE: Please put the log files "block1.txt", "block2.txt" and "block3.txt" in the folder before starting the servers.
e. On Screen Messages Examples
   use following command to start 4 servers:
   ./serverM
   ./serverA
   ./serverB
   ./serverC
 1. CHECK BALANCE:
      client side command: ./clientA Alice

      Terminal at serverA:
      The ServerA is up and running using UDP on port 21483
      The ServerA received a request from the Main Server
      The ServerA finished sending the response to the Main Server

      Terminal at serverB:
      The ServerB is up and running using UDP on port 22483
      The ServerB received a request from the Main Server
      The ServerB finished sending the response to the Main Server

      Terminal at serverC:
      The ServerC is up and running using UDP on port 23483
      The ServerC received a request from the Main Server
      The ServerC finished sending the response to the Main Server

      Terminal at serverM:
      The main server is up and running
      The main server received input ="Alice" from the client using TCP over port 25483
      The main server sent a request to Server A
      The main server received transactions from Server A using UDP over port 21483
      The main server sent a request to Server B
      The main server received transactions from Server B using UDP over port 22483
      The main server sent a request to Server C
      The main server received transactions from Server C using UDP over port 23483
      The main server sent the current balance to client A

      Terminal at clientA (if Alice in network):
      The clientA is up and runing
      Alice sent a balance enquiry request to the main server
      The current balance of "Alice" is: 1079 alicoins
      
      Terminal at clientA (if Alice out of network):
      The clientA is up and runing
      Alice sent a balance enquiry request to the main server
      "Alice" is not part of the network
  
   2. TXCOINS (assume Alice is in network):
      client side command: ./clientB Alice Ali 100

      Terminal at serverA:
      The ServerA is up and running using UDP on port 21483
      The ServerA received a request from the Main Server
      The ServerA finished sending the response to the Main Server

      Terminal at serverB (if transaction succeed):
      The ServerB is up and running using UDP on port 22483
      The ServerB received a request from the Main Server
      The ServerB finished sending the response to the Main Server
      The ServerB received a request from the Main Server
      The ServerB finished sending the response to the Main Server
      (serverB recieved request twice because serverM randomly choose serverB to record this transcation)

      Terminal at serverB(if transaction failed):
      The ServerB is up and running using UDP on port 22483
      The ServerB received a request from the Main Server
      The ServerB finished sending the response to the Main Server

      Terminal at serverC:
      The ServerC is up and running using UDP on port 23483
      The ServerC received a request from the Main Server
      The ServerC finished sending the response to the Main Server

      Terminal at serverM:
      The main server is up and running
      The main server received from Alice to transfer 100 coins to Ali using TCP over port 26483
      The main server sent a request to Server A
      The main server received the feedback from Server A using UDP over port 21483
      The main server sent a request to Server B
      The main server received the feedback from Server B using UDP over port 22483
      The main server sent a request to Server C
      The main server received the feedback from Server C using UDP over port 23483
      The main server sent the result of transaction to client B

      Terminal at clientB (if succeed):
      The clientB is up and runing
      Alice has requested to transfer 100 coins to Ali
      Alice successfully transfered 100 alcoins to Ali
      The current balance of "Alice" is: 979 alicoins

      Terminal at clientB (if failed due to insufficient balance):
      The clientB is up and runing
      Alice has requested to transfer 100 coins to Ali
      Alice was unable to transfer 100 alcoins to Ali because of insufficient balance
      The current balance of "Alice" is: 979 alicoins

      Terminal at clientB (if failed because Alice ouf of network):
      The clientB is up and runing
      Alice has requested to transfer 100 coins to Ali
      Unable to proceed with transaction as Alice is not part of the network

      Terminal at clientB (if failed because Ali ouf of network):
      The clientB is up and runing
      Alice has requested to transfer 100 coins to Ali
      Unable to proceed with transaction as Ali is not part of the network

      Terminal at clientB (if failed because both ouf of network):
      The clientB is up and runing
      Alice has requested to transfer 100 coins to Ali
      Unable to proceed with transaction as Alice and Ali are not part of the network

   3. TXLIST:
      client side command: ./clientA TXLIST

      Terminal at serverA:
      The ServerA is up and running using UDP on port 21483

      Terminal at serverB:
      The ServerB is up and running using UDP on port 22483

      Terminal at serverC:
      The ServerC is up and running using UDP on port 23483

      Terminal at serverM:
      The main server is up and running
      A TXLIST request has been received
      The sorted file is up and ready

      Terminal at clientA:
      The clientA is up and runing
      clientA sent a sorted list request to the main server

   4. STATS:
      client side command: ./clientA Ali stats

      Terminal at serverA:
      The ServerA is up and running using UDP on port 21483

      Terminal at serverB:
      The ServerB is up and running using UDP on port 22483

      Terminal at serverC:
      The ServerC is up and running using UDP on port 23483

      Terminal at serverM:
      The main server is up and running
      A STATS request has been received

      Terminal at clientB (if Ali is in network):
      The clientB is up and runing
      Ali sent a statistics enquiry request to the main server
      Ali statistics enquiry are the following.:
      Rank--Username--NumofTransactions--Total
      1     Luke              9           192         
      2     Racheal           8           8           
      3     Alice             2           -200        
      4     Chinmay           1           100         
      5     Rishil            1           -100  

      Terminal at clientB (if Ali is out of network):
      The clientB is up and runing
      Ali sent a statistics enquiry request to the main server
      Ali is not part of the network


h. Reused code:
   I used codes from Beej's Guide.
      




   

