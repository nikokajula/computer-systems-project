################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
%.obj: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"C:/ti/ccstheia151/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS/bin/armcl" -mv7M3 --code_state=16 --float_support=none -me --include_path="C:/Users/nikok/workspace_ccstheia/empty_CC2650STK_TI" --include_path="C:/Users/nikok/workspace_ccstheia/empty_CC2650STK_TI" --include_path="C:/ti/ccstheia151/tirtos_cc13xx_cc26xx_2_21_00_06/products/cc26xxware_2_24_03_17272" --include_path="C:/ti/ccstheia151/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS/include" --define=ccs -g --diag_warning=225 --diag_warning=255 --diag_wrap=off --display_error_number --gen_func_subsections=on --abi=eabi --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

build-1067269937:
	@$(MAKE) --no-print-directory -Onone -f subdir_rules.mk build-1067269937-inproc

build-1067269937-inproc: ../empty.cfg
	@echo 'Building file: "$<"'
	@echo 'Invoking: XDCtools'
	"C:/ti/ccstheia151/xdctools_3_32_01_22_core/xs" --xdcpath="C:/ti/ccstheia151/tirtos_cc13xx_cc26xx_2_21_00_06/packages;C:/ti/ccstheia151/tirtos_cc13xx_cc26xx_2_21_00_06/products/tidrivers_cc13xx_cc26xx_2_21_00_04/packages;C:/ti/ccstheia151/tirtos_cc13xx_cc26xx_2_21_00_06/products/bios_6_46_01_37/packages;C:/ti/ccstheia151/tirtos_cc13xx_cc26xx_2_21_00_06/products/uia_2_01_00_01/packages;" xdc.tools.configuro -o configPkg -t ti.targets.arm.elf.M3 -p ti.platforms.simplelink:CC2650F128 -r release -c "C:/ti/ccstheia151/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS" --compileOptions "-mv7M3 --code_state=16 --float_support=none -me --include_path=\"C:/Users/nikok/workspace_ccstheia/empty_CC2650STK_TI\" --include_path=\"C:/Users/nikok/workspace_ccstheia/empty_CC2650STK_TI\" --include_path=\"C:/ti/ccstheia151/tirtos_cc13xx_cc26xx_2_21_00_06/products/cc26xxware_2_24_03_17272\" --include_path=\"C:/ti/ccstheia151/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS/include\" --define=ccs -g --diag_warning=225 --diag_warning=255 --diag_wrap=off --display_error_number --gen_func_subsections=on --abi=eabi  " "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

configPkg/linker.cmd: build-1067269937 ../empty.cfg
configPkg/compiler.opt: build-1067269937
configPkg: build-1067269937


