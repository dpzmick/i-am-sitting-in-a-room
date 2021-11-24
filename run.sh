set -x
set -e

#in="/nas/data/music/mad-rush-sally-whitwell/01 - Glass, P - Glassworks - 1. Opening.flac"
#in="/nas/scratch/jetpacks_was_yes.wav"
in="/nas/scratch/the_walk.wav"
loops=1024

for i in $(seq 1 $loops)
do
    inpt="/nas/scratch/out_$((i - 1)).wav"
    if [ $i -eq 1 ]
    then
        inpt=$in
    fi

    ./room "$inpt" "/nas/scratch/out_$i.wav"&

    sleep 2

    jack_connect room:out1 system:playback_12
    jack_connect room:out2 system:playback_13
    jack_connect room:in1 system:capture_12
    jack_connect room:in2 system:capture_13

    wait
done
