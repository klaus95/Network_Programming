#This script intentially contains an errorous command
./runClient.sh 12346 "127.0.0.1" "Initial message" << END
Did the first server got the initial message?
Looks like he did :)
PEER:Spy Server:PORT=12345:IP=127.0.0.1%
PEERS?
Ohh! Hi there new server.
Adding some peers...
PEER:Klaus:PORT=12341:IP=127.0.0.1%
PEER:Peter:PORT=12342:IP=127.0.0.1%
PEER:Peter:PRT=12342:IP=127.0.0.1%
Opps! That was my bad.
Lets see how many peers we have now...
PEERS?
Bye Bye
END
