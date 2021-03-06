[Defines]
  DEFINE EARLYDEBUG_TYPE = DISABLED
  DEFINE DISPLAY_TYPE    = DISABLED

[LibraryClasses.common]
  RealTimeClockLib|EFIDroidPkg/Qcom/Library/RealTimeClockLib/RealTimeClockLib.inf
  PTableLib|EFIDroidPkg/Qcom/Library/PTableLib/PTableLib.inf
  FbConLib|EFIDroidPkg/Qcom/Library/FbConLib/FbConLib.inf
  DloadUtilLib|EFIDroidPkg/Qcom/Library/DloadUtilLib/DloadUtilLib.inf
  PlatformIdLib|EFIDroidPkg/Qcom/Library/PlatformIdLib/PlatformIdLib.inf

  QcomPlatformI2cQupLib|EFIDroidPkg/Qcom/Library/QcomPlatformI2cQupLibNull/QcomPlatformI2cQupLibNull.inf
  QcomPlatformMmcClockOverrideLib|EFIDroidPkg/Qcom/Library/QcomPlatformMmcClockOverrideLibNull/QcomPlatformMmcClockOverrideLibNull.inf
  QcomUsbPhyLib|EFIDroidPkg/Qcom/Library/QcomUsbPhyLib/QcomUsbPhyLib.inf

[LibraryClasses.common.DXE_DRIVER]
  ChipVendorDxeInitLib|EFIDroidPkg/Qcom/Library/ChipVendorDxeInitLib/ChipVendorDxeInitLib.inf

[Components.common]
  EFIDroidPkg/Qcom/Drivers/TimerDxe/TimerDxe.inf
  !if $(DISPLAY_TYPE) != DISABLED && $(EARLYDEBUG_TYPE) != DISPLAY
  EFIDroidPkg/Qcom/Drivers/GraphicsOutputDxe/GraphicsOutputDxe.inf
  !else
  EFIDroidPkg/Qcom/Drivers/EmuGraphicsOutputDxe/EmuGraphicsOutputDxe.inf
  !endif
