#!/bin/sh

# to run 
# bash ./gen-traffic.sh

# generate traffic 
# file sizes : (20K, 200K, 1M, 5M, 10M, 20M)?

# local 45.56.85.133 port 49960 connected with 45.33.58.123 port 5001 (ct=3.23 ms)
# [ ID] Interval        Transfer    Bandwidth       Write/Err  Rtry     Cwnd/RTT        NetPwr

# [  3] 0.00-1.00 sec   126 MBytes  1.05 Gbits/sec  1006/0          0       56K/626 us  210636.47
# [  3] 1.00-2.00 sec   138 MBytes  1.15 Gbits/sec  1100/0        299      483K/3884 us  37121.32

USE_MPTCP="/home/vad/mptcp-tools/use_mptcp/use_mptcp.sh"
DO_STUFF=true
COUNT=0
T_BW=0;
T_DELAY=0;

F_SIZE[0]="200K"
F_SIZE[1]="1M"
F_SIZE[2]="5M"
F_SIZE[3]="3M"
F_SIZE[4]="500K"
F_SIZE[5]="2M"
F_SIZE[6]="100K"
F_SIZE[7]="10M"

F_ARR_SIZE=${#F_SIZE[@]} 

# save initial values of TX bytes for each interface  
ETH_0_TX=$(ip -s link ls eth0 | awk  {'print $1'} | cut -d$'\n' -f6)
ETH_1_TX=$(ip -s link ls eth1 | awk  {'print $1'} | cut -d$'\n' -f6)

TIME_START=$(date +%s)

# this function is called when Ctrl-C is sent
trap_ctrlc() 
{
    # change cond var
    DO_STUFF=false 
    if [ "$DO_STUFF" = false ]; then 
        echo "Ctrl-C caught...out from loop"
    fi
    
}

# initialise trap to call trap_ctrlc function
# when signal 2 (SIGINT) is received
trap "trap_ctrlc" 2


while [ "$DO_STUFF" = true ] && [ "$COUNT" -lt 5 ] 
do
    index=$(($RANDOM % $F_ARR_SIZE))
    echo "Size =  ${F_SIZE[$index]} "

    # -x CS -y C  # comma separate value reperesentation without additional info
    CMD="$USE_MPTCP iperf -c 13.0.0.2 -e -f m -n ${F_SIZE[$index]} -x CS -y C"

    # execute command and save output to var 
    # output like : 20211207111724.527,,,,,3,0.0-0.3,204800,6102207
    OUTPUT=$($CMD)

    # filter data 
    # | awk -F 'MBytes' {'print $2'} | awk {'print $1'}

    VAL_BW=$(echo $OUTPUT | cut -d',' -f9)      # get Goodput 
    VAL_DELAY=$(echo $OUTPUT | cut -d',' -f7)    
    VAL_DELAY=$(echo $VAL_DELAY | cut -d'-' -f2)  # get delay 

    # echo "${VAL_BW}"   # as array 
    echo "BW = $VAL_BW , DELAY = $VAL_DELAY"    
    
    

    # BW calc 
    COUNT=$(( $COUNT + 1 ))
    T_BW=$(echo "scale=3 ; $T_BW + $VAL_BW" | bc)
    T_DELAY=$(echo "scale=3 ; $T_DELAY + $VAL_DELAY" | bc)

done


######################## statistics  ######################## 

# avarage BW and delay 
AV_BW=$(echo "scale=0 ; $T_BW / $COUNT  " | bc) 
AV_DELAY=$(echo "scale=3 ; $T_DELAY / $COUNT" | bc)
echo "N = $COUNT, AvBW = $AV_BW, AvDelay = $AV_DELAY"

# TX bytes for each iface 
ETH_0_TX_F=$(ip -s link ls eth0 | awk  {'print $1'} | cut -d$'\n' -f6)
ETH_1_TX_F=$(ip -s link ls eth1 | awk  {'print $1'} | cut -d$'\n' -f6)

TIME_NOW=$(date +%s)
TIME_ALL=$(echo "$TIME_NOW - $TIME_START" | bc) 

 
ETH_0_TX=$(echo "scale=3 ; ($ETH_0_TX_F - $ETH_0_TX)/$TIME_ALL" | bc)
ETH_1_TX=$(echo "scale=3 ; ($ETH_1_TX_F - $ETH_1_TX)/$TIME_ALL" | bc)

echo "eth0 = $ETH_0_TX, eth1 = $ETH_1_TX, time = $TIME_ALL"

# OUTPUT : 
# N = 5, AvBW = 3700454, AvDelay = 14.840
# eth0 = 262628.373, eth1 = 231693.253, time = 75
