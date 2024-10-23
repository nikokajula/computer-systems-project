#
_XDCBUILDCOUNT = 
ifneq (,$(findstring path,$(_USEXDCENV_)))
override XDCPATH = C:/ti/ccstheia151/tirtos_cc13xx_cc26xx_2_21_00_06/packages;C:/ti/ccstheia151/tirtos_cc13xx_cc26xx_2_21_00_06/products/tidrivers_cc13xx_cc26xx_2_21_00_04/packages;C:/ti/ccstheia151/tirtos_cc13xx_cc26xx_2_21_00_06/products/bios_6_46_01_37/packages;C:/ti/ccstheia151/tirtos_cc13xx_cc26xx_2_21_00_06/products/uia_2_01_00_01/packages
override XDCROOT = C:/ti/ccstheia151/xdctools_3_32_01_22_core
override XDCBUILDCFG = ./config.bld
endif
ifneq (,$(findstring args,$(_USEXDCENV_)))
override XDCARGS = 
override XDCTARGETS = 
endif
#
ifeq (0,1)
PKGPATH = C:/ti/ccstheia151/tirtos_cc13xx_cc26xx_2_21_00_06/packages;C:/ti/ccstheia151/tirtos_cc13xx_cc26xx_2_21_00_06/products/tidrivers_cc13xx_cc26xx_2_21_00_04/packages;C:/ti/ccstheia151/tirtos_cc13xx_cc26xx_2_21_00_06/products/bios_6_46_01_37/packages;C:/ti/ccstheia151/tirtos_cc13xx_cc26xx_2_21_00_06/products/uia_2_01_00_01/packages;C:/ti/ccstheia151/xdctools_3_32_01_22_core/packages;..
HOSTOS = Windows
endif
