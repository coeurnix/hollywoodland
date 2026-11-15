#!/usr/bin/env bash
# The gameplay trailer's shots (1080p, 30 fps, in-engine): tools/capture/trailer_shots.sh [name ...]
# Each shot lands in hollywoodland-ue/Saved/Capture/t_<name>/; tools/capture/trailer_cut.py edits them to the theme song.
cd "$(dirname "$0")/../.."
S=tools/capture/shoot.sh
want() { [ $# -eq 0 ] || [[ " $ALL " == *" $1 "* ]]; }
ALL="$*"
shot() { local n=$1; shift; if [ -z "$ALL" ] || [[ " $ALL " == *" $n "* ]]; then bash $S "t_$n" "$@"; fi; }
# the creator under the sign at sunset, no interface (the opening and the end card)
shot intro 8 480 -HLAuto=nurse -HLCreateShot -HLAutoTitleShot -HLTitleWait=900 -HLSpin -HLHideUI -HLAutoQuit=900
# the boulevard at golden hour, a slow pan, no interface
shot blvd 5 200 -HLAuto=nurse -HLAutoLevel=10 -HLViews -HLViewSet=show -HLViewHold=12 -HLViewPan=5 -HLTime=18.3 -HLHideUI -HLAutoQuit=900
# characters: a new outfit and a gesture every 3 s, close up, no interface
shot gestures 4 720 -HLAuto=nurse -HLLineup -HLLineupGestures -HLHideUI -HLTime=16 -HLAutoQuit=900
# the landmarks, one every 2.5 s, no interface
shot landmarks 4 600 -HLAuto=veteran -HLViews -HLViewSet=landmarks -HLViewHold=2.5 -HLHideUI -HLTime=17 -HLAutoQuit=900
# the auction house, pages flipping (interface on)
shot auction 6 150 -HLAuto=nurse -HLAutoLevel=10 -HLViews -HLViewSet=show -HLViewHold=30 -HLOpenAuction -HLAuctionFlip -HLTime=15 -HLAutoQuit=900
# a hive surge on the forecourt, the town rallying (interface on)
shot surge 80 300 -HLAuto=veteran -HLAutoLevel=10 -HLEventsSoon -HLViews -HLViewSet=crowd -HLViewHold=600 -HLTime=17 -HLAutoQuit=900
# picture night: the forecourt crowd at dusk (interface on)
shot film 185 180 -HLAuto=nurse -HLAutoLevel=10 -HLEventsSoon -HLViews -HLViewSet=crowd -HLViewHold=600 -HLTime=19.25 -HLAutoQuit=900
# a gunman against an Invader Soldier (interface on)
shot combat 12 150 -HLAuto=gunman -HLAutoLevel=10 -HLGotoMob=mob_invader_soldier -HLCamOrbit=60,8,4.5 -HLTime=16.5 -HLAutoQuit=900
# a stuntman by lamplight (interface on)
shot night 12 150 -HLAuto=stuntman -HLAutoLevel=10 -HLTime=23 -HLGotoMob=mob_invader_spitter -HLCamOrbit=40,8,5 -HLAutoQuit=900
# the Egyptian Theatre, a party against the Brood Keeper (interface on)
shot dungeon 115 450 -HLAuto=veteran -HLAutoLevel=10 -HLAutoDungeon -HLCamOrbit=40,20,8.5 -HLAutoQuit=1200
# --- second pass: the player on the move (route mode), mounts, the red car, the windows
C="-HLParty -HLCrowd -HLAutoQuit=900"
shot run 8 200 -HLAuto=nurse -HLAutoLevel=10 "-HLRoute=120,203;270,205;330,203" $C -HLCamOrbit=160,8,7 -HLTime=17.5
shot horse 8 200 -HLAuto=gunman -HLAutoLevel=10 "-HLRoute=560,203;700,203;800,205" -HLMount=0 $C -HLCamOrbit=20,10,8 -HLTime=18.2
shot scout 8 150 -HLAuto=stuntman -HLAutoLevel=10 "-HLRoute=330,203;480,203;560,203" -HLMount=1 $C -HLCamOrbit=150,8,7 -HLTime=16.5
shot modela 8 150 -HLAuto=veteran -HLAutoLevel=10 "-HLRoute=800,203;640,203;560,203" -HLMount=2 $C -HLCamOrbit=30,10,10 -HLTime=18.6
shot redcar 6 240 -HLAuto=nurse -HLAutoLevel=10 "-HLRoute=240,205;268,204" -HLRide=2 $C -HLCamOrbit=35,10,11 -HLTime=17
shot gear 8 120 -HLAuto=veteran -HLAutoLevel=10 "-HLRoute=96,197" -HLCrowd -HLOpenBags -HLCamOrbit=180,8,6 -HLTime=17 -HLAutoQuit=900
shot talents 8 120 -HLAuto=nurse -HLAutoLevel=20 "-HLRoute=96,197" -HLCrowd -HLOpenTalents -HLCamOrbit=180,8,6 -HLTime=17 -HLAutoQuit=900
