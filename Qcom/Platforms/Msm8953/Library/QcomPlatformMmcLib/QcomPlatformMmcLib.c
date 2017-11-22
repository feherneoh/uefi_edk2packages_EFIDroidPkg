#include <Base.h>
#include <Library/LKEnvLib.h>
#include <Library/QcomClockLib.h>
#include <Library/QcomPlatformMmcLib.h>
//#include <Library/PlatformIdLib.h>
#include <Platform/iomap.h>
#include <Chipset/mmc_sdhci.h>

VOID LibQcomPlatformMmcClockInit(UINT32 interface)
{
  CHAR8 clk_name[64];
  INTN ret;

  snprintf(clk_name, sizeof(clk_name), "sdc%u_iface_clk", interface);

  /* enable interface clock */
  ret = gClock->clk_get_set_enable(clk_name, 0, 1);
  if(ret)
  {
    DEBUG((EFI_D_ERROR, "failed to set sdc%u_iface_clk ret = %d\n", interface, ret));
    ASSERT(0);
  }
}

VOID LibQcomPlatformMmcClockConfig(UINT32 interface, UINT32 freq)
{
  INTN ret = 0;
  CHAR8 clk_name[64];

  snprintf(clk_name, sizeof(clk_name), "sdc%u_core_clk", interface);

  if(freq == MMC_CLK_400KHZ)
  {
    ret = gClock->clk_get_set_enable(clk_name, 400000, 1);
  }
  else if(freq == MMC_CLK_50MHZ)
  {
    ret = gClock->clk_get_set_enable(clk_name, 50000000, 1);
  }
  else if(freq == MMC_CLK_177MHZ)
  {
    ret = gClock->clk_get_set_enable(clk_name, 177770000, 1);
  }
  else if(freq == MMC_CLK_192MHZ)
  {
    ret = gClock->clk_get_set_enable(clk_name, 192000000, 1);
  }
  else if(freq == MMC_CLK_200MHZ)
  {
    ret = gClock->clk_get_set_enable(clk_name, 200000000, 1);
  }
  else if(freq == MMC_CLK_400MHZ)
  {
    ret = gClock->clk_get_set_enable(clk_name, 384000000, 1);
  }
  else
  {
    DEBUG((EFI_D_ERROR, "sdc frequency (%u) is not supported\n", freq));
    //ret = gClock->clk_get_set_enable(clk_name, freq, 1);
    ASSERT(0);
  }


  if(ret)
  {
    DEBUG((EFI_D_ERROR, "failed to set sdc%u_core_clk ret = %d\n", interface, ret));
    ASSERT(0);
  }
}

VOID LibQcomPlatformMmcClockConfigCdc(UINT32 interface)
{

}
