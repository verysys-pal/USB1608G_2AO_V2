#!../../bin/linux-x86_64/USB1608G_2AO_V2
< envPaths

## Register all support components
dbLoadDatabase("$(TOP)/dbd/USB1608G_2AO_V2.dbd")
USB1608G_2AO_V2_registerRecordDeviceDriver(pdbbase)

## Environment variable setup
epicsEnvSet("PREFIX", "USB1608G_2AO:")
epicsEnvSet("PORT", "USB1608G_2AO_PORT")
epicsEnvSet("WDIG_POINTS", "1048576")
epicsEnvSet("WGEN_POINTS", "1048576")
epicsEnvSet("UNIQUE_ID", "01D97CFA")

## Configure port driver
MultiFunctionConfig("$(PORT)", "$(UNIQUE_ID)", $(WDIG_POINTS), $(WGEN_POINTS))

## Configure threshold logic controller
epicsEnvSet("THRESHOLD_PORT", "THRESHOLD_LOGIC_PORT")
ThresholdLogicConfig("$(THRESHOLD_PORT)", "$(PORT)", 0)

## Load database templates
dbLoadTemplate("$(TOP)/USB1608G_2AO_V2App/Db/USB1608G_2AO.substitutions", "P=$(PREFIX),PORT=$(PORT),WDIG_POINTS=$(WDIG_POINTS),WGEN_POINTS=$(WGEN_POINTS)")

## Load save/restore configuration
< save_restore.cmd

## Initialize IOC
iocInit

## Setup autosave monitoring
create_monitor_set("auto_settings.req",30,"P=$(PREFIX)")

