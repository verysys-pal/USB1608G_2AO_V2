TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
# Build the IOC application


PROD_IOC = USB1608G_2AO_V2
# USB1608G_2AO_V2.dbd will be created and installed
DBD += USB1608G_2AO_V2.dbd


# USB1608G_2AO_V2.dbd will be made up from these files:
USB1608G_2AO_V2_DBD += base.dbd
USB1608G_2AO_V2_DBD += measCompApp.dbd
USB1608G_2AO_V2_DBD += measCompSupport.dbd
USB1608G_2AO_V2_DBD += thresholdLogicSupport.dbd


# Include dbd files from all support applications:
#USB1608G_2AO_V2_DBD += xxx.dbd

# Add all the support libraries needed by this IOC
#USB1608G_2AO_V2_LIBS += xxx

# #USB1608G_2AO_V2_registerRecordDeviceDriver.cpp derives from #USB1608G_2AO_V2.dbd
USB1608G_2AO_V2_SRCS += USB1608G_2AO_V2_registerRecordDeviceDriver.cpp
USB1608G_2AO_V2_SRCS += drvMultiFunction.cpp
USB1608G_2AO_V2_SRCS += drvUSBCTR.cpp
USB1608G_2AO_V2_SRCS += measCompDiscover.cpp
USB1608G_2AO_V2_SRCS += ThresholdLogicController.cpp  # publishes threshold result via asyn
USB1608G_2AO_V2_SRCS += ErrorHandler.cpp
USB1608G_2AO_V2_SRCS += USBCTR_SNL.st


# Build the main IOC entry point on workstation OSs.
USB1608G_2AO_V2_SRCS_DEFAULT += USB1608G_2AO_V2Main.cpp
USB1608G_2AO_V2_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#USB1608G_2AO_V2_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
USB1608G_2AO_V2_LIBS += $(EPICS_BASE_IOC_LIBS)
USB1608G_2AO_V2_LIBS += measComp
USB1608G_2AO_V2_LIBS += scaler
USB1608G_2AO_V2_LIBS += busy
USB1608G_2AO_V2_LIBS += calc
USB1608G_2AO_V2_LIBS += mca
USB1608G_2AO_V2_LIBS += sscan
USB1608G_2AO_V2_LIBS += autosave
USB1608G_2AO_V2_LIBS += asyn
USB1608G_2AO_V2_LIBS += seq pv

USB1608G_2AO_V2_SYS_LIBS_Linux += uldaq
USB1608G_2AO_V2_SYS_LIBS_Linux += usb-1.0
#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
