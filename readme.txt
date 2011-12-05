CS 15-441 Computer Networks
Project: Congestion Control with BitTorrent
File: readme.txt

Author: Hong Jai Cho <hongjaic@andrew.cmu.edu>, Raul Gonzalez <rggonzal>

----------------------------------------------------------------------------------
CP1
----------------------------------------------------------------------------------

Stop and wait protocol.

----------------------------------------------------------------------------------



----------------------------------------------------------------------------------
CP2
----------------------------------------------------------------------------------

In this project, we implemented a BitTorrentish file transfer protocol buildind
TCP congestion control protocal on top of UDP. The major part of this project
was to implement the sliding window protocol.

To implement the sliding window protocol, we used a queue (linked list), and we
applied the following algorithms:

1) The sending sliding window can grow only upto the size of congestion window
size. There are cases though in which the sliding window could fall outside the
congestion window size (in the cases of dupacks for instnce). In such case,
timeout is used to do necessary retransmissions. The elements in the sendind
window are removed up to and not including the final ack received.

2) As soon as triple dupacks sequence is detected, we retransmit.

3) The receiving sliding window can grow indefinitely. This data structure is
used to build ack packets. Ack number is chosen such that it is the largest
number of a consecutive sequcne. When an ack is found, the entries upto the ack
found are removed.

Notes:
1) We keep our nodes.map file in the local tmp folder. (.tmp/nodes.map)
2) We keep our topo.map file in the local tmp fold. (.tmp/topo.map)
3) We assume that all tests are done locally (virtual internet environment),
and we exploit this. When the file is not in the tmp folder, we predetermine
that the file does not in the system overall and notify the user.


----------------------------------------------------------------------------------
