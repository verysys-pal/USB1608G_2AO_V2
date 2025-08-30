#!/bin/bash

# ========== Logging ==========
LOG_DIR="/root/log"
script_name="${0##*/}";
script_name="${script_name%.*}"
LOG_FILE="$LOG_DIR/$script_name.log"

APPNAME="USB1608G_2AO_V2"

init_log() {
    mkdir -p "$LOG_DIR"
    {
        echo "========================================="
        echo " Log File: $LOG_FILE"
        echo " Created: $(date '+%Y-%m-%d %H:%M:%S')"
        echo " User: $(whoami)"
        echo " Host: $(hostname)"
        echo "========================================="
    } > "$LOG_FILE"
}


export EPICS_SITE=${EPICS_PATH}/siteApp
# ========== Functions ==========
# /usr/local/epics/EPICS_R7.0/siteApp/USB1608G/USB1608GApp/op/USB1608G.adl

USB1608G_2AO_exec() {
    local OPDIR=${EPICS_SITE}/${APPNAME}/${APPNAME}App/op

    cd ${OPDIR}
    # args="P=USB1608G_2AO:,Bi=Bi,Li=Li,Bo=Bo,Lo=Lo,Bd=Bd,Ai=Ai,Ao=Ao,Ct=Counter,Wd=WaveDig,Wg=WaveGen,Pg=PulseGen,Tg=Trig"

    # Build macro string
    local MACRO_STR="P=USB1608G_2AO:,Bi=Bi,Li=Li,Bo=Bo,Lo=Lo,Bd=Bd,Ai=Ai,Ao=Ao,Ct=Counter,Wd=WaveDig,Wg=WaveGen,Pg=PulseGen,Tg=Trig"    

    # Call medm with explicit options, properly quoted. / 명시적 옵션과 올바른 따옴표
    medm -x -macro "$MACRO_STR" -displayFont fixed "${APPNAME}.adl" &
    echo " - cd ${OPDIR}"
}

USB1608G_2AO_edit() {
    local OPDIR=${EPICS_SITE}/${APPNAME}/${APPNAME}App/op

    cd ${OPDIR}
    # args="P=USB1608G_2AO:,Bi=Bi,Li=Li,Bo=Bo,Lo=Lo,Bd=Bd,Ai=Ai,Ao=Ao,Ct=Counter,Wd=WaveDig,Wg=WaveGen,Pg=PulseGen,Tg=Trig"
    export MEDM_MACRO="-macro P=USB1608G_2AO:,Bi=Bi,Li=Li,Bo=Bo,Lo=Lo,Bd=Bd,Ai=Ai,Ao=Ao,Ct=Counter,Wd=WaveDig,Wg=WaveGen,Pg=PulseGen,Tg=Trig"
    export MEDM_FONT='-displayFont fixed'
    medm $MEDM_MACRO $MEDM_FONT ${APPNAME}.adl &
    echo " - cd ${OPDIR}"
}


measCompDigitalIO8_exec() {
    local OPDIR=${EPICS_SITE}/${APPNAME}/${APPNAME}App/op

    cd ${OPDIR}    
    export MEDM_MACRO="-macro P=USB1608G_2AO:,Bi=Bi,Li=Li,Bo=Bo,Lo=Lo,Bd=Bd"
    export MEDM_FONT='-displayFont fixed'
    medm -x $MEDM_MACRO $MEDM_FONT measCompDigitalIO8.adl &
    echo " - cd ${OPDIR}"
}

measCompDigitalIO8_edit() {
    local OPDIR=${EPICS_SITE}/${APPNAME}/${APPNAME}App/op

    cd ${OPDIR}    
    export MEDM_MACRO="-macro P=USB1608G_2AO:,Bi=Bi,Li=Li,Bo=Bo,Lo=Lo,Bd=Bd"
    export MEDM_FONT='-displayFont fixed'
    medm $MEDM_MACRO $MEDM_FONT measCompDigitalIO8.adl &
    echo " - cd ${OPDIR}"
}






# ========== Common function ==========
kill_medm() {
    echo "[INFO] Checking for existing medm processes..."

    local medm_pids
    medm_pids=$(pgrep -x medm)

    if [[ -z "$medm_pids" ]]; then
        printf "%7s%s\n" "" "- No medm process found. Nothing to kill."
    else
        printf "%7s%s\n" "" "- Found medm process(es): $medm_pids"
        printf "%7s%s\n" "" "- Killing medm process(es)..."
        kill $medm_pids
        sleep 0.5

        if pgrep -x medm >/dev/null; then
            printf "%7s%s\n" "" "- [WARN] Some medm process(es) may still be running."
        else
            printf "%7s%s\n" "" "- All medm process(es) terminated successfully."
        fi
    fi
}













# ========== Main ==========
handle_selection() {
	case "$1" in
			1) USB1608G_2AO_exec ;;
			2) USB1608G_2AO_edit ;;
            3) measCompDigitalIO8_exec ;;
            4) measCompDigitalIO8_edit ;;
			0)
					echo "Exit the script"
					return 0  # << exit 대신 return
					;;
			*)
					echo ""
					echo "You have entered '${1}'"
					echo "Please select the number in the list..."
					echo "Exit the script"
					echo ""
					return 1
					;;
	esac

	echo -e "\n========================================"
	echo "[DONE] All tasks completed"
}


main() {
    #init_log
    #printf '\n%.0s' {1..3}
    kill_medm

    echo ""
    echo "Enter the number of you want to script"
    echo "1 : USB1608G_2AO_exec (with ThresholdLogic)"
    echo "2 : USB1608G_2AO_edit (with ThresholdLogic)"
    echo ""
    echo "3 : measCompDigitalIO8_exec"
    echo "4 : measCompDigitalIO8_edit"
    echo ""
    echo "0 : Exit script"
    echo -n "Enter the number : "
    read answer

    handle_selection "$answer"
    if [[ $? -ne 0 ]]; then
        exit 1
    fi
}
main

