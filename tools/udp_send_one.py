import socket
import sys

if len(sys.argv) == 3:
    # Get "IP address of Server" and also the "port number" from argument 1 and argument 2
    ip = sys.argv[1]
    port = int(sys.argv[2])
else:
    print("Run like : python3 client.py <arg1 server ip 192.168.1.102> <arg2 server port 4444 >")
    exit(1)

# Create socket for server
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
print("Do Ctrl+c to exit the program !!")

# Let's send data through UDP protocol
send_data = 'Feb 25 00:20:41 10.11.40.129 date=2020-02-25 time=00:20:44 devname="Bandirma_Uni_Forti" devid="FG1K5D3I16805260" logid="0000000013" type="traffic" subtype="forward" level="notice" vd="root" eventtime=1582579244 srcip=10.11.20.16 srcport=60693 srcintf="BMS" srcintfrole="lan" dstip=10.111.5.10 dstport=1433 dstintf="ServerINT" dstintfrole="lan" poluuid="97a5d6c4-394b-51e7-1774-aadb3d67ecce" sessionid=416374330 proto=6 action="close" policyid=103 policytype="policy" service="MS-SQL" dstcountry="Reserved" srccountry="Reserved" trandisp="noop" duration=2 sentbyte=1035 rcvdbyte=590 sentpkt=8 rcvdpkt=6 appcat="unscanned" dstdevtype="Router/AHM Device" dstdevcategory="None" masterdstmac="00:c8:8b:b7:48:3f"'

#send_data = 'Feb 25 00:20:40 10.11.40.129 date=2020-02-25 time=00:20:44 devname="Bandirma_Uni_Forti" devid="FG1K5D3I16805260" logid="0000000013" type="traffic" subtype="forward" level="notice" vd="root" eventtime=1582579244 srcip=10.11.20.16 srcport=60693 srcintf="BMS" srcintfrole="lan" dstip=10.111.5.10 dstport=1433 dstintf="ServerINT" dstintfrole="lan" poluuid="97a5d6c4-394b-51e7-1774-aadb3d67ecce" sessionid=416374330 proto=6 action="close" policyid=103 policytype="policy" service="MS-SQL" dstcountry="Reserved" srccountry="Reserved" trandisp="noop" duration=2 sentbyte=1035 rcvdbyte=590 sentpkt=8 rcvdpkt=6 appcat="unscanned" dstdevtype="Router/AHM Device" dstdevcategory="None" masterdstmac="00:c8:8b:b7:48:3f" srcmac="00:c8:8b:b7:48:3d" dstmac="00:c8:8b:b7:48:3f" dstserver=0'


#send_data = 'Feb 25 00:20:40 10.11.40.129 date=2020-02-25 time=00:20:44 devname="Bandirma_Uni_Forti" devid="FG1K5D3I16805260" devid="FG1K5D3I16805260" logid="0000000013" type="traffic" subtype="forward" level="notice" vd="root" eventtime=1582579244 srcip=10.11.20.16 srcport=60693 srcintf="BMS" srcintfrole="lan" dstip=10.111.5.10 dstport=1433 dstintf="ServerINT" dstintfrole="lan" poluuid="97a5d6c4-394b-51e7-1774-aadb3d67ecce" sessionid=416374330 proto=6 action="close" policyid=103 policytype="policy" service="MS-SQL" dstcountry="Reserved" srccountry="Reserved" trandisp="noop" duration=2 sentbyte=1035 rcvdbyte=590 sentpkt=8 rcvdpkt=6 appcat="unscanned" dstdevtype="Router/AHM Device" dstdevcategory="None" masterdstmac="00:c8:8b:b7:48:3f" srcmac="00:c8:8b:b7:48:3d" dstmac="00:c8 dstserver=0'
s.sendto(send_data.encode('utf-8'), (ip, port))

s.close()

