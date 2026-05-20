################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/adc.c \
../Core/Src/app.c \
../Core/Src/can_comp.c \
../Core/Src/dma.c \
../Core/Src/fdcan.c \
../Core/Src/fan.c \
../Core/Src/gpio.c \
../Core/Src/link.c \
../Core/Src/iwdg.c \
../Core/Src/main.c \
../Core/Src/params.c \
../Core/Src/pumps.c \
../Core/Src/setpoint.c \
../Core/Src/sensors.c \
../Core/Src/stm32g4xx_hal_msp.c \
../Core/Src/stm32g4xx_hal_timebase_tim.c \
../Core/Src/stm32g4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32g4xx.c \
../Core/Src/temp_meas.c \
../Core/Src/thermal_control.c \
../Core/Src/tim.c \
../Core/Src/usart.c \
../Core/Src/valves.c 

OBJS += \
./Core/Src/adc.o \
./Core/Src/app.o \
./Core/Src/can_comp.o \
./Core/Src/dma.o \
./Core/Src/fdcan.o \
./Core/Src/fan.o \
./Core/Src/gpio.o \
./Core/Src/link.o \
./Core/Src/iwdg.o \
./Core/Src/main.o \
./Core/Src/params.o \
./Core/Src/pumps.o \
./Core/Src/setpoint.o \
./Core/Src/sensors.o \
./Core/Src/stm32g4xx_hal_msp.o \
./Core/Src/stm32g4xx_hal_timebase_tim.o \
./Core/Src/stm32g4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32g4xx.o \
./Core/Src/temp_meas.o \
./Core/Src/thermal_control.o \
./Core/Src/tim.o \
./Core/Src/usart.o \
./Core/Src/valves.o 

C_DEPS += \
./Core/Src/adc.d \
./Core/Src/app.d \
./Core/Src/can_comp.d \
./Core/Src/dma.d \
./Core/Src/fdcan.d \
./Core/Src/gpio.d \
./Core/Src/iwdg.d \
./Core/Src/main.d \
./Core/Src/params.d \
./Core/Src/pumps.d \
./Core/Src/sensors.d \
./Core/Src/stm32g4xx_hal_msp.d \
./Core/Src/stm32g4xx_hal_timebase_tim.d \
./Core/Src/stm32g4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32g4xx.d \
./Core/Src/temp_meas.d \
./Core/Src/thermal_control.d \
./Core/Src/tim.d \
./Core/Src/valves.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/adc.cyclo ./Core/Src/adc.d ./Core/Src/adc.o ./Core/Src/adc.su ./Core/Src/app.cyclo ./Core/Src/app.d ./Core/Src/app.o ./Core/Src/app.su ./Core/Src/can_comp.cyclo ./Core/Src/can_comp.d ./Core/Src/can_comp.o ./Core/Src/can_comp.su ./Core/Src/dma.cyclo ./Core/Src/dma.d ./Core/Src/dma.o ./Core/Src/dma.su ./Core/Src/fdcan.cyclo ./Core/Src/fdcan.d ./Core/Src/fdcan.o ./Core/Src/fdcan.su ./Core/Src/gpio.cyclo ./Core/Src/gpio.d ./Core/Src/gpio.o ./Core/Src/gpio.su ./Core/Src/iwdg.cyclo ./Core/Src/iwdg.d ./Core/Src/iwdg.o ./Core/Src/iwdg.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/params.cyclo ./Core/Src/params.d ./Core/Src/params.o ./Core/Src/params.su ./Core/Src/pumps.cyclo ./Core/Src/pumps.d ./Core/Src/pumps.o ./Core/Src/pumps.su ./Core/Src/sensors.cyclo ./Core/Src/sensors.d ./Core/Src/sensors.o ./Core/Src/sensors.su ./Core/Src/stm32g4xx_hal_msp.cyclo ./Core/Src/stm32g4xx_hal_msp.d ./Core/Src/stm32g4xx_hal_msp.o ./Core/Src/stm32g4xx_hal_msp.su ./Core/Src/stm32g4xx_hal_timebase_tim.cyclo ./Core/Src/stm32g4xx_hal_timebase_tim.d ./Core/Src/stm32g4xx_hal_timebase_tim.o ./Core/Src/stm32g4xx_hal_timebase_tim.su ./Core/Src/stm32g4xx_it.cyclo ./Core/Src/stm32g4xx_it.d ./Core/Src/stm32g4xx_it.o ./Core/Src/stm32g4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32g4xx.cyclo ./Core/Src/system_stm32g4xx.d ./Core/Src/system_stm32g4xx.o ./Core/Src/system_stm32g4xx.su ./Core/Src/temp_meas.cyclo ./Core/Src/temp_meas.d ./Core/Src/temp_meas.o ./Core/Src/temp_meas.su ./Core/Src/thermal_control.cyclo ./Core/Src/thermal_control.d ./Core/Src/thermal_control.o ./Core/Src/thermal_control.su ./Core/Src/tim.cyclo ./Core/Src/tim.d ./Core/Src/tim.o ./Core/Src/tim.su ./Core/Src/valves.cyclo ./Core/Src/valves.d ./Core/Src/valves.o ./Core/Src/valves.su

.PHONY: clean-Core-2f-Src

