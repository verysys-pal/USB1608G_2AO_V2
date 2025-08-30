# 0 "../USBCTR_SNL.st"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "../USBCTR_SNL.st"
program USBCTR_SNL("P=USBCTR:MCS:, R=mca, NUM_COUNTERS=8,  FIELD=READ")
# 11 "../USBCTR_SNL.st"
option +r;



option -c;

%%#include <stdlib.h>
%%#include <errlog.h>
%%#include <string.h>




int i;
int n;
int numCounters;
char temp[100];

char *prefix;
char *record;
char *field;

int ReadArray[9];
assign ReadArray to {};
int ReadArrays; assign ReadArrays to "{P}DoReadAll";
monitor ReadArrays; evflag ReadArraysMon; sync ReadArrays ReadArraysMon;

int HardwareAcquiring; assign HardwareAcquiring to "{P}HardwareAcquiring";
monitor HardwareAcquiring; evflag HardwareAcquiringMon; sync HardwareAcquiring HardwareAcquiringMon;

int MCSAbsTimeWF; assign MCSAbsTimeWF to "{P}AbsTimeWF.PROC";

int SNL_Connected; assign SNL_Connected to "{P}SNL_Connected";
int AsynDebug; assign AsynDebug to "{P}Asyn.TB1";
monitor AsynDebug;

int Acquiring; assign Acquiring to "{P}Acquiring";

ss mca_control {
  state init {
    when() {
      prefix = macValueGet("P");
      record = macValueGet("R");
      field = macValueGet("FIELD");
      numCounters = atoi(macValueGet("NUM_COUNTERS"));
      if ((numCounters <= 0) || (numCounters > 9)) {
        printf ("NUM_COUNTERS is illegal.\n");
        numCounters = 0;
      }
      for (i=0; i<numCounters; i++) {
        n = i+1;
        sprintf(temp, "%s%s%d.%s", prefix, record, n, field);
        pvAssign(ReadArray[i], temp);
       }
    } state waitConnected
  }

  state waitConnected {
    when (numCounters <= 0) {
      printf (">>>>>>>>>>>>>> USBCTR is dead. <<<<<<<<<<<<<<\n");
    } state dead

    when (pvAssignCount () == pvConnectCount ()) {
      printf ("USBCTR: All channels connected.\n");
      SNL_Connected = 1;
      pvPut(SNL_Connected);
    } state monitor_changes
  }

  state dead {
    when (delay (3600.0)) {
    } state dead
  }

  state monitor_changes {
    when (pvAssignCount () != pvConnectCount ()) {
      printf ("USBCTR: Not all channels connected.\n");
    } state waitConnected

    when(efTestAndClear(ReadArraysMon) && (ReadArrays == 1)) {
      if (AsynDebug) printf("USBCTR.st: Read array data\n");
      for (i=0; i<numCounters; i++) {
        ReadArray[i] = 1;
        pvPut(ReadArray[i]);
      }
      MCSAbsTimeWF = 1;
      pvPut(MCSAbsTimeWF);
      ReadArrays = 0;
      pvPut(ReadArrays);
    } state monitor_changes

    when(efTestAndClear(HardwareAcquiringMon)) {
      if (AsynDebug) printf("USBCTR.st: HardwareAcquiringMon, HardwareAcquiring=%d\n", HardwareAcquiring);

      if (!HardwareAcquiring) {

        for (i=0; i<numCounters; i++) {
          ReadArray[i] = 1;
          pvPut(ReadArray[i], SYNC);
        }
        MCSAbsTimeWF = 1;
        pvPut(MCSAbsTimeWF);

        Acquiring = 0;
        pvPut(Acquiring);
      }
    } state monitor_changes
  }
}
